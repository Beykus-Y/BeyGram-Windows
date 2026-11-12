#include "telegram/GlobalSearchEngine.h"

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram::globalsearch;

namespace {

SearchHit makeHit(SearchSource source, qint64 chatId, qint64 identity, int score, qint64 date,
                  const QString& title = {})
{
    SearchHit hit;
    hit.source = source;
    hit.chatId = chatId;
    hit.score = score;
    hit.date = date;
    hit.title = title;
    switch (source) {
    case SearchSource::Chats:
        break;
    case SearchSource::Messages:
        hit.messageId = identity;
        break;
    case SearchSource::People:
        hit.userId = identity;
        break;
    }
    return hit;
}

QVector<QString> titlesOf(const QVector<SearchHit>& hits)
{
    QVector<QString> titles;
    titles.reserve(hits.size());
    for (const SearchHit& hit : hits)
        titles.append(hit.title);
    return titles;
}

QVector<int> scoresOf(const QVector<SearchHit>& hits)
{
    QVector<int> scores;
    scores.reserve(hits.size());
    for (const SearchHit& hit : hits)
        scores.append(hit.score);
    return scores;
}

QVector<int> sourcesOf(const QVector<SearchHit>& hits)
{
    QVector<int> sources;
    sources.reserve(hits.size());
    for (const SearchHit& hit : hits)
        sources.append(static_cast<int>(hit.source));
    return sources;
}

} // namespace

class GlobalSearchEngineTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsChatSearchRequest();
    void buildsMessageSearchRequest();
    void buildsContactSearchRequest();
    void blankQueryBuildsNoRequest();
    void mergeSortsByScoreThenDate();
    void mergeDeduplicatesWithinEachSource();
    void mergeKeepsTheHighestScoringCopy();
    void mergeKeepsSourcesApart();
    void mergeCapsEverySource();
    void mergeInterleavesTheSources();
    void mergeKeepsStableOrderForEqualRanks();
};

void GlobalSearchEngineTest::buildsChatSearchRequest()
{
    const QJsonObject request = searchChatsOnServerRequest(QStringLiteral("  anna  "), 500);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("searchChatsOnServer"));
    QCOMPARE(request.value(QStringLiteral("query")).toString(), QStringLiteral("anna"));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 100);
    QVERIFY(request.value(QStringLiteral("chat_list")).isNull());

    const QJsonObject clampedLow = searchChatsOnServerRequest(QStringLiteral("anna"), 0);
    QCOMPARE(clampedLow.value(QStringLiteral("limit")).toInt(), 1);

    const QJsonObject unchanged = searchChatsOnServerRequest(QStringLiteral("anna"), 42);
    QCOMPARE(unchanged.value(QStringLiteral("limit")).toInt(), 42);
}

void GlobalSearchEngineTest::buildsMessageSearchRequest()
{
    const QJsonObject request =
        searchMessagesOnServerRequest(QStringLiteral("\tanna\n"), 1000);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("searchMessages"));
    QCOMPARE(request.value(QStringLiteral("query")).toString(), QStringLiteral("anna"));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 100);
    QVERIFY(request.value(QStringLiteral("chat_list")).isNull());
    QVERIFY(request.value(QStringLiteral("offset")).isString());
    QVERIFY(request.value(QStringLiteral("offset")).toString().isEmpty());
    QVERIFY(request.value(QStringLiteral("filter")).isNull());
    QVERIFY(request.value(QStringLiteral("chat_type_filter")).isNull());
    QCOMPARE(request.value(QStringLiteral("min_date")).toInt(), 0);
    QCOMPARE(request.value(QStringLiteral("max_date")).toInt(), 0);

    const QJsonObject clampedLow = searchMessagesOnServerRequest(QStringLiteral("anna"), -10);
    QCOMPARE(clampedLow.value(QStringLiteral("limit")).toInt(), 1);
}

void GlobalSearchEngineTest::buildsContactSearchRequest()
{
    const QJsonObject request = searchContactsRequest(QStringLiteral("  carol  "), 42);

    QCOMPARE(request.value(QStringLiteral("@type")).toString(),
             QStringLiteral("searchContacts"));
    QCOMPARE(request.value(QStringLiteral("query")).toString(), QStringLiteral("carol"));
    QCOMPARE(request.value(QStringLiteral("limit")).toInt(), 42);

    const QJsonObject clampedHigh = searchContactsRequest(QStringLiteral("carol"), 101);
    QCOMPARE(clampedHigh.value(QStringLiteral("limit")).toInt(), 100);
}

void GlobalSearchEngineTest::blankQueryBuildsNoRequest()
{
    QVERIFY(searchChatsOnServerRequest(QString(), 10).isEmpty());
    QVERIFY(searchChatsOnServerRequest(QStringLiteral("   \t\n"), 10).isEmpty());
    QVERIFY(searchMessagesOnServerRequest(QStringLiteral("  "), 10).isEmpty());
    QVERIFY(searchContactsRequest(QStringLiteral("\n"), 10).isEmpty());
}

void GlobalSearchEngineTest::mergeSortsByScoreThenDate()
{
    // Score wins first; inside one score the newer date wins. C and E share
    // both ranks, so the earlier input row stays first.
    const QVector<SearchHit> chats{
        makeHit(SearchSource::Chats, 1, 0, 10, 100, QStringLiteral("A")),
        makeHit(SearchSource::Chats, 2, 0, 20, 50, QStringLiteral("B")),
        makeHit(SearchSource::Chats, 3, 0, 10, 300, QStringLiteral("C")),
        makeHit(SearchSource::Chats, 4, 0, 20, 80, QStringLiteral("D")),
        makeHit(SearchSource::Chats, 5, 0, 10, 300, QStringLiteral("E")),
    };

    const QVector<SearchHit> merged = mergeSearchHits(chats, {}, {}, 10);

    QCOMPARE(titlesOf(merged),
             (QVector<QString>{QStringLiteral("D"), QStringLiteral("B"),
                               QStringLiteral("C"), QStringLiteral("E"),
                               QStringLiteral("A")}));
    QCOMPARE(scoresOf(merged), (QVector<int>{20, 20, 10, 10, 10}));
}

void GlobalSearchEngineTest::mergeDeduplicatesWithinEachSource()
{
    // One chat id is one chat row.
    const QVector<SearchHit> chats{
        makeHit(SearchSource::Chats, 5, 0, 4, 100, QStringLiteral("first")),
        makeHit(SearchSource::Chats, 5, 0, 2, 200, QStringLiteral("ignored")),
        makeHit(SearchSource::Chats, 6, 0, 3, 100, QStringLiteral("other")),
    };
    const QVector<SearchHit> mergedChats = mergeSearchHits(chats, {}, {}, 10);
    QCOMPARE(mergedChats.size(), qsizetype(2));
    QCOMPARE(titlesOf(mergedChats),
             (QVector<QString>{QStringLiteral("first"), QStringLiteral("other")}));

    // A message is identified by chat id plus message id: the same message id
    // in another chat is a different row.
    const QVector<SearchHit> messages{
        makeHit(SearchSource::Messages, 1, 10, 5, 100, QStringLiteral("one")),
        makeHit(SearchSource::Messages, 1, 10, 9, 100, QStringLiteral("better")),
        makeHit(SearchSource::Messages, 2, 10, 7, 100, QStringLiteral("other chat")),
        makeHit(SearchSource::Messages, 1, 11, 6, 100, QStringLiteral("other message")),
    };
    const QVector<SearchHit> mergedMessages = mergeSearchHits({}, messages, {}, 10);
    QCOMPARE(mergedMessages.size(), qsizetype(3));
    QCOMPARE(titlesOf(mergedMessages),
             (QVector<QString>{QStringLiteral("better"), QStringLiteral("other chat"),
                               QStringLiteral("other message")}));

    // People are unique by user id.
    const QVector<SearchHit> people{
        makeHit(SearchSource::People, 0, 7, 3, 100, QStringLiteral("low")),
        makeHit(SearchSource::People, 0, 7, 8, 100, QStringLiteral("high")),
        makeHit(SearchSource::People, 0, 8, 8, 100, QStringLiteral("second user")),
    };
    const QVector<SearchHit> mergedPeople = mergeSearchHits({}, {}, people, 10);
    QCOMPARE(mergedPeople.size(), qsizetype(2));
    QCOMPARE(titlesOf(mergedPeople),
             (QVector<QString>{QStringLiteral("high"), QStringLiteral("second user")}));
}

void GlobalSearchEngineTest::mergeKeepsTheHighestScoringCopy()
{
    // A duplicate with a higher score replaces the earlier row, including its
    // display fields.
    const QVector<SearchHit> messages{
        makeHit(SearchSource::Messages, 1, 10, 1, 100, QStringLiteral("low")),
        makeHit(SearchSource::Messages, 1, 10, 9, 100, QStringLiteral("high")),
    };
    const QVector<SearchHit> mergedMessages = mergeSearchHits({}, messages, {}, 10);
    QCOMPARE(mergedMessages.size(), qsizetype(1));
    QCOMPARE(mergedMessages.first().title, QStringLiteral("high"));
    QCOMPARE(mergedMessages.first().score, 9);

    // An equal score is not an improvement, so the first row keeps its fields.
    const QVector<SearchHit> chats{
        makeHit(SearchSource::Chats, 1, 0, 5, 100, QStringLiteral("first")),
        makeHit(SearchSource::Chats, 1, 0, 5, 300, QStringLiteral("second")),
    };
    const QVector<SearchHit> mergedChats = mergeSearchHits(chats, {}, {}, 10);
    QCOMPARE(mergedChats.size(), qsizetype(1));
    QCOMPARE(mergedChats.first().title, QStringLiteral("first"));
    QCOMPARE(mergedChats.first().date, 100);
}

void GlobalSearchEngineTest::mergeKeepsSourcesApart()
{
    // The same numeric ids in different sources are unrelated rows.
    const QVector<SearchHit> chats{
        makeHit(SearchSource::Chats, 1, 0, 5, 100, QStringLiteral("chat")),
    };
    const QVector<SearchHit> messages{
        makeHit(SearchSource::Messages, 1, 1, 5, 100, QStringLiteral("message")),
    };
    const QVector<SearchHit> people{
        makeHit(SearchSource::People, 0, 1, 5, 100, QStringLiteral("person")),
    };

    const QVector<SearchHit> merged = mergeSearchHits(chats, messages, people, 10);

    QCOMPARE(merged.size(), qsizetype(3));
    QCOMPARE(sourcesOf(merged), (QVector<int>{0, 1, 2}));
    QCOMPARE(titlesOf(merged),
             (QVector<QString>{QStringLiteral("chat"), QStringLiteral("message"),
                               QStringLiteral("person")}));
}

void GlobalSearchEngineTest::mergeCapsEverySource()
{
    const QVector<SearchHit> chats{
        makeHit(SearchSource::Chats, 1, 0, 1, 100),
        makeHit(SearchSource::Chats, 2, 0, 9, 100),
        makeHit(SearchSource::Chats, 3, 0, 5, 100),
    };
    const QVector<SearchHit> messages{
        makeHit(SearchSource::Messages, 1, 1, 8, 100),
        makeHit(SearchSource::Messages, 1, 2, 2, 100),
        makeHit(SearchSource::Messages, 1, 3, 6, 100),
    };
    const QVector<SearchHit> people{
        makeHit(SearchSource::People, 0, 1, 3, 100),
        makeHit(SearchSource::People, 0, 2, 7, 100),
        makeHit(SearchSource::People, 0, 3, 4, 100),
    };

    const QVector<SearchHit> merged = mergeSearchHits(chats, messages, people, 2);

    // The cap keeps the two best rows of every source, then interleaves them.
    QCOMPARE(merged.size(), qsizetype(6));
    QCOMPARE(scoresOf(merged), (QVector<int>{9, 8, 7, 5, 6, 4}));
    QCOMPARE(sourcesOf(merged), (QVector<int>{0, 1, 2, 0, 1, 2}));

    // A larger cap keeps everything.
    QCOMPARE(mergeSearchHits(chats, messages, people, 10).size(), qsizetype(9));

    // No rows fit when the cap is not positive.
    QVERIFY(mergeSearchHits(chats, messages, people, 0).isEmpty());
    QVERIFY(mergeSearchHits(chats, messages, people, -5).isEmpty());
    QVERIFY(mergeSearchHits({}, {}, {}, 10).isEmpty());
}

void GlobalSearchEngineTest::mergeInterleavesTheSources()
{
    const QVector<SearchHit> chats{
        makeHit(SearchSource::Chats, 1, 0, 5, 10, QStringLiteral("chat-one")),
        makeHit(SearchSource::Chats, 2, 0, 5, 10, QStringLiteral("chat-two")),
    };
    const QVector<SearchHit> messages{
        makeHit(SearchSource::Messages, 1, 1, 5, 10, QStringLiteral("msg-one")),
        makeHit(SearchSource::Messages, 1, 2, 5, 10, QStringLiteral("msg-two")),
        makeHit(SearchSource::Messages, 1, 3, 5, 10, QStringLiteral("msg-three")),
    };
    const QVector<SearchHit> people{
        makeHit(SearchSource::People, 0, 1, 5, 10, QStringLiteral("person-one")),
    };

    const QVector<SearchHit> merged = mergeSearchHits(chats, messages, people, 10);

    // Round-robin, and a source that runs out is skipped without breaking the
    // order of the remaining sources.
    QCOMPARE(titlesOf(merged),
             (QVector<QString>{QStringLiteral("chat-one"), QStringLiteral("msg-one"),
                               QStringLiteral("person-one"), QStringLiteral("chat-two"),
                               QStringLiteral("msg-two"), QStringLiteral("msg-three")}));
    QCOMPARE(sourcesOf(merged), (QVector<int>{0, 1, 2, 0, 1, 1}));
}

void GlobalSearchEngineTest::mergeKeepsStableOrderForEqualRanks()
{
    // Equal score and date keep the caller's order.
    const QVector<SearchHit> messages{
        makeHit(SearchSource::Messages, 1, 1, 5, 100, QStringLiteral("one")),
        makeHit(SearchSource::Messages, 1, 2, 5, 100, QStringLiteral("two")),
        makeHit(SearchSource::Messages, 1, 3, 5, 100, QStringLiteral("three")),
    };
    QCOMPARE(titlesOf(mergeSearchHits({}, messages, {}, 10)),
             (QVector<QString>{QStringLiteral("one"), QStringLiteral("two"),
                               QStringLiteral("three")}));

    // Equal score still sorts by the newer date first.
    const QVector<SearchHit> dated{
        makeHit(SearchSource::Messages, 1, 1, 5, 100, QStringLiteral("old")),
        makeHit(SearchSource::Messages, 1, 2, 5, 300, QStringLiteral("new")),
        makeHit(SearchSource::Messages, 1, 3, 5, 200, QStringLiteral("middle")),
    };
    QCOMPARE(titlesOf(mergeSearchHits({}, dated, {}, 10)),
             (QVector<QString>{QStringLiteral("new"), QStringLiteral("middle"),
                               QStringLiteral("old")}));
}

QTEST_GUILESS_MAIN(GlobalSearchEngineTest)
#include "tst_global_search_engine.moc"
