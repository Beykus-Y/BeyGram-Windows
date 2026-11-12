#include "telegram/PollSend.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject contentOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("input_message_content")).toObject();
}

QJsonArray optionsOf(const QJsonObject& request)
{
    return contentOf(request).value(QStringLiteral("options")).toArray();
}

QJsonObject typeOf(const QJsonObject& request)
{
    return contentOf(request).value(QStringLiteral("type")).toObject();
}

OutgoingPoll twoOptionPoll()
{
    OutgoingPoll poll;
    poll.question = QStringLiteral("Where do we eat?");
    poll.options = {QStringLiteral("Pizza"), QStringLiteral("Sushi")};
    return poll;
}

} // namespace

class PollSendTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsRegularPoll();
    void honoursAnonymityAndMultipleAnswers();
    void buildsQuiz();
    void quizWithoutExplanationSendsNull();
    void regularPollIgnoresCorrectOption();
    void trimsQuestionOptionsAndExplanation();
    void rejectsEmptyQuestion();
    void rejectsTooFewOptions();
    void rejectsTooManyOptions();
    void acceptsOptionBounds();
    void rejectsEmptyOption();
    void quizRequiresValidCorrectOption();
    void requestShape();
};

void PollSendTest::buildsRegularPoll()
{
    const QJsonObject request = sendPollRequest(42, twoOptionPoll());

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendMessage"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(42));

    const QJsonObject content = contentOf(request);
    QCOMPARE(content.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessagePoll"));

    const QJsonObject question = content.value(QStringLiteral("question")).toObject();
    QCOMPARE(question.value(QStringLiteral("@type")).toString(),
             QStringLiteral("formattedText"));
    QCOMPARE(question.value(QStringLiteral("text")).toString(),
             QStringLiteral("Where do we eat?"));
    QVERIFY(question.value(QStringLiteral("entities")).isArray());
    QVERIFY(question.value(QStringLiteral("entities")).toArray().isEmpty());

    const QJsonArray options = optionsOf(request);
    QCOMPARE(options.size(), 2);
    for (int i = 0; i < options.size(); ++i) {
        const QJsonObject option = options.at(i).toObject();
        QCOMPARE(option.value(QStringLiteral("@type")).toString(),
                 QStringLiteral("inputPollOption"));
        QCOMPARE(option.value(QStringLiteral("text")).toObject()
                     .value(QStringLiteral("@type")).toString(),
                 QStringLiteral("formattedText"));
        QVERIFY(option.value(QStringLiteral("media")).isNull());
    }
    QCOMPARE(options.at(0).toObject().value(QStringLiteral("text")).toObject()
                 .value(QStringLiteral("text")).toString(),
             QStringLiteral("Pizza"));
    QCOMPARE(options.at(1).toObject().value(QStringLiteral("text")).toObject()
                 .value(QStringLiteral("text")).toString(),
             QStringLiteral("Sushi"));

    QVERIFY(content.value(QStringLiteral("description")).isNull());
    QVERIFY(content.value(QStringLiteral("media")).isNull());
    QVERIFY(content.value(QStringLiteral("is_anonymous")).toBool());
    QVERIFY(!content.value(QStringLiteral("allows_multiple_answers")).toBool());
    QVERIFY(content.value(QStringLiteral("allows_revoting")).toBool());
    QVERIFY(!content.value(QStringLiteral("members_only")).toBool());
    QVERIFY(content.value(QStringLiteral("country_codes")).isArray());
    QVERIFY(content.value(QStringLiteral("country_codes")).toArray().isEmpty());
    QVERIFY(!content.value(QStringLiteral("shuffle_options")).toBool());
    QVERIFY(!content.value(QStringLiteral("hide_results_until_closes")).toBool());
    QCOMPARE(content.value(QStringLiteral("open_period")).toInt(), 0);
    QCOMPARE(content.value(QStringLiteral("close_date")).toInt(), 0);
    QVERIFY(!content.value(QStringLiteral("is_closed")).toBool());

    const QJsonObject type = typeOf(request);
    QCOMPARE(type.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputPollTypeRegular"));
    QVERIFY(!type.value(QStringLiteral("allow_adding_options")).toBool());
    QVERIFY(!type.contains(QStringLiteral("correct_option_ids")));
    QVERIFY(!type.contains(QStringLiteral("explanation")));
}

void PollSendTest::honoursAnonymityAndMultipleAnswers()
{
    OutgoingPoll poll = twoOptionPoll();
    poll.anonymous = false;
    poll.multiple = true;

    const QJsonObject content = contentOf(sendPollRequest(42, poll));
    QVERIFY(!content.value(QStringLiteral("is_anonymous")).toBool());
    QVERIFY(content.value(QStringLiteral("allows_multiple_answers")).toBool());
    QVERIFY(content.value(QStringLiteral("allows_revoting")).toBool());
}

void PollSendTest::buildsQuiz()
{
    OutgoingPoll poll;
    poll.question = QStringLiteral("Pick one");
    poll.options = {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C")};
    poll.anonymous = false;
    poll.multiple = true;
    poll.quiz = true;
    poll.correctOption = 2;
    poll.explanation = QStringLiteral("Because");

    const QJsonObject content = contentOf(sendPollRequest(42, poll));

    // Quizzes are anonymous on Telegram regardless of the composer toggle.
    QVERIFY(content.value(QStringLiteral("is_anonymous")).toBool());
    QVERIFY(content.value(QStringLiteral("allows_multiple_answers")).toBool());
    QVERIFY(!content.value(QStringLiteral("allows_revoting")).toBool());

    const QJsonObject type = typeOf(sendPollRequest(42, poll));
    QCOMPARE(type.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputPollTypeQuiz"));

    const QJsonArray correct = type.value(QStringLiteral("correct_option_ids")).toArray();
    QCOMPARE(correct.size(), 1);
    QCOMPARE(correct.at(0).toInt(), 2);

    const QJsonObject explanation = type.value(QStringLiteral("explanation")).toObject();
    QCOMPARE(explanation.value(QStringLiteral("@type")).toString(),
             QStringLiteral("formattedText"));
    QCOMPARE(explanation.value(QStringLiteral("text")).toString(),
             QStringLiteral("Because"));
    QVERIFY(type.value(QStringLiteral("explanation_media")).isNull());
}

void PollSendTest::quizWithoutExplanationSendsNull()
{
    OutgoingPoll poll = twoOptionPoll();
    poll.quiz = true;
    poll.correctOption = 0;
    poll.explanation = QStringLiteral("   ");

    const QJsonObject type = typeOf(sendPollRequest(42, poll));
    QCOMPARE(type.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputPollTypeQuiz"));
    QVERIFY(type.value(QStringLiteral("explanation")).isNull());
}

void PollSendTest::regularPollIgnoresCorrectOption()
{
    OutgoingPoll poll = twoOptionPoll();
    poll.correctOption = 1;

    const QJsonObject type = typeOf(sendPollRequest(42, poll));
    QCOMPARE(type.value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputPollTypeRegular"));
    QVERIFY(!type.contains(QStringLiteral("correct_option_ids")));
    QVERIFY(!type.contains(QStringLiteral("explanation")));
}

void PollSendTest::trimsQuestionOptionsAndExplanation()
{
    OutgoingPoll poll;
    poll.question = QStringLiteral("  Pick one  ");
    poll.options = {QStringLiteral(" A "), QStringLiteral(" B ")};
    poll.quiz = true;
    poll.correctOption = 1;
    poll.explanation = QStringLiteral("  Because  ");

    const QJsonObject request = sendPollRequest(42, poll);
    const QJsonObject content = contentOf(request);

    QCOMPARE(content.value(QStringLiteral("question")).toObject()
                 .value(QStringLiteral("text")).toString(),
             QStringLiteral("Pick one"));

    const QJsonArray options = optionsOf(request);
    QCOMPARE(options.size(), 2);
    QCOMPARE(options.at(0).toObject().value(QStringLiteral("text")).toObject()
                 .value(QStringLiteral("text")).toString(),
             QStringLiteral("A"));
    QCOMPARE(options.at(1).toObject().value(QStringLiteral("text")).toObject()
                 .value(QStringLiteral("text")).toString(),
             QStringLiteral("B"));

    QCOMPARE(typeOf(request).value(QStringLiteral("explanation")).toObject()
                 .value(QStringLiteral("text")).toString(),
             QStringLiteral("Because"));
}

void PollSendTest::rejectsEmptyQuestion()
{
    OutgoingPoll poll = twoOptionPoll();
    poll.question = QString();
    QVERIFY(sendPollRequest(42, poll).isEmpty());

    poll.question = QStringLiteral("   ");
    QVERIFY(sendPollRequest(42, poll).isEmpty());
}

void PollSendTest::rejectsTooFewOptions()
{
    OutgoingPoll poll = twoOptionPoll();

    poll.options.clear();
    QVERIFY(sendPollRequest(42, poll).isEmpty());

    poll.options = {QStringLiteral("Only one")};
    QVERIFY(sendPollRequest(42, poll).isEmpty());
}

void PollSendTest::rejectsTooManyOptions()
{
    OutgoingPoll poll = twoOptionPoll();
    poll.options.clear();
    for (int i = 0; i < 11; ++i)
        poll.options.append(QStringLiteral("Option %1").arg(i));

    QVERIFY(sendPollRequest(42, poll).isEmpty());
}

void PollSendTest::acceptsOptionBounds()
{
    OutgoingPoll poll = twoOptionPoll();
    QCOMPARE(optionsOf(sendPollRequest(42, poll)).size(), 2);

    poll.options.clear();
    for (int i = 0; i < 10; ++i)
        poll.options.append(QStringLiteral("Option %1").arg(i));
    QCOMPARE(optionsOf(sendPollRequest(42, poll)).size(), 10);
}

void PollSendTest::rejectsEmptyOption()
{
    OutgoingPoll poll = twoOptionPoll();
    poll.options = {QStringLiteral("Pizza"), QStringLiteral("   ")};
    QVERIFY(sendPollRequest(42, poll).isEmpty());
}

void PollSendTest::quizRequiresValidCorrectOption()
{
    OutgoingPoll poll = twoOptionPoll();
    poll.quiz = true;

    // The default -1 means "no correct answer" and cannot be sent as a quiz.
    poll.correctOption = -1;
    QVERIFY(sendPollRequest(42, poll).isEmpty());

    poll.correctOption = 2;
    QVERIFY(sendPollRequest(42, poll).isEmpty());

    poll.correctOption = 1;
    QCOMPARE(typeOf(sendPollRequest(42, poll))
                 .value(QStringLiteral("correct_option_ids"))
                 .toArray()
                 .at(0)
                 .toInt(),
             1);
}

void PollSendTest::requestShape()
{
    const QJsonObject request = sendPollRequest(-1001234567890LL, twoOptionPoll());

    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(),
             qint64(-1001234567890LL));
    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QVERIFY(request.value(QStringLiteral("reply_to")).isNull());
    QVERIFY(request.value(QStringLiteral("options")).isNull());
    QVERIFY(request.value(QStringLiteral("reply_markup")).isNull());
    QVERIFY(!request.contains(QStringLiteral("@extra")));

    // The builder must survive the compact JSON round-trip the transport uses.
    const QByteArray json = QJsonDocument(request).toJson(QJsonDocument::Compact);
    QVERIFY(!json.isEmpty());
    const QJsonObject decoded = QJsonDocument::fromJson(json).object();
    QCOMPARE(decoded.value(QStringLiteral("@type")).toString(),
             QStringLiteral("sendMessage"));
    QCOMPARE(decoded.value(QStringLiteral("input_message_content")).toObject()
                 .value(QStringLiteral("@type")).toString(),
             QStringLiteral("inputMessagePoll"));

    const QByteArray lower = json.toLower();
    QVERIFY(!lower.contains("http://"));
    QVERIFY(!lower.contains("https://"));
    QVERIFY(!lower.contains("file://"));
}

QTEST_GUILESS_MAIN(PollSendTest)
#include "tst_poll_send.moc"
