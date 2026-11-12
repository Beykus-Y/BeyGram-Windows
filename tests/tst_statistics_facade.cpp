#include "ui/StatisticsFacade.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

#include <initializer_list>

using namespace beygram::ui;

namespace {

QStringList labels(std::initializer_list<const char*> items)
{
    QStringList list;
    for (const char* item : items)
        list.append(QString::fromUtf8(item));
    return list;
}

} // namespace

class StatisticsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void chatStatsFillOverview();
    void overviewValuesGroupThousands();
    void overviewValuesKeepUnits();
    void overviewValuesRoundToTwoDecimals();
    void overviewValuesDropMalformedNumbers();
    void overviewValuesIgnoreExtraValues();
    void overviewDelegatesEmptyLabelsAndDuplicates();
    void chartValuesAlignWithLabels();
    void chartPadsMissingValues();
    void chartTruncatesExtraValues();
    void chartAloneMakesStatsAvailable();
    void setChatStatsReplacesPrevious();
    void setChatStatsIsSilentWhenUnchanged();
    void chatStatsChangedSignalFires();

    void requestChatStatsEmitsAndKeepsData();

    void openMessageStatsActivatesAndRequests();
    void openMessageStatsNormalizesIds();
    void openMessageStatsIgnoresInvalidIds();
    void openMessageStatsResetsCounters();
    void openMessageStatsKeepsChatStats();
    void messageCountsStoreValues();
    void messageCountsClampNegatives();
    void messageCountsIgnoredWhileInactive();
    void messageCountsSilentWhenUnchanged();

    void boostsStoreValues();
    void boostsClampNegatives();
    void boostProgressIsRatioOfCountToTarget();
    void boostProgressIsZeroWithoutTarget();
    void boostProgressIsZeroWithoutBoosts();
    void boostProgressClampsAtOne();
    void boostsSilentWhenUnchanged();

    void clearDropsEverything();
    void clearIsSilentWhenEmpty();
    void changedCoversAllThreeModels();
};

void StatisticsFacadeTest::startsEmpty()
{
    const StatisticsFacade facade;

    QVERIFY(!facade.chatStatsAvailable());
    QVERIFY(facade.overviewLabels().isEmpty());
    QVERIFY(facade.overviewValues().isEmpty());
    QVERIFY(facade.chartLabels().isEmpty());
    QVERIFY(facade.chartValues().isEmpty());

    QVERIFY(!facade.messageStatsActive());
    QCOMPARE(facade.views(), 0);
    QCOMPARE(facade.forwards(), 0);
    QCOMPARE(facade.reactions(), 0);

    QCOMPARE(facade.boostLevel(), 0);
    QCOMPARE(facade.boostCount(), 0);
    QCOMPARE(facade.boostProgress(), 0.0);
}

void StatisticsFacadeTest::chatStatsFillOverview()
{
    StatisticsFacade facade;
    facade.setChatStats(labels({"Messages", "Shares"}), labels({"1234", "56"}), {}, {});

    QVERIFY(facade.chatStatsAvailable());
    QCOMPARE(facade.overviewLabels(), labels({"Messages", "Shares"}));
    QCOMPARE(facade.overviewValues(), labels({"1 234", "56"}));
    QVERIFY(facade.chartLabels().isEmpty());
    QVERIFY(facade.chartValues().isEmpty());
}

void StatisticsFacadeTest::overviewValuesGroupThousands()
{
    StatisticsFacade facade;
    facade.setChatStats(labels({"A", "B", "C", "D"}),
                        labels({"999", "1000", "1234567", "1234567890"}), {}, {});

    QCOMPARE(facade.overviewValues(),
             labels({"999", "1 000", "1 234 567", "1 234 567 890"}));
}

void StatisticsFacadeTest::overviewValuesKeepUnits()
{
    StatisticsFacade facade;
    facade.setChatStats(labels({"A", "B", "C", "D", "E"}),
                        labels({"12.5%", "1 234 MB", "3", "-1234", "12abc"}), {}, {});

    QCOMPARE(facade.overviewValues(),
             labels({"12.5%", "1 234 MB", "3", "-1 234", "12 abc"}));
}

void StatisticsFacadeTest::overviewValuesRoundToTwoDecimals()
{
    StatisticsFacade facade;
    facade.setChatStats(labels({"A", "B", "C", "D"}),
                        labels({"1.234", "12.50", "0.5", "0.004"}), {}, {});

    QCOMPARE(facade.overviewValues(), labels({"1.23", "12.5", "0.5", "0"}));
}

void StatisticsFacadeTest::overviewValuesDropMalformedNumbers()
{
    StatisticsFacade facade;
    facade.setChatStats(labels({"A", "B", "C"}), labels({"abc", "", "   "}), {}, {});

    QCOMPARE(facade.overviewValues(), labels({"0", "0", "0"}));
}

void StatisticsFacadeTest::overviewValuesIgnoreExtraValues()
{
    StatisticsFacade facade;
    facade.setChatStats(labels({"A"}), labels({"7", "8", "9"}), {}, {});

    QCOMPARE(facade.overviewLabels(), labels({"A"}));
    QCOMPARE(facade.overviewValues(), labels({"7"}));
}

void StatisticsFacadeTest::overviewDelegatesEmptyLabelsAndDuplicates()
{
    StatisticsFacade facade;
    facade.setChatStats(labels({"A", "", "B", "A"}), labels({"1", "2", "3", "4"}), {}, {});

    QCOMPARE(facade.overviewLabels(), labels({"A", "B"}));
    QCOMPARE(facade.overviewValues(), labels({"1", "3"}));
}

void StatisticsFacadeTest::chartValuesAlignWithLabels()
{
    StatisticsFacade facade;
    facade.setChatStats({}, {}, labels({"Mon", "Tue", "Wed"}), labels({"10", "1 250", "0"}));

    QVERIFY(facade.overviewLabels().isEmpty());
    QCOMPARE(facade.chartLabels(), labels({"Mon", "Tue", "Wed"}));
    QCOMPARE(facade.chartValues(), labels({"10", "1 250", "0"}));
}

void StatisticsFacadeTest::chartPadsMissingValues()
{
    StatisticsFacade facade;
    facade.setChatStats({}, {}, labels({"Mon", "Tue", "Wed"}), labels({"10"}));

    QCOMPARE(facade.chartLabels(), labels({"Mon", "Tue", "Wed"}));
    QCOMPARE(facade.chartValues(), labels({"10", "0", "0"}));
}

void StatisticsFacadeTest::chartTruncatesExtraValues()
{
    StatisticsFacade facade;
    facade.setChatStats({}, {}, labels({"Mon"}), labels({"5", "9"}));

    QCOMPARE(facade.chartLabels(), labels({"Mon"}));
    QCOMPARE(facade.chartValues(), labels({"5"}));
}

void StatisticsFacadeTest::chartAloneMakesStatsAvailable()
{
    StatisticsFacade facade;
    facade.setChatStats({}, {}, labels({"Mon"}), labels({"5"}));

    QVERIFY(facade.chatStatsAvailable());
}

void StatisticsFacadeTest::setChatStatsReplacesPrevious()
{
    StatisticsFacade facade;
    facade.setChatStats(labels({"A", "B"}), labels({"1", "2"}), labels({"Mon", "Tue"}),
                        labels({"3", "4"}));

    facade.setChatStats(labels({"C"}), labels({"5"}), {}, {});

    QCOMPARE(facade.overviewLabels(), labels({"C"}));
    QCOMPARE(facade.overviewValues(), labels({"5"}));
    QVERIFY(facade.chartLabels().isEmpty());
    QVERIFY(facade.chartValues().isEmpty());
}

void StatisticsFacadeTest::setChatStatsIsSilentWhenUnchanged()
{
    StatisticsFacade facade;
    facade.setChatStats(labels({"A"}), labels({"1234"}), labels({"Mon"}), labels({"5"}));

    QSignalSpy spy(&facade, &StatisticsFacade::changed);
    facade.setChatStats(labels({"A"}), labels({"1 234"}), labels({"Mon"}), labels({"5"}));

    QCOMPARE(spy.count(), 0);
}

void StatisticsFacadeTest::chatStatsChangedSignalFires()
{
    StatisticsFacade facade;
    QSignalSpy spy(&facade, &StatisticsFacade::changed);

    facade.setChatStats(labels({"A"}), labels({"1"}), labels({"Mon"}), labels({"2"}));
    QVERIFY(spy.count() >= 1);

    const int afterFirst = spy.count();
    facade.setChatStats(labels({"A"}), labels({"1"}), labels({"Mon"}), labels({"3"}));
    QVERIFY(spy.count() > afterFirst);
}

void StatisticsFacadeTest::requestChatStatsEmitsAndKeepsData()
{
    StatisticsFacade facade;
    facade.setChatStats(labels({"A"}), labels({"1"}), {}, {});

    QSignalSpy spy(&facade, &StatisticsFacade::chatStatsRequested);
    facade.requestChatStats();

    QCOMPARE(spy.count(), 1);
    QVERIFY(facade.chatStatsAvailable());
    QCOMPARE(facade.overviewValues(), labels({"1"}));
}

void StatisticsFacadeTest::openMessageStatsActivatesAndRequests()
{
    StatisticsFacade facade;
    QSignalSpy spy(&facade, &StatisticsFacade::messageStatsRequested);

    facade.openMessageStats(QStringLiteral("42"), QStringLiteral("7"));

    QVERIFY(facade.messageStatsActive());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("42"));
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("7"));
}

void StatisticsFacadeTest::openMessageStatsNormalizesIds()
{
    StatisticsFacade facade;
    QSignalSpy spy(&facade, &StatisticsFacade::messageStatsRequested);

    facade.openMessageStats(QStringLiteral(" 42 "), QStringLiteral("0007"));

    QVERIFY(facade.messageStatsActive());
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("42"));
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("7"));
}

void StatisticsFacadeTest::openMessageStatsIgnoresInvalidIds()
{
    StatisticsFacade facade;
    QSignalSpy spy(&facade, &StatisticsFacade::messageStatsRequested);

    facade.openMessageStats(QStringLiteral("42"), QStringLiteral("0"));
    facade.openMessageStats(QStringLiteral("0"), QStringLiteral("7"));
    facade.openMessageStats(QStringLiteral("-1"), QStringLiteral("7"));
    facade.openMessageStats(QString(), QStringLiteral("7"));
    facade.openMessageStats(QStringLiteral("abc"), QStringLiteral("7"));
    facade.openMessageStats(QStringLiteral("1.5"), QStringLiteral("7"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!facade.messageStatsActive());
}

void StatisticsFacadeTest::openMessageStatsResetsCounters()
{
    StatisticsFacade facade;
    facade.openMessageStats(QStringLiteral("42"), QStringLiteral("7"));
    facade.setMessageCounts(10, 20, 30);

    facade.openMessageStats(QStringLiteral("43"), QStringLiteral("8"));

    QVERIFY(facade.messageStatsActive());
    QCOMPARE(facade.views(), 0);
    QCOMPARE(facade.forwards(), 0);
    QCOMPARE(facade.reactions(), 0);
}

void StatisticsFacadeTest::openMessageStatsKeepsChatStats()
{
    StatisticsFacade facade;
    facade.setChatStats(labels({"Messages"}), labels({"1234"}), labels({"Mon"}), labels({"5"}));

    facade.openMessageStats(QStringLiteral("42"), QStringLiteral("7"));

    QVERIFY(facade.chatStatsAvailable());
    QCOMPARE(facade.overviewValues(), labels({"1 234"}));
    QCOMPARE(facade.chartValues(), labels({"5"}));
}

void StatisticsFacadeTest::messageCountsStoreValues()
{
    StatisticsFacade facade;
    facade.openMessageStats(QStringLiteral("42"), QStringLiteral("7"));

    facade.setMessageCounts(1500, 12, 3);

    QCOMPARE(facade.views(), 1500);
    QCOMPARE(facade.forwards(), 12);
    QCOMPARE(facade.reactions(), 3);
}

void StatisticsFacadeTest::messageCountsClampNegatives()
{
    StatisticsFacade facade;
    facade.openMessageStats(QStringLiteral("42"), QStringLiteral("7"));

    facade.setMessageCounts(-5, -1, -2);

    QCOMPARE(facade.views(), 0);
    QCOMPARE(facade.forwards(), 0);
    QCOMPARE(facade.reactions(), 0);
}

void StatisticsFacadeTest::messageCountsIgnoredWhileInactive()
{
    StatisticsFacade facade;
    QSignalSpy spy(&facade, &StatisticsFacade::changed);

    facade.setMessageCounts(5, 6, 7);

    QCOMPARE(facade.views(), 0);
    QCOMPARE(facade.forwards(), 0);
    QCOMPARE(facade.reactions(), 0);
    QCOMPARE(spy.count(), 0);
}

void StatisticsFacadeTest::messageCountsSilentWhenUnchanged()
{
    StatisticsFacade facade;
    facade.openMessageStats(QStringLiteral("42"), QStringLiteral("7"));
    facade.setMessageCounts(5, 6, 7);

    QSignalSpy spy(&facade, &StatisticsFacade::changed);
    facade.setMessageCounts(5, 6, 7);

    QCOMPARE(spy.count(), 0);
}

void StatisticsFacadeTest::boostsStoreValues()
{
    StatisticsFacade facade;
    facade.setBoosts(2, 5, 10);

    QCOMPARE(facade.boostLevel(), 2);
    QCOMPARE(facade.boostCount(), 5);
}

void StatisticsFacadeTest::boostsClampNegatives()
{
    StatisticsFacade facade;
    facade.setBoosts(-1, -5, -10);

    QCOMPARE(facade.boostLevel(), 0);
    QCOMPARE(facade.boostCount(), 0);
    QCOMPARE(facade.boostProgress(), 0.0);
}

void StatisticsFacadeTest::boostProgressIsRatioOfCountToTarget()
{
    StatisticsFacade facade;
    facade.setBoosts(2, 5, 10);

    QCOMPARE(facade.boostProgress(), 0.5);
}

void StatisticsFacadeTest::boostProgressIsZeroWithoutTarget()
{
    StatisticsFacade facade;
    facade.setBoosts(1, 5, 0);

    QCOMPARE(facade.boostProgress(), 0.0);
}

void StatisticsFacadeTest::boostProgressIsZeroWithoutBoosts()
{
    StatisticsFacade facade;
    facade.setBoosts(0, 0, 10);

    QCOMPARE(facade.boostProgress(), 0.0);
}

void StatisticsFacadeTest::boostProgressClampsAtOne()
{
    StatisticsFacade facade;
    facade.setBoosts(3, 25, 20);

    QCOMPARE(facade.boostProgress(), 1.0);
}

void StatisticsFacadeTest::boostsSilentWhenUnchanged()
{
    StatisticsFacade facade;
    facade.setBoosts(2, 5, 10);

    QSignalSpy spy(&facade, &StatisticsFacade::changed);
    facade.setBoosts(2, 5, 10);

    QCOMPARE(spy.count(), 0);
}

void StatisticsFacadeTest::clearDropsEverything()
{
    StatisticsFacade facade;
    facade.setChatStats(labels({"Messages"}), labels({"1234"}), labels({"Mon"}), labels({"5"}));
    facade.openMessageStats(QStringLiteral("42"), QStringLiteral("7"));
    facade.setMessageCounts(10, 20, 30);
    facade.setBoosts(2, 5, 10);

    QSignalSpy spy(&facade, &StatisticsFacade::changed);
    facade.clear();

    QVERIFY(!facade.chatStatsAvailable());
    QVERIFY(facade.overviewLabels().isEmpty());
    QVERIFY(facade.overviewValues().isEmpty());
    QVERIFY(facade.chartLabels().isEmpty());
    QVERIFY(facade.chartValues().isEmpty());

    QVERIFY(!facade.messageStatsActive());
    QCOMPARE(facade.views(), 0);
    QCOMPARE(facade.forwards(), 0);
    QCOMPARE(facade.reactions(), 0);

    QCOMPARE(facade.boostLevel(), 0);
    QCOMPARE(facade.boostCount(), 0);
    QCOMPARE(facade.boostProgress(), 0.0);

    QVERIFY(spy.count() >= 1);
}

void StatisticsFacadeTest::clearIsSilentWhenEmpty()
{
    StatisticsFacade facade;

    QSignalSpy spy(&facade, &StatisticsFacade::changed);
    facade.clear();

    QCOMPARE(spy.count(), 0);
}

void StatisticsFacadeTest::changedCoversAllThreeModels()
{
    StatisticsFacade facade;
    QSignalSpy spy(&facade, &StatisticsFacade::changed);

    facade.setChatStats(labels({"A"}), labels({"1"}), {}, {});
    const int afterChat = spy.count();
    QVERIFY(afterChat >= 1);

    facade.openMessageStats(QStringLiteral("42"), QStringLiteral("7"));
    QVERIFY(spy.count() > afterChat);
    const int afterMessage = spy.count();

    facade.setMessageCounts(1, 2, 3);
    QVERIFY(spy.count() > afterMessage);
    const int afterCounts = spy.count();

    facade.setBoosts(1, 2, 3);
    QVERIFY(spy.count() > afterCounts);
}

QTEST_GUILESS_MAIN(StatisticsFacadeTest)
#include "tst_statistics_facade.moc"
