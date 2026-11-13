#include "ui/ForumTopicsFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtTest>

using beygram::ui::ForumTopicsFacade;

namespace {

/// The list the tests load: a pinned General topic, an unread Design topic and
/// a pinned Off topic. Ids are deliberately not in numeric order.
QStringList ids()
{
    return {QStringLiteral("1"), QStringLiteral("42"), QStringLiteral("7")};
}

QStringList titles()
{
    return {QStringLiteral("General"), QStringLiteral("Design"),
            QStringLiteral("Off topic")};
}

QStringList unread()
{
    return {QStringLiteral("0"), QStringLiteral("5"), QStringLiteral("12")};
}

QStringList pinned()
{
    return {QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("1")};
}

void loadDefault(ForumTopicsFacade& facade)
{
    facade.setTopics(ids(), titles(), unread(), pinned());
}

} // namespace

class ForumTopicsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void loadsTopics();
    void loadsTopicsEmitsChangedOnce();
    void normalisesIds();
    void dropsUnaddressableRows();
    void defaultsMissingAuxiliaryValues();
    void truncatesMismatchedIdsAndTitles();
    void parsesUnreadCounts();
    void parsesPinnedFlags();
    void setTopicsIsSilentForSameContent();
    void setTopicsEmitsWhenUnreadMoves();
    void setTopicsEmitsWhenPinnedMoves();
    void setTopicsReplacesPreviousRows();
    void setTopicsWithNoRowsClears();
    void openTopicEmitsStoredId();
    void openTopicTrimsInput();
    void openTopicIgnoresUnknownIds();
    void openTopicDoesNotChangeState();
    void togglePinnedFlipsAndRequests();
    void togglePinnedUsesStoredId();
    void togglePinnedTouchesOnlyTheTarget();
    void togglePinnedIgnoresUnknownIds();
    void clearDropsEverything();
    void clearIsSilentWhenEmpty();
    void reloadAfterClearWorks();
};

void ForumTopicsFacadeTest::startsEmpty()
{
    ForumTopicsFacade facade;

    QVERIFY(facade.ids().isEmpty());
    QVERIFY(facade.titles().isEmpty());
    QCOMPARE(facade.count(), 0);
    QVERIFY(facade.unreadCounts().isEmpty());
    QVERIFY(facade.pinnedFlags().isEmpty());
}

void ForumTopicsFacadeTest::loadsTopics()
{
    ForumTopicsFacade facade;
    loadDefault(facade);

    QCOMPARE(facade.ids(), ids());
    QCOMPARE(facade.titles(), titles());
    QCOMPARE(facade.count(), 3);

    QCOMPARE(int(facade.unreadCounts().size()), 3);
    QCOMPARE(facade.unreadCounts().at(0).toInt(), 0);
    QCOMPARE(facade.unreadCounts().at(1).toInt(), 5);
    QCOMPARE(facade.unreadCounts().at(2).toInt(), 12);

    QCOMPARE(int(facade.pinnedFlags().size()), 3);
    QVERIFY(facade.pinnedFlags().at(0).toBool());
    QVERIFY(!facade.pinnedFlags().at(1).toBool());
    QVERIFY(facade.pinnedFlags().at(2).toBool());
}

void ForumTopicsFacadeTest::loadsTopicsEmitsChangedOnce()
{
    ForumTopicsFacade facade;
    QSignalSpy spy(&facade, &ForumTopicsFacade::changed);

    loadDefault(facade);

    QCOMPARE(spy.count(), 1);
}

void ForumTopicsFacadeTest::normalisesIds()
{
    ForumTopicsFacade facade;
    facade.setTopics({QStringLiteral(" 007 "), QStringLiteral("+42")},
                     {QStringLiteral("General"), QStringLiteral("Design")},
                     {QStringLiteral("0"), QStringLiteral("0")},
                     {QStringLiteral("false"), QStringLiteral("false")});

    QCOMPARE(facade.ids(),
             (QStringList{QStringLiteral("7"), QStringLiteral("42")}));

    // The panel only ever sees canonical ids, so activation must accept them.
    QSignalSpy spy(&facade, &ForumTopicsFacade::topicActivated);
    facade.openTopic(QStringLiteral("7"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("7"));
}

void ForumTopicsFacadeTest::dropsUnaddressableRows()
{
    ForumTopicsFacade facade;
    facade.setTopics(
        {QStringLiteral("0"), QStringLiteral("-2"), QStringLiteral("abc"),
         QStringLiteral("1"), QStringLiteral("5"), QStringLiteral("6")},
        {QStringLiteral("Zero"), QStringLiteral("Negative"), QStringLiteral("Broken"),
         QStringLiteral("General"), QStringLiteral("Real"), QStringLiteral("   ")},
        {QStringLiteral("9"), QStringLiteral("9"), QStringLiteral("9"),
         QStringLiteral("3"), QStringLiteral("7"), QStringLiteral("9")},
        {QStringLiteral("true"), QStringLiteral("true"), QStringLiteral("true"),
         QStringLiteral("false"), QStringLiteral("true"), QStringLiteral("true")});

    // Id 0/negative/unparsable rows and the blank title are gone; the two
    // survivors keep their order and their counts stay aligned.
    QCOMPARE(facade.count(), 2);
    QCOMPARE(facade.ids(),
             (QStringList{QStringLiteral("1"), QStringLiteral("5")}));
    QCOMPARE(facade.titles(),
             (QStringList{QStringLiteral("General"), QStringLiteral("Real")}));
    QCOMPARE(facade.unreadCounts().at(0).toInt(), 3);
    QCOMPARE(facade.unreadCounts().at(1).toInt(), 7);
    QVERIFY(!facade.pinnedFlags().at(0).toBool());
    QVERIFY(facade.pinnedFlags().at(1).toBool());
}

void ForumTopicsFacadeTest::defaultsMissingAuxiliaryValues()
{
    // The counts and pins are shorter than the topics: a missing entry means
    // 0/false, not a dropped topic.
    ForumTopicsFacade facade;
    facade.setTopics({QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")},
                     {QStringLiteral("One"), QStringLiteral("Two"),
                      QStringLiteral("Three")},
                     {QStringLiteral("4")},
                     {QStringLiteral("true")});

    QCOMPARE(facade.count(), 3);
    QCOMPARE(facade.ids(),
             (QStringList{QStringLiteral("1"), QStringLiteral("2"),
                          QStringLiteral("3")}));
    QCOMPARE(int(facade.unreadCounts().size()), 3);
    QCOMPARE(facade.unreadCounts().at(0).toInt(), 4);
    QCOMPARE(facade.unreadCounts().at(1).toInt(), 0);
    QCOMPARE(facade.unreadCounts().at(2).toInt(), 0);
    QVERIFY(facade.pinnedFlags().at(0).toBool());
    QVERIFY(!facade.pinnedFlags().at(1).toBool());
    QVERIFY(!facade.pinnedFlags().at(2).toBool());
}

void ForumTopicsFacadeTest::truncatesMismatchedIdsAndTitles()
{
    // An id without a title has nothing to show, so ids and titles are cut to
    // the shorter of the two.
    ForumTopicsFacade facade;
    facade.setTopics({QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3")},
                     {QStringLiteral("One"), QStringLiteral("Two")},
                     {QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0")},
                     {QStringLiteral("false"), QStringLiteral("false"),
                      QStringLiteral("false")});

    QCOMPARE(facade.count(), 2);
    QCOMPARE(facade.ids(),
             (QStringList{QStringLiteral("1"), QStringLiteral("2")}));
    QCOMPARE(facade.titles(),
             (QStringList{QStringLiteral("One"), QStringLiteral("Two")}));
}

void ForumTopicsFacadeTest::parsesUnreadCounts()
{
    ForumTopicsFacade facade;
    facade.setTopics(
        {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
         QStringLiteral("4"), QStringLiteral("5")},
        {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"),
         QStringLiteral("D"), QStringLiteral("E")},
        {QStringLiteral(" 7 "), QStringLiteral("-3"), QStringLiteral("abc"),
         QString(), QStringLiteral("1000")},
        {QStringLiteral("false"), QStringLiteral("false"), QStringLiteral("false"),
         QStringLiteral("false"), QStringLiteral("false")});

    QCOMPARE(int(facade.unreadCounts().size()), 5);
    QCOMPARE(facade.unreadCounts().at(0).toInt(), 7);
    QCOMPARE(facade.unreadCounts().at(1).toInt(), 0);
    QCOMPARE(facade.unreadCounts().at(2).toInt(), 0);
    QCOMPARE(facade.unreadCounts().at(3).toInt(), 0);
    QCOMPARE(facade.unreadCounts().at(4).toInt(), 1000);
}

void ForumTopicsFacadeTest::parsesPinnedFlags()
{
    ForumTopicsFacade facade;
    facade.setTopics(
        {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
         QStringLiteral("4"), QStringLiteral("5"), QStringLiteral("6"),
         QStringLiteral("7")},
        {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"),
         QStringLiteral("D"), QStringLiteral("E"), QStringLiteral("F"),
         QStringLiteral("G")},
        {QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0"),
         QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0"),
         QStringLiteral("0")},
        {QStringLiteral("true"), QStringLiteral("TRUE"), QStringLiteral("1"),
         QStringLiteral("false"), QStringLiteral("0"), QStringLiteral("yes"),
         QString()});

    QCOMPARE(int(facade.pinnedFlags().size()), 7);
    QVERIFY(facade.pinnedFlags().at(0).toBool());
    QVERIFY(facade.pinnedFlags().at(1).toBool());
    QVERIFY(facade.pinnedFlags().at(2).toBool());
    QVERIFY(!facade.pinnedFlags().at(3).toBool());
    QVERIFY(!facade.pinnedFlags().at(4).toBool());
    // "yes" is not a QVariant bool spelling, so it is not pinned.
    QVERIFY(!facade.pinnedFlags().at(5).toBool());
    QVERIFY(!facade.pinnedFlags().at(6).toBool());
}

void ForumTopicsFacadeTest::setTopicsIsSilentForSameContent()
{
    ForumTopicsFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &ForumTopicsFacade::changed);

    loadDefault(facade);
    loadDefault(facade);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.titles(), titles());
}

void ForumTopicsFacadeTest::setTopicsEmitsWhenUnreadMoves()
{
    ForumTopicsFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &ForumTopicsFacade::changed);

    facade.setTopics(ids(), titles(),
                     {QStringLiteral("0"), QStringLiteral("6"),
                      QStringLiteral("12")},
                     pinned());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.unreadCounts().at(1).toInt(), 6);
}

void ForumTopicsFacadeTest::setTopicsEmitsWhenPinnedMoves()
{
    ForumTopicsFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &ForumTopicsFacade::changed);

    facade.setTopics(ids(), titles(), unread(),
                     {QStringLiteral("false"), QStringLiteral("false"),
                      QStringLiteral("1")});

    QCOMPARE(spy.count(), 1);
    QVERIFY(!facade.pinnedFlags().at(0).toBool());
}

void ForumTopicsFacadeTest::setTopicsReplacesPreviousRows()
{
    ForumTopicsFacade facade;
    loadDefault(facade);

    facade.setTopics({QStringLiteral("9")}, {QStringLiteral("Archive")},
                     {QStringLiteral("2")}, {QStringLiteral("true")});

    QCOMPARE(facade.count(), 1);
    QCOMPARE(facade.ids(), QStringList{QStringLiteral("9")});
    QCOMPARE(facade.titles(), QStringList{QStringLiteral("Archive")});

    QSignalSpy spy(&facade, &ForumTopicsFacade::topicActivated);
    facade.openTopic(QStringLiteral("42"));
    QCOMPARE(spy.count(), 0);

    facade.openTopic(QStringLiteral("9"));
    QCOMPARE(spy.count(), 1);
}

void ForumTopicsFacadeTest::setTopicsWithNoRowsClears()
{
    ForumTopicsFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &ForumTopicsFacade::changed);

    facade.setTopics({}, {}, {}, {});

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.count(), 0);
    QVERIFY(facade.ids().isEmpty());
    QVERIFY(facade.titles().isEmpty());
    QVERIFY(facade.unreadCounts().isEmpty());
    QVERIFY(facade.pinnedFlags().isEmpty());
}

void ForumTopicsFacadeTest::openTopicEmitsStoredId()
{
    ForumTopicsFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &ForumTopicsFacade::topicActivated);

    facade.openTopic(QStringLiteral("42"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("42"));
}

void ForumTopicsFacadeTest::openTopicTrimsInput()
{
    ForumTopicsFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &ForumTopicsFacade::topicActivated);

    facade.openTopic(QStringLiteral(" 42 "));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("42"));
}

void ForumTopicsFacadeTest::openTopicIgnoresUnknownIds()
{
    ForumTopicsFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &ForumTopicsFacade::topicActivated);

    facade.openTopic(QStringLiteral("999"));
    facade.openTopic(QString());
    facade.openTopic(QStringLiteral("nope"));
    facade.openTopic(QStringLiteral("0"));

    QCOMPARE(spy.count(), 0);
}

void ForumTopicsFacadeTest::openTopicDoesNotChangeState()
{
    ForumTopicsFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &ForumTopicsFacade::changed);

    facade.openTopic(QStringLiteral("1"));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.count(), 3);
    QCOMPARE(facade.titles(), titles());
}

void ForumTopicsFacadeTest::togglePinnedFlipsAndRequests()
{
    ForumTopicsFacade facade;
    loadDefault(facade);
    QSignalSpy changedSpy(&facade, &ForumTopicsFacade::changed);
    QSignalSpy pinSpy(&facade, &ForumTopicsFacade::pinRequested);

    // General starts pinned.
    facade.togglePinned(QStringLiteral("1"));

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(pinSpy.count(), 1);
    QCOMPARE(pinSpy.at(0).at(0).toString(), QStringLiteral("1"));
    QCOMPARE(pinSpy.at(0).at(1).toBool(), false);
    QVERIFY(!facade.pinnedFlags().at(0).toBool());

    facade.togglePinned(QStringLiteral("1"));

    QCOMPARE(changedSpy.count(), 2);
    QCOMPARE(pinSpy.count(), 2);
    QCOMPARE(pinSpy.at(1).at(0).toString(), QStringLiteral("1"));
    QCOMPARE(pinSpy.at(1).at(1).toBool(), true);
    QVERIFY(facade.pinnedFlags().at(0).toBool());
}

void ForumTopicsFacadeTest::togglePinnedUsesStoredId()
{
    ForumTopicsFacade facade;
    facade.setTopics({QStringLiteral(" 007 ")}, {QStringLiteral("General")},
                     {QStringLiteral("0")}, {QStringLiteral("false")});
    QSignalSpy pinSpy(&facade, &ForumTopicsFacade::pinRequested);

    facade.togglePinned(QStringLiteral("7"));

    QCOMPARE(pinSpy.count(), 1);
    QCOMPARE(pinSpy.at(0).at(0).toString(), QStringLiteral("7"));
    QCOMPARE(pinSpy.at(0).at(1).toBool(), true);
}

void ForumTopicsFacadeTest::togglePinnedTouchesOnlyTheTarget()
{
    ForumTopicsFacade facade;
    loadDefault(facade);

    facade.togglePinned(QStringLiteral("42"));

    // Default flags are true/false/true; only Design moved.
    QVERIFY(facade.pinnedFlags().at(0).toBool());
    QVERIFY(facade.pinnedFlags().at(1).toBool());
    QVERIFY(facade.pinnedFlags().at(2).toBool());

    // The rest of the row and the other rows are untouched.
    QCOMPARE(facade.ids(), ids());
    QCOMPARE(facade.titles(), titles());
    QCOMPARE(facade.unreadCounts().at(1).toInt(), 5);
}

void ForumTopicsFacadeTest::togglePinnedIgnoresUnknownIds()
{
    ForumTopicsFacade facade;
    loadDefault(facade);
    QSignalSpy changedSpy(&facade, &ForumTopicsFacade::changed);
    QSignalSpy pinSpy(&facade, &ForumTopicsFacade::pinRequested);

    facade.togglePinned(QStringLiteral("999"));
    facade.togglePinned(QString());
    facade.togglePinned(QStringLiteral("nope"));

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(pinSpy.count(), 0);
    QVERIFY(facade.pinnedFlags().at(0).toBool());
}

void ForumTopicsFacadeTest::clearDropsEverything()
{
    ForumTopicsFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &ForumTopicsFacade::changed);

    facade.clear();

    QCOMPARE(spy.count(), 1);
    QVERIFY(facade.ids().isEmpty());
    QVERIFY(facade.titles().isEmpty());
    QCOMPARE(facade.count(), 0);
    QVERIFY(facade.unreadCounts().isEmpty());
    QVERIFY(facade.pinnedFlags().isEmpty());
}

void ForumTopicsFacadeTest::clearIsSilentWhenEmpty()
{
    ForumTopicsFacade facade;
    QSignalSpy spy(&facade, &ForumTopicsFacade::changed);

    facade.clear();

    QCOMPARE(spy.count(), 0);
}

void ForumTopicsFacadeTest::reloadAfterClearWorks()
{
    ForumTopicsFacade facade;
    loadDefault(facade);
    facade.clear();
    QSignalSpy spy(&facade, &ForumTopicsFacade::changed);

    loadDefault(facade);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.count(), 3);
    QCOMPARE(facade.ids(), ids());
}

QTEST_GUILESS_MAIN(ForumTopicsFacadeTest)
#include "tst_forum_topics_facade.moc"
