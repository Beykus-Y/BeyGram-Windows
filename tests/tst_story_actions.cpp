#include "telegram/StoryActions.h"
#include "telegram/StoryRequests.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
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

qint64 numberAt(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

QJsonObject contentOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("input_message_content")).toObject();
}

QJsonObject textOf(const QJsonObject& request)
{
    return contentOf(request).value(QStringLiteral("text")).toObject();
}

} // namespace

class StoryActionsTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsOpenStoryRequest();
    void buildsCloseStoryRequest();
    void buildsGetStoryRequest();
    void buildsViewStoryRequest();
    void buildsReactionRequest();
    void emptyEmojiRemovesReaction();
    void buildsReplyRequest();
    void replyUsesChatAsPoster();
    void rejectsNonPositiveStoryIds();
    void rejectsStoryIdsAboveInt32();
    void acceptsLargestInt32StoryId();
    void rejectsInvalidReplyArguments();
    void trimsEmojiAndReplyText();
    void preservesLargeChatIds();
    void requestsRoundTripThroughJson();
    void delegatesToStoryRequestsBuilders();
};

void StoryActionsTest::buildsOpenStoryRequest()
{
    const QJsonObject request = serialized(openStoryRequest(7));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("openStory"));
    QCOMPARE(numberAt(request, QStringLiteral("story_id")), qint64(7));
    // The one-argument form has no poster chat, so the delegated builder's
    // placeholder is dropped; the caller merges the real poster.
    QVERIFY(!request.contains(QStringLiteral("story_poster_chat_id")));
    QCOMPARE(request.size(), 2);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void StoryActionsTest::buildsCloseStoryRequest()
{
    const QJsonObject request = serialized(closeStoryRequest(8));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("closeStory"));
    QCOMPARE(numberAt(request, QStringLiteral("story_id")), qint64(8));
    QVERIFY(!request.contains(QStringLiteral("story_poster_chat_id")));
    QCOMPARE(request.size(), 2);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void StoryActionsTest::buildsGetStoryRequest()
{
    const QJsonObject request = serialized(getStoryRequest(9));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getStory"));
    QCOMPARE(numberAt(request, QStringLiteral("story_id")), qint64(9));
    // only_local is required by the schema and this builder always asks for the
    // server copy.
    QVERIFY(request.value(QStringLiteral("only_local")).isBool());
    QVERIFY(!request.value(QStringLiteral("only_local")).toBool());
    QVERIFY(!request.contains(QStringLiteral("story_poster_chat_id")));
    QCOMPARE(request.size(), 3);
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void StoryActionsTest::buildsViewStoryRequest()
{
    const QJsonObject request = serialized(viewStoryRequest(7));

    // The pinned schema has no viewStory function; the name is emitted verbatim
    // for a transport or proxy that exposes it. openStoryRequest is the real
    // read-marking call against the pinned server.
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("viewStory"));
    QCOMPARE(numberAt(request, QStringLiteral("story_id")), qint64(7));
    QCOMPARE(request.size(), 2);
    QVERIFY(!request.contains(QStringLiteral("story_poster_chat_id")));
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void StoryActionsTest::buildsReactionRequest()
{
    const QJsonObject request =
        serialized(reactStoryRequest(10, QStringLiteral("  👍  ")));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setStoryReaction"));
    QCOMPARE(numberAt(request, QStringLiteral("story_id")), qint64(10));
    QCOMPARE(request.value(QStringLiteral("update_recent_reactions")).toBool(false),
             true);

    const QJsonObject reaction =
        request.value(QStringLiteral("reaction_type")).toObject();
    QCOMPARE(reaction.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reactionTypeEmoji"));
    QCOMPARE(reaction.value(QStringLiteral("emoji")).toString(), QStringLiteral("👍"));
    QCOMPARE(reaction.size(), 2);

    // The poster chat is not part of this signature; the caller adds it.
    QVERIFY(!request.contains(QStringLiteral("story_poster_chat_id")));
    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void StoryActionsTest::emptyEmojiRemovesReaction()
{
    const QVector<QString> emptyEmoji{
        QString(),
        QStringLiteral("   "),
        QStringLiteral("\t\r\n"),
    };

    for (const QString& emoji : emptyEmoji) {
        const QJsonObject request = serialized(reactStoryRequest(11, emoji));

        QCOMPARE(request.value(QStringLiteral("@type")).toString(),
                 QStringLiteral("setStoryReaction"));
        QCOMPARE(numberAt(request, QStringLiteral("story_id")), qint64(11));
        // TDLib removes the reaction when reaction_type is null.
        QVERIFY(request.value(QStringLiteral("reaction_type")).isNull());
        QCOMPARE(request.value(QStringLiteral("update_recent_reactions")).toBool(false),
                 true);
    }
}

void StoryActionsTest::buildsReplyRequest()
{
    const QJsonObject request =
        serialized(replyToStoryRequest(12, 42, QStringLiteral("  hi  ")));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendMessage"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QVERIFY(request.value(QStringLiteral("options")).isNull());
    QVERIFY(request.value(QStringLiteral("reply_markup")).isNull());

    const QJsonObject replyTo = request.value(QStringLiteral("reply_to")).toObject();
    QCOMPARE(replyTo.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageReplyToStory"));
    QCOMPARE(numberAt(replyTo, QStringLiteral("story_poster_chat_id")), qint64(42));
    QCOMPARE(numberAt(replyTo, QStringLiteral("story_id")), qint64(12));

    const QJsonObject content = contentOf(request);
    QCOMPARE(content.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessageText"));
    QVERIFY(content.value(QStringLiteral("link_preview_options")).isNull());
    QCOMPARE(content.value(QStringLiteral("clear_draft")).toBool(true), false);

    const QJsonObject text = textOf(request);
    QCOMPARE(text.value(QStringLiteral("@type")).toString(),
             QStringLiteral("formattedText"));
    QCOMPARE(text.value(QStringLiteral("text")).toString(), QStringLiteral("hi"));
    QVERIFY(text.value(QStringLiteral("entities")).isArray());
    QCOMPARE(text.value(QStringLiteral("entities")).toArray().size(), 0);

    QVERIFY(!request.contains(QStringLiteral("@extra")));
}

void StoryActionsTest::replyUsesChatAsPoster()
{
    // TDLib allows a story reply only in the chat that posted the story, so the
    // destination chat is also the reply target's poster.
    const qint64 chatId = 1234567890123LL;
    const QJsonObject request =
        serialized(replyToStoryRequest(77, chatId, QStringLiteral("ok")));

    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), chatId);
    QCOMPARE(numberAt(request.value(QStringLiteral("reply_to")).toObject(),
                      QStringLiteral("story_poster_chat_id")),
             chatId);
}

void StoryActionsTest::rejectsNonPositiveStoryIds()
{
    const QVector<qint64> invalid{
        0,
        -1,
        -7,
        std::numeric_limits<qint64>::min(),
    };

    for (const qint64 id : invalid) {
        QVERIFY(openStoryRequest(id).isEmpty());
        QVERIFY(closeStoryRequest(id).isEmpty());
        QVERIFY(viewStoryRequest(id).isEmpty());
        QVERIFY(reactStoryRequest(id, QStringLiteral("👍")).isEmpty());
        QVERIFY(replyToStoryRequest(id, 42, QStringLiteral("hi")).isEmpty());
        QVERIFY(getStoryRequest(id).isEmpty());
    }
}

void StoryActionsTest::rejectsStoryIdsAboveInt32()
{
    const qint64 tooLarge =
        static_cast<qint64>(std::numeric_limits<int>::max()) + 1;

    QVERIFY(openStoryRequest(tooLarge).isEmpty());
    QVERIFY(closeStoryRequest(tooLarge).isEmpty());
    QVERIFY(viewStoryRequest(tooLarge).isEmpty());
    QVERIFY(reactStoryRequest(tooLarge, QStringLiteral("👍")).isEmpty());
    QVERIFY(replyToStoryRequest(tooLarge, 42, QStringLiteral("hi")).isEmpty());
    QVERIFY(getStoryRequest(tooLarge).isEmpty());
}

void StoryActionsTest::acceptsLargestInt32StoryId()
{
    const qint64 maxId = std::numeric_limits<int>::max();

    QCOMPARE(numberAt(openStoryRequest(maxId), QStringLiteral("story_id")), maxId);
    QCOMPARE(numberAt(closeStoryRequest(maxId), QStringLiteral("story_id")), maxId);
    QCOMPARE(numberAt(viewStoryRequest(maxId), QStringLiteral("story_id")), maxId);
    QCOMPARE(numberAt(reactStoryRequest(maxId, QStringLiteral("👍")),
                      QStringLiteral("story_id")),
             maxId);

    const QJsonObject reply =
        replyToStoryRequest(maxId, 42, QStringLiteral("hi"));
    QCOMPARE(numberAt(reply.value(QStringLiteral("reply_to")).toObject(),
                      QStringLiteral("story_id")),
             maxId);

    QCOMPARE(numberAt(getStoryRequest(maxId), QStringLiteral("story_id")), maxId);
}

void StoryActionsTest::rejectsInvalidReplyArguments()
{
    QVERIFY(replyToStoryRequest(1, 0, QStringLiteral("hi")).isEmpty());
    QVERIFY(replyToStoryRequest(1, -42, QStringLiteral("hi")).isEmpty());
    QVERIFY(replyToStoryRequest(1, 42, QString()).isEmpty());
    QVERIFY(replyToStoryRequest(1, 42, QStringLiteral("   ")).isEmpty());
    QVERIFY(replyToStoryRequest(1, 42, QStringLiteral("\t\r\n")).isEmpty());
}

void StoryActionsTest::trimsEmojiAndReplyText()
{
    const QJsonObject reaction =
        reactStoryRequest(2, QStringLiteral("  ❤  "));
    QCOMPARE(reaction.value(QStringLiteral("reaction_type"))
                 .toObject()
                 .value(QStringLiteral("emoji"))
                 .toString(),
             QStringLiteral("❤"));

    const QJsonObject reply =
        replyToStoryRequest(3, 42, QStringLiteral("  hello  "));
    QCOMPARE(textOf(reply).value(QStringLiteral("text")).toString(),
             QStringLiteral("hello"));
}

void StoryActionsTest::preservesLargeChatIds()
{
    // Telegram chat ids exceed 32 bits; they must not be truncated. A story
    // reply goes to a private chat, whose id is the user id.
    const qint64 chatId = 1234567890123LL;

    const QJsonObject request =
        serialized(replyToStoryRequest(1, chatId, QStringLiteral("hi")));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), chatId);
    QCOMPARE(numberAt(request.value(QStringLiteral("reply_to")).toObject(),
                      QStringLiteral("story_poster_chat_id")),
             chatId);
}

void StoryActionsTest::requestsRoundTripThroughJson()
{
    const QVector<QJsonObject> requests{
        openStoryRequest(1),
        closeStoryRequest(2),
        viewStoryRequest(3),
        reactStoryRequest(4, QStringLiteral("👍")),
        reactStoryRequest(5, QString()),
        replyToStoryRequest(6, 42, QStringLiteral("hi")),
        getStoryRequest(7),
    };

    for (const QJsonObject& request : requests) {
        QVERIFY(!request.isEmpty());

        const QJsonObject roundTripped = serialized(request);
        QCOMPARE(roundTripped, request);
        QVERIFY(!roundTripped.contains(QStringLiteral("@extra")));
    }
}

void StoryActionsTest::delegatesToStoryRequestsBuilders()
{
    // The one-argument forms reuse the poster-aware builders in StoryRequests.h
    // and drop only the poster field they cannot fill.
    QJsonObject open = beygram::telegram::openStoryRequest(0, 7);
    open.remove(QStringLiteral("story_poster_chat_id"));
    QCOMPARE(openStoryRequest(7), open);

    QJsonObject close = beygram::telegram::closeStoryRequest(0, 8);
    close.remove(QStringLiteral("story_poster_chat_id"));
    QCOMPARE(closeStoryRequest(8), close);

    QJsonObject get = beygram::telegram::getStoryRequest(0, 9);
    get.remove(QStringLiteral("story_poster_chat_id"));
    QCOMPARE(getStoryRequest(9), get);
}

QTEST_GUILESS_MAIN(StoryActionsTest)
#include "tst_story_actions.moc"
