#include "telegram/PollAdmin.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
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

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

} // namespace

class PollAdminTest final : public QObject {
    Q_OBJECT

private slots:
    void votersRequestShape();
    void clampsVotersPaging();
    void addOptionRequestShape();
    void stopPollRequestShape();
    void parsesUserVoters();
    void parsesChatAndMixedVoters();
    void keepsVoterOrderAndDuplicates();
    void handlesMissingAndMalformedVoters();
};

void PollAdminTest::votersRequestShape()
{
    const QJsonObject request = roundTrip(getPollVotersRequest(
        qint64(-1001234567890LL), qint64(1234567890123LL), 3, 20, 50));

    QCOMPARE(request.size(), 6);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("getPollVoters"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(-1001234567890LL));
    QCOMPARE(request.value(QStringLiteral("message_id")).toVariant().toLongLong(),
             qint64(1234567890123LL));
    QCOMPARE(request.value(QStringLiteral("option_id")).toInt(), 3);
    QCOMPARE(request.value(QStringLiteral("offset")).toInt(), 20);
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);

    // The schema names the option position option_id, not option_index.
    QVERIFY(!request.contains(QStringLiteral("option_index")));
}

void PollAdminTest::clampsVotersPaging()
{
    const auto offsetOf = [](const QJsonObject& request) {
        return request.value(QStringLiteral("offset")).toInt();
    };
    const auto limitOf = [](const QJsonObject& request) {
        return request.value(QStringLiteral("limit")).toInt();
    };

    // offset must be non-negative.
    QCOMPARE(offsetOf(getPollVotersRequest(1, 2, 0, -7, 10)), 0);
    QCOMPARE(offsetOf(getPollVotersRequest(1, 2, 0, -1, 10)), 0);
    QCOMPARE(offsetOf(getPollVotersRequest(1, 2, 0, 0, 10)), 0);
    QCOMPARE(offsetOf(getPollVotersRequest(1, 2, 0, 12, 10)), 12);

    // limit must be in 1..50; this schema rejects 0 and anything above 50.
    QCOMPARE(limitOf(getPollVotersRequest(1, 2, 0, 0, 0)), 1);
    QCOMPARE(limitOf(getPollVotersRequest(1, 2, 0, 0, -100)), 1);
    QCOMPARE(limitOf(getPollVotersRequest(1, 2, 0, 0, 1)), 1);
    QCOMPARE(limitOf(getPollVotersRequest(1, 2, 0, 0, 25)), 25);
    QCOMPARE(limitOf(getPollVotersRequest(1, 2, 0, 0, 50)), 50);
    QCOMPARE(limitOf(getPollVotersRequest(1, 2, 0, 0, 51)), 50);
    QCOMPARE(limitOf(getPollVotersRequest(1, 2, 0, 0, 1000)), 50);

    // Clamping paging must not disturb the option the caller asked about.
    const QJsonObject clamped = getPollVotersRequest(1, 2, 7, -4, 999);
    QCOMPARE(clamped.value(QStringLiteral("option_id")).toInt(), 7);
    QCOMPARE(offsetOf(clamped), 0);
    QCOMPARE(limitOf(clamped), 50);
}

void PollAdminTest::addOptionRequestShape()
{
    const QString text = QStringLiteral("  Paris, \"France\"\n");
    const QJsonObject request =
        roundTrip(addPollOptionRequest(qint64(42), qint64(900), text));

    QCOMPARE(request.size(), 4);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("addPollOption"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(42));
    QCOMPARE(request.value(QStringLiteral("message_id")).toVariant().toLongLong(),
             qint64(900));

    const QJsonObject option = request.value(QStringLiteral("option")).toObject();
    QCOMPARE(option.size(), 3);
    QCOMPARE(option.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputPollOption"));

    const QJsonObject formatted = option.value(QStringLiteral("text")).toObject();
    QCOMPARE(formatted.size(), 3);
    QCOMPARE(formatted.value(QStringLiteral("@type")).toString(),
             QStringLiteral("formattedText"));
    // The builder passes the text through: trimming and validation belong to
    // the caller that re-reads the poll before sending.
    QCOMPARE(formatted.value(QStringLiteral("text")).toString(), text);

    const QJsonValue entities = formatted.value(QStringLiteral("entities"));
    QVERIFY(entities.isArray());
    QVERIFY(entities.toArray().isEmpty());

    // inputPollOption.media is required and null means a text-only option.
    const QJsonValue media = option.value(QStringLiteral("media"));
    QVERIFY(media.isNull());

    const QJsonObject empty =
        roundTrip(addPollOptionRequest(qint64(-1001234567890LL), qint64(1), QString()));
    const QJsonObject emptyOption =
        empty.value(QStringLiteral("option")).toObject();
    QVERIFY(emptyOption.value(QStringLiteral("media")).isNull());
    QCOMPARE(emptyOption.value(QStringLiteral("text"))
                 .toObject()
                 .value(QStringLiteral("text"))
                 .toString(),
             QString());
}

void PollAdminTest::stopPollRequestShape()
{
    const QJsonObject request =
        roundTrip(stopPollAdminRequest(qint64(-1001234567890LL), qint64(901)));

    QCOMPARE(request.size(), 4);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("stopPoll"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(-1001234567890LL));
    QCOMPARE(request.value(QStringLiteral("message_id")).toVariant().toLongLong(),
             qint64(901));

    // stopPoll.reply_markup is part of the schema; null means no new keyboard.
    QVERIFY(request.contains(QStringLiteral("reply_markup")));
    QVERIFY(request.value(QStringLiteral("reply_markup")).isNull());
}

void PollAdminTest::parsesUserVoters()
{
    const QJsonObject response = parseObject(R"({
        "@type": "pollVoters",
        "total_count": 3,
        "voters": [
            {
                "@type": "pollVoter",
                "voter_id": { "@type": "messageSenderUser", "user_id": 100 },
                "date": 1700000000
            },
            {
                "@type": "pollVoter",
                "voter_id": { "@type": "messageSenderUser", "user_id": "9007199254740991" },
                "date": 1700000001
            },
            {
                "@type": "pollVoter",
                "voter_id": { "@type": "messageSenderUser", "user_id": 0 },
                "date": 1700000002
            }
        ]
    })");

    QCOMPARE(voterIdsFromResponse(response),
             (QVector<qint64>{100, 9007199254740991LL}));

    // total_count is approximate and must not pad or truncate the page.
    QCOMPARE(voterIdsFromResponse(parseObject(R"({
        "@type": "pollVoters",
        "total_count": 99,
        "voters": [
            { "voter_id": { "user_id": 5 } }
        ]
    })")),
             QVector<qint64>{5});
}

void PollAdminTest::parsesChatAndMixedVoters()
{
    const QJsonObject response = parseObject(R"({
        "@type": "pollVoters",
        "voters": [
            { "voter_id": { "@type": "messageSenderChat", "chat_id": -1001234567890 } },
            { "voter_id": { "@type": "messageSenderUser", "user_id": 5 } },
            { "voter_id": { "@type": "messageSenderUser", "user_id": 7, "chat_id": -9 } },
            { "voter_id": { "@type": "messageSenderUser", "user_id": 0, "chat_id": -9 } },
            { "voter_id": { "@type": "messageSenderUser", "user_id": -3 } },
            { "voter_id": { "@type": "messageSenderFuture", "id": 8 } }
        ]
    })");

    // A positive user id wins; otherwise the chat id is kept, including its
    // negative supergroup/channel form.
    QCOMPARE(voterIdsFromResponse(response),
             (QVector<qint64>{-1001234567890LL, 5, 7, -9}));
}

void PollAdminTest::keepsVoterOrderAndDuplicates()
{
    const QJsonObject response = parseObject(R"({
        "@type": "pollVoters",
        "voters": [
            { "voter_id": { "user_id": 5 } },
            { "voter_id": { "user_id": 9 } },
            { "voter_id": { "user_id": 5 } }
        ]
    })");

    // Paging code de-duplicates across options; a single answer stays intact.
    QCOMPARE(voterIdsFromResponse(response), (QVector<qint64>{5, 9, 5}));
}

void PollAdminTest::handlesMissingAndMalformedVoters()
{
    QVERIFY(voterIdsFromResponse(QJsonObject{}).isEmpty());
    QVERIFY(voterIdsFromResponse(parseObject(R"({ "voters": null })")).isEmpty());
    QVERIFY(voterIdsFromResponse(parseObject(R"({ "voters": "nope" })")).isEmpty());
    QVERIFY(voterIdsFromResponse(parseObject(R"({ "voters": {} })")).isEmpty());
    QVERIFY(voterIdsFromResponse(parseObject(R"({ "voters": [] })")).isEmpty());

    const QJsonObject mixed = parseObject(R"({
        "@type": "pollVoters",
        "voters": [
            null,
            5,
            {},
            { "voter_id": null },
            { "voter_id": [] },
            { "voter_id": {} },
            { "voter_id": { "user_id": 0 } },
            { "voter_id": { "chat_id": 0 } },
            { "voter_id": { "@type": "messageSenderUser", "user_id": 12 } }
        ]
    })");

    QCOMPARE(voterIdsFromResponse(mixed), QVector<qint64>{12});

    // The parser keys off the voters array, not the response class, so an
    // unexpected @type cannot hide a usable page.
    QCOMPARE(voterIdsFromResponse(parseObject(R"({
        "@type": "error",
        "voters": [ { "voter_id": { "user_id": 3 } } ]
    })")),
             QVector<qint64>{3});
}

QTEST_GUILESS_MAIN(PollAdminTest)
#include "tst_poll_admin.moc"
