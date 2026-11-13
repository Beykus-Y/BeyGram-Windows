#include "ui/SavedMessagesFacade.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QtTest>

#include <initializer_list>

using namespace beygram::ui;

namespace {

/// QML hands the facade strings, so the tests build the same shape.
QStringList strings(std::initializer_list<QString> values)
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const QString& value : values)
        result.append(value);
    return result;
}

} // namespace

class SavedMessagesFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void clearOnEmptyListIsSilent();

    void addPublishesRow();
    void addStampsTheCurrentTime();
    void addRejectsIdsThatCannotAddressAMessage();
    void addKeepsFirstCopyOfAStoredPair();
    void addStoresPreviewVerbatim();
    void addAtUsesTheSuppliedTime();

    void idsDistinguishTheSameMessageInDifferentChats();
    void ordersNewestFirst();
    void sameSavedAtBreaksTiesByMessageId();
    void fullTieKeepsArrivalOrder();

    void removeDropsOnlyThatPair();
    void removeIgnoresInvalidAndAbsentPairs();
    void removeAtUsesVisibleIndex();
    void removeAtIgnoresOutOfRangeIndexes();

    void openRequestsTheStoredPair();
    void openIgnoresOutOfRangeIndexes();

    void clearDropsEverything();
    void changedFiresOnlyOnRealChanges();
};

void SavedMessagesFacadeTest::startsEmpty()
{
    const SavedMessagesFacade facade;

    QCOMPARE(facade.count(), 0);
    QVERIFY(facade.previews().isEmpty());
    QVERIFY(facade.ids().isEmpty());
    QVERIFY(facade.dates().isEmpty());
}

void SavedMessagesFacadeTest::clearOnEmptyListIsSilent()
{
    SavedMessagesFacade facade;
    QSignalSpy spy(&facade, &SavedMessagesFacade::changed);
    QVERIFY(spy.isValid());

    facade.clear();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.count(), 0);
}

void SavedMessagesFacadeTest::addPublishesRow()
{
    SavedMessagesFacade facade;
    QSignalSpy spy(&facade, &SavedMessagesFacade::changed);
    QVERIFY(spy.isValid());

    facade.add(QStringLiteral("42"), QStringLiteral("7"), QStringLiteral("Hello there"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.count(), 1);
    QCOMPARE(facade.previews(), strings({QStringLiteral("Hello there")}));
    QCOMPARE(facade.ids(), strings({QStringLiteral("42:7")}));
    QCOMPARE(facade.dates().size(), 1);
    QVERIFY(facade.dates().at(0).toLongLong() > 0);
}

void SavedMessagesFacadeTest::addStampsTheCurrentTime()
{
    SavedMessagesFacade facade;

    const qint64 before = QDateTime::currentSecsSinceEpoch();
    facade.add(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("now"));
    const qint64 after = QDateTime::currentSecsSinceEpoch();

    QCOMPARE(facade.dates().size(), 1);
    const qint64 savedAt = facade.dates().at(0).toLongLong();
    QVERIFY(savedAt >= before);
    QVERIFY(savedAt <= after);
}

void SavedMessagesFacadeTest::addRejectsIdsThatCannotAddressAMessage()
{
    SavedMessagesFacade facade;
    QSignalSpy spy(&facade, &SavedMessagesFacade::changed);

    facade.add(QStringLiteral("abc"), QStringLiteral("1"), QStringLiteral("a"));
    facade.add(QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("b"));
    facade.add(QStringLiteral("-5"), QStringLiteral("1"), QStringLiteral("c"));
    facade.add(QStringLiteral("1"), QStringLiteral("abc"), QStringLiteral("d"));
    facade.add(QStringLiteral("1"), QStringLiteral("0"), QStringLiteral("e"));
    facade.add(QStringLiteral("1"), QStringLiteral("-2"), QStringLiteral("f"));
    facade.add(QString(), QString(), QStringLiteral("g"));
    facade.add(QStringLiteral("   "), QStringLiteral("  "), QStringLiteral("h"));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.count(), 0);
}

void SavedMessagesFacadeTest::addKeepsFirstCopyOfAStoredPair()
{
    SavedMessagesFacade facade;
    facade.addAt(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("first"), 100);
    QSignalSpy spy(&facade, &SavedMessagesFacade::changed);

    facade.addAt(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("second"), 200);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.count(), 1);
    QCOMPARE(facade.previews(), strings({QStringLiteral("first")}));
    QCOMPARE(facade.dates(), strings({QStringLiteral("100")}));
}

void SavedMessagesFacadeTest::addStoresPreviewVerbatim()
{
    SavedMessagesFacade facade;

    // The caller decides how much of the message the row quotes, so the facade
    // must not trim, collapse or otherwise rewrite it.
    const QString preview = QStringLiteral("  first line\nsecond\tline  ");
    facade.addAt(QStringLiteral("3"), QStringLiteral("4"), preview, 50);

    QCOMPARE(facade.previews(), strings({preview}));
}

void SavedMessagesFacadeTest::addAtUsesTheSuppliedTime()
{
    SavedMessagesFacade facade;

    facade.addAt(QStringLiteral("8"), QStringLiteral("9"), QStringLiteral("restored"), 1700000000);

    QCOMPARE(facade.dates(), strings({QStringLiteral("1700000000")}));
}

void SavedMessagesFacadeTest::idsDistinguishTheSameMessageInDifferentChats()
{
    SavedMessagesFacade facade;

    // A message id is only unique inside its chat, so the key keeps both parts.
    facade.addAt(QStringLiteral("1"), QStringLiteral("5"), QStringLiteral("from chat 1"), 100);
    facade.addAt(QStringLiteral("2"), QStringLiteral("5"), QStringLiteral("from chat 2"), 100);

    QCOMPARE(facade.count(), 2);
    QCOMPARE(facade.ids(), strings({QStringLiteral("1:5"), QStringLiteral("2:5")}));
    QCOMPARE(facade.previews(), strings({QStringLiteral("from chat 1"),
                                        QStringLiteral("from chat 2")}));
}

void SavedMessagesFacadeTest::ordersNewestFirst()
{
    SavedMessagesFacade facade;

    facade.addAt(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("old"), 100);
    facade.addAt(QStringLiteral("1"), QStringLiteral("30"), QStringLiteral("newest"), 300);
    facade.addAt(QStringLiteral("1"), QStringLiteral("20"), QStringLiteral("middle"), 200);

    QCOMPARE(facade.previews(), strings({QStringLiteral("newest"),
                                        QStringLiteral("middle"),
                                        QStringLiteral("old")}));
    QCOMPARE(facade.ids(), strings({QStringLiteral("1:30"),
                                    QStringLiteral("1:20"),
                                    QStringLiteral("1:10")}));
    QCOMPARE(facade.dates(), strings({QStringLiteral("300"),
                                      QStringLiteral("200"),
                                      QStringLiteral("100")}));
}

void SavedMessagesFacadeTest::sameSavedAtBreaksTiesByMessageId()
{
    SavedMessagesFacade facade;

    // Entries saved in the same second are ordered like the model defines:
    // the larger message id is newer.
    facade.addAt(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("ten"), 500);
    facade.addAt(QStringLiteral("1"), QStringLiteral("20"), QStringLiteral("twenty"), 500);

    QCOMPARE(facade.previews(), strings({QStringLiteral("twenty"),
                                        QStringLiteral("ten")}));
}

void SavedMessagesFacadeTest::fullTieKeepsArrivalOrder()
{
    SavedMessagesFacade facade;

    // Same savedAt and same messageId, but different chats: nothing can order
    // these two, so they keep the order they arrived in.
    facade.addAt(QStringLiteral("9"), QStringLiteral("7"), QStringLiteral("first"), 600);
    facade.addAt(QStringLiteral("3"), QStringLiteral("7"), QStringLiteral("second"), 600);

    QCOMPARE(facade.previews(), strings({QStringLiteral("first"),
                                        QStringLiteral("second")}));
    QCOMPARE(facade.ids(), strings({QStringLiteral("9:7"),
                                    QStringLiteral("3:7")}));
}

void SavedMessagesFacadeTest::removeDropsOnlyThatPair()
{
    SavedMessagesFacade facade;
    facade.addAt(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("a"), 100);
    facade.addAt(QStringLiteral("1"), QStringLiteral("20"), QStringLiteral("b"), 200);
    facade.addAt(QStringLiteral("2"), QStringLiteral("10"), QStringLiteral("c"), 300);
    QSignalSpy spy(&facade, &SavedMessagesFacade::changed);

    facade.remove(QStringLiteral("1"), QStringLiteral("10"));

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.count(), 2);
    QCOMPARE(facade.ids(), strings({QStringLiteral("2:10"),
                                    QStringLiteral("1:20")}));
    QCOMPARE(facade.previews(), strings({QStringLiteral("c"),
                                        QStringLiteral("b")}));
}

void SavedMessagesFacadeTest::removeIgnoresInvalidAndAbsentPairs()
{
    SavedMessagesFacade facade;
    facade.addAt(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("a"), 100);
    QSignalSpy spy(&facade, &SavedMessagesFacade::changed);

    facade.remove(QStringLiteral("1"), QStringLiteral("99"));
    facade.remove(QStringLiteral("abc"), QStringLiteral("10"));
    facade.remove(QStringLiteral("1"), QStringLiteral("0"));
    facade.remove(QString(), QString());

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.count(), 1);
}

void SavedMessagesFacadeTest::removeAtUsesVisibleIndex()
{
    SavedMessagesFacade facade;
    facade.addAt(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("older"), 100);
    facade.addAt(QStringLiteral("1"), QStringLiteral("20"), QStringLiteral("newer"), 200);
    QSignalSpy spy(&facade, &SavedMessagesFacade::changed);

    // Index 0 is the row the page shows first, i.e. the newest one.
    facade.removeAt(0);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.count(), 1);
    QCOMPARE(facade.ids(), strings({QStringLiteral("1:10")}));
    QCOMPARE(facade.previews(), strings({QStringLiteral("older")}));
}

void SavedMessagesFacadeTest::removeAtIgnoresOutOfRangeIndexes()
{
    SavedMessagesFacade facade;
    facade.addAt(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("a"), 100);
    QSignalSpy spy(&facade, &SavedMessagesFacade::changed);

    facade.removeAt(-1);
    facade.removeAt(1);
    facade.removeAt(99);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.count(), 1);
}

void SavedMessagesFacadeTest::openRequestsTheStoredPair()
{
    SavedMessagesFacade facade;
    facade.addAt(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("older"), 100);
    facade.addAt(QStringLiteral("2"), QStringLiteral("20"), QStringLiteral("newer"), 200);
    QSignalSpy spy(&facade, &SavedMessagesFacade::openRequested);
    QVERIFY(spy.isValid());

    facade.open(0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("2"));
    QCOMPARE(spy.at(0).at(1).toString(), QStringLiteral("20"));

    facade.open(1);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("1"));
    QCOMPARE(spy.at(1).at(1).toString(), QStringLiteral("10"));
}

void SavedMessagesFacadeTest::openIgnoresOutOfRangeIndexes()
{
    SavedMessagesFacade facade;
    QSignalSpy spy(&facade, &SavedMessagesFacade::openRequested);

    // Opening a row is not a change of the list, so an empty facade has
    // nothing to announce either.
    facade.open(0);
    facade.open(-1);

    facade.addAt(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("a"), 100);
    facade.open(1);
    facade.open(-1);

    QCOMPARE(spy.count(), 0);
}

void SavedMessagesFacadeTest::clearDropsEverything()
{
    SavedMessagesFacade facade;
    facade.addAt(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("a"), 100);
    facade.addAt(QStringLiteral("2"), QStringLiteral("20"), QStringLiteral("b"), 200);
    QSignalSpy spy(&facade, &SavedMessagesFacade::changed);

    facade.clear();

    QCOMPARE(spy.count(), 1);
    QCOMPARE(facade.count(), 0);
    QVERIFY(facade.previews().isEmpty());
    QVERIFY(facade.ids().isEmpty());
    QVERIFY(facade.dates().isEmpty());

    facade.clear();
    QCOMPARE(spy.count(), 1);
}

void SavedMessagesFacadeTest::changedFiresOnlyOnRealChanges()
{
    SavedMessagesFacade facade;
    QSignalSpy spy(&facade, &SavedMessagesFacade::changed);

    // Nothing stored yet: every operation is silent.
    facade.clear();
    facade.remove(QStringLiteral("1"), QStringLiteral("10"));
    facade.removeAt(0);
    QCOMPARE(spy.count(), 0);

    facade.addAt(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("a"), 100);
    QCOMPARE(spy.count(), 1);

    // A repeated pair keeps the stored copy, so nothing visible moved.
    facade.addAt(QStringLiteral("1"), QStringLiteral("10"), QStringLiteral("b"), 200);
    QCOMPARE(spy.count(), 1);

    facade.addAt(QStringLiteral("1"), QStringLiteral("20"), QStringLiteral("c"), 200);
    QCOMPARE(spy.count(), 2);

    // Removing an absent row is silent; removing a stored one is not.
    facade.remove(QStringLiteral("1"), QStringLiteral("30"));
    QCOMPARE(spy.count(), 2);

    facade.removeAt(0);
    QCOMPARE(spy.count(), 3);

    facade.clear();
    QCOMPARE(spy.count(), 4);

    facade.clear();
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(SavedMessagesFacadeTest)
#include "tst_saved_messages_facade.moc"
