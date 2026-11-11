#include "telegram/TypingStatus.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

// Requests are compared after a JSON round trip so the assertions see exactly
// what would be handed to TDLib.
QJsonObject roundTrip(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
}

QJsonObject actionOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("action")).toObject();
}

} // namespace

class TypingStatusTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsTypingRequest();
    void buildsUploadPhotoRequest();
    void buildsUploadDocumentRequest();
    void keepsLargeChatIds();
    void doesNotSendMessageThreadId();
    void labelsKnownActions();
    void clearsForCancelAndUndisplayedActions();
    void requestsSurviveSerialization();
};

void TypingStatusTest::buildsTypingRequest()
{
    const QJsonObject request = sendTypingRequest(42);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendChatAction"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(42));
    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QVERIFY(request.value(QStringLiteral("business_connection_id")).isString());
    QCOMPARE(request.value(QStringLiteral("business_connection_id")).toString(),
             QString());

    const QJsonObject action = actionOf(request);
    QCOMPARE(action.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatActionTyping"));
    // chatActionTyping has no fields; only uploading actions carry progress.
    QCOMPARE(action.size(), 1);
}

void TypingStatusTest::buildsUploadPhotoRequest()
{
    const QJsonObject request = sendUploadPhotoRequest(42);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendChatAction"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(42));

    const QJsonObject action = actionOf(request);
    QCOMPARE(action.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatActionUploadingPhoto"));
    QCOMPARE(action.value(QStringLiteral("progress")).toInt(), 0);
}

void TypingStatusTest::buildsUploadDocumentRequest()
{
    const QJsonObject request = sendUploadDocumentRequest(42);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendChatAction"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(42));

    const QJsonObject action = actionOf(request);
    QCOMPARE(action.value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatActionUploadingDocument"));
    QCOMPARE(action.value(QStringLiteral("progress")).toInt(), 0);
}

void TypingStatusTest::keepsLargeChatIds()
{
    // Telegram chat ids exceed 32 bits; they must not be truncated.
    const qint64 chatId = 4242424242LL;
    QCOMPARE(sendTypingRequest(chatId)
                 .value(QStringLiteral("chat_id"))
                 .toVariant()
                 .toLongLong(),
             chatId);
    QCOMPARE(sendUploadPhotoRequest(chatId)
                 .value(QStringLiteral("chat_id"))
                 .toVariant()
                 .toLongLong(),
             chatId);
    QCOMPARE(sendUploadDocumentRequest(chatId)
                 .value(QStringLiteral("chat_id"))
                 .toVariant()
                 .toLongLong(),
             chatId);
}

void TypingStatusTest::doesNotSendMessageThreadId()
{
    // This TDLib schema uses the nullable topic_id; message_thread_id is not a
    // field of sendChatAction and would be rejected as unknown.
    const QVector<QJsonObject> requests{
        sendTypingRequest(1),
        sendUploadPhotoRequest(1),
        sendUploadDocumentRequest(1),
    };
    for (const QJsonObject& request : requests) {
        QVERIFY(!request.contains(QStringLiteral("message_thread_id")));
        QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    }
}

void TypingStatusTest::labelsKnownActions()
{
    QCOMPARE(chatActionLabel(QStringLiteral("chatActionTyping")),
             QStringLiteral("typing…"));
    QCOMPARE(chatActionLabel(QStringLiteral("chatActionUploadingPhoto")),
             QStringLiteral("sending a photo…"));
    QCOMPARE(chatActionLabel(QStringLiteral("chatActionUploadingDocument")),
             QStringLiteral("sending a file…"));
}

void TypingStatusTest::clearsForCancelAndUndisplayedActions()
{
    QCOMPARE(chatActionLabel(QStringLiteral("chatActionCancel")), QString());
    QCOMPARE(chatActionLabel(QStringLiteral("chatActionRecordingVideo")), QString());
    QCOMPARE(chatActionLabel(QStringLiteral("chatActionStartPlayingGame")), QString());
    QCOMPARE(chatActionLabel(QStringLiteral("notAnAction")), QString());
    QCOMPARE(chatActionLabel(QString()), QString());
}

void TypingStatusTest::requestsSurviveSerialization()
{
    const QJsonObject typing = roundTrip(sendTypingRequest(7));
    QCOMPARE(typing.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendChatAction"));
    QCOMPARE(typing.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(7));
    QCOMPARE(actionOf(typing).value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatActionTyping"));

    const QJsonObject photo = roundTrip(sendUploadPhotoRequest(7));
    QCOMPARE(actionOf(photo).value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatActionUploadingPhoto"));
    QCOMPARE(actionOf(photo).value(QStringLiteral("progress")).toInt(), 0);

    const QJsonObject document = roundTrip(sendUploadDocumentRequest(7));
    QCOMPARE(actionOf(document).value(QStringLiteral("@type")).toString(),
             QStringLiteral("chatActionUploadingDocument"));
    QCOMPARE(actionOf(document).value(QStringLiteral("progress")).toInt(), 0);
}

QTEST_GUILESS_MAIN(TypingStatusTest)
#include "tst_typing_status.moc"
