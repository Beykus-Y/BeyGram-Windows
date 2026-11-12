#include "telegram/ChatExport.h"
#include "telegram/ChatExportWriter.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <QtTest>

using namespace beygram::telegram;

namespace {

QJsonObject parseObject(const char* json)
{
    return QJsonDocument::fromJson(QByteArray(json)).object();
}

qint64 int64At(const QJsonObject& object, const QString& key)
{
    return object.value(key).toVariant().toLongLong();
}

QByteArray compactJson(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

ExportedMessage message(qint64 id, qint64 date, const QString& sender, const QString& text,
                        const QString& mediaType = QString())
{
    ExportedMessage value;
    value.id = id;
    value.date = date;
    value.sender = sender;
    value.text = text;
    value.mediaType = mediaType;
    return value;
}

} // namespace

class ChatExportTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsRequestShape();
    void usesOptionDefaults();
    void rejectsInvalidChatId();
    void clampsLimit();
    void normalizesDateRange();
    void normalizesNegativeMessageId();
    void parsesProgressUpdate();
    void parsesNestedProgressObject();
    void parsesCamelCaseAndStringCounters();
    void clampsNegativeProgressCounters();
    void ignoresUnrelatedUpdate();
    void marksCompletedFileUpdateDone();
    void trimsErrorObject();
    void htmlEscapesAllText();
    void htmlOrdersMessagesByDateThenId();
    void htmlUsesChatFallbackTitle();
    void htmlHandlesEmptyExport();
    void jsonShape();
    void jsonOrdersMessagesByDateThenId();
    void jsonUsesIsoDatesAndEmptyForMissingDate();
    void outputIsStable();
};

void ChatExportTest::buildsRequestShape()
{
    ChatExportOptions options;
    options.includeMedia = true;
    options.includePhotos = false;
    options.includeVideos = true;
    options.includeFiles = false;
    options.includeVoice = true;
    options.includeLinks = false;
    options.onlyMyMessages = true;
    options.fromDate = 1000;
    options.toDate = 2000;

    const QJsonObject request = exportChatHistoryRequest(42, options, 900, 50);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("exportChatHistory"));
    QCOMPARE(int64At(request, QStringLiteral("chat_id")), qint64(42));
    QCOMPARE(int64At(request, QStringLiteral("from_message_id")), qint64(900));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);
    QCOMPARE(int64At(request, QStringLiteral("from_date")), qint64(1000));
    QCOMPARE(int64At(request, QStringLiteral("to_date")), qint64(2000));
    QVERIFY(request.value(QStringLiteral("only_my_messages")).toBool());
    QVERIFY(request.value(QStringLiteral("include_media")).toBool());
    QVERIFY(!request.value(QStringLiteral("include_photos")).toBool());
    QVERIFY(request.value(QStringLiteral("include_videos")).toBool());
    QVERIFY(!request.value(QStringLiteral("include_files")).toBool());
    QVERIFY(request.value(QStringLiteral("include_voice")).toBool());
    QVERIFY(!request.value(QStringLiteral("include_links")).toBool());
}

void ChatExportTest::usesOptionDefaults()
{
    const ChatExportOptions options;
    const QJsonObject request = exportChatHistoryRequest(1, options, 0, 10);

    QVERIFY(!request.value(QStringLiteral("include_media")).toBool());
    QVERIFY(request.value(QStringLiteral("include_photos")).toBool());
    QVERIFY(request.value(QStringLiteral("include_videos")).toBool());
    QVERIFY(request.value(QStringLiteral("include_files")).toBool());
    QVERIFY(request.value(QStringLiteral("include_voice")).toBool());
    QVERIFY(request.value(QStringLiteral("include_links")).toBool());
    QVERIFY(!request.value(QStringLiteral("only_my_messages")).toBool());
    QCOMPARE(int64At(request, QStringLiteral("from_date")), qint64(0));
    QCOMPARE(int64At(request, QStringLiteral("to_date")), qint64(0));
}

void ChatExportTest::rejectsInvalidChatId()
{
    const ChatExportOptions options;
    QVERIFY(exportChatHistoryRequest(0, options, 0, 10).isEmpty());
    QVERIFY(exportChatHistoryRequest(-7, options, 0, 10).isEmpty());
}

void ChatExportTest::clampsLimit()
{
    const ChatExportOptions options;
    QCOMPARE(exportChatHistoryRequest(1, options, 0, 0)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1);
    QCOMPARE(exportChatHistoryRequest(1, options, 0, -20)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1);
    QCOMPARE(exportChatHistoryRequest(1, options, 0, 1001)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1000);
    QCOMPARE(exportChatHistoryRequest(1, options, 0, 1000)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1000);
}

void ChatExportTest::normalizesDateRange()
{
    ChatExportOptions options;
    options.fromDate = 2000;
    options.toDate = 1000;
    QJsonObject request = exportChatHistoryRequest(1, options, 0, 10);
    QCOMPARE(int64At(request, QStringLiteral("from_date")), qint64(1000));
    QCOMPARE(int64At(request, QStringLiteral("to_date")), qint64(2000));

    options.fromDate = -50;
    options.toDate = -10;
    request = exportChatHistoryRequest(1, options, 0, 10);
    QCOMPARE(int64At(request, QStringLiteral("from_date")), qint64(0));
    QCOMPARE(int64At(request, QStringLiteral("to_date")), qint64(0));

    options.fromDate = -50;
    options.toDate = 100;
    request = exportChatHistoryRequest(1, options, 0, 10);
    QCOMPARE(int64At(request, QStringLiteral("from_date")), qint64(0));
    QCOMPARE(int64At(request, QStringLiteral("to_date")), qint64(100));
}

void ChatExportTest::normalizesNegativeMessageId()
{
    const ChatExportOptions options;
    const QJsonObject request = exportChatHistoryRequest(1, options, -99, 10);
    QCOMPARE(int64At(request, QStringLiteral("from_message_id")), qint64(0));
}

void ChatExportTest::parsesProgressUpdate()
{
    const QJsonObject update = parseObject(R"({
        "@type": "updateChatExportProgress",
        "messages_exported": 5,
        "messages_total": 10,
        "files_exported": 2,
        "done": true,
        "error": "  disk full  "
    })");

    const ChatExportProgress progress = exportProgressFromUpdate(update);

    QCOMPARE(progress.messagesExported, 5);
    QCOMPARE(progress.messagesTotal, 10);
    QCOMPARE(progress.filesExported, 2);
    QVERIFY(progress.done);
    QCOMPARE(progress.error, QStringLiteral("disk full"));
}

void ChatExportTest::parsesNestedProgressObject()
{
    const QJsonObject update = parseObject(R"({
        "@type": "updateChatExportProgress",
        "progress": {
            "messages_exported": 3,
            "messages_total": 8,
            "files_exported": 1,
            "status": "completed"
        }
    })");

    const ChatExportProgress progress = exportProgressFromUpdate(update);

    QCOMPARE(progress.messagesExported, 3);
    QCOMPARE(progress.messagesTotal, 8);
    QCOMPARE(progress.filesExported, 1);
    QVERIFY(progress.done);
}

void ChatExportTest::parsesCamelCaseAndStringCounters()
{
    const QJsonObject update = parseObject(R"({
        "@type": "updateChatExportProgress",
        "messagesExported": "7",
        "messagesTotal": "12",
        "filesExported": "3",
        "finished": true
    })");

    const ChatExportProgress progress = exportProgressFromUpdate(update);

    QCOMPARE(progress.messagesExported, 7);
    QCOMPARE(progress.messagesTotal, 12);
    QCOMPARE(progress.filesExported, 3);
    QVERIFY(progress.done);
    QVERIFY(progress.error.isEmpty());
}

void ChatExportTest::clampsNegativeProgressCounters()
{
    const QJsonObject update = parseObject(R"({
        "@type": "updateChatExportProgress",
        "messages_exported": -5,
        "messages_total": -1,
        "files_exported": -100
    })");

    const ChatExportProgress progress = exportProgressFromUpdate(update);

    QCOMPARE(progress.messagesExported, 0);
    QCOMPARE(progress.messagesTotal, 0);
    QCOMPARE(progress.filesExported, 0);
    QVERIFY(!progress.done);
}

void ChatExportTest::ignoresUnrelatedUpdate()
{
    const ChatExportProgress progress = exportProgressFromUpdate(
        parseObject(R"({"@type": "updateConnectionState", "state": "ready"})"));

    QCOMPARE(progress.messagesExported, 0);
    QCOMPARE(progress.messagesTotal, 0);
    QCOMPARE(progress.filesExported, 0);
    QVERIFY(!progress.done);
    QVERIFY(progress.error.isEmpty());
}

void ChatExportTest::marksCompletedFileUpdateDone()
{
    const QJsonObject update = parseObject(R"({
        "@type": "updateFile",
        "file": {
            "id": 7,
            "local": {"is_downloading_completed": true, "path": "C:/cache/file"}
        }
    })");

    const ChatExportProgress progress = exportProgressFromUpdate(update);
    QVERIFY(progress.done);
}

void ChatExportTest::trimsErrorObject()
{
    const QJsonObject update = parseObject(R"({
        "@type": "updateChatExportProgress",
        "error": {"code": 420, "message": "  flood wait  "}
    })");

    const ChatExportProgress progress = exportProgressFromUpdate(update);
    QCOMPARE(progress.error, QStringLiteral("flood wait"));
}

void ChatExportTest::htmlEscapesAllText()
{
    const QVector<ExportedMessage> messages{
        message(1, 1700000000, QStringLiteral("A & B <x>"),
                QStringLiteral("<script>alert('x')</script>"),
                QStringLiteral("\"quoted\" & <tag>")),
    };

    const QString html = ChatExportWriter::toHtml(
        QStringLiteral("T <&> \"Q\""), messages);

    QVERIFY(html.contains(QStringLiteral("&lt;script&gt;alert(&#39;x&#39;)&lt;/script&gt;")));
    QVERIFY(!html.contains(QStringLiteral("<script>")));
    QVERIFY(html.contains(QStringLiteral("A &amp; B &lt;x&gt;")));
    QVERIFY(html.contains(QStringLiteral("&quot;quoted&quot; &amp; &lt;tag&gt;")));
    QVERIFY(html.contains(QStringLiteral("<title>T &lt;&amp;&gt; &quot;Q&quot;</title>")));
    QVERIFY(html.contains(QStringLiteral("<h1>T &lt;&amp;&gt; &quot;Q&quot;</h1>")));
}

void ChatExportTest::htmlOrdersMessagesByDateThenId()
{
    const QVector<ExportedMessage> messages{
        message(1, 200, QStringLiteral("A"), QStringLiteral("late")),
        message(9, 100, QStringLiteral("B"), QStringLiteral("same date, high id")),
        message(2, 100, QStringLiteral("C"), QStringLiteral("same date, low id")),
        message(5, 0, QStringLiteral("D"), QStringLiteral("no date")),
    };

    const QString html = ChatExportWriter::toHtml(QStringLiteral("Order"), messages);

    const int first = html.indexOf(QStringLiteral("id=\"message-5\""));
    const int second = html.indexOf(QStringLiteral("id=\"message-2\""));
    const int third = html.indexOf(QStringLiteral("id=\"message-9\""));
    const int fourth = html.indexOf(QStringLiteral("id=\"message-1\""));

    QVERIFY(first >= 0);
    QVERIFY(second > first);
    QVERIFY(third > second);
    QVERIFY(fourth > third);
}

void ChatExportTest::htmlUsesChatFallbackTitle()
{
    const QString empty = ChatExportWriter::toHtml(QString(), {});
    QVERIFY(empty.contains(QStringLiteral("<title>Chat</title>")));
    QVERIFY(empty.contains(QStringLiteral("<h1>Chat</h1>")));

    const QString blank = ChatExportWriter::toHtml(QStringLiteral("   "), {});
    QVERIFY(blank.contains(QStringLiteral("<title>Chat</title>")));
}

void ChatExportTest::htmlHandlesEmptyExport()
{
    const QString html = ChatExportWriter::toHtml(QStringLiteral("Empty"), {});

    QVERIFY(html.startsWith(QStringLiteral("<!doctype html>")));
    QVERIFY(html.contains(QStringLiteral("<meta charset=\"utf-8\">")));
    QVERIFY(html.contains(QStringLiteral("0 messages")));
    QVERIFY(html.endsWith(QStringLiteral("</html>\n")));
}

void ChatExportTest::jsonShape()
{
    const QVector<ExportedMessage> messages{
        message(7, 1700000000, QStringLiteral("Alice"), QStringLiteral("Hi"),
                QStringLiteral("photo")),
    };

    const QJsonObject document =
        ChatExportWriter::toJson(QStringLiteral("Project"), messages);

    QCOMPARE(document.value(QStringLiteral("format")).toString(),
             QStringLiteral("beygram-chat-export"));
    QCOMPARE(document.value(QStringLiteral("version")).toInt(), 1);
    QCOMPARE(document.value(QStringLiteral("chat"))
                 .toObject()
                 .value(QStringLiteral("title"))
                 .toString(),
             QStringLiteral("Project"));

    const QJsonArray array = document.value(QStringLiteral("messages")).toArray();
    QCOMPARE(array.size(), 1);

    const QJsonObject entry = array.at(0).toObject();
    QCOMPARE(int64At(entry, QStringLiteral("id")), qint64(7));
    QCOMPARE(int64At(entry, QStringLiteral("date")), qint64(1700000000));
    QCOMPARE(entry.value(QStringLiteral("dateIso")).toString(),
             QStringLiteral("2023-11-14T22:13:20Z"));
    QCOMPARE(entry.value(QStringLiteral("sender")).toString(), QStringLiteral("Alice"));
    QCOMPARE(entry.value(QStringLiteral("text")).toString(), QStringLiteral("Hi"));
    QCOMPARE(entry.value(QStringLiteral("mediaType")).toString(), QStringLiteral("photo"));
    QCOMPARE(document.value(QStringLiteral("summary"))
                 .toObject()
                 .value(QStringLiteral("messages"))
                 .toInt(),
             1);
}

void ChatExportTest::jsonOrdersMessagesByDateThenId()
{
    const QVector<ExportedMessage> messages{
        message(1, 200, QStringLiteral("A"), QStringLiteral("late")),
        message(9, 100, QStringLiteral("B"), QStringLiteral("same date, high id")),
        message(2, 100, QStringLiteral("C"), QStringLiteral("same date, low id")),
        message(5, 0, QStringLiteral("D"), QStringLiteral("no date")),
    };

    const QJsonArray array =
        ChatExportWriter::toJson(QStringLiteral("Order"), messages)
            .value(QStringLiteral("messages"))
            .toArray();

    QCOMPARE(array.size(), 4);
    QCOMPARE(int64At(array.at(0).toObject(), QStringLiteral("id")), qint64(5));
    QCOMPARE(int64At(array.at(1).toObject(), QStringLiteral("id")), qint64(2));
    QCOMPARE(int64At(array.at(2).toObject(), QStringLiteral("id")), qint64(9));
    QCOMPARE(int64At(array.at(3).toObject(), QStringLiteral("id")), qint64(1));
}

void ChatExportTest::jsonUsesIsoDatesAndEmptyForMissingDate()
{
    const QVector<ExportedMessage> messages{
        message(1, 0, QStringLiteral("A"), QStringLiteral("epoch")),
        message(2, -5, QStringLiteral("B"), QStringLiteral("negative")),
    };

    const QJsonArray array =
        ChatExportWriter::toJson(QStringLiteral("Dates"), messages)
            .value(QStringLiteral("messages"))
            .toArray();

    QCOMPARE(array.at(0).toObject().value(QStringLiteral("dateIso")).toString(),
             QString());
    QCOMPARE(array.at(1).toObject().value(QStringLiteral("dateIso")).toString(),
             QString());

    const QString html = ChatExportWriter::toHtml(QStringLiteral("Dates"), messages);
    QVERIFY(html.contains(QStringLiteral("<time>Unknown date</time>")));
}

void ChatExportTest::outputIsStable()
{
    const QVector<ExportedMessage> messages{
        message(2, 100, QStringLiteral("B"), QStringLiteral("two")),
        message(1, 100, QStringLiteral("A"), QStringLiteral("one")),
    };

    QCOMPARE(ChatExportWriter::toHtml(QStringLiteral("Stable"), messages),
             ChatExportWriter::toHtml(QStringLiteral("Stable"), messages));
    QCOMPARE(compactJson(ChatExportWriter::toJson(QStringLiteral("Stable"), messages)),
             compactJson(ChatExportWriter::toJson(QStringLiteral("Stable"), messages)));

    const QJsonObject empty = ChatExportWriter::toJson(QStringLiteral("Chat"), {});
    QVERIFY(empty.value(QStringLiteral("messages")).isArray());
    QCOMPARE(empty.value(QStringLiteral("messages")).toArray().size(), 0);

    const QJsonObject blankTitle = ChatExportWriter::toJson(QStringLiteral("   "), {});
    QCOMPARE(blankTitle.value(QStringLiteral("chat"))
                 .toObject()
                 .value(QStringLiteral("title"))
                 .toString(),
             QStringLiteral("Chat"));
}

QTEST_GUILESS_MAIN(ChatExportTest)

#include "tst_chat_export.moc"
