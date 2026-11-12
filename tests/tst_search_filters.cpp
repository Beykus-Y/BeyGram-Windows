#include "telegram/SearchFilters.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QtTest>

using namespace beygram::telegram;

namespace {

constexpr qint64 kMaxSearchDate = 2147483647LL;

SearchFilter filterWithKind(const QString& kind)
{
    SearchFilter filter;
    filter.kind = kind;
    return filter;
}

QJsonValue kindFilterOf(const QJsonObject& request)
{
    return request.value(QStringLiteral("filter"));
}

QString kindTypeOf(const QJsonObject& request)
{
    return kindFilterOf(request).toObject().value(QStringLiteral("@type")).toString();
}

} // namespace

class SearchFiltersTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsInChatRequest();
    void mapsKindsToTdlibFilters_data();
    void mapsKindsToTdlibFilters();
    void setsInChatSender();
    void ignoresSenderForGlobalSearch();
    void clampsLimits();
    void buildsGlobalRequest();
    void clampsAndOrdersDates();
    void omitsDatesFromInChatRequest();
};

void SearchFiltersTest::buildsInChatRequest()
{
    const QJsonObject request = searchChatMessagesFilteredRequest(
        42, QStringLiteral("needle"), SearchFilter{}, 1000, 50);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("searchChatMessages"));
    QCOMPARE(request.value(QStringLiteral("chat_id")).toVariant().toLongLong(), qint64(42));
    QCOMPARE(request.value(QStringLiteral("query")).toString(), QStringLiteral("needle"));
    QCOMPARE(request.value(QStringLiteral("from_message_id")).toVariant().toLongLong(),
             qint64(1000));
    QCOMPARE(request.value(QStringLiteral("offset")).toInt(), 0);
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 50);

    QVERIFY(request.value(QStringLiteral("topic_id")).isNull());
    QVERIFY(request.value(QStringLiteral("sender_id")).isNull());
    QVERIFY(kindFilterOf(request).isNull());
    QVERIFY(!request.contains(QStringLiteral("chat_list")));
}

void SearchFiltersTest::mapsKindsToTdlibFilters_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<QString>("expectedType");

    QTest::newRow("any") << QStringLiteral("any") << QString();
    QTest::newRow("empty") << QString() << QString();
    QTest::newRow("photo") << QStringLiteral("photo")
                           << QStringLiteral("searchMessagesFilterPhoto");
    QTest::newRow("video") << QStringLiteral("video")
                           << QStringLiteral("searchMessagesFilterVideo");
    QTest::newRow("document") << QStringLiteral("document")
                              << QStringLiteral("searchMessagesFilterDocument");
    QTest::newRow("audio") << QStringLiteral("audio")
                           << QStringLiteral("searchMessagesFilterAudio");
    QTest::newRow("voice") << QStringLiteral("voice")
                           << QStringLiteral("searchMessagesFilterVoiceNote");
    QTest::newRow("url") << QStringLiteral("url") << QStringLiteral("searchMessagesFilterUrl");
    QTest::newRow("mention") << QStringLiteral("mention")
                             << QStringLiteral("searchMessagesFilterMention");

    // The kind comes from UI state, so casing and padding must not matter.
    QTest::newRow("padded upper") << QStringLiteral("  PHOTO ")
                                  << QStringLiteral("searchMessagesFilterPhoto");

    // Unknown kinds fall back to "any" instead of building an invalid request.
    QTest::newRow("unknown") << QStringLiteral("sticker") << QString();
}

void SearchFiltersTest::mapsKindsToTdlibFilters()
{
    QFETCH(QString, kind);
    QFETCH(QString, expectedType);

    const SearchFilter filter = filterWithKind(kind);

    const QJsonObject inChat =
        searchChatMessagesFilteredRequest(1, QStringLiteral("q"), filter, 0, 20);
    const QJsonObject global =
        searchMessagesFilteredRequest(QStringLiteral("q"), filter, QString(), 20);

    if (expectedType.isEmpty()) {
        QVERIFY(kindFilterOf(inChat).isNull());
        QVERIFY(kindFilterOf(global).isNull());
    } else {
        QCOMPARE(kindTypeOf(inChat), expectedType);
        QCOMPARE(kindTypeOf(global), expectedType);
    }
}

void SearchFiltersTest::setsInChatSender()
{
    SearchFilter filter;
    filter.senderUserId = 7;

    const QJsonObject request =
        searchChatMessagesFilteredRequest(1, QStringLiteral("q"), filter, 0, 20);
    const QJsonObject sender = request.value(QStringLiteral("sender_id")).toObject();
    QCOMPARE(sender.value(QStringLiteral("@type")).toString(),
             QStringLiteral("messageSenderUser"));
    QCOMPARE(sender.value(QStringLiteral("user_id")).toVariant().toLongLong(), qint64(7));

    filter.senderUserId = 0;
    QVERIFY(searchChatMessagesFilteredRequest(1, QStringLiteral("q"), filter, 0, 20)
                .value(QStringLiteral("sender_id"))
                .isNull());

    filter.senderUserId = -3;
    QVERIFY(searchChatMessagesFilteredRequest(1, QStringLiteral("q"), filter, 0, 20)
                .value(QStringLiteral("sender_id"))
                .isNull());
}

void SearchFiltersTest::ignoresSenderForGlobalSearch()
{
    SearchFilter filter;
    filter.senderUserId = 99;

    const QJsonObject request =
        searchMessagesFilteredRequest(QStringLiteral("q"), filter, QString(), 20);
    QVERIFY(!request.contains(QStringLiteral("sender_id")));
}

void SearchFiltersTest::clampsLimits()
{
    QCOMPARE(searchChatMessagesFilteredRequest(1, QStringLiteral("q"), SearchFilter{}, 0, 0)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1);
    QCOMPARE(searchChatMessagesFilteredRequest(1, QStringLiteral("q"), SearchFilter{}, 0, -10)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1);
    QCOMPARE(searchChatMessagesFilteredRequest(1, QStringLiteral("q"), SearchFilter{}, 0, 100)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             100);
    QCOMPARE(searchChatMessagesFilteredRequest(1, QStringLiteral("q"), SearchFilter{}, 0, 101)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             100);
    QCOMPARE(searchChatMessagesFilteredRequest(1, QStringLiteral("q"), SearchFilter{}, 0, 5000)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             100);

    QCOMPARE(searchMessagesFilteredRequest(QStringLiteral("q"), SearchFilter{}, QString(), 0)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             1);
    QCOMPARE(searchMessagesFilteredRequest(QStringLiteral("q"), SearchFilter{}, QString(), 101)
                 .value(QStringLiteral("limit"))
                 .toInt(),
             100);
}

void SearchFiltersTest::buildsGlobalRequest()
{
    SearchFilter filter;
    filter.kind = QStringLiteral("video");
    filter.minDate = 1700000000;
    filter.maxDate = 1700003600;

    const QJsonObject request = searchMessagesFilteredRequest(
        QStringLiteral("cat videos"), filter, QStringLiteral("opaque-offset"), 250);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("searchMessages"));
    QVERIFY(request.value(QStringLiteral("chat_list")).isNull());
    QCOMPARE(request.value(QStringLiteral("query")).toString(),
             QStringLiteral("cat videos"));
    QCOMPARE(request.value(QStringLiteral("offset")).toString(),
             QStringLiteral("opaque-offset"));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 100);
    QCOMPARE(kindTypeOf(request), QStringLiteral("searchMessagesFilterVideo"));
    QVERIFY(request.value(QStringLiteral("chat_type_filter")).isNull());
    QCOMPARE(request.value(QStringLiteral("min_date")).toVariant().toLongLong(),
             qint64(1700000000));
    QCOMPARE(request.value(QStringLiteral("max_date")).toVariant().toLongLong(),
             qint64(1700003600));
    QVERIFY(!request.contains(QStringLiteral("from_message_id")));

    // The first page is an empty offset, and the query is passed through
    // verbatim (the server owns trimming).
    const QJsonObject firstPage =
        searchMessagesFilteredRequest(QStringLiteral("  spaced  "), SearchFilter{}, QString(), 20);
    QVERIFY(firstPage.value(QStringLiteral("offset")).isString());
    QVERIFY(firstPage.value(QStringLiteral("offset")).toString().isEmpty());
    QCOMPARE(firstPage.value(QStringLiteral("query")).toString(), QStringLiteral("  spaced  "));
}

void SearchFiltersTest::clampsAndOrdersDates()
{
    SearchFilter filter;
    QCOMPARE(searchMessagesFilteredRequest(QStringLiteral("q"), filter, QString(), 20)
                 .value(QStringLiteral("min_date"))
                 .toVariant()
                 .toLongLong(),
             qint64(0));
    QCOMPARE(searchMessagesFilteredRequest(QStringLiteral("q"), filter, QString(), 20)
                 .value(QStringLiteral("max_date"))
                 .toVariant()
                 .toLongLong(),
             qint64(0));

    filter.minDate = -500;
    filter.maxDate = -1;
    QJsonObject request = searchMessagesFilteredRequest(QStringLiteral("q"), filter, QString(), 20);
    QCOMPARE(request.value(QStringLiteral("min_date")).toVariant().toLongLong(), qint64(0));
    QCOMPARE(request.value(QStringLiteral("max_date")).toVariant().toLongLong(), qint64(0));

    filter.minDate = kMaxSearchDate + 1000;
    filter.maxDate = kMaxSearchDate * 4;
    request = searchMessagesFilteredRequest(QStringLiteral("q"), filter, QString(), 20);
    QCOMPARE(request.value(QStringLiteral("min_date")).toVariant().toLongLong(), kMaxSearchDate);
    QCOMPARE(request.value(QStringLiteral("max_date")).toVariant().toLongLong(), kMaxSearchDate);

    // One-sided ranges keep the unbounded side at 0.
    filter.minDate = 1000;
    filter.maxDate = 0;
    request = searchMessagesFilteredRequest(QStringLiteral("q"), filter, QString(), 20);
    QCOMPARE(request.value(QStringLiteral("min_date")).toVariant().toLongLong(), qint64(1000));
    QCOMPARE(request.value(QStringLiteral("max_date")).toVariant().toLongLong(), qint64(0));

    filter.minDate = 0;
    filter.maxDate = 2000;
    request = searchMessagesFilteredRequest(QStringLiteral("q"), filter, QString(), 20);
    QCOMPARE(request.value(QStringLiteral("min_date")).toVariant().toLongLong(), qint64(0));
    QCOMPARE(request.value(QStringLiteral("max_date")).toVariant().toLongLong(), qint64(2000));

    // A reversed range becomes a valid interval instead of an empty one.
    filter.minDate = 2000;
    filter.maxDate = 1000;
    request = searchMessagesFilteredRequest(QStringLiteral("q"), filter, QString(), 20);
    QCOMPARE(request.value(QStringLiteral("min_date")).toVariant().toLongLong(), qint64(1000));
    QCOMPARE(request.value(QStringLiteral("max_date")).toVariant().toLongLong(), qint64(2000));

    // Equal bounds are a single instant, which TDLib accepts as-is.
    filter.minDate = 1500;
    filter.maxDate = 1500;
    request = searchMessagesFilteredRequest(QStringLiteral("q"), filter, QString(), 20);
    QCOMPARE(request.value(QStringLiteral("min_date")).toVariant().toLongLong(), qint64(1500));
    QCOMPARE(request.value(QStringLiteral("max_date")).toVariant().toLongLong(), qint64(1500));
}

void SearchFiltersTest::omitsDatesFromInChatRequest()
{
    SearchFilter filter;
    filter.minDate = 1000;
    filter.maxDate = 2000;

    const QJsonObject request =
        searchChatMessagesFilteredRequest(1, QStringLiteral("q"), filter, 0, 20);
    QVERIFY(!request.contains(QStringLiteral("min_date")));
    QVERIFY(!request.contains(QStringLiteral("max_date")));
}

QTEST_GUILESS_MAIN(SearchFiltersTest)
#include "tst_search_filters.moc"
