#include "telegram/ArchiveSettingsModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class ArchiveSettingsModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithAllSwitchesOff();
    void startsAsDefault();
    void setArchiveAndMuteNewChatsStoresTheFlag();
    void setArchiveAndMuteNewChatsEmitsOnlyOnRealChange();
    void setKeepArchivedUnmutedStoresTheFlag();
    void setKeepArchivedUnmutedEmitsOnlyOnRealChange();
    void setKeepArchivedFoldersStoresTheFlag();
    void setKeepArchivedFoldersEmitsOnlyOnRealChange();
    void setArchiveStoriesStoresTheFlag();
    void setArchiveStoriesEmitsOnlyOnRealChange();
    void switchesAreIndependent();
    void isDefaultTracksAllFourSwitches();
    void resetRestoresDefaults();
    void resetIsSilentWhenAlreadyDefault();
    void resetEmitsOnceForAnyCombination();
    void changedTracksOnlyRealChanges();
};

void ArchiveSettingsModelTest::startsWithAllSwitchesOff()
{
    const ArchiveSettingsModel model;

    QVERIFY2(!model.archiveAndMuteNewChats(), "new chats are not archived by default");
    QVERIFY2(!model.keepArchivedUnmuted(), "unmuted chats leave the archive by default");
    QVERIFY2(!model.keepArchivedFolders(), "archived chats leave their folders by default");
    QVERIFY2(!model.archiveStories(), "stories are not archived by default");
}

void ArchiveSettingsModelTest::startsAsDefault()
{
    ArchiveSettingsModel model;
    QSignalSpy spy(&model, &ArchiveSettingsModel::changed);

    QVERIFY(model.isDefault());
    QCOMPARE(spy.count(), 0);
}

void ArchiveSettingsModelTest::setArchiveAndMuteNewChatsStoresTheFlag()
{
    ArchiveSettingsModel model;

    model.setArchiveAndMuteNewChats(true);
    QVERIFY(model.archiveAndMuteNewChats());

    model.setArchiveAndMuteNewChats(false);
    QVERIFY(!model.archiveAndMuteNewChats());
}

void ArchiveSettingsModelTest::setArchiveAndMuteNewChatsEmitsOnlyOnRealChange()
{
    ArchiveSettingsModel model;
    QSignalSpy spy(&model, &ArchiveSettingsModel::changed);

    model.setArchiveAndMuteNewChats(true);
    QCOMPARE(spy.count(), 1);

    // Writing the stored value again is silent.
    model.setArchiveAndMuteNewChats(true);
    QCOMPARE(spy.count(), 1);

    model.setArchiveAndMuteNewChats(false);
    QCOMPARE(spy.count(), 2);

    model.setArchiveAndMuteNewChats(false);
    QCOMPARE(spy.count(), 2);
}

void ArchiveSettingsModelTest::setKeepArchivedUnmutedStoresTheFlag()
{
    ArchiveSettingsModel model;

    model.setKeepArchivedUnmuted(true);
    QVERIFY(model.keepArchivedUnmuted());

    model.setKeepArchivedUnmuted(false);
    QVERIFY(!model.keepArchivedUnmuted());
}

void ArchiveSettingsModelTest::setKeepArchivedUnmutedEmitsOnlyOnRealChange()
{
    ArchiveSettingsModel model;
    QSignalSpy spy(&model, &ArchiveSettingsModel::changed);

    model.setKeepArchivedUnmuted(true);
    QCOMPARE(spy.count(), 1);

    model.setKeepArchivedUnmuted(true);
    QCOMPARE(spy.count(), 1);

    model.setKeepArchivedUnmuted(false);
    QCOMPARE(spy.count(), 2);

    model.setKeepArchivedUnmuted(false);
    QCOMPARE(spy.count(), 2);
}

void ArchiveSettingsModelTest::setKeepArchivedFoldersStoresTheFlag()
{
    ArchiveSettingsModel model;

    model.setKeepArchivedFolders(true);
    QVERIFY(model.keepArchivedFolders());

    model.setKeepArchivedFolders(false);
    QVERIFY(!model.keepArchivedFolders());
}

void ArchiveSettingsModelTest::setKeepArchivedFoldersEmitsOnlyOnRealChange()
{
    ArchiveSettingsModel model;
    QSignalSpy spy(&model, &ArchiveSettingsModel::changed);

    model.setKeepArchivedFolders(true);
    QCOMPARE(spy.count(), 1);

    model.setKeepArchivedFolders(true);
    QCOMPARE(spy.count(), 1);

    model.setKeepArchivedFolders(false);
    QCOMPARE(spy.count(), 2);

    model.setKeepArchivedFolders(false);
    QCOMPARE(spy.count(), 2);
}

void ArchiveSettingsModelTest::setArchiveStoriesStoresTheFlag()
{
    ArchiveSettingsModel model;

    model.setArchiveStories(true);
    QVERIFY(model.archiveStories());

    model.setArchiveStories(false);
    QVERIFY(!model.archiveStories());
}

void ArchiveSettingsModelTest::setArchiveStoriesEmitsOnlyOnRealChange()
{
    ArchiveSettingsModel model;
    QSignalSpy spy(&model, &ArchiveSettingsModel::changed);

    model.setArchiveStories(true);
    QCOMPARE(spy.count(), 1);

    model.setArchiveStories(true);
    QCOMPARE(spy.count(), 1);

    model.setArchiveStories(false);
    QCOMPARE(spy.count(), 2);

    model.setArchiveStories(false);
    QCOMPARE(spy.count(), 2);
}

void ArchiveSettingsModelTest::switchesAreIndependent()
{
    ArchiveSettingsModel model;

    // Turning one switch on must not disturb the other three.
    model.setArchiveAndMuteNewChats(true);
    QVERIFY(!model.keepArchivedUnmuted());
    QVERIFY(!model.keepArchivedFolders());
    QVERIFY(!model.archiveStories());

    model.setKeepArchivedUnmuted(true);
    QVERIFY(model.archiveAndMuteNewChats());
    QVERIFY(!model.keepArchivedFolders());
    QVERIFY(!model.archiveStories());

    model.setKeepArchivedFolders(true);
    QVERIFY(model.archiveAndMuteNewChats());
    QVERIFY(model.keepArchivedUnmuted());
    QVERIFY(!model.archiveStories());

    model.setArchiveStories(true);
    QVERIFY(model.archiveAndMuteNewChats());
    QVERIFY(model.keepArchivedUnmuted());
    QVERIFY(model.keepArchivedFolders());

    // And turning one off leaves the other three on.
    model.setKeepArchivedUnmuted(false);
    QVERIFY(!model.keepArchivedUnmuted());
    QVERIFY(model.archiveAndMuteNewChats());
    QVERIFY(model.keepArchivedFolders());
    QVERIFY(model.archiveStories());
}

void ArchiveSettingsModelTest::isDefaultTracksAllFourSwitches()
{
    ArchiveSettingsModel model;
    QVERIFY(model.isDefault());

    model.setArchiveAndMuteNewChats(true);
    QVERIFY2(!model.isDefault(), "any single switch breaks the default");
    model.setArchiveAndMuteNewChats(false);
    QVERIFY(model.isDefault());

    model.setKeepArchivedUnmuted(true);
    QVERIFY(!model.isDefault());
    model.setKeepArchivedUnmuted(false);
    QVERIFY(model.isDefault());

    model.setKeepArchivedFolders(true);
    QVERIFY(!model.isDefault());
    model.setKeepArchivedFolders(false);
    QVERIFY(model.isDefault());

    model.setArchiveStories(true);
    QVERIFY(!model.isDefault());
    model.setArchiveStories(false);
    QVERIFY(model.isDefault());

    // All four at once is not the default either.
    model.setArchiveAndMuteNewChats(true);
    model.setKeepArchivedUnmuted(true);
    model.setKeepArchivedFolders(true);
    model.setArchiveStories(true);
    QVERIFY(!model.isDefault());
}

void ArchiveSettingsModelTest::resetRestoresDefaults()
{
    ArchiveSettingsModel model;
    model.setArchiveAndMuteNewChats(true);
    model.setKeepArchivedUnmuted(true);
    model.setKeepArchivedFolders(true);
    model.setArchiveStories(true);

    model.reset();

    QVERIFY(!model.archiveAndMuteNewChats());
    QVERIFY(!model.keepArchivedUnmuted());
    QVERIFY(!model.keepArchivedFolders());
    QVERIFY(!model.archiveStories());
    QVERIFY(model.isDefault());
}

void ArchiveSettingsModelTest::resetIsSilentWhenAlreadyDefault()
{
    ArchiveSettingsModel model;
    QSignalSpy spy(&model, &ArchiveSettingsModel::changed);

    model.reset();
    QCOMPARE(spy.count(), 0);
    QVERIFY(model.isDefault());

    // Back at the defaults after a round trip, reset() is silent again.
    model.setArchiveStories(true);
    QCOMPARE(spy.count(), 1);

    model.setArchiveStories(false);
    QCOMPARE(spy.count(), 2);

    model.reset();
    QCOMPARE(spy.count(), 2);
}

void ArchiveSettingsModelTest::resetEmitsOnceForAnyCombination()
{
    ArchiveSettingsModel model;

    model.setKeepArchivedFolders(true);
    QSignalSpy spy(&model, &ArchiveSettingsModel::changed);

    // One signal for the whole reset, not one per switch.
    model.reset();
    QCOMPARE(spy.count(), 1);
    QVERIFY(model.isDefault());

    model.setArchiveAndMuteNewChats(true);
    model.setKeepArchivedUnmuted(true);
    model.setKeepArchivedFolders(true);
    model.setArchiveStories(true);
    spy.clear();

    model.reset();
    QCOMPARE(spy.count(), 1);
    QVERIFY(model.isDefault());
}

void ArchiveSettingsModelTest::changedTracksOnlyRealChanges()
{
    ArchiveSettingsModel model;
    QSignalSpy spy(&model, &ArchiveSettingsModel::changed);

    // A full round trip of every switch emits exactly eight changes.
    model.setArchiveAndMuteNewChats(true);
    model.setKeepArchivedUnmuted(true);
    model.setKeepArchivedFolders(true);
    model.setArchiveStories(true);
    QCOMPARE(spy.count(), 4);

    model.setArchiveAndMuteNewChats(false);
    model.setKeepArchivedUnmuted(false);
    model.setKeepArchivedFolders(false);
    model.setArchiveStories(false);
    QCOMPARE(spy.count(), 8);

    // Repeating the stored values adds nothing.
    model.setArchiveAndMuteNewChats(false);
    model.setKeepArchivedUnmuted(false);
    model.setKeepArchivedFolders(false);
    model.setArchiveStories(false);
    QCOMPARE(spy.count(), 8);
}

QTEST_GUILESS_MAIN(ArchiveSettingsModelTest)
#include "tst_archive_settings_model.moc"
