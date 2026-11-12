#include "telegram/DraftSync.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& request)
{
    return QJsonDocument::fromJson(QJsonDocument(request).toJson(QJsonDocument::Compact)).object();
}

qint64 idOf(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

QJsonObject updateFromJson(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

QJsonObject draftMessageFrom(const QJsonObject& setRequest)
{
    return setRequest.value(QStringLiteral("draft_message")).toObject();
}

QJsonObject contentFrom(const QJsonObject& draft)
{
    return draft.value(QStringLiteral("content")).toObject();
}

} // namespace

class DraftSyncTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsSetRequestShape();
    void sendsNoReplyWhenTargetIsMissing();
    void sendsEmptyTextVerbatim();
    void buildsClearRequest();
    void parsesTextAndReply();
    void parsesNullDraftAsEmpty();
    void parsesMissingDraftAsEmpty();
    void parsesStringIds();
    void parsesReplyToStoryAsNoReply();
    void parsesLegacyInputMessageText();
    void ignoresOtherUpdates();
    void ignoresMalformedDraft();
    void preservesLargeIdsThroughJson();
};

void DraftSyncTest::buildsSetRequestShape()
{
    const QJsonObject request =
        serialized(setChatDraftMessageRequest(42, QStringLiteral("Hello there"), 77));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setChatDraftMessage"));
    QCOMPARE(idOf(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(request.value(QStringLiteral("topic_id")).type(), QJsonValue::Null);
    QCOMPARE(int(request.size()), 4);

    QVERIFY(request.value(QStringLiteral("draft_message")).isObject());
    const QJsonObject draft = draftMessageFrom(request);
    QCOMPARE(draft.value(QStringLiteral("@type")).toString(), QStringLiteral("draftMessage"));
    QVERIFY(draft.value(QStringLiteral("date")).isDouble());
    QVERIFY(idOf(draft, QStringLiteral("date")) > 0);
    QCOMPARE(idOf(draft, QStringLiteral("effect_id")), qint64(0));
    QCOMPARE(draft.value(QStringLiteral("suggested_post_info")).type(), QJsonValue::Null);

    const QJsonObject reply = draft.value(QStringLiteral("reply_to")).toObject();
    QCOMPARE(reply.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageReplyToMessage"));
    QCOMPARE(idOf(reply, QStringLiteral("message_id")), qint64(77));
    QCOMPARE(reply.value(QStringLiteral("quote")).type(), QJsonValue::Null);
    QCOMPARE(idOf(reply, QStringLiteral("checklist_task_id")), qint64(0));
    QCOMPARE(reply.value(QStringLiteral("poll_option_id")).toString(), QString());

    const QJsonObject content = contentFrom(draft);
    QCOMPARE(content.value(QStringLiteral("@type")).toString(),
             QStringLiteral("draftMessageContentText"));
    QCOMPARE(content.value(QStringLiteral("link_preview_options")).type(), QJsonValue::Null);

    const QJsonObject text = content.value(QStringLiteral("text")).toObject();
    QCOMPARE(text.value(QStringLiteral("@type")).toString(), QStringLiteral("formattedText"));
    QCOMPARE(text.value(QStringLiteral("text")).toString(), QStringLiteral("Hello there"));
    QVERIFY(text.value(QStringLiteral("entities")).isArray());
    QCOMPARE(int(text.value(QStringLiteral("entities")).toArray().size()), 0);
}

void DraftSyncTest::sendsNoReplyWhenTargetIsMissing()
{
    for (const qint64 replyTo : {qint64(0), qint64(-7)}) {
        const QJsonObject request =
            serialized(setChatDraftMessageRequest(42, QStringLiteral("No reply"), replyTo));
        const QJsonObject draft = draftMessageFrom(request);

        QCOMPARE(draft.value(QStringLiteral("reply_to")).type(), QJsonValue::Null);
        QCOMPARE(contentFrom(draft).value(QStringLiteral("text")).toObject()
                     .value(QStringLiteral("text")).toString(),
                 QStringLiteral("No reply"));
    }
}

void DraftSyncTest::sendsEmptyTextVerbatim()
{
    const QJsonObject request = serialized(setChatDraftMessageRequest(42, QString()));
    const QJsonObject draft = draftMessageFrom(request);

    QCOMPARE(draft.value(QStringLiteral("reply_to")).type(), QJsonValue::Null);
    const QJsonObject text = contentFrom(draft).value(QStringLiteral("text")).toObject();
    QCOMPARE(text.value(QStringLiteral("text")).toString(), QString());
    QCOMPARE(int(text.value(QStringLiteral("entities")).toArray().size()), 0);
}

void DraftSyncTest::buildsClearRequest()
{
    const QJsonObject request = serialized(clearChatDraftRequest(42));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setChatDraftMessage"));
    QCOMPARE(idOf(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(request.value(QStringLiteral("topic_id")).type(), QJsonValue::Null);
    QCOMPARE(request.value(QStringLiteral("draft_message")).type(), QJsonValue::Null);
    QCOMPARE(int(request.size()), 4);
}

void DraftSyncTest::parsesTextAndReply()
{
    const QJsonObject update = updateFromJson(R"({
        "@type": "updateChatDraftMessage",
        "chat_id": 42,
        "draft_message": {
            "@type": "draftMessage",
            "reply_to": {
                "@type": "inputMessageReplyToMessage",
                "message_id": 77,
                "quote": null,
                "checklist_task_id": 0,
                "poll_option_id": ""
            },
            "date": 1700000000,
            "content": {
                "@type": "draftMessageContentText",
                "text": {
                    "@type": "formattedText",
                    "text": "Hi there",
                    "entities": []
                },
                "link_preview_options": null
            },
            "effect_id": 0,
            "suggested_post_info": null
        },
        "positions": []
    })");

    const ChatDraft draft = draftFromUpdate(update);
    QCOMPARE(draft.chatId, qint64(42));
    QCOMPARE(draft.replyToMessageId, qint64(77));
    QCOMPARE(draft.text, QStringLiteral("Hi there"));
    QVERIFY(!draft.unsynced);
}

void DraftSyncTest::parsesNullDraftAsEmpty()
{
    const QJsonObject update = updateFromJson(R"({
        "@type": "updateChatDraftMessage",
        "chat_id": 42,
        "draft_message": null,
        "positions": []
    })");

    const ChatDraft draft = draftFromUpdate(update);
    QCOMPARE(draft.chatId, qint64(42));
    QCOMPARE(draft.replyToMessageId, qint64(0));
    QVERIFY(draft.text.isEmpty());
    QVERIFY(!draft.unsynced);
}

void DraftSyncTest::parsesMissingDraftAsEmpty()
{
    const QJsonObject update = updateFromJson(R"({
        "@type": "updateChatDraftMessage",
        "chat_id": 42
    })");

    const ChatDraft draft = draftFromUpdate(update);
    QCOMPARE(draft.chatId, qint64(42));
    QCOMPARE(draft.replyToMessageId, qint64(0));
    QVERIFY(draft.text.isEmpty());
}

void DraftSyncTest::parsesStringIds()
{
    const QJsonObject update = updateFromJson(R"({
        "@type": "updateChatDraftMessage",
        "chat_id": "42",
        "draft_message": {
            "reply_to": {
                "@type": "inputMessageReplyToMessage",
                "message_id": "1234567890123"
            },
            "content": {
                "@type": "draftMessageContentText",
                "text": { "@type": "formattedText", "text": "string ids", "entities": [] }
            }
        }
    })");

    const ChatDraft draft = draftFromUpdate(update);
    QCOMPARE(draft.chatId, qint64(42));
    QCOMPARE(draft.replyToMessageId, qint64(1234567890123LL));
    QCOMPARE(draft.text, QStringLiteral("string ids"));
}

void DraftSyncTest::parsesReplyToStoryAsNoReply()
{
    const QJsonObject update = updateFromJson(R"({
        "@type": "updateChatDraftMessage",
        "chat_id": 42,
        "draft_message": {
            "reply_to": {
                "@type": "inputMessageReplyToStory",
                "story_poster_chat_id": 900,
                "story_id": 5
            },
            "content": {
                "@type": "draftMessageContentText",
                "text": { "@type": "formattedText", "text": "story", "entities": [] }
            }
        }
    })");

    const ChatDraft draft = draftFromUpdate(update);
    QCOMPARE(draft.replyToMessageId, qint64(0));
    QCOMPARE(draft.text, QStringLiteral("story"));
}

void DraftSyncTest::parsesLegacyInputMessageText()
{
    // Revisions before DraftMessageContent used inputMessageText directly.
    const QJsonObject update = updateFromJson(R"({
        "@type": "updateChatDraftMessage",
        "chat_id": 42,
        "draft_message": {
            "reply_to": null,
            "input_message_text": {
                "@type": "inputMessageText",
                "text": { "@type": "formattedText", "text": "legacy draft", "entities": [] }
            }
        }
    })");

    const ChatDraft draft = draftFromUpdate(update);
    QCOMPARE(draft.chatId, qint64(42));
    QCOMPARE(draft.replyToMessageId, qint64(0));
    QCOMPARE(draft.text, QStringLiteral("legacy draft"));
}

void DraftSyncTest::ignoresOtherUpdates()
{
    QCOMPARE(draftFromUpdate(QJsonObject{}).chatId, qint64(0));

    const QJsonObject update = updateFromJson(R"({
        "@type": "updateChatLastMessage",
        "chat_id": 42,
        "draft_message": {
            "content": {
                "@type": "draftMessageContentText",
                "text": { "@type": "formattedText", "text": "not a draft", "entities": [] }
            }
        }
    })");

    const ChatDraft draft = draftFromUpdate(update);
    QCOMPARE(draft.chatId, qint64(0));
    QVERIFY(draft.text.isEmpty());
}

void DraftSyncTest::ignoresMalformedDraft()
{
    const QJsonObject stringDraft = updateFromJson(R"({
        "@type": "updateChatDraftMessage",
        "chat_id": 42,
        "draft_message": "not an object"
    })");
    const ChatDraft fromString = draftFromUpdate(stringDraft);
    QCOMPARE(fromString.chatId, qint64(42));
    QVERIFY(fromString.text.isEmpty());

    const QJsonObject unknownContent = updateFromJson(R"({
        "@type": "updateChatDraftMessage",
        "chat_id": 42,
        "draft_message": {
            "reply_to": null,
            "content": { "@type": "draftMessageContentVideoNote", "file_path": "x" }
        }
    })");
    const ChatDraft fromUnknown = draftFromUpdate(unknownContent);
    QCOMPARE(fromUnknown.chatId, qint64(42));
    QCOMPARE(fromUnknown.replyToMessageId, qint64(0));
    QVERIFY(fromUnknown.text.isEmpty());
}

void DraftSyncTest::preservesLargeIdsThroughJson()
{
    const qint64 chatId = -1001234567890LL;
    const qint64 messageId = 1234567890123LL;

    const QJsonObject set =
        serialized(setChatDraftMessageRequest(chatId, QStringLiteral("x"), messageId));
    QCOMPARE(idOf(set, QStringLiteral("chat_id")), chatId);

    const QJsonObject draft = draftMessageFrom(set);
    QCOMPARE(idOf(draft.value(QStringLiteral("reply_to")).toObject(),
                  QStringLiteral("message_id")),
             messageId);

    const QJsonObject clear = serialized(clearChatDraftRequest(chatId));
    QCOMPARE(idOf(clear, QStringLiteral("chat_id")), chatId);
}

QTEST_GUILESS_MAIN(DraftSyncTest)
#include "tst_draft_sync.moc"
