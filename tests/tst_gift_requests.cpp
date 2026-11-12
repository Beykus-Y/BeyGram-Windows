#include "telegram/GiftRequests.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QJsonObject responseFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

QStringList receivedGiftFlags()
{
    return {
        QStringLiteral("exclude_unsaved"),
        QStringLiteral("exclude_saved"),
        QStringLiteral("exclude_unlimited"),
        QStringLiteral("exclude_upgradable"),
        QStringLiteral("exclude_non_upgradable"),
        QStringLiteral("exclude_upgraded"),
        QStringLiteral("exclude_without_colors"),
        QStringLiteral("exclude_hosted"),
        QStringLiteral("sort_by_price"),
    };
}

} // namespace

class GiftRequestsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsAvailableGiftsRequest();
    void buildsSendGiftRequest();
    void keepsGiftTextVerbatim();
    void buildsFirstReceivedGiftsPage();
    void clampsReceivedGiftsLimit();
    void passesCursorThrough();
    void parsesCatalogGifts();
    void parsesBareGiftEntries();
    void fallsBackToThumbnailFileId();
    void ignoresMalformedGiftResponses();
    void clampsStarCountToIntRange();
    void readsStringEncodedNumbers();
    void preservesLargeIdsThroughJson();
};

void GiftRequestsTest::buildsAvailableGiftsRequest()
{
    const QJsonObject request = getAvailableGiftsRequest();
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getAvailableGifts"));
    QCOMPARE(request.size(), 1);

    const QJsonObject encoded = serialized(request);
    QCOMPARE(encoded.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getAvailableGifts"));
    QCOMPARE(encoded.size(), 1);
}

void GiftRequestsTest::buildsSendGiftRequest()
{
    const QJsonObject request = sendGiftRequest(555, 777, QStringLiteral("Happy birthday"));
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendGift"));
    QCOMPARE(numberAt(request, QStringLiteral("gift_id")), qint64(555));
    QCOMPARE(request.size(), 6);

    // sendGift names its target through owner_id, never chat_id.
    QVERIFY(!request.contains(QStringLiteral("chat_id")));
    const QJsonObject owner =
        request.value(QStringLiteral("owner_id")).toObject();
    QCOMPARE(owner.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(numberAt(owner, QStringLiteral("user_id")), qint64(777));
    QCOMPARE(owner.size(), 2);

    // The schema types text as formattedText, so a plain string would be
    // rejected by TDLib.
    QVERIFY(request.value(QStringLiteral("text")).isObject());
    const QJsonObject text = request.value(QStringLiteral("text")).toObject();
    QCOMPARE(text.value(QStringLiteral("text")).toString(),
             QStringLiteral("Happy birthday"));
    QVERIFY(text.value(QStringLiteral("entities")).isArray());
    QCOMPARE(text.value(QStringLiteral("entities")).toArray().size(), 0);
    QCOMPARE(text.size(), 2);

    QCOMPARE(request.value(QStringLiteral("is_private")).toBool(true), false);
    QCOMPARE(request.value(QStringLiteral("pay_for_upgrade")).toBool(true), false);
}

void GiftRequestsTest::keepsGiftTextVerbatim()
{
    const QString text = QStringLiteral("  Привет!\n\"Gift\" 🎁\ttrailing  \n");
    const QJsonObject request = serialized(sendGiftRequest(1, 2, text));
    const QJsonObject formatted = request.value(QStringLiteral("text")).toObject();
    QCOMPARE(formatted.value(QStringLiteral("text")).toString(), text);
    QCOMPARE(formatted.value(QStringLiteral("entities")).toArray().size(), 0);

    // An empty message is valid: 0 is the lower bound of the schema limit.
    const QJsonObject empty = sendGiftRequest(1, 2, QString());
    QCOMPARE(empty.value(QStringLiteral("text"))
                 .toObject()
                 .value(QStringLiteral("text"))
                 .toString(),
             QString());
}

void GiftRequestsTest::buildsFirstReceivedGiftsPage()
{
    const QJsonObject request = getReceivedGiftsRequest(777, 0, 30);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getReceivedGifts"));
    QCOMPARE(request.value(QStringLiteral("business_connection_id")).toString(),
             QString());
    QCOMPARE(numberAt(request, QStringLiteral("collection_id")), qint64(0));
    QCOMPARE(numberAt(request, QStringLiteral("limit")), qint64(30));

    // The cursor is a string even for the first page.
    QVERIFY(request.value(QStringLiteral("offset")).isString());
    QCOMPARE(request.value(QStringLiteral("offset")).toString(), QString());

    const QJsonObject owner =
        request.value(QStringLiteral("owner_id")).toObject();
    QCOMPARE(owner.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(numberAt(owner, QStringLiteral("user_id")), qint64(777));

    for (const QString& flag : receivedGiftFlags()) {
        QVERIFY2(request.contains(flag), qPrintable(flag));
        QVERIFY2(request.value(flag).isBool(), qPrintable(flag));
        QCOMPARE(request.value(flag).toBool(true), false);
    }

    QCOMPARE(request.size(), 15);
}

void GiftRequestsTest::clampsReceivedGiftsLimit()
{
    const QVector<QPair<int, int>> cases{
        {0, 1},
        {-7, 1},
        {1, 1},
        {30, 30},
        {100, 100},
        {101, 100},
        {5000, 100},
    };

    for (const auto& testCase : cases) {
        const QJsonObject request =
            getReceivedGiftsRequest(42, 0, testCase.first);
        QCOMPARE(numberAt(request, QStringLiteral("limit")),
                 static_cast<qint64>(testCase.second));
    }
}

void GiftRequestsTest::passesCursorThrough()
{
    const QJsonObject first = getReceivedGiftsRequest(42, 0, 30);
    QCOMPARE(first.value(QStringLiteral("offset")).toString(), QString());

    const QJsonObject next =
        getReceivedGiftsRequest(42, QStringLiteral("opaque-next-cursor"), 30);
    QCOMPARE(next.value(QStringLiteral("offset")).toString(),
             QStringLiteral("opaque-next-cursor"));
    QCOMPARE(numberAt(next, QStringLiteral("limit")), qint64(30));

    // A negative integer offset is the first page.
    const QJsonObject negative = getReceivedGiftsRequest(42, -1, 30);
    QCOMPARE(negative.value(QStringLiteral("offset")).toString(), QString());

    // The integer overload cannot carry a real cursor; a positive value is
    // encoded as its decimal string, which TDLib will not match.
    const QJsonObject numeric = getReceivedGiftsRequest(42, 5, 30);
    QCOMPARE(numeric.value(QStringLiteral("offset")).toString(),
             QStringLiteral("5"));

    // The cursor overload clamps the limit as well.
    const QJsonObject clamped =
        getReceivedGiftsRequest(42, QStringLiteral("cursor"), 500);
    QCOMPARE(numberAt(clamped, QStringLiteral("limit")), qint64(100));
}

void GiftRequestsTest::parsesCatalogGifts()
{
    const QJsonObject response = responseFromJson(R"({
        "@type": "availableGifts",
        "gifts": [
            {
                "@type": "availableGift",
                "gift": {
                    "@type": "gift",
                    "id": 100,
                    "star_count": 25,
                    "sticker": {
                        "@type": "sticker",
                        "emoji": "🎁",
                        "sticker": { "@type": "file", "id": 9 },
                        "thumbnail": { "file": { "id": 5 } }
                    }
                },
                "resale_count": 3,
                "min_resale_star_count": 100,
                "title": "Plush Pepe"
            },
            {
                "@type": "availableGift",
                "gift": {
                    "@type": "gift",
                    "id": 101,
                    "star_count": 50,
                    "sticker": {
                        "@type": "sticker",
                        "sticker": { "@type": "file", "id": 10 }
                    }
                },
                "resale_count": 0,
                "min_resale_star_count": 0,
                "title": ""
            },
            { "@type": "availableGift", "gift": null }
        ]
    })");

    const QVector<GiftInfo> gifts = giftsFromResponse(response);
    QCOMPARE(gifts.size(), 2);

    QCOMPARE(gifts.at(0).id, qint64(100));
    QCOMPARE(gifts.at(0).title, QStringLiteral("Plush Pepe"));
    QCOMPARE(gifts.at(0).starCount, 25);
    QCOMPARE(gifts.at(0).fileId, 9);

    QCOMPARE(gifts.at(1).id, qint64(101));
    QVERIFY(gifts.at(1).title.isEmpty());
    QCOMPARE(gifts.at(1).starCount, 50);
    QCOMPARE(gifts.at(1).fileId, 10);
}

void GiftRequestsTest::parsesBareGiftEntries()
{
    const QJsonObject response = responseFromJson(R"({
        "gifts": [
            {
                "@type": "gift",
                "id": 7,
                "star_count": 15,
                "sticker": { "sticker": { "id": 11 } }
            },
            {
                "@type": "upgradedGift",
                "id": 8,
                "title": "Plush Pepe",
                "model": { "sticker": { "id": 12 } }
            }
        ]
    })");

    const QVector<GiftInfo> gifts = giftsFromResponse(response);
    QCOMPARE(gifts.size(), 2);

    QCOMPARE(gifts.at(0).id, qint64(7));
    QVERIFY(gifts.at(0).title.isEmpty());
    QCOMPARE(gifts.at(0).starCount, 15);
    QCOMPARE(gifts.at(0).fileId, 11);

    // A bare upgraded gift has its title on the gift and no star_count.
    QCOMPARE(gifts.at(1).id, qint64(8));
    QCOMPARE(gifts.at(1).title, QStringLiteral("Plush Pepe"));
    QCOMPARE(gifts.at(1).starCount, 0);
    QCOMPARE(gifts.at(1).fileId, 0);
}

void GiftRequestsTest::fallsBackToThumbnailFileId()
{
    const QJsonObject response = responseFromJson(R"({
        "gifts": [
            {
                "gift": {
                    "id": 5,
                    "star_count": 10,
                    "sticker": { "thumbnail": { "file": { "id": 42 } } }
                }
            },
            {
                "gift": {
                    "id": 6,
                    "sticker": {
                        "sticker": { "id": 0 },
                        "thumbnail": { "file": { "id": "9007199254740993" } }
                    }
                }
            }
        ]
    })");

    const QVector<GiftInfo> gifts = giftsFromResponse(response);
    QCOMPARE(gifts.size(), 2);
    QCOMPARE(gifts.at(0).fileId, 42);
    // A file id outside the int32 range is "no file", not a truncation.
    QCOMPARE(gifts.at(1).fileId, 0);
}

void GiftRequestsTest::ignoresMalformedGiftResponses()
{
    QVERIFY(giftsFromResponse(QJsonObject()).isEmpty());
    QVERIFY(giftsFromResponse(responseFromJson(R"({ "gifts": null })")).isEmpty());
    QVERIFY(giftsFromResponse(responseFromJson(R"({ "gifts": {} })")).isEmpty());
    QVERIFY(giftsFromResponse(responseFromJson(R"({ "gifts": "gift" })")).isEmpty());
    QVERIFY(giftsFromResponse(responseFromJson(R"({ "gifts": [] })")).isEmpty());

    const QJsonObject malformed = responseFromJson(R"({
        "gifts": [
            null,
            5,
            "gift",
            {},
            { "gift": null },
            { "gift": {} },
            { "gift": { "id": 0 } },
            { "gift": { "id": -3, "star_count": 10 } }
        ]
    })");
    QVERIFY(giftsFromResponse(malformed).isEmpty());
}

void GiftRequestsTest::clampsStarCountToIntRange()
{
    const QJsonObject response = responseFromJson(R"({
        "gifts": [
            { "gift": { "id": 1, "star_count": 2147483648 } },
            { "gift": { "id": 2, "star_count": -5 } },
            { "gift": { "id": 3 } },
            { "gift": { "id": 4, "star_count": 2147483647 } }
        ]
    })");

    const QVector<GiftInfo> gifts = giftsFromResponse(response);
    QCOMPARE(gifts.size(), 4);
    QCOMPARE(gifts.at(0).starCount, std::numeric_limits<int>::max());
    QCOMPARE(gifts.at(1).starCount, 0);
    QCOMPARE(gifts.at(2).starCount, 0);
    QCOMPARE(gifts.at(3).starCount, std::numeric_limits<int>::max());
}

void GiftRequestsTest::readsStringEncodedNumbers()
{
    // tdjson can hand int64 and int32 values over as strings to preserve
    // precision, so the parser accepts both encodings.
    const QJsonObject response = responseFromJson(R"({
        "gifts": [
            {
                "title": "Resale",
                "gift": {
                    "id": "1234567890123",
                    "star_count": "75",
                    "sticker": { "sticker": { "id": "44" } }
                }
            }
        ]
    })");

    const QVector<GiftInfo> gifts = giftsFromResponse(response);
    QCOMPARE(gifts.size(), 1);
    QCOMPARE(gifts.at(0).id, qint64(1234567890123LL));
    QCOMPARE(gifts.at(0).title, QStringLiteral("Resale"));
    QCOMPARE(gifts.at(0).starCount, 75);
    QCOMPARE(gifts.at(0).fileId, 44);
}

void GiftRequestsTest::preservesLargeIdsThroughJson()
{
    const QJsonObject request = serialized(
        sendGiftRequest(1234567890123LL, 987654321098LL, QStringLiteral("text")));
    QCOMPARE(numberAt(request, QStringLiteral("gift_id")), qint64(1234567890123LL));
    QCOMPARE(numberAt(request.value(QStringLiteral("owner_id")).toObject(),
                      QStringLiteral("user_id")),
             qint64(987654321098LL));

    const QJsonObject response = serialized(responseFromJson(R"({
        "gifts": [
            {
                "gift": {
                    "id": 1234567890124,
                    "star_count": 500,
                    "sticker": { "sticker": { "id": 45 } }
                }
            }
        ]
    })"));

    const QVector<GiftInfo> gifts = giftsFromResponse(response);
    QCOMPARE(gifts.size(), 1);
    QCOMPARE(gifts.at(0).id, qint64(1234567890124LL));
    QCOMPARE(gifts.at(0).starCount, 500);
    QCOMPARE(gifts.at(0).fileId, 45);
}

QTEST_GUILESS_MAIN(GiftRequestsTest)
#include "tst_gift_requests.moc"
