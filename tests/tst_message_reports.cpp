#include "telegram/MessageReports.h"

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

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

// Requests are compared after a JSON round trip so the assertions see exactly
// what would be handed to TDLib.
QJsonObject roundTrip(const QJsonObject& object)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QJsonDocument::fromJson(json).object();
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

class MessageReportsTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesReasonsInOrder();
    void skipsReasonsWithoutIdOrTitle();
    void acceptsReasonListAndNestedResult();
    void ignoresMalformedResponses();
    void buildsOptionsRequest();
    void normalizesMessageIds();
    void rejectsInvalidOptionsRequests();
    void buildsReportChatRequest();
    void trimsAndClampsComment();
    void rejectsEmptyReason();
    void buildsReportChatPhotoRequest();
    void rejectsInvalidPhotoRequest();
    void buildsSupergroupSpamRequest();
    void supergroupSpamNeedsMessageIds();
    void buildsAntiSpamFalsePositiveRequest();
    void requestsSurviveSerialization();
};

void MessageReportsTest::parsesReasonsInOrder()
{
    const QJsonObject response = parseObject(R"({
        "@type": "reportChatResultOptionRequired",
        "title": "Report",
        "options": [
            {
                "@type": "reportOption",
                "id": "c3BhbQ==",
                "text": "Spam",
                "description": "Unsolicited messages",
                "requires_comment": false
            },
            {
                "@type": "reportOption",
                "id": "dmlvbGVuY2U=",
                "text": "Violence",
                "requires_comment": true
            },
            {
                "@type": "reportOption",
                "id": "cGVyc29uYWw=",
                "text": "Personal data",
                "description": "Shares private data"
            }
        ]
    })");

    const QVector<ReportReason> reasons = reportReasonsFromOptions(response);
    QCOMPARE(reasons.size(), 3);

    QCOMPARE(reasons.at(0).id, QStringLiteral("c3BhbQ=="));
    QCOMPARE(reasons.at(0).title, QStringLiteral("Spam"));
    QCOMPARE(reasons.at(0).description, QStringLiteral("Unsolicited messages"));
    QCOMPARE(reasons.at(0).requiresComment, false);

    QCOMPARE(reasons.at(1).id, QStringLiteral("dmlvbGVuY2U="));
    QCOMPARE(reasons.at(1).title, QStringLiteral("Violence"));
    QCOMPARE(reasons.at(1).description, QString());
    QCOMPARE(reasons.at(1).requiresComment, true);

    // The third reason keeps its position even though the first two were also
    // complete; the parser never sorts or deduplicates the list.
    QCOMPARE(reasons.at(2).id, QStringLiteral("cGVyc29uYWw="));
    QCOMPARE(reasons.at(2).title, QStringLiteral("Personal data"));
    QCOMPARE(reasons.at(2).description, QStringLiteral("Shares private data"));
    QCOMPARE(reasons.at(2).requiresComment, false);
}

void MessageReportsTest::skipsReasonsWithoutIdOrTitle()
{
    const QJsonObject response = parseObject(R"({
        "options": [
            { "id": "", "title": "No id" },
            { "id": "no-title", "title": "   " },
            { "id": "  ", "title": "Blank id" },
            { "id": "kept", "title": "Kept" },
            { "id": "kept-too", "text": "Kept too" },
            { "id": "kept-label", "label": "Kept label" },
            42,
            null,
            "not an object"
        ]
    })");

    const QVector<ReportReason> reasons = reportReasonsFromOptions(response);
    QCOMPARE(reasons.size(), 3);
    QCOMPARE(reasons.at(0).id, QStringLiteral("kept"));
    QCOMPARE(reasons.at(0).title, QStringLiteral("Kept"));
    QCOMPARE(reasons.at(1).id, QStringLiteral("kept-too"));
    QCOMPARE(reasons.at(1).title, QStringLiteral("Kept too"));
    QCOMPARE(reasons.at(2).id, QStringLiteral("kept-label"));
    QCOMPARE(reasons.at(2).title, QStringLiteral("Kept label"));
}

void MessageReportsTest::acceptsReasonListAndNestedResult()
{
    const QJsonObject reasonsResponse = parseObject(R"({
        "reasons": [
            {
                "reason_id": " spam ",
                "title": " Spam ",
                "subtitle": " Junk ",
                "requiresComment": true
            }
        ]
    })");

    const QVector<ReportReason> reasons = reportReasonsFromOptions(reasonsResponse);
    QCOMPARE(reasons.size(), 1);
    QCOMPARE(reasons.at(0).id, QStringLiteral("spam"));
    QCOMPARE(reasons.at(0).title, QStringLiteral("Spam"));
    QCOMPARE(reasons.at(0).description, QStringLiteral("Junk"));
    QCOMPARE(reasons.at(0).requiresComment, true);

    const QJsonObject nested = parseObject(R"({
        "result": {
            "options": [
                { "id": "violence", "title": "Violence" }
            ]
        }
    })");
    const QVector<ReportReason> nestedReasons = reportReasonsFromOptions(nested);
    QCOMPARE(nestedReasons.size(), 1);
    QCOMPARE(nestedReasons.at(0).id, QStringLiteral("violence"));
    QCOMPARE(nestedReasons.at(0).title, QStringLiteral("Violence"));
}

void MessageReportsTest::ignoresMalformedResponses()
{
    QVERIFY(reportReasonsFromOptions(QJsonObject{}).isEmpty());
    QVERIFY(reportReasonsFromOptions(parseObject(R"({"options": {}})")).isEmpty());
    QVERIFY(reportReasonsFromOptions(parseObject(R"({"options": []})")).isEmpty());
    QVERIFY(reportReasonsFromOptions(parseObject(R"({"reasons": "spam"})")).isEmpty());
    QVERIFY(reportReasonsFromOptions(
                parseObject(R"({"@type": "ok"})")).isEmpty());
    QVERIFY(reportReasonsFromOptions(
                parseObject(R"({"@type": "error", "message": "404"})")).isEmpty());
}

void MessageReportsTest::buildsOptionsRequest()
{
    const QJsonObject request = getReportChatOptionsRequest(42, {7, 8, 7, 0, -3});

    // TDLib has no getReportChatOptions: the option list is requested with an
    // initial reportChat that carries an empty option_id and no text.
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reportChat"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(request.value(QStringLiteral("option_id")).toString(), QString());
    QCOMPARE(request.value(QStringLiteral("text")).toString(), QString());
    QVERIFY(!request.contains(QStringLiteral("reason")));

    const QJsonArray ids = idsAt(request);
    QCOMPARE(ids.size(), 2);
    QCOMPARE(ids.at(0).toVariant().toLongLong(), qint64(7));
    QCOMPARE(ids.at(1).toVariant().toLongLong(), qint64(8));

    // Reporting the chat as a whole is a valid first step, so an empty id list
    // still produces a request.
    const QJsonObject wholeChat = getReportChatOptionsRequest(42, {});
    QCOMPARE(wholeChat.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reportChat"));
    QVERIFY(idsAt(wholeChat).isEmpty());
}

void MessageReportsTest::normalizesMessageIds()
{
    QVector<qint64> input;
    input.reserve(110);
    for (int i = 0; i < 105; ++i)
        input.append(i + 1);
    input.append(1);
    input.append(0);
    input.append(-5);

    const QJsonArray ids = idsAt(getReportChatOptionsRequest(1, input));
    QCOMPARE(ids.size(), 100);
    QCOMPARE(ids.at(0).toVariant().toLongLong(), qint64(1));
    QCOMPARE(ids.at(99).toVariant().toLongLong(), qint64(100));

    // The same normalization applies to the actual report.
    const QJsonObject report = reportChatRequest(1, {3, 3, 2, -1, 0},
                                                 QStringLiteral("spam"), QString());
    const QJsonArray reportIds = idsAt(report);
    QCOMPARE(reportIds.size(), 2);
    QCOMPARE(reportIds.at(0).toVariant().toLongLong(), qint64(3));
    QCOMPARE(reportIds.at(1).toVariant().toLongLong(), qint64(2));
}

void MessageReportsTest::rejectsInvalidOptionsRequests()
{
    QVERIFY(getReportChatOptionsRequest(0, {1}).isEmpty());
    QVERIFY(getReportChatOptionsRequest(-1, {1}).isEmpty());
    QVERIFY(getReportChatOptionsRequest(-1001234567890LL, {1}).isEmpty());
}

void MessageReportsTest::buildsReportChatRequest()
{
    const QJsonObject request = reportChatRequest(
        5, {77, 77, 0}, QStringLiteral("  c3BhbQ==  "), QStringLiteral("  details  "));

    QCOMPARE(request.size(), 5);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reportChat"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(5));
    // The option id is opaque bytes and travels base64-encoded, exactly as it
    // came back in ReportReason::id.
    QCOMPARE(request.value(QStringLiteral("option_id")).toString(),
             QStringLiteral("c3BhbQ=="));
    QCOMPARE(request.value(QStringLiteral("text")).toString(),
             QStringLiteral("details"));
    QVERIFY(!request.contains(QStringLiteral("reason")));

    const QJsonArray ids = idsAt(request);
    QCOMPARE(ids.size(), 1);
    QCOMPARE(ids.at(0).toVariant().toLongLong(), qint64(77));
}

void MessageReportsTest::trimsAndClampsComment()
{
    const QJsonObject trimmed = reportChatRequest(
        5, {77}, QStringLiteral("spam"), QStringLiteral("  details  "));
    QCOMPARE(trimmed.value(QStringLiteral("text")).toString(),
             QStringLiteral("details"));

    const QJsonObject longComment = reportChatRequest(
        5, {77}, QStringLiteral("spam"), QString(2000, QLatin1Char('x')));
    QCOMPARE(longComment.value(QStringLiteral("text")).toString().size(), 1024);

    // Trimming happens before the clamp, so surrounding whitespace cannot eat
    // into the 1024 characters the schema allows.
    const QString padded = QStringLiteral("  ") + QString(2000, QLatin1Char('y'))
        + QStringLiteral("  ");
    const QJsonObject paddedComment =
        reportChatRequest(5, {77}, QStringLiteral("spam"), padded);
    const QString text = paddedComment.value(QStringLiteral("text")).toString();
    QCOMPARE(text.size(), 1024);
    QCOMPARE(text.at(0), QLatin1Char('y'));
    QCOMPARE(text.at(1023), QLatin1Char('y'));

    const QJsonObject photo = reportChatPhotoRequest(
        5, 9, QStringLiteral("reportReasonSpam"), QString(2000, QLatin1Char('z')));
    QCOMPARE(photo.value(QStringLiteral("text")).toString().size(), 1024);
}

void MessageReportsTest::rejectsEmptyReason()
{
    QVERIFY(reportChatRequest(5, {77}, QString(), QStringLiteral("details")).isEmpty());
    QVERIFY(reportChatRequest(5, {77}, QStringLiteral("   "),
                              QStringLiteral("details")).isEmpty());
    QVERIFY(reportChatRequest(0, {77}, QStringLiteral("spam"), QString()).isEmpty());
    QVERIFY(reportChatRequest(-5, {77}, QStringLiteral("spam"), QString()).isEmpty());
}

void MessageReportsTest::buildsReportChatPhotoRequest()
{
    const QJsonObject request = reportChatPhotoRequest(
        5, 900, QStringLiteral("  reportReasonSpam  "), QStringLiteral("  details  "));

    QCOMPARE(request.size(), 5);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reportChatPhoto"));
    QCOMPARE(numberAt(request, QStringLiteral("chat_id")), qint64(5));
    QCOMPARE(numberAt(request, QStringLiteral("file_id")), qint64(900));
    QCOMPARE(request.value(QStringLiteral("text")).toString(),
             QStringLiteral("details"));

    // reportChatPhoto carries a ReportReason object, not an option_id.
    const QJsonObject reason = request.value(QStringLiteral("reason")).toObject();
    QCOMPARE(reason.size(), 1);
    QCOMPARE(reason.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reportReasonSpam"));
    QVERIFY(!request.contains(QStringLiteral("option_id")));
}

void MessageReportsTest::rejectsInvalidPhotoRequest()
{
    QVERIFY(reportChatPhotoRequest(0, 9, QStringLiteral("reportReasonSpam"),
                                   QString()).isEmpty());
    QVERIFY(reportChatPhotoRequest(-5, 9, QStringLiteral("reportReasonSpam"),
                                   QString()).isEmpty());
    QVERIFY(reportChatPhotoRequest(5, 0, QStringLiteral("reportReasonSpam"),
                                   QString()).isEmpty());
    QVERIFY(reportChatPhotoRequest(5, -9, QStringLiteral("reportReasonSpam"),
                                   QString()).isEmpty());
    QVERIFY(reportChatPhotoRequest(5, 9, QString(), QString()).isEmpty());
    QVERIFY(reportChatPhotoRequest(5, 9, QStringLiteral("   "), QString()).isEmpty());
}

void MessageReportsTest::buildsSupergroupSpamRequest()
{
    const QJsonObject request = reportSupergroupSpamRequest(123, {9, 8, 9, 0, -4});

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reportSupergroupSpam"));
    QCOMPARE(numberAt(request, QStringLiteral("supergroup_id")), qint64(123));

    const QJsonArray ids = idsAt(request);
    QCOMPARE(ids.size(), 2);
    QCOMPARE(ids.at(0).toVariant().toLongLong(), qint64(9));
    QCOMPARE(ids.at(1).toVariant().toLongLong(), qint64(8));
    QVERIFY(!request.contains(QStringLiteral("chat_id")));
}

void MessageReportsTest::supergroupSpamNeedsMessageIds()
{
    QVERIFY(reportSupergroupSpamRequest(123, {}).isEmpty());
    QVERIFY(reportSupergroupSpamRequest(123, {0, -1}).isEmpty());
    QVERIFY(reportSupergroupSpamRequest(0, {9}).isEmpty());
    QVERIFY(reportSupergroupSpamRequest(-123, {9}).isEmpty());
}

void MessageReportsTest::buildsAntiSpamFalsePositiveRequest()
{
    const QJsonObject request =
        reportSupergroupAntiSpamFalsePositiveRequest(123, 456);

    QCOMPARE(request.size(), 3);
    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reportSupergroupAntiSpamFalsePositive"));
    QCOMPARE(numberAt(request, QStringLiteral("supergroup_id")), qint64(123));
    QCOMPARE(numberAt(request, QStringLiteral("message_id")), qint64(456));

    QVERIFY(reportSupergroupAntiSpamFalsePositiveRequest(0, 456).isEmpty());
    QVERIFY(reportSupergroupAntiSpamFalsePositiveRequest(-123, 456).isEmpty());
    QVERIFY(reportSupergroupAntiSpamFalsePositiveRequest(123, 0).isEmpty());
    QVERIFY(reportSupergroupAntiSpamFalsePositiveRequest(123, -456).isEmpty());
}

void MessageReportsTest::requestsSurviveSerialization()
{
    const QJsonObject options = roundTrip(getReportChatOptionsRequest(42, {7, 8}));
    QCOMPARE(options.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reportChat"));
    QCOMPARE(numberAt(options, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(options.value(QStringLiteral("option_id")).toString(), QString());
    QCOMPARE(idsAt(options).size(), 2);

    const QJsonObject report = roundTrip(reportChatRequest(
        9007199254740991LL, {9007199254740991LL},
        QStringLiteral("c3BhbQ=="), QStringLiteral("details")));
    QCOMPARE(numberAt(report, QStringLiteral("chat_id")), qint64(9007199254740991LL));
    QCOMPARE(idsAt(report).at(0).toVariant().toLongLong(), qint64(9007199254740991LL));
    QCOMPARE(report.value(QStringLiteral("option_id")).toString(),
             QStringLiteral("c3BhbQ=="));

    const QJsonObject photo = roundTrip(reportChatPhotoRequest(
        5, 900, QStringLiteral("reportReasonSpam"), QStringLiteral("details")));
    QCOMPARE(photo.value(QStringLiteral("reason"))
                 .toObject()
                 .value(QStringLiteral("@type"))
                 .toString(),
             QStringLiteral("reportReasonSpam"));
    QCOMPARE(numberAt(photo, QStringLiteral("file_id")), qint64(900));

    const QJsonObject spam = roundTrip(reportSupergroupSpamRequest(123, {9}));
    QCOMPARE(spam.value(QStringLiteral("@type")).toString(),
             QStringLiteral("reportSupergroupSpam"));
    QCOMPARE(idsAt(spam).size(), 1);

    const QJsonObject falsePositive = roundTrip(
        reportSupergroupAntiSpamFalsePositiveRequest(123, 456));
    QCOMPARE(numberAt(falsePositive, QStringLiteral("message_id")), qint64(456));
}

QTEST_GUILESS_MAIN(MessageReportsTest)
#include "tst_message_reports.moc"
