#include "telegram/StickerSend.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject contentOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("input_message_content")).toObject();
}

QJsonObject stickerOf(const QJsonObject& request)
{
    return contentOf(request).value(QStringLiteral("sticker")).toObject();
}

QJsonObject stickerFileOf(const QJsonObject& request)
{
    return stickerOf(request).value(QStringLiteral("sticker")).toObject();
}

} // namespace

class StickerSendTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsStickerRequest();
    void keepsReplyTarget();
    void omitsReplyWithoutTarget();
    void referencesFileById();
    void fallsBackToRemoteIdentifier();
    void requestHasNoExtraOrUrls();
};

void StickerSendTest::buildsStickerRequest()
{
    const QJsonObject request = sendStickerRequest(42, 9876543210LL, 55);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendMessage"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(42));
    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QVERIFY(request.value(QStringLiteral("reply_to")).isNull());
    QVERIFY(request.value(QStringLiteral("options")).isNull());
    QVERIFY(request.value(QStringLiteral("reply_markup")).isNull());

    const QJsonObject content = contentOf(request);
    QCOMPARE(content.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageSticker"));
    QCOMPARE(content.value(QStringLiteral("emoji")).toString(), QString());
    QVERIFY(content.value(QStringLiteral("emoji")).isString());

    const QJsonObject sticker = stickerOf(request);
    QCOMPARE(sticker.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputSticker"));
    QVERIFY(sticker.value(QStringLiteral("thumbnail")).isNull());
    QCOMPARE(sticker.value(QStringLiteral("width")).toInt(), 0);
    QCOMPARE(sticker.value(QStringLiteral("height")).toInt(), 0);
}

void StickerSendTest::keepsReplyTarget()
{
    const QJsonObject request = sendStickerRequest(42, 9876543210LL, 55, 777);

    const QJsonObject reply = request.value(QStringLiteral("reply_to")).toObject();
    QCOMPARE(reply.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageReplyToMessage"));
    QCOMPARE(reply.value(QStringLiteral("message_id")).toVariant().toLongLong(),
             qint64(777));
    QVERIFY(reply.value(QStringLiteral("quote")).isNull());

    const QJsonObject large = sendStickerRequest(42, 1, 2, 1234567890123LL);
    QCOMPARE(large.value(QStringLiteral("reply_to"))
                 .toObject()
                 .value(QStringLiteral("message_id"))
                 .toVariant()
                 .toLongLong(),
             qint64(1234567890123LL));
}

void StickerSendTest::omitsReplyWithoutTarget()
{
    QVERIFY(sendStickerRequest(42, 1, 2).value(QStringLiteral("reply_to")).isNull());
    QVERIFY(sendStickerRequest(42, 1, 2, 0).value(QStringLiteral("reply_to")).isNull());
    QVERIFY(sendStickerRequest(42, 1, 2, -5).value(QStringLiteral("reply_to")).isNull());
}

void StickerSendTest::referencesFileById()
{
    const QJsonObject request = sendStickerRequest(-1001234567890LL, 7, 1234);
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(-1001234567890LL));

    const QJsonObject file = stickerFileOf(request);
    QCOMPARE(file.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputFileId"));
    QCOMPARE(file.value(QStringLiteral("id")).toInt(), 1234);
}

void StickerSendTest::fallsBackToRemoteIdentifier()
{
    const QJsonObject file = stickerFileOf(sendStickerRequest(42, 9876543210LL, 0));
    QCOMPARE(file.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputFileRemote"));
    QCOMPARE(file.value(QStringLiteral("id")).toString(),
             QStringLiteral("9876543210"));

    const QJsonObject negative = stickerFileOf(sendStickerRequest(42, 5, -1));
    QCOMPARE(negative.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputFileRemote"));
    QCOMPARE(negative.value(QStringLiteral("id")).toString(), QStringLiteral("5"));
}

void StickerSendTest::requestHasNoExtraOrUrls()
{
    const QJsonObject request = sendStickerRequest(42, 9876543210LL, 55, 7);
    QVERIFY(!request.contains(QStringLiteral("@extra")));

    const QByteArray json = QJsonDocument(request).toJson(QJsonDocument::Compact);
    QVERIFY(!json.isEmpty());
    const QByteArray lower = json.toLower();
    QVERIFY(!lower.contains("http://"));
    QVERIFY(!lower.contains("https://"));
    QVERIFY(!lower.contains("file://"));
}

QTEST_GUILESS_MAIN(StickerSendTest)
#include "tst_sticker_send.moc"
