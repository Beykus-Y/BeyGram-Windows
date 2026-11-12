#include "telegram/PaidContent.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QJsonObject contentFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

QJsonArray idsAt(const QJsonObject& request)
{
    return request.value(QStringLiteral("message_ids")).toArray();
}

} // namespace

class PaidContentTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesPaidMediaContent();
    void readsStringEncodedStarCount();
    void readsOptionalMessageIds();
    void capsOptionalMessageIds();
    void rejectsMalformedContent();
    void buildsGetPaidMediaRequest();
    void forwardsPreviewFlag();
    void filtersDedupesAndCapsIds();
    void rejectsInvalidRequests();
    void preservesLargeIdsThroughSerialization();
    void openPrivacyRequestHasOnlyType();
};

void PaidContentTest::parsesPaidMediaContent()
{
    const QJsonObject content = contentFromJson(R"({
        "@type": "messagePaidMedia",
        "star_count": 25,
        "media": [
            {
                "@type": "paidMediaPreview",
                "width": 640,
                "height": 480,
                "duration": 12
            },
            {
                "@type": "paidMediaPhoto",
                "photo": { "@type": "photo", "sizes": [] }
            }
        ],
        "caption": { "@type": "formattedText", "text": "Full set", "entities": [] },
        "show_caption_above_media": false
    })");

    const PaidMediaInfo info = paidMediaFromContent(content);
    QCOMPARE(info.starCount, qint64(25));
    QCOMPARE(info.partCount, 2);
    QVERIFY(info.messageIds.isEmpty());
}

void PaidContentTest::readsStringEncodedStarCount()
{
    // tdjson can hand int53 values over as strings to preserve precision.
    const QJsonObject content = contentFromJson(R"({
        "@type": "messagePaidMedia",
        "star_count": "75",
        "media": [{ "@type": "paidMediaUnsupported" }]
    })");

    const PaidMediaInfo info = paidMediaFromContent(content);
    QCOMPARE(info.starCount, qint64(75));
    QCOMPARE(info.partCount, 1);
}

void PaidContentTest::readsOptionalMessageIds()
{
    // messagePaidMedia itself has no message ids, but a wrapper for a bundle
    // may carry them. Non-positive and duplicate entries are dropped.
    const QJsonObject content = contentFromJson(R"({
        "@type": "messagePaidMedia",
        "star_count": 5,
        "media": [{ "@type": "paidMediaPreview" }],
        "message_ids": [9, 0, -2, 9, "10", 11, 10]
    })");

    const PaidMediaInfo info = paidMediaFromContent(content);
    QCOMPARE(info.messageIds.size(), 3);
    QCOMPARE(info.messageIds.at(0), qint64(9));
    QCOMPARE(info.messageIds.at(1), qint64(10));
    QCOMPARE(info.messageIds.at(2), qint64(11));

    // Optional metadata of the wrong shape does not make valid content
    // malformed; it simply carries no ids.
    const PaidMediaInfo ignored = paidMediaFromContent(contentFromJson(R"({
        "@type": "messagePaidMedia",
        "star_count": 5,
        "media": [{ "@type": "paidMediaPreview" }],
        "message_ids": 9
    })"));
    QCOMPARE(ignored.starCount, qint64(5));
    QVERIFY(ignored.messageIds.isEmpty());
}

void PaidContentTest::capsOptionalMessageIds()
{
    QJsonArray ids;
    for (int i = 1; i <= 150; ++i)
        ids.append(i);

    QJsonArray media;
    media.append(QJsonObject{});

    QJsonObject content;
    content.insert(QStringLiteral("@type"), QStringLiteral("messagePaidMedia"));
    content.insert(QStringLiteral("star_count"), 5);
    content.insert(QStringLiteral("media"), media);
    content.insert(QStringLiteral("message_ids"), ids);

    const PaidMediaInfo info = paidMediaFromContent(content);
    QCOMPARE(info.messageIds.size(), 100);
    QCOMPARE(info.messageIds.first(), qint64(1));
    QCOMPARE(info.messageIds.last(), qint64(100));
}

void PaidContentTest::rejectsMalformedContent()
{
    const QVector<const char*> malformed{
        R"({})",
        R"({ "@type": "messageText", "star_count": 5, "media": [{}] })",
        R"({ "@type": "messagePaidMedia", "media": [{}] })",
        R"({ "@type": "messagePaidMedia", "star_count": 0, "media": [{}] })",
        R"({ "@type": "messagePaidMedia", "star_count": -3, "media": [{}] })",
        R"({ "@type": "messagePaidMedia", "star_count": "abc", "media": [{}] })",
        R"({ "@type": "messagePaidMedia", "star_count": 5 })",
        R"({ "@type": "messagePaidMedia", "star_count": 5, "media": null })",
        R"({ "@type": "messagePaidMedia", "star_count": 5, "media": {} })",
        R"({ "@type": "messagePaidMedia", "star_count": 5, "media": [] })",
        R"({ "@type": "messagePaidMedia", "star_count": 5, "media": [null] })",
        R"({ "@type": "messagePaidMedia", "star_count": 5, "media": ["photo"] })",
    };

    for (const char* json : malformed) {
        const PaidMediaInfo info = paidMediaFromContent(contentFromJson(json));
        QVERIFY2(info.starCount == 0, json);
        QVERIFY2(info.partCount == 0, json);
        QVERIFY2(info.messageIds.isEmpty(), json);
    }

    // An untagged content object is accepted when the paid-media fields are
    // otherwise complete.
    const PaidMediaInfo untagged = paidMediaFromContent(contentFromJson(R"({
        "star_count": 7,
        "media": [{ "@type": "paidMediaPreview" }]
    })"));
    QCOMPARE(untagged.starCount, qint64(7));
    QCOMPARE(untagged.partCount, 1);
}

void PaidContentTest::buildsGetPaidMediaRequest()
{
    const QJsonObject request =
        serialized(getPaidMediaRequest(42, QVector<qint64>{7, 8}, false));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getPaidMedia"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(request.size(), 4);
    QVERIFY(!request.contains(QStringLiteral("@extra")));

    const QJsonArray ids = idsAt(request);
    QCOMPARE(ids.size(), 2);
    QCOMPARE(ids.at(0).toVariant().toLongLong(), qint64(7));
    QCOMPARE(ids.at(1).toVariant().toLongLong(), qint64(8));

    QVERIFY(request.contains(QStringLiteral("only_preview")));
    QVERIFY(!request.value(QStringLiteral("only_preview")).toBool(true));
}

void PaidContentTest::forwardsPreviewFlag()
{
    const QJsonObject previews =
        serialized(getPaidMediaRequest(42, QVector<qint64>{7}, true));
    QVERIFY(previews.value(QStringLiteral("only_preview")).toBool(false));

    const QJsonObject full =
        serialized(getPaidMediaRequest(42, QVector<qint64>{7}, false));
    QVERIFY(!full.value(QStringLiteral("only_preview")).toBool(true));
}

void PaidContentTest::filtersDedupesAndCapsIds()
{
    const QJsonObject filtered = serialized(
        getPaidMediaRequest(42, QVector<qint64>{0, -1, 5, 5, 7, 7, 9, -3}, false));
    const QJsonArray ids = idsAt(filtered);
    QCOMPARE(ids.size(), 3);
    QCOMPARE(ids.at(0).toVariant().toLongLong(), qint64(5));
    QCOMPARE(ids.at(1).toVariant().toLongLong(), qint64(7));
    QCOMPARE(ids.at(2).toVariant().toLongLong(), qint64(9));

    QVector<qint64> many;
    for (qint64 i = 1; i <= 150; ++i)
        many.append(i);
    const QJsonArray capped = idsAt(serialized(getPaidMediaRequest(42, many, false)));
    QCOMPARE(capped.size(), 100);
    QCOMPARE(capped.first().toVariant().toLongLong(), qint64(1));
    QCOMPARE(capped.last().toVariant().toLongLong(), qint64(100));
}

void PaidContentTest::rejectsInvalidRequests()
{
    QVERIFY(getPaidMediaRequest(0, QVector<qint64>{7}, false).isEmpty());
    QVERIFY(getPaidMediaRequest(-1, QVector<qint64>{7}, false).isEmpty());
    QVERIFY(getPaidMediaRequest(42, QVector<qint64>{}, false).isEmpty());
    QVERIFY(getPaidMediaRequest(42, QVector<qint64>{0, -1, -5}, false).isEmpty());
}

void PaidContentTest::preservesLargeIdsThroughSerialization()
{
    // Above 2^53, so a double round-trip would lose precision.
    const qint64 chatId = 9007199254740993LL;
    const qint64 messageId = 9007199254740994LL;

    const QJsonObject request = serialized(
        getPaidMediaRequest(chatId, QVector<qint64>{messageId}, false));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), chatId);
    QCOMPARE(idsAt(request).at(0).toVariant().toLongLong(), messageId);
}

void PaidContentTest::openPrivacyRequestHasOnlyType()
{
    const QJsonObject request = serialized(openPaidReactionPrivacyRequest());
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("openPaidReactionPrivacy"));
    QCOMPARE(request.size(), 1);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

QTEST_GUILESS_MAIN(PaidContentTest)
#include "tst_paid_content.moc"
