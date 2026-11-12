#include "telegram/ContactSortModel.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtTest>

#include <algorithm>

using namespace beygram::telegram;

namespace {

ContactRow makeContact(qint64 userId, const QString& name, bool online = false,
                       qint64 lastSeen = 0)
{
    ContactRow contact;
    contact.userId = userId;
    contact.name = name;
    contact.online = online;
    contact.lastSeen = lastSeen;
    return contact;
}

QVector<qint64> idsOf(const QVector<ContactRow>& contacts)
{
    QVector<qint64> ids;
    ids.reserve(contacts.size());
    for (const ContactRow& contact : contacts)
        ids.append(contact.userId);
    return ids;
}

QStringList namesOf(const QVector<ContactRow>& contacts)
{
    QStringList names;
    names.reserve(contacts.size());
    for (const ContactRow& contact : contacts)
        names.append(contact.name);
    return names;
}

// ContactRow deliberately carries no operator==, so whole-row equality is
// checked field by field instead of through QCOMPARE.
bool sameRows(const QVector<ContactRow>& left, const QVector<ContactRow>& right)
{
    if (left.size() != right.size())
        return false;
    for (qsizetype i = 0; i < left.size(); ++i) {
        const ContactRow& lhs = left.at(i);
        const ContactRow& rhs = right.at(i);
        if (lhs.userId != rhs.userId || lhs.name != rhs.name || lhs.online != rhs.online
            || lhs.lastSeen != rhs.lastSeen)
            return false;
    }
    return true;
}

// Every id, timestamp and online flag is chosen so that a sort on the wrong key
// produces a visibly different id order.
QVector<ContactRow> sampleContacts()
{
    return {
        makeContact(1, QStringLiteral("Grace"), false, 300),
        makeContact(2, QStringLiteral("Ada"), true, 100),
        makeContact(3, QStringLiteral("Linus"), false, 200),
        makeContact(4, QStringLiteral("Barbara"), true, 900),
    };
}

} // namespace

class ContactSortModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithNameSortAndNoFilter();
    void setSortEmitsOnlyOnChange();
    void setOnlineOnlyEmitsOnlyOnChange();
    void sortedHandlesEmptyAndSingleContact();
    void nameSortsAlphabetically();
    void nameSortMatchesLocaleAwareCompare();
    void nameSortIsStableForEqualNames();
    void nameSortIgnoresPresence();
    void lastSeenSortsNewestFirst();
    void lastSeenTiesKeepInputOrder();
    void lastSeenSortsUnknownTimestampsLast();
    void onlineFirstPutsOnlineContactsAhead();
    void onlineFirstSortsEachGroupByName();
    void onlineFirstIsStableWhenBothKeysTie();
    void onlineOnlyFiltersBeforeSorting();
    void onlineOnlyAppliesToEverySortMode();
    void sortedLeavesTheInputUntouched();
    void sortedIsRepeatableAndUsesCurrentSettings();
    void sortedDoesNotEmitChanged();
    void indexLettersReturnsSortedUniqueInitials();
    void indexLettersUppercasesLowercaseNames();
    void indexLettersSkipsEmptyAndNonLetters();
    void indexLettersIgnoresSurroundingWhitespace();
    void indexLettersKeepsNonLatinLetters();
    void indexLettersRespectsOnlineOnly();
    void indexLettersHandlesEmptyInput();
    void indexLettersDoesNotEmitChanged();
};

void ContactSortModelTest::startsWithNameSortAndNoFilter()
{
    const ContactSortModel model;

    QVERIFY(model.sort() == ContactSort::Name);
    QVERIFY(!model.onlineOnly());
}

void ContactSortModelTest::setSortEmitsOnlyOnChange()
{
    ContactSortModel model;
    QSignalSpy spy(&model, &ContactSortModel::changed);

    model.setSort(ContactSort::Name);
    QCOMPARE(spy.count(), 0);

    model.setSort(ContactSort::LastSeen);
    QCOMPARE(spy.count(), 1);
    QVERIFY(model.sort() == ContactSort::LastSeen);

    model.setSort(ContactSort::LastSeen);
    QCOMPARE(spy.count(), 1);

    model.setSort(ContactSort::OnlineFirst);
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.sort() == ContactSort::OnlineFirst);

    // Back to the initial value still counts as a real transition.
    model.setSort(ContactSort::Name);
    QCOMPARE(spy.count(), 3);
    QVERIFY(model.sort() == ContactSort::Name);
}

void ContactSortModelTest::setOnlineOnlyEmitsOnlyOnChange()
{
    ContactSortModel model;
    QSignalSpy spy(&model, &ContactSortModel::changed);

    model.setOnlineOnly(false);
    QCOMPARE(spy.count(), 0);

    model.setOnlineOnly(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(model.onlineOnly());

    model.setOnlineOnly(true);
    QCOMPARE(spy.count(), 1);

    model.setOnlineOnly(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!model.onlineOnly());
}

void ContactSortModelTest::sortedHandlesEmptyAndSingleContact()
{
    ContactSortModel model;

    QVERIFY(model.sorted({}).isEmpty());

    const QVector<ContactRow> one{makeContact(7, QStringLiteral("Solo"), true, 42)};
    QVERIFY(sameRows(model.sorted(one), one));

    model.setSort(ContactSort::LastSeen);
    QVERIFY(sameRows(model.sorted(one), one));

    model.setSort(ContactSort::OnlineFirst);
    QVERIFY(sameRows(model.sorted(one), one));

    model.setOnlineOnly(true);
    QVERIFY(sameRows(model.sorted(one), one));
}

void ContactSortModelTest::nameSortsAlphabetically()
{
    ContactSortModel model;

    QCOMPARE(namesOf(model.sorted(sampleContacts())),
             (QStringList{QStringLiteral("Ada"), QStringLiteral("Barbara"),
                          QStringLiteral("Grace"), QStringLiteral("Linus")}));
}

void ContactSortModelTest::nameSortMatchesLocaleAwareCompare()
{
    ContactSortModel model;

    // Mixed case and accents: the expected order comes from the same
    // locale-aware comparison the model documents, not from code points.
    const QVector<ContactRow> contacts{
        makeContact(1, QStringLiteral("zoe")),
        makeContact(2, QStringLiteral("Émile")),
        makeContact(3, QStringLiteral("Zara")),
        makeContact(4, QStringLiteral("ada")),
        makeContact(5, QStringLiteral("Bob")),
    };

    QVector<ContactRow> expected = contacts;
    std::stable_sort(expected.begin(), expected.end(),
                     [](const ContactRow& left, const ContactRow& right) {
                         return QString::localeAwareCompare(left.name, right.name) < 0;
                     });

    QCOMPARE(idsOf(model.sorted(contacts)), idsOf(expected));
}

void ContactSortModelTest::nameSortIsStableForEqualNames()
{
    ContactSortModel model;

    const QVector<ContactRow> contacts{
        makeContact(10, QStringLiteral("Ada"), false, 100),
        makeContact(11, QStringLiteral("Ada"), true, 900),
        makeContact(12, QStringLiteral("Ada"), false, 500),
    };

    QCOMPARE(idsOf(model.sorted(contacts)), (QVector<qint64>{10, 11, 12}));
}

void ContactSortModelTest::nameSortIgnoresPresence()
{
    ContactSortModel model;

    // Barbara and Ada are online, Grace and Linus are not: online state must not
    // move a row under the Name mode.
    const QVector<ContactRow> sorted = model.sorted(sampleContacts());

    QCOMPARE(idsOf(sorted), (QVector<qint64>{2, 4, 1, 3}));
    QVERIFY(sorted.at(0).online);
    QVERIFY(!sorted.at(2).online);
}

void ContactSortModelTest::lastSeenSortsNewestFirst()
{
    ContactSortModel model;
    model.setSort(ContactSort::LastSeen);

    // 900 Barbara, 300 Grace, 200 Linus, 100 Ada.
    QCOMPARE(idsOf(model.sorted(sampleContacts())), (QVector<qint64>{4, 1, 3, 2}));
}

void ContactSortModelTest::lastSeenTiesKeepInputOrder()
{
    ContactSortModel model;
    model.setSort(ContactSort::LastSeen);

    const QVector<ContactRow> contacts{
        makeContact(1, QStringLiteral("Grace"), false, 200),
        makeContact(2, QStringLiteral("Ada"), true, 900),
        makeContact(3, QStringLiteral("Linus"), false, 200),
        makeContact(4, QStringLiteral("Barbara"), true, 200),
    };

    QCOMPARE(idsOf(model.sorted(contacts)), (QVector<qint64>{2, 1, 3, 4}));
}

void ContactSortModelTest::lastSeenSortsUnknownTimestampsLast()
{
    ContactSortModel model;
    model.setSort(ContactSort::LastSeen);

    const QVector<ContactRow> contacts{
        makeContact(1, QStringLiteral("Grace"), false, 0),
        makeContact(2, QStringLiteral("Ada"), false, 5),
        makeContact(3, QStringLiteral("Linus"), false, 0),
    };

    QCOMPARE(idsOf(model.sorted(contacts)), (QVector<qint64>{2, 1, 3}));
}

void ContactSortModelTest::onlineFirstPutsOnlineContactsAhead()
{
    ContactSortModel model;
    model.setSort(ContactSort::OnlineFirst);

    const QVector<ContactRow> sorted = model.sorted(sampleContacts());

    QCOMPARE(idsOf(sorted), (QVector<qint64>{2, 4, 1, 3}));
    QVERIFY(sorted.at(0).online);
    QVERIFY(sorted.at(1).online);
    QVERIFY(!sorted.at(2).online);
    QVERIFY(!sorted.at(3).online);
}

void ContactSortModelTest::onlineFirstSortsEachGroupByName()
{
    ContactSortModel model;
    model.setSort(ContactSort::OnlineFirst);

    // The newest timestamps belong to the offline contacts, so a model that
    // still sorted by lastSeen would put Zoe first instead of last.
    const QVector<ContactRow> contacts{
        makeContact(1, QStringLiteral("Zoe"), false, 900),
        makeContact(2, QStringLiteral("Bob"), true, 10),
        makeContact(3, QStringLiteral("Ada"), true, 20),
        makeContact(4, QStringLiteral("Amy"), false, 800),
    };

    QCOMPARE(idsOf(model.sorted(contacts)), (QVector<qint64>{3, 2, 4, 1}));
    QCOMPARE(namesOf(model.sorted(contacts)),
             (QStringList{QStringLiteral("Ada"), QStringLiteral("Bob"),
                          QStringLiteral("Amy"), QStringLiteral("Zoe")}));
}

void ContactSortModelTest::onlineFirstIsStableWhenBothKeysTie()
{
    ContactSortModel model;
    model.setSort(ContactSort::OnlineFirst);

    const QVector<ContactRow> contacts{
        makeContact(1, QStringLiteral("Ada"), true, 100),
        makeContact(2, QStringLiteral("Ada"), false, 100),
        makeContact(3, QStringLiteral("Ada"), true, 900),
        makeContact(4, QStringLiteral("Ada"), false, 900),
    };

    QCOMPARE(idsOf(model.sorted(contacts)), (QVector<qint64>{1, 3, 2, 4}));
}

void ContactSortModelTest::onlineOnlyFiltersBeforeSorting()
{
    ContactSortModel model;
    model.setOnlineOnly(true);

    QCOMPARE(idsOf(model.sorted(sampleContacts())), (QVector<qint64>{2, 4}));

    const QVector<ContactRow> allOffline{
        makeContact(1, QStringLiteral("Grace")),
        makeContact(2, QStringLiteral("Ada")),
    };
    QVERIFY(model.sorted(allOffline).isEmpty());
}

void ContactSortModelTest::onlineOnlyAppliesToEverySortMode()
{
    ContactSortModel model;
    model.setOnlineOnly(true);

    model.setSort(ContactSort::Name);
    QCOMPARE(idsOf(model.sorted(sampleContacts())), (QVector<qint64>{2, 4}));

    // Ada (100) and Barbara (900): newest first among the online rows only.
    model.setSort(ContactSort::LastSeen);
    QCOMPARE(idsOf(model.sorted(sampleContacts())), (QVector<qint64>{4, 2}));

    model.setSort(ContactSort::OnlineFirst);
    QCOMPARE(idsOf(model.sorted(sampleContacts())), (QVector<qint64>{2, 4}));

    model.setOnlineOnly(false);
    model.setSort(ContactSort::Name);
    QCOMPARE(idsOf(model.sorted(sampleContacts())), (QVector<qint64>{2, 4, 1, 3}));
}

void ContactSortModelTest::sortedLeavesTheInputUntouched()
{
    ContactSortModel model;
    model.setOnlineOnly(true);

    const QVector<ContactRow> contacts = sampleContacts();
    const QVector<ContactRow> before = contacts;

    model.setSort(ContactSort::LastSeen);
    model.sorted(contacts);
    QVERIFY(sameRows(contacts, before));

    model.setSort(ContactSort::OnlineFirst);
    model.sorted(contacts);
    QVERIFY(sameRows(contacts, before));

    model.setOnlineOnly(false);
    model.setSort(ContactSort::Name);
    model.sorted(contacts);
    QVERIFY(sameRows(contacts, before));
}

void ContactSortModelTest::sortedIsRepeatableAndUsesCurrentSettings()
{
    ContactSortModel model;

    const QVector<ContactRow> contacts = sampleContacts();
    const QVector<ContactRow> first = model.sorted(contacts);
    QVERIFY(sameRows(model.sorted(contacts), first));

    model.setSort(ContactSort::LastSeen);
    const QVector<ContactRow> byLastSeen = model.sorted(contacts);
    QCOMPARE(idsOf(byLastSeen), (QVector<qint64>{4, 1, 3, 2}));
    QVERIFY(sameRows(model.sorted(contacts), byLastSeen));

    // The previous result is not cached: switching back restores the old order.
    model.setSort(ContactSort::Name);
    QVERIFY(sameRows(model.sorted(contacts), first));
}

void ContactSortModelTest::sortedDoesNotEmitChanged()
{
    ContactSortModel model;
    QSignalSpy spy(&model, &ContactSortModel::changed);

    model.sorted(sampleContacts());
    model.indexLetters(sampleContacts());

    QCOMPARE(spy.count(), 0);
}

void ContactSortModelTest::indexLettersReturnsSortedUniqueInitials()
{
    ContactSortModel model;

    const QVector<ContactRow> contacts{
        makeContact(1, QStringLiteral("Grace")),
        makeContact(2, QStringLiteral("Ada")),
        makeContact(3, QStringLiteral("Barbara")),
        makeContact(4, QStringLiteral("Grace")),
        makeContact(5, QStringLiteral("Linus")),
    };

    QCOMPARE(model.indexLetters(contacts),
             (QStringList{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("G"),
                          QStringLiteral("L")}));
}

void ContactSortModelTest::indexLettersUppercasesLowercaseNames()
{
    ContactSortModel model;

    const QVector<ContactRow> contacts{
        makeContact(1, QStringLiteral("zoe")),
        makeContact(2, QStringLiteral("ada")),
        makeContact(3, QStringLiteral("Ada")),
        makeContact(4, QStringLiteral("bob")),
    };

    QCOMPARE(model.indexLetters(contacts),
             (QStringList{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("Z")}));
}

void ContactSortModelTest::indexLettersSkipsEmptyAndNonLetters()
{
    ContactSortModel model;

    const QVector<ContactRow> contacts{
        makeContact(1, QString()),
        makeContact(2, QStringLiteral("   ")),
        makeContact(3, QStringLiteral("42")),
        makeContact(4, QStringLiteral("#tag")),
        makeContact(5, QStringLiteral("-dash")),
        makeContact(6, QStringLiteral("Ada")),
    };

    QCOMPARE(model.indexLetters(contacts), QStringList{QStringLiteral("A")});
}

void ContactSortModelTest::indexLettersIgnoresSurroundingWhitespace()
{
    ContactSortModel model;

    const QVector<ContactRow> contacts{
        makeContact(1, QStringLiteral("  Ada")),
        makeContact(2, QStringLiteral("Bob\n")),
        makeContact(3, QStringLiteral("\tLinus")),
    };

    QCOMPARE(model.indexLetters(contacts),
             (QStringList{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("L")}));
}

void ContactSortModelTest::indexLettersKeepsNonLatinLetters()
{
    ContactSortModel model;

    const QVector<ContactRow> contacts{
        makeContact(1, QStringLiteral("Яна")),
        makeContact(2, QStringLiteral("émile")),
        makeContact(3, QStringLiteral("Ada")),
    };

    QCOMPARE(model.indexLetters(contacts),
             (QStringList{QStringLiteral("A"), QStringLiteral("É"), QStringLiteral("Я")}));
}

void ContactSortModelTest::indexLettersRespectsOnlineOnly()
{
    ContactSortModel model;

    const QVector<ContactRow> contacts{
        makeContact(1, QStringLiteral("Grace"), false),
        makeContact(2, QStringLiteral("Ada"), true),
        makeContact(3, QStringLiteral("Linus"), false),
        makeContact(4, QStringLiteral("Barbara"), true),
    };

    QCOMPARE(model.indexLetters(contacts),
             (QStringList{QStringLiteral("A"), QStringLiteral("B"), QStringLiteral("G"),
                          QStringLiteral("L")}));

    model.setOnlineOnly(true);
    QCOMPARE(model.indexLetters(contacts),
             (QStringList{QStringLiteral("A"), QStringLiteral("B")}));

    // The index matches the rows sorted() would return under the same filter.
    QCOMPARE(model.indexLetters(model.sorted(contacts)),
             (QStringList{QStringLiteral("A"), QStringLiteral("B")}));
}

void ContactSortModelTest::indexLettersHandlesEmptyInput()
{
    ContactSortModel model;

    QVERIFY(model.indexLetters({}).isEmpty());
    QVERIFY(model.indexLetters({makeContact(1, QString())}).isEmpty());
}

void ContactSortModelTest::indexLettersDoesNotEmitChanged()
{
    ContactSortModel model;
    QSignalSpy spy(&model, &ContactSortModel::changed);

    model.indexLetters(sampleContacts());
    model.setSort(ContactSort::LastSeen);
    model.indexLetters(sampleContacts());

    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(ContactSortModelTest)
#include "tst_contact_sort_model.moc"
