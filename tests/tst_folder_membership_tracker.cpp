#include "telegram/FolderMembershipTracker.h"

#include <QSignalSpy>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

class FolderMembershipTrackerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setFoldersSanitizesAndKeepsOrder();
    void setFoldersPrunesRemovedFolders();
    void setMembersReplacesAndSanitizes();
    void setMembersIgnoresUnknownFolders();
    void addChatIsIdempotent();
    void removeChatIsIdempotent();
    void removeChatEverywhereRemovesFromAllFolders();
    void foldersForChatFollowsFolderOrder();
    void clearEmitsOnlyWhenNotEmpty();
};

void FolderMembershipTrackerTest::startsEmpty()
{
    FolderMembershipTracker tracker;
    QVERIFY(tracker.folders().isEmpty());
    QVERIFY(tracker.members(0).isEmpty());
    QVERIFY(tracker.members(9).isEmpty());
    QVERIFY(tracker.foldersForChat(42).isEmpty());

    // Clearing an empty tracker is not a change.
    QSignalSpy spy(&tracker, &FolderMembershipTracker::changed);
    tracker.clear();
    QCOMPARE(spy.count(), 0);
}

void FolderMembershipTrackerTest::setFoldersSanitizesAndKeepsOrder()
{
    FolderMembershipTracker tracker;
    QSignalSpy spy(&tracker, &FolderMembershipTracker::changed);

    tracker.setFolders({0, 7, -3, 7, 2});
    QCOMPARE(tracker.folders(), (QVector<qint64>{0, 7, 2}));
    QCOMPARE(spy.count(), 1);

    // The same folders plus duplicates and a negative id are not a change.
    tracker.setFolders({0, 7, 2, 7, -1});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(tracker.folders(), (QVector<qint64>{0, 7, 2}));

    // Reordering the same set is a real change.
    tracker.setFolders({2, 0, 7});
    QCOMPARE(spy.count(), 2);
    QCOMPARE(tracker.folders(), (QVector<qint64>{2, 0, 7}));

    tracker.setFolders(QVector<qint64>{});
    QCOMPARE(spy.count(), 3);
    QVERIFY(tracker.folders().isEmpty());
}

void FolderMembershipTrackerTest::setFoldersPrunesRemovedFolders()
{
    FolderMembershipTracker tracker;
    tracker.setFolders({0, 5, 8});
    tracker.setMembers(0, {100, 101});
    tracker.setMembers(8, {200});

    QSignalSpy spy(&tracker, &FolderMembershipTracker::changed);
    tracker.setFolders({0, 5});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(tracker.members(0), (QVector<qint64>{100, 101}));
    QVERIFY(tracker.members(8).isEmpty());
    QVERIFY(tracker.foldersForChat(200).isEmpty());

    // Adding an unrelated folder keeps the remaining membership.
    tracker.setMembers(5, {300});
    spy.clear();
    tracker.setFolders({0, 5, 9});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(tracker.members(0), (QVector<qint64>{100, 101}));
    QCOMPARE(tracker.members(5), (QVector<qint64>{300}));
}

void FolderMembershipTrackerTest::setMembersReplacesAndSanitizes()
{
    FolderMembershipTracker tracker;
    tracker.setFolders({3});
    QSignalSpy spy(&tracker, &FolderMembershipTracker::changed);

    tracker.setMembers(3, {10, -1, 0, 10, 11});
    QCOMPARE(tracker.members(3), (QVector<qint64>{10, 11}));
    QCOMPARE(spy.count(), 1);

    tracker.setMembers(3, {10, 11});
    QCOMPARE(spy.count(), 1);

    tracker.setMembers(3, {11, 10});
    QCOMPARE(spy.count(), 2);
    QCOMPARE(tracker.members(3), (QVector<qint64>{11, 10}));

    tracker.setMembers(3, QVector<qint64>{});
    QCOMPARE(spy.count(), 3);
    QVERIFY(tracker.members(3).isEmpty());

    tracker.setMembers(3, QVector<qint64>{});
    QCOMPARE(spy.count(), 3);
}

void FolderMembershipTrackerTest::setMembersIgnoresUnknownFolders()
{
    FolderMembershipTracker tracker;
    tracker.setFolders({0});
    QSignalSpy spy(&tracker, &FolderMembershipTracker::changed);

    tracker.setMembers(4, {10});
    tracker.setMembers(-2, {10});
    QCOMPARE(spy.count(), 0);
    QVERIFY(tracker.members(4).isEmpty());
    QVERIFY(tracker.members(-2).isEmpty());
    QVERIFY(tracker.foldersForChat(10).isEmpty());
}

void FolderMembershipTrackerTest::addChatIsIdempotent()
{
    FolderMembershipTracker tracker;
    tracker.setFolders({0, 2});
    QSignalSpy spy(&tracker, &FolderMembershipTracker::changed);

    tracker.addChat(2, 50);
    QCOMPARE(tracker.members(2), (QVector<qint64>{50}));
    QCOMPARE(spy.count(), 1);

    tracker.addChat(2, 50);
    QCOMPARE(spy.count(), 1);

    // Non-positive chat ids and unknown folders are ignored.
    tracker.addChat(2, 0);
    tracker.addChat(2, -5);
    tracker.addChat(9, 60);
    QCOMPARE(spy.count(), 1);

    tracker.addChat(0, 60);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(tracker.members(2), (QVector<qint64>{50}));
    QCOMPARE(tracker.members(0), (QVector<qint64>{60}));
}

void FolderMembershipTrackerTest::removeChatIsIdempotent()
{
    FolderMembershipTracker tracker;
    tracker.setFolders({2});
    tracker.setMembers(2, {1, 2, 3});
    QSignalSpy spy(&tracker, &FolderMembershipTracker::changed);

    tracker.removeChat(2, 2);
    QCOMPARE(tracker.members(2), (QVector<qint64>{1, 3}));
    QCOMPARE(spy.count(), 1);

    tracker.removeChat(2, 2);
    tracker.removeChat(7, 1);
    QCOMPARE(spy.count(), 1);

    tracker.removeChat(2, 1);
    QCOMPARE(spy.count(), 2);
    tracker.removeChat(2, 3);
    QCOMPARE(spy.count(), 3);
    QVERIFY(tracker.members(2).isEmpty());
}

void FolderMembershipTrackerTest::removeChatEverywhereRemovesFromAllFolders()
{
    FolderMembershipTracker tracker;
    tracker.setFolders({0, 2, 5});
    tracker.setMembers(0, {7, 8});
    tracker.setMembers(2, {8, 9});
    tracker.setMembers(5, {8});
    QCOMPARE(tracker.foldersForChat(8), (QVector<qint64>{0, 2, 5}));

    QSignalSpy spy(&tracker, &FolderMembershipTracker::changed);
    tracker.removeChatEverywhere(8);
    QCOMPARE(spy.count(), 1);
    QVERIFY(tracker.foldersForChat(8).isEmpty());
    QCOMPARE(tracker.members(0), (QVector<qint64>{7}));
    QCOMPARE(tracker.members(2), (QVector<qint64>{9}));
    QVERIFY(tracker.members(5).isEmpty());

    // A chat that is gone, an unknown chat and an invalid id change nothing.
    tracker.removeChatEverywhere(8);
    tracker.removeChatEverywhere(12345);
    tracker.removeChatEverywhere(-1);
    QCOMPARE(spy.count(), 1);
}

void FolderMembershipTrackerTest::foldersForChatFollowsFolderOrder()
{
    FolderMembershipTracker tracker;
    tracker.setFolders({5, 0, 2});
    tracker.addChat(2, 42);
    tracker.addChat(0, 42);
    tracker.addChat(5, 42);
    QCOMPARE(tracker.foldersForChat(42), (QVector<qint64>{5, 0, 2}));

    // The result follows folders(), not the order chats were added in.
    tracker.setFolders({2, 5, 0});
    QCOMPARE(tracker.foldersForChat(42), (QVector<qint64>{2, 5, 0}));

    QVERIFY(tracker.foldersForChat(43).isEmpty());
    QVERIFY(tracker.foldersForChat(0).isEmpty());
    QVERIFY(tracker.foldersForChat(-42).isEmpty());
}

void FolderMembershipTrackerTest::clearEmitsOnlyWhenNotEmpty()
{
    FolderMembershipTracker tracker;
    tracker.setFolders({0});
    tracker.setMembers(0, {1});

    QSignalSpy spy(&tracker, &FolderMembershipTracker::changed);
    tracker.clear();
    QCOMPARE(spy.count(), 1);
    QVERIFY(tracker.folders().isEmpty());
    QVERIFY(tracker.members(0).isEmpty());
    QVERIFY(tracker.foldersForChat(1).isEmpty());

    tracker.clear();
    QCOMPARE(spy.count(), 1);

    // Folders without members are still state worth a notification.
    tracker.setFolders({3});
    spy.clear();
    tracker.clear();
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(FolderMembershipTrackerTest)
#include "tst_folder_membership_tracker.moc"
