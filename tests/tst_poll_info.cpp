#include "telegram/PollInfo.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QtTest>

using namespace beygram::telegram;

namespace {

/// Round-trips through JSON text so the assertions cover what the transport
/// actually puts on the wire.
QJsonObject serialized(const QJsonObject& request)
{
    return QJsonDocument::fromJson(QJsonDocument(request).toJson(QJsonDocument::Compact)).object();
}

} // namespace

class PollInfoTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesRegularPoll();
    void parsesQuizPoll();
    void parsesMultipleChoicePoll();
    void parsesClosedPoll();
    void readsPollIdFromNumberAndString();
    void handlesMissingAndMalformedPoll();
    void buildsVotePollRequest();
    void buildsStopPollRequest();
};

void PollInfoTest::parsesRegularPoll()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 20,
        "chat_id": 42,
        "content": {
            "@type": "messagePoll",
            "poll": {
                "@type": "poll",
                "id": "9876543210",
                "question": {
                    "@type": "formattedText",
                    "text": "Where do we eat?",
                    "entities": []
                },
                "options": [
                    {
                        "@type": "pollOption",
                        "id": "a",
                        "text": { "@type": "formattedText", "text": "Pizza", "entities": [] },
                        "media": null,
                        "voter_count": 4,
                        "vote_percentage": 40,
                        "is_chosen": true,
                        "is_being_chosen": false,
                        "author": null,
                        "addition_date": 0
                    },
                    {
                        "@type": "pollOption",
                        "id": "b",
                        "text": { "@type": "formattedText", "text": "Sushi", "entities": [] },
                        "media": null,
                        "voter_count": 6,
                        "vote_percentage": 60,
                        "is_chosen": false,
                        "is_being_chosen": false,
                        "author": null,
                        "addition_date": 0
                    }
                ],
                "total_voter_count": 10,
                "recent_voter_ids": [],
                "can_get_voters": false,
                "can_see_results": true,
                "is_anonymous": true,
                "allows_multiple_answers": false,
                "allows_revoting": false,
                "members_only": false,
                "country_codes": [],
                "option_order": [],
                "type": { "@type": "pollTypeRegular" },
                "open_period": 0,
                "close_date": 0,
                "is_closed": false,
                "vote_restriction_reason": null
            },
            "description": { "@type": "formattedText", "text": "" },
            "media": null,
            "can_add_option": false
        }
    })")
                                    .object();

    const PollInfo poll = pollFromMessage(message);
    QCOMPARE(poll.id, qint64(9876543210LL));
    QCOMPARE(poll.question, QStringLiteral("Where do we eat?"));
    QCOMPARE(poll.options.size(), 2);
    QCOMPARE(poll.options.at(0).text, QStringLiteral("Pizza"));
    QCOMPARE(poll.options.at(0).voterCount, 4);
    QVERIFY(poll.options.at(0).chosen);
    QCOMPARE(poll.options.at(1).text, QStringLiteral("Sushi"));
    QCOMPARE(poll.options.at(1).voterCount, 6);
    QVERIFY(!poll.options.at(1).chosen);
    QCOMPARE(poll.totalVoterCount, 10);
    QVERIFY(poll.isAnonymous);
    QVERIFY(!poll.isClosed);
    QVERIFY(!poll.isQuiz);
    QVERIFY(!poll.multipleChoice);
    QVERIFY(poll.hasVoted);
}

void PollInfoTest::parsesQuizPoll()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 21,
        "chat_id": 42,
        "content": {
            "@type": "messagePoll",
            "poll": {
                "@type": "poll",
                "id": 3001,
                "question": { "@type": "formattedText", "text": "2 + 2?" },
                "options": [
                    {
                        "@type": "pollOption",
                        "id": "0",
                        "text": { "@type": "formattedText", "text": "3" },
                        "voter_count": 1,
                        "vote_percentage": 20,
                        "is_chosen": false
                    },
                    {
                        "@type": "pollOption",
                        "id": "1",
                        "text": { "@type": "formattedText", "text": "4" },
                        "voter_count": 4,
                        "vote_percentage": 80,
                        "is_chosen": false
                    }
                ],
                "total_voter_count": 5,
                "is_anonymous": true,
                "allows_multiple_answers": false,
                "option_order": [],
                "type": {
                    "@type": "pollTypeQuiz",
                    "correct_option_ids": [1],
                    "explanation": { "@type": "formattedText", "text": "Two and two" },
                    "explanation_media": null
                },
                "is_closed": false
            },
            "description": { "@type": "formattedText", "text": "" },
            "media": null,
            "can_add_option": false
        }
    })")
                                    .object();

    const PollInfo poll = pollFromMessage(message);
    QCOMPARE(poll.id, qint64(3001));
    QCOMPARE(poll.question, QStringLiteral("2 + 2?"));
    QVERIFY(poll.isQuiz);
    QVERIFY(poll.isAnonymous);
    QVERIFY(!poll.multipleChoice);
    QVERIFY(!poll.hasVoted);
    QCOMPARE(poll.totalVoterCount, 5);

    // Older revisions sent a bare is_quiz flag instead of poll.type.
    const QJsonObject legacy = QJsonDocument::fromJson(R"({
        "content": {
            "poll": {
                "id": 3006,
                "question": { "text": "Legacy quiz?" },
                "options": [],
                "is_quiz": true
            }
        }
    })")
                                   .object();
    QVERIFY(pollFromMessage(legacy).isQuiz);

    const QJsonObject legacyRegular = QJsonDocument::fromJson(R"({
        "content": {
            "poll": { "id": 3007, "question": { "text": "Plain" }, "is_quiz": false }
        }
    })")
                                          .object();
    QVERIFY(!pollFromMessage(legacyRegular).isQuiz);
}

void PollInfoTest::parsesMultipleChoicePoll()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 22,
        "chat_id": 42,
        "content": {
            "@type": "messagePoll",
            "poll": {
                "@type": "poll",
                "id": 3002,
                "question": { "@type": "formattedText", "text": "Pick any" },
                "options": [
                    {
                        "@type": "pollOption",
                        "id": "0",
                        "text": { "@type": "formattedText", "text": "Red" },
                        "voter_count": 3,
                        "is_chosen": true
                    },
                    {
                        "@type": "pollOption",
                        "id": "1",
                        "text": { "@type": "formattedText", "text": "Green" },
                        "voter_count": 0,
                        "is_chosen": false
                    },
                    {
                        "@type": "pollOption",
                        "id": "2",
                        "text": { "@type": "formattedText", "text": "Blue" },
                        "voter_count": 2,
                        "is_chosen": true
                    }
                ],
                "total_voter_count": 5,
                "is_anonymous": false,
                "allows_multiple_answers": true,
                "option_order": [],
                "type": { "@type": "pollTypeRegular" },
                "is_closed": false
            },
            "description": null,
            "media": null,
            "can_add_option": false
        }
    })")
                                    .object();

    const PollInfo poll = pollFromMessage(message);
    QCOMPARE(poll.options.size(), 3);
    QVERIFY(poll.multipleChoice);
    QVERIFY(!poll.isAnonymous);
    QVERIFY(!poll.isQuiz);
    QVERIFY(poll.hasVoted);
    QVERIFY(poll.options.at(0).chosen);
    QVERIFY(!poll.options.at(1).chosen);
    QVERIFY(poll.options.at(2).chosen);
    QCOMPARE(poll.options.at(1).voterCount, 0);
}

void PollInfoTest::parsesClosedPoll()
{
    const QJsonObject message = QJsonDocument::fromJson(R"({
        "@type": "message",
        "id": 23,
        "chat_id": 42,
        "content": {
            "@type": "messagePoll",
            "poll": {
                "@type": "poll",
                "id": 3003,
                "question": { "@type": "formattedText", "text": "Finished?" },
                "options": [
                    {
                        "@type": "pollOption",
                        "id": "0",
                        "text": { "@type": "formattedText", "text": "Yes" },
                        "voter_count": 0,
                        "is_chosen": false
                    },
                    {
                        "@type": "pollOption",
                        "id": "1",
                        "text": { "@type": "formattedText", "text": "No" },
                        "voter_count": 0,
                        "is_chosen": false
                    }
                ],
                "total_voter_count": 0,
                "is_anonymous": false,
                "allows_multiple_answers": false,
                "option_order": [1, 0],
                "type": { "@type": "pollTypeRegular" },
                "is_closed": true
            },
            "description": null,
            "media": null,
            "can_add_option": false
        }
    })")
                                    .object();

    const PollInfo poll = pollFromMessage(message);
    QVERIFY(poll.isClosed);
    QVERIFY(!poll.isAnonymous);
    QVERIFY(!poll.hasVoted);
    QCOMPARE(poll.totalVoterCount, 0);
    QCOMPARE(poll.options.size(), 2);
    // option_order is a display hint; the wire order keeps vote ids valid.
    QCOMPARE(poll.options.at(0).text, QStringLiteral("Yes"));
    QCOMPARE(poll.options.at(1).text, QStringLiteral("No"));
}

void PollInfoTest::readsPollIdFromNumberAndString()
{
    const QJsonObject numeric = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messagePoll",
            "poll": {
                "id": 1234567890123,
                "question": { "text": "Q" },
                "options": [],
                "total_voter_count": 0
            }
        }
    })")
                                    .object();
    const QJsonObject textual = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messagePoll",
            "poll": {
                "id": "1234567890123",
                "question": { "text": "Q" },
                "options": [],
                "total_voter_count": 0
            }
        }
    })")
                                    .object();

    QCOMPARE(pollFromMessage(numeric).id, qint64(1234567890123LL));
    QCOMPARE(pollFromMessage(textual).id, qint64(1234567890123LL));
}

void PollInfoTest::handlesMissingAndMalformedPoll()
{
    const PollInfo empty = pollFromMessage(QJsonObject{});
    QCOMPARE(empty.id, qint64(0));
    QVERIFY(empty.question.isEmpty());
    QVERIFY(empty.options.isEmpty());
    QCOMPARE(empty.totalVoterCount, 0);
    QVERIFY(empty.isAnonymous);
    QVERIFY(!empty.isClosed);
    QVERIFY(!empty.isQuiz);
    QVERIFY(!empty.multipleChoice);
    QVERIFY(!empty.hasVoted);

    const QJsonObject noPoll = QJsonDocument::fromJson(R"({
        "content": { "@type": "messageText", "text": { "text": "hi" } }
    })")
                                   .object();
    QVERIFY(pollFromMessage(noPoll).options.isEmpty());

    const QJsonObject nullPoll = QJsonDocument::fromJson(R"({
        "content": { "@type": "messagePoll", "poll": null }
    })")
                                     .object();
    QVERIFY(pollFromMessage(nullPoll).options.isEmpty());

    const QJsonObject malformed = QJsonDocument::fromJson(R"({
        "content": {
            "@type": "messagePoll",
            "poll": {
                "id": 3004,
                "options": [
                    null,
                    5,
                    { "text": { "text": "Kept" }, "voter_count": -3, "is_chosen": "yes" },
                    { "voter_count": 2, "is_chosen": false }
                ],
                "total_voter_count": -1,
                "type": { "@type": "pollTypeFuture" }
            }
        }
    })")
                                      .object();

    const PollInfo parsed = pollFromMessage(malformed);
    QCOMPARE(parsed.id, qint64(3004));
    QVERIFY(parsed.question.isEmpty());
    QCOMPARE(parsed.totalVoterCount, 0);
    QVERIFY(!parsed.isQuiz);
    QCOMPARE(parsed.options.size(), 2);
    QCOMPARE(parsed.options.at(0).text, QStringLiteral("Kept"));
    QCOMPARE(parsed.options.at(0).voterCount, 0);
    QVERIFY(!parsed.options.at(0).chosen);
    QVERIFY(parsed.options.at(1).text.isEmpty());
    QCOMPARE(parsed.options.at(1).voterCount, 2);
    QVERIFY(!parsed.hasVoted);

    const QJsonObject wrongOptions = QJsonDocument::fromJson(R"({
        "content": { "poll": { "id": 3005, "options": "not-an-array" } }
    })")
                                         .object();
    QVERIFY(pollFromMessage(wrongOptions).options.isEmpty());
}

void PollInfoTest::buildsVotePollRequest()
{
    const QJsonObject request =
        serialized(votePollRequest(qint64(42), qint64(900), QVector<int>{1, 3}));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("setPollAnswer"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(42));
    QCOMPARE(request.value(QStringLiteral("message_id")).toVariant().toLongLong(), qint64(900));

    const QJsonArray ids = request.value(QStringLiteral("option_ids")).toArray();
    QCOMPARE(ids.size(), 2);
    QCOMPARE(ids.at(0).toInt(), 1);
    QCOMPARE(ids.at(1).toInt(), 3);
    QVERIFY(!request.contains(QStringLiteral("option_indices")));

    const QJsonObject empty =
        serialized(votePollRequest(qint64(-1001234567890LL), qint64(1234567890123LL), {}));
    QCOMPARE(empty.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(-1001234567890LL));
    QCOMPARE(empty.value(QStringLiteral("message_id")).toVariant().toLongLong(),
             qint64(1234567890123LL));
    QVERIFY(empty.value(QStringLiteral("option_ids")).isArray());
    QVERIFY(empty.value(QStringLiteral("option_ids")).toArray().isEmpty());
}

void PollInfoTest::buildsStopPollRequest()
{
    const QJsonObject request = serialized(stopPollRequest(qint64(42), qint64(901)));

    QCOMPARE(request.value(QStringLiteral("@type")).toString(), QStringLiteral("stopPoll"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(42));
    QCOMPARE(request.value(QStringLiteral("message_id")).toVariant().toLongLong(), qint64(901));
    QVERIFY(request.contains(QStringLiteral("reply_markup")));
    QVERIFY(request.value(QStringLiteral("reply_markup")).isNull());
}

QTEST_GUILESS_MAIN(PollInfoTest)
#include "tst_poll_info.moc"
