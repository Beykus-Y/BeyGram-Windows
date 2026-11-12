#include "ui/ContactsFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

using namespace beygram::ui;

namespace {

// Same fixture as tst_contact_sort_model: every id, timestamp and online flag
// is chosen so that sorting on the wrong key produces a visibly different id
// order.
void loadSample(ContactsFacade& facade)
{
    facade.setContacts(
        {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"), QStringLiteral("4")},
        {QStringLiteral("Grace"), QStringLiteral("Ada"), QStringLiteral("Linus"),
         QStringLiteral("Barbara")},
        {QStringLiteral("false"), QStringLiteral("true"), QStringLiteral("false"),
         QStringLiteral("true")},
        {QStringLiteral("300"), QStringLiteral("100"), QStringLiteral("200"),
         QStringLiteral("900")});
}

} // namespace

class ContactsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmptyWithNameSort();
    void setContactsSortsByName();
    void setContactsEmitsChangedOnce();
    void setContactsDropsUnusableIds();
    void setContactsToleratesShortLists();
    void setContactsParsesOnlineColumn();
    void setContactsParsesLastSeenColumn();
    void setContactsKeepsTheActiveQuery();
    void sortModeChangesTheOrder();
    void sortModeAcceptsLooseSpellings();
    void sortModeIgnoresUnknownTokens();
    void sortModeEmitsChangedOnlyOnRealChange();
    void onlineOnlyFiltersNamesAndIndex();
    void onlineOnlyEmitsChangedOnlyOnRealChange();
    void queryFiltersNamesAndIndex();
    void queryIsTrimmedAndCaseInsensitive();
    void queryEmitsSearchRequestedOnlyWhenNonEmpty();
    void queryNoOpEmitsNothing();
    void userIdAtMapsTheVisibleRows();
    void userIdAtIsEmptyOutOfRange();
    void activateEmitsContactActivated();
    void activateFollowsTheFilteredOrder();
    void activateIgnoresOutOfRangeIndices();
    void loadingEmitsChangedOnlyOnRealChange();
    void readsNeverEmitChanged();
    void querySortAndOnlineOnlyCombine();

private:
    static QStringList idOrder(const ContactsFacade& facade);
};

QStringList ContactsFacadeTest::idOrder(const ContactsFacade& facade)
{
    QStringList ids;
    const QStringList names = facade.names();
    ids.reserve(names.size());
    for (int i = 0; i < names.size(); ++i)
        ids.append(facade.userIdAt(i));
    return ids;
}

void ContactsFacadeTest::startsEmptyWithNameSort()
{
    const ContactsFacade facade;

    QVERIFY(facade.names().isEmpty());
    QVERIFY(facade.indexLetters().isEmpty());
    QCOMPARE(facade.sortMode(), QStringLiteral("name"));
    QVERIFY(!facade.onlineOnly());
    QCOMPARE(facade.query(), QString());
    QVERIFY(!facade.loading());

    QCOMPARE(facade.userIdAt(0), QString());
    QCOMPARE(facade.userIdAt(-1), QString());
}

void ContactsFacadeTest::setContactsSortsByName()
{
    ContactsFacade facade;
    loadSample(facade);

    QCOMPARE(facade.names(),
             (QStringList{QStringLiteral("Ada"), QStringLiteral("Barbara"),
                          QStringLiteral("Grace"), QStringLiteral("Linus")}));
    QCOMPARE(facade.indexLetters(),
             (QStringList{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("G"),
                          QStringLiteral("L")}));
    QCOMPARE(idOrder(facade),
             (QStringList{QStringLiteral("2"), QStringLiteral("4"), QStringLiteral("1"),
                          QStringLiteral("3")}));
}

void ContactsFacadeTest::setContactsEmitsChangedOnce()
{
    ContactsFacade facade;
    QSignalSpy spy(&facade, &ContactsFacade::changed);

    loadSample(facade);
    QCOMPARE(spy.count(), 1);

    // A second snapshot is fresh server data, so it notifies again.
    loadSample(facade);
    QCOMPARE(spy.count(), 2);
}

void ContactsFacadeTest::setContactsDropsUnusableIds()
{
    ContactsFacade facade;

    // 0, a negative id, a non-numeric id and an empty id can never name a
    // TDLib user, so their rows are dropped instead of being kept as
    // un-activatable entries.
    facade.setContacts(
        {QStringLiteral("1"), QStringLiteral("0"), QStringLiteral("-4"), QStringLiteral("abc"),
         QStringLiteral("7"), QString(), QStringLiteral("9")},
        {QStringLiteral("Ada"), QStringLiteral("Zero"), QStringLiteral("Neg"),
         QStringLiteral("Text"), QStringLiteral("Grace"), QStringLiteral("Empty"),
         QStringLiteral("Linus")},
        {}, {});

    QCOMPARE(facade.names(),
             (QStringList{QStringLiteral("Ada"), QStringLiteral("Grace"),
                          QStringLiteral("Linus")}));
    QCOMPARE(idOrder(facade),
             (QStringList{QStringLiteral("1"), QStringLiteral("7"), QStringLiteral("9")}));
}

void ContactsFacadeTest::setContactsToleratesShortLists()
{
    ContactsFacade facade;

    // Only the first contact has a name, an online flag and a timestamp; the
    // other two keep the documented defaults instead of failing the update.
    facade.setContacts({QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7")},
                       {QStringLiteral("Zoe")}, {QStringLiteral("true")},
                       {QStringLiteral("42")});

    QCOMPARE(facade.names().size(), 3);

    facade.setOnlineOnly(true);
    QCOMPARE(facade.names(), QStringList{QStringLiteral("Zoe")});
    QCOMPARE(facade.userIdAt(0), QStringLiteral("5"));

    facade.setOnlineOnly(false);
    facade.setSortMode(QStringLiteral("lastSeen"));

    // 42 beats the two unknown (0) timestamps, which keep their input order.
    QCOMPARE(idOrder(facade),
             (QStringList{QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7")}));
}

void ContactsFacadeTest::setContactsParsesOnlineColumn()
{
    ContactsFacade facade;

    facade.setContacts(
        {QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"), QStringLiteral("4"),
         QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7"), QStringLiteral("8")},
        {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"), QStringLiteral("D"),
         QStringLiteral("E"), QStringLiteral("F"), QStringLiteral("G"), QStringLiteral("H")},
        {QStringLiteral("true"), QStringLiteral("1"), QStringLiteral("YES"),
         QStringLiteral(" Online "), QStringLiteral("false"), QStringLiteral("0"), QString(),
         QStringLiteral("maybe")},
        {});

    facade.setOnlineOnly(true);

    QCOMPARE(idOrder(facade),
             (QStringList{QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                          QStringLiteral("4")}));
}

void ContactsFacadeTest::setContactsParsesLastSeenColumn()
{
    ContactsFacade facade;

    facade.setContacts({QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                        QStringLiteral("4"), QStringLiteral("5")},
                       {QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("C"),
                        QStringLiteral("D"), QStringLiteral("E")},
                       {},
                       {QStringLiteral(" 20 "), QStringLiteral("10"), QStringLiteral("abc"),
                        QStringLiteral("-5"), QStringLiteral("0")});

    facade.setSortMode(QStringLiteral("lastSeen"));

    // Unparsable, negative and zero all become "unknown" and keep input order.
    QCOMPARE(idOrder(facade),
             (QStringList{QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"),
                          QStringLiteral("4"), QStringLiteral("5")}));
}

void ContactsFacadeTest::setContactsKeepsTheActiveQuery()
{
    ContactsFacade facade;
    facade.setQuery(QStringLiteral("ada"));

    loadSample(facade);
    QCOMPARE(facade.names(), QStringList{QStringLiteral("Ada")});

    // A server answer that replaces the snapshot is filtered by the same query.
    facade.setContacts({QStringLiteral("8")}, {QStringLiteral("Ada Lovelace")}, {}, {});
    QCOMPARE(facade.names(), QStringList{QStringLiteral("Ada Lovelace")});
}

void ContactsFacadeTest::sortModeChangesTheOrder()
{
    ContactsFacade facade;
    loadSample(facade);

    facade.setSortMode(QStringLiteral("lastSeen"));
    QCOMPARE(facade.sortMode(), QStringLiteral("lastSeen"));
    QCOMPARE(idOrder(facade),
             (QStringList{QStringLiteral("4"), QStringLiteral("1"), QStringLiteral("3"),
                          QStringLiteral("2")}));

    facade.setSortMode(QStringLiteral("onlineFirst"));
    QCOMPARE(facade.sortMode(), QStringLiteral("onlineFirst"));
    QCOMPARE(facade.names(),
             (QStringList{QStringLiteral("Ada"), QStringLiteral("Barbara"),
                          QStringLiteral("Grace"), QStringLiteral("Linus")}));
    QCOMPARE(idOrder(facade),
             (QStringList{QStringLiteral("2"), QStringLiteral("4"), QStringLiteral("1"),
                          QStringLiteral("3")}));

    facade.setSortMode(QStringLiteral("name"));
    QCOMPARE(facade.sortMode(), QStringLiteral("name"));
    QCOMPARE(facade.names(),
             (QStringList{QStringLiteral("Ada"), QStringLiteral("Barbara"),
                          QStringLiteral("Grace"), QStringLiteral("Linus")}));
    QCOMPARE(idOrder(facade),
             (QStringList{QStringLiteral("2"), QStringLiteral("4"), QStringLiteral("1"),
                          QStringLiteral("3")}));
}

void ContactsFacadeTest::sortModeAcceptsLooseSpellings()
{
    ContactsFacade facade;
    loadSample(facade);

    facade.setSortMode(QStringLiteral("LASTSEEN"));
    QCOMPARE(facade.sortMode(), QStringLiteral("lastSeen"));

    facade.setSortMode(QStringLiteral(" online_first "));
    QCOMPARE(facade.sortMode(), QStringLiteral("onlineFirst"));

    facade.setSortMode(QStringLiteral("Name"));
    QCOMPARE(facade.sortMode(), QStringLiteral("name"));
}

void ContactsFacadeTest::sortModeIgnoresUnknownTokens()
{
    ContactsFacade facade;
    loadSample(facade);

    facade.setSortMode(QStringLiteral("lastSeen"));
    QSignalSpy spy(&facade, &ContactsFacade::changed);

    facade.setSortMode(QStringLiteral("recent"));
    facade.setSortMode(QString());

    // Neither token is an order, so the previous one survives untouched.
    QCOMPARE(facade.sortMode(), QStringLiteral("lastSeen"));
    QCOMPARE(spy.count(), 0);
}

void ContactsFacadeTest::sortModeEmitsChangedOnlyOnRealChange()
{
    ContactsFacade facade;
    loadSample(facade);

    QSignalSpy spy(&facade, &ContactsFacade::changed);

    facade.setSortMode(QStringLiteral("name"));
    QCOMPARE(spy.count(), 0);

    facade.setSortMode(QStringLiteral("lastSeen"));
    QCOMPARE(spy.count(), 1);

    facade.setSortMode(QStringLiteral("lastSeen"));
    QCOMPARE(spy.count(), 1);

    facade.setSortMode(QStringLiteral("onlineFirst"));
    QCOMPARE(spy.count(), 2);
}

void ContactsFacadeTest::onlineOnlyFiltersNamesAndIndex()
{
    ContactsFacade facade;
    loadSample(facade);

    facade.setOnlineOnly(true);
    QVERIFY(facade.onlineOnly());
    QCOMPARE(facade.names(),
             (QStringList{QStringLiteral("Ada"), QStringLiteral("Barbara")}));
    QCOMPARE(facade.indexLetters(),
             (QStringList{QStringLiteral("A"), QStringLiteral("B")}));

    facade.setOnlineOnly(false);
    QCOMPARE(facade.names().size(), 4);
    QCOMPARE(facade.indexLetters(),
             (QStringList{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("G"),
                          QStringLiteral("L")}));
}

void ContactsFacadeTest::onlineOnlyEmitsChangedOnlyOnRealChange()
{
    ContactsFacade facade;
    loadSample(facade);

    QSignalSpy spy(&facade, &ContactsFacade::changed);

    facade.setOnlineOnly(false);
    QCOMPARE(spy.count(), 0);

    facade.setOnlineOnly(true);
    QCOMPARE(spy.count(), 1);

    facade.setOnlineOnly(true);
    QCOMPARE(spy.count(), 1);

    facade.setOnlineOnly(false);
    QCOMPARE(spy.count(), 2);
}

void ContactsFacadeTest::queryFiltersNamesAndIndex()
{
    ContactsFacade facade;
    loadSample(facade);

    facade.setQuery(QStringLiteral("a"));

    // Grace, Ada and Barbara contain "a"; Linus does not.
    QCOMPARE(facade.names(),
             (QStringList{QStringLiteral("Ada"), QStringLiteral("Barbara"),
                          QStringLiteral("Grace")}));
    QCOMPARE(facade.indexLetters(),
             (QStringList{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("G")}));

    // The index and the list stay in step once the filter is combined.
    facade.setQuery(QStringLiteral("ba"));
    QCOMPARE(facade.names(), QStringList{QStringLiteral("Barbara")});
    QCOMPARE(facade.indexLetters(), QStringList{QStringLiteral("B")});

    facade.setQuery(QString());
    QCOMPARE(facade.names().size(), 4);
}

void ContactsFacadeTest::queryIsTrimmedAndCaseInsensitive()
{
    ContactsFacade facade;
    loadSample(facade);

    facade.setQuery(QStringLiteral("  ADA  "));
    QCOMPARE(facade.query(), QStringLiteral("ADA"));
    QCOMPARE(facade.names(), QStringList{QStringLiteral("Ada")});

    facade.setQuery(QStringLiteral("bArBaRa"));
    QCOMPARE(facade.names(), QStringList{QStringLiteral("Barbara")});

    facade.setQuery(QString());
    QVERIFY(facade.query().isEmpty());
    QCOMPARE(facade.names().size(), 4);
}

void ContactsFacadeTest::queryEmitsSearchRequestedOnlyWhenNonEmpty()
{
    ContactsFacade facade;
    loadSample(facade);

    QSignalSpy searchSpy(&facade, &ContactsFacade::searchRequested);
    QSignalSpy changeSpy(&facade, &ContactsFacade::changed);

    facade.setQuery(QStringLiteral(" ad "));
    QCOMPARE(searchSpy.count(), 1);
    QCOMPARE(searchSpy.at(0).at(0).toString(), QStringLiteral("ad"));
    QCOMPARE(changeSpy.count(), 1);

    // Same trimmed query: no filter change, so nothing is announced.
    facade.setQuery(QStringLiteral("ad"));
    QCOMPARE(searchSpy.count(), 1);
    QCOMPARE(changeSpy.count(), 1);

    // Clearing the filter re-renders the list but has nothing to search for.
    facade.setQuery(QStringLiteral("   "));
    QCOMPARE(searchSpy.count(), 1);
    QCOMPARE(changeSpy.count(), 2);
    QVERIFY(facade.query().isEmpty());
    QCOMPARE(facade.names().size(), 4);

    facade.setQuery(QStringLiteral("lin"));
    QCOMPARE(searchSpy.count(), 2);
    QCOMPARE(searchSpy.at(1).at(0).toString(), QStringLiteral("lin"));
}

void ContactsFacadeTest::queryNoOpEmitsNothing()
{
    ContactsFacade facade;
    loadSample(facade);
    facade.setQuery(QStringLiteral("ada"));

    QSignalSpy searchSpy(&facade, &ContactsFacade::searchRequested);
    QSignalSpy changeSpy(&facade, &ContactsFacade::changed);

    facade.setQuery(QStringLiteral("ada"));
    facade.setQuery(QStringLiteral("  ada  "));

    QCOMPARE(searchSpy.count(), 0);
    QCOMPARE(changeSpy.count(), 0);
}

void ContactsFacadeTest::userIdAtMapsTheVisibleRows()
{
    ContactsFacade facade;
    loadSample(facade);

    // Name order: Ada(2), Barbara(4), Grace(1), Linus(3).
    QCOMPARE(facade.userIdAt(0), QStringLiteral("2"));
    QCOMPARE(facade.userIdAt(1), QStringLiteral("4"));
    QCOMPARE(facade.userIdAt(2), QStringLiteral("1"));
    QCOMPARE(facade.userIdAt(3), QStringLiteral("3"));

    facade.setSortMode(QStringLiteral("lastSeen"));
    QCOMPARE(facade.userIdAt(0), QStringLiteral("4"));
    QCOMPARE(facade.userIdAt(3), QStringLiteral("2"));
}

void ContactsFacadeTest::userIdAtIsEmptyOutOfRange()
{
    ContactsFacade facade;

    QCOMPARE(facade.userIdAt(0), QString());

    loadSample(facade);
    QCOMPARE(facade.userIdAt(4), QString());
    QCOMPARE(facade.userIdAt(-1), QString());
    QCOMPARE(facade.userIdAt(1000), QString());
}

void ContactsFacadeTest::activateEmitsContactActivated()
{
    ContactsFacade facade;
    loadSample(facade);

    QSignalSpy spy(&facade, &ContactsFacade::contactActivated);

    facade.activate(0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("2"));

    facade.activate(2);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("1"));
}

void ContactsFacadeTest::activateFollowsTheFilteredOrder()
{
    ContactsFacade facade;
    loadSample(facade);
    facade.setQuery(QStringLiteral("ra"));

    QCOMPARE(facade.names(),
             (QStringList{QStringLiteral("Barbara"), QStringLiteral("Grace")}));

    QSignalSpy spy(&facade, &ContactsFacade::contactActivated);

    facade.activate(0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("4"));

    facade.activate(1);
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("1"));
}

void ContactsFacadeTest::activateIgnoresOutOfRangeIndices()
{
    ContactsFacade facade;
    loadSample(facade);

    QSignalSpy spy(&facade, &ContactsFacade::contactActivated);

    facade.activate(-1);
    facade.activate(4);
    facade.activate(99);

    QCOMPARE(spy.count(), 0);
}

void ContactsFacadeTest::loadingEmitsChangedOnlyOnRealChange()
{
    ContactsFacade facade;
    QSignalSpy spy(&facade, &ContactsFacade::changed);

    facade.setLoading(false);
    QCOMPARE(spy.count(), 0);
    QVERIFY(!facade.loading());

    facade.setLoading(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(facade.loading());

    facade.setLoading(true);
    QCOMPARE(spy.count(), 1);

    facade.setLoading(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!facade.loading());
}

void ContactsFacadeTest::readsNeverEmitChanged()
{
    ContactsFacade facade;
    loadSample(facade);

    QSignalSpy spy(&facade, &ContactsFacade::changed);

    facade.names();
    facade.indexLetters();
    facade.userIdAt(0);
    facade.sortMode();
    facade.onlineOnly();
    facade.query();
    facade.loading();

    QCOMPARE(spy.count(), 0);

    // The cached rows are rebuilt once per real change, not per read.
    facade.setQuery(QStringLiteral("a"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.names().size(), 3);
    QCOMPARE(spy.count(), 1);
}

void ContactsFacadeTest::querySortAndOnlineOnlyCombine()
{
    ContactsFacade facade;
    loadSample(facade);

    facade.setOnlineOnly(true);
    facade.setQuery(QStringLiteral("a"));
    facade.setSortMode(QStringLiteral("lastSeen"));

    // Online rows are Ada (100) and Barbara (900); both match "a".
    QCOMPARE(facade.names(),
             (QStringList{QStringLiteral("Barbara"), QStringLiteral("Ada")}));
    QCOMPARE(facade.indexLetters(),
             (QStringList{QStringLiteral("A"), QStringLiteral("B")}));

    // Dropping the online filter brings Grace back without re-sorting wrongly.
    facade.setOnlineOnly(false);
    QCOMPARE(facade.names(),
             (QStringList{QStringLiteral("Barbara"), QStringLiteral("Grace"),
                          QStringLiteral("Ada")}));
}

QTEST_GUILESS_MAIN(ContactsFacadeTest)
#include "tst_contacts_facade.moc"
