#include "ui/FoldersFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

using beygram::ui::FoldersFacade;

namespace {

/// The strip the tests load: the main list plus two custom folders.
QStringList ids()
{
    return {QStringLiteral("0"), QStringLiteral("7"), QStringLiteral("42")};
}

QStringList titles()
{
    return {QStringLiteral("All chats"), QStringLiteral("Work"), QStringLiteral("Trips")};
}

void loadDefault(FoldersFacade& facade)
{
    facade.setFolders(ids(), titles());
}

} // namespace

class FoldersFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void loadsFolders();
    void setFoldersIsSilentForSameContent();
    void setFoldersKeepsValidSelection();
    void setFoldersClampsInvalidSelection();
    void setFoldersTruncatesMismatchedLists();
    void selectFolderEmitsIdAndIndex();
    void selectFolderIgnoresInvalidIndex();
    void selectFolderIsSilentOnSameIndex();
    void selectFolderMinusOneClearsSelection();
    void beginCreateStartsEditing();
    void beginEditLoadsFolderTitle();
    void beginEditIgnoresMainList();
    void beginEditIgnoresInvalidIndex();
    void setDraftTitleMarksSaveable();
    void setDraftTitleIsIgnoredWhenNotEditing();
    void canSaveRequiresAChange();
    void cancelEditClosesEditing();
    void saveCreatesFolderWithTitle();
    void saveRenamesFolderWithId();
    void saveNormalisesTitle();
    void saveRejectsBlankTitle();
    void saveRejectsUnchangedTitle();
    void saveKeepsEditingOpen();
    void requestDeleteEmitsFolderId();
    void requestDeleteIgnoresMainListAndInvalidIndex();
    void requestDeleteCancelsOpenEdit();
    void archiveToggleDefaultsOff();
    void archiveToggleEmitsChangedOnce();
    void archiveToggleIsSilentOnSameValue();
};

void FoldersFacadeTest::startsEmpty()
{
    FoldersFacade facade;

    QVERIFY(facade.folderIds().isEmpty());
    QVERIFY(facade.folderTitles().isEmpty());
    QCOMPARE(facade.currentIndex(), -1);
    QVERIFY(!facade.editing());
    QCOMPARE(facade.draftTitle(), QString());
    QVERIFY(!facade.canSave());
    QVERIFY(!facade.archiveAndMuteNewChats());
}

void FoldersFacadeTest::loadsFolders()
{
    FoldersFacade facade;
    QSignalSpy spy(&facade, &FoldersFacade::changed);

    loadDefault(facade);

    QCOMPARE(facade.folderIds(), ids());
    QCOMPARE(facade.folderTitles(), titles());
    // Loading a strip is not selecting a row: the parent decides that.
    QCOMPARE(facade.currentIndex(), -1);
    QCOMPARE(spy.count(), 1);
}

void FoldersFacadeTest::setFoldersIsSilentForSameContent()
{
    FoldersFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &FoldersFacade::changed);

    loadDefault(facade);
    loadDefault(facade);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.folderTitles(), titles());
}

void FoldersFacadeTest::setFoldersKeepsValidSelection()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.selectFolder(1);

    facade.setFolders({QStringLiteral("0"), QStringLiteral("7"), QStringLiteral("43")},
                      {QStringLiteral("All chats"), QStringLiteral("Work"), QStringLiteral("Travel")});

    QCOMPARE(facade.currentIndex(), 1);
    QCOMPARE(facade.folderIds().at(1), QStringLiteral("7"));
}

void FoldersFacadeTest::setFoldersClampsInvalidSelection()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.selectFolder(2);
    QCOMPARE(facade.currentIndex(), 2);

    facade.setFolders({QStringLiteral("0")}, {QStringLiteral("All chats")});

    QCOMPARE(facade.currentIndex(), -1);
}

void FoldersFacadeTest::setFoldersTruncatesMismatchedLists()
{
    // Two ids, three titles: the orphan title is dropped rather than shown
    // against a folder that does not exist.
    FoldersFacade facade;

    facade.setFolders({QStringLiteral("0"), QStringLiteral("7")},
                      {QStringLiteral("All chats"), QStringLiteral("Work"), QStringLiteral("Orphan")});

    QCOMPARE(facade.folderIds().size(), 2);
    QCOMPARE(facade.folderTitles(), (QStringList{QStringLiteral("All chats"), QStringLiteral("Work")}));
}

void FoldersFacadeTest::selectFolderEmitsIdAndIndex()
{
    FoldersFacade facade;
    loadDefault(facade);
    QSignalSpy changedSpy(&facade, &FoldersFacade::changed);
    QSignalSpy selectSpy(&facade, &FoldersFacade::folderSelected);

    facade.selectFolder(2);

    QCOMPARE(facade.currentIndex(), 2);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(selectSpy.count(), 1);
    QCOMPARE(selectSpy.at(0).at(0).toString(), QStringLiteral("42"));
}

void FoldersFacadeTest::selectFolderIgnoresInvalidIndex()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.selectFolder(1);
    QSignalSpy changedSpy(&facade, &FoldersFacade::changed);
    QSignalSpy selectSpy(&facade, &FoldersFacade::folderSelected);

    facade.selectFolder(-2);
    facade.selectFolder(3);
    facade.selectFolder(99);

    QCOMPARE(facade.currentIndex(), 1);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(selectSpy.count(), 0);
}

void FoldersFacadeTest::selectFolderIsSilentOnSameIndex()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.selectFolder(1);
    QSignalSpy changedSpy(&facade, &FoldersFacade::changed);
    QSignalSpy selectSpy(&facade, &FoldersFacade::folderSelected);

    facade.selectFolder(1);

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(selectSpy.count(), 0);
}

void FoldersFacadeTest::selectFolderMinusOneClearsSelection()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.selectFolder(1);
    QSignalSpy changedSpy(&facade, &FoldersFacade::changed);
    QSignalSpy selectSpy(&facade, &FoldersFacade::folderSelected);

    facade.selectFolder(-1);

    QCOMPARE(facade.currentIndex(), -1);
    QCOMPARE(changedSpy.count(), 1);
    // Clearing is not a folder selection, so no id is emitted.
    QCOMPARE(selectSpy.count(), 0);
}

void FoldersFacadeTest::beginCreateStartsEditing()
{
    FoldersFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &FoldersFacade::changed);

    facade.beginCreate();

    QVERIFY(facade.editing());
    QCOMPARE(facade.draftTitle(), QString());
    // An empty draft has nothing to save yet.
    QVERIFY(!facade.canSave());
    QCOMPARE(spy.count(), 1);
}

void FoldersFacadeTest::beginEditLoadsFolderTitle()
{
    FoldersFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &FoldersFacade::changed);

    facade.beginEdit(1);

    QVERIFY(facade.editing());
    QCOMPARE(facade.draftTitle(), QStringLiteral("Work"));
    QVERIFY(!facade.canSave());
    QCOMPARE(spy.count(), 1);
}

void FoldersFacadeTest::beginEditIgnoresMainList()
{
    FoldersFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &FoldersFacade::changed);

    facade.beginEdit(0);

    // The main chat list is numbered 0 and is not a folder.
    QVERIFY(!facade.editing());
    QCOMPARE(facade.draftTitle(), QString());
    QCOMPARE(spy.count(), 0);
}

void FoldersFacadeTest::beginEditIgnoresInvalidIndex()
{
    FoldersFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &FoldersFacade::changed);

    facade.beginEdit(-1);
    facade.beginEdit(3);

    QVERIFY(!facade.editing());
    QCOMPARE(spy.count(), 0);
}

void FoldersFacadeTest::setDraftTitleMarksSaveable()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.beginEdit(1);
    QSignalSpy spy(&facade, &FoldersFacade::changed);

    facade.setDraftTitle(QStringLiteral("Office"));

    QCOMPARE(facade.draftTitle(), QStringLiteral("Office"));
    QVERIFY(facade.canSave());
    QCOMPARE(spy.count(), 1);
}

void FoldersFacadeTest::setDraftTitleIsIgnoredWhenNotEditing()
{
    FoldersFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &FoldersFacade::changed);

    facade.setDraftTitle(QStringLiteral("Office"));

    QCOMPARE(facade.draftTitle(), QString());
    QVERIFY(!facade.editing());
    QCOMPARE(spy.count(), 0);
}

void FoldersFacadeTest::canSaveRequiresAChange()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.beginEdit(1);

    // The stored title is not a change.
    QVERIFY(!facade.canSave());

    facade.setDraftTitle(QStringLiteral(" Work "));
    QVERIFY(!facade.canSave());

    facade.setDraftTitle(QStringLiteral("Office"));
    QVERIFY(facade.canSave());
}

void FoldersFacadeTest::cancelEditClosesEditing()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.beginEdit(1);
    facade.setDraftTitle(QStringLiteral("Office"));
    QSignalSpy spy(&facade, &FoldersFacade::changed);

    facade.cancelEdit();

    QVERIFY(!facade.editing());
    QCOMPARE(facade.draftTitle(), QString());
    QVERIFY(!facade.canSave());
    QCOMPARE(spy.count(), 1);
}

void FoldersFacadeTest::saveCreatesFolderWithTitle()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.beginCreate();
    facade.setDraftTitle(QStringLiteral("Trips"));
    QSignalSpy createSpy(&facade, &FoldersFacade::createRequested);
    QSignalSpy renameSpy(&facade, &FoldersFacade::renameRequested);

    QVERIFY(facade.save());

    QCOMPARE(createSpy.count(), 1);
    QCOMPARE(createSpy.at(0).at(0).toString(), QStringLiteral("Trips"));
    QCOMPARE(renameSpy.count(), 0);
}

void FoldersFacadeTest::saveRenamesFolderWithId()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.beginEdit(2);
    facade.setDraftTitle(QStringLiteral("Holidays"));
    QSignalSpy createSpy(&facade, &FoldersFacade::createRequested);
    QSignalSpy renameSpy(&facade, &FoldersFacade::renameRequested);

    QVERIFY(facade.save());

    QCOMPARE(renameSpy.count(), 1);
    QCOMPARE(renameSpy.at(0).at(0).toString(), QStringLiteral("42"));
    QCOMPARE(renameSpy.at(0).at(1).toString(), QStringLiteral("Holidays"));
    QCOMPARE(createSpy.count(), 0);
}

void FoldersFacadeTest::saveNormalisesTitle()
{
    // The payload is what createChatFolderRequest would send: folded, trimmed
    // and cut to twelve code points.
    FoldersFacade facade;
    facade.beginCreate();
    facade.setDraftTitle(QStringLiteral("  Work\n  chats  "));
    QSignalSpy createSpy(&facade, &FoldersFacade::createRequested);

    QVERIFY(facade.save());
    QCOMPARE(createSpy.at(0).at(0).toString(), QStringLiteral("Work chats"));

    facade.beginCreate();
    facade.setDraftTitle(QStringLiteral("abcdefghijklmnop"));
    QVERIFY(facade.save());
    QCOMPARE(createSpy.at(1).at(0).toString(), QStringLiteral("abcdefghijkl"));
}

void FoldersFacadeTest::saveRejectsBlankTitle()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.beginEdit(1);
    facade.setDraftTitle(QStringLiteral("   "));
    QSignalSpy createSpy(&facade, &FoldersFacade::createRequested);
    QSignalSpy renameSpy(&facade, &FoldersFacade::renameRequested);

    QVERIFY(!facade.canSave());
    QVERIFY(!facade.save());

    QCOMPARE(createSpy.count(), 0);
    QCOMPARE(renameSpy.count(), 0);
    // The failed save left the draft alone, so the user can fix it.
    QVERIFY(facade.editing());
}

void FoldersFacadeTest::saveRejectsUnchangedTitle()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.beginEdit(1);
    QSignalSpy createSpy(&facade, &FoldersFacade::createRequested);
    QSignalSpy renameSpy(&facade, &FoldersFacade::renameRequested);

    QVERIFY(!facade.save());

    QCOMPARE(createSpy.count(), 0);
    QCOMPARE(renameSpy.count(), 0);
}

void FoldersFacadeTest::saveKeepsEditingOpen()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.beginEdit(1);
    facade.setDraftTitle(QStringLiteral("Office"));

    QVERIFY(facade.save());

    // Saving marks the draft saved instead of closing the editor, so the field
    // still shows the accepted name and a second Save is a no-op.
    QVERIFY(facade.editing());
    QCOMPARE(facade.draftTitle(), QStringLiteral("Office"));
    QVERIFY(!facade.canSave());
    QVERIFY(!facade.save());
}

void FoldersFacadeTest::requestDeleteEmitsFolderId()
{
    FoldersFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &FoldersFacade::deleteRequested);

    facade.requestDelete(1);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("7"));
}

void FoldersFacadeTest::requestDeleteIgnoresMainListAndInvalidIndex()
{
    FoldersFacade facade;
    loadDefault(facade);
    QSignalSpy spy(&facade, &FoldersFacade::deleteRequested);

    facade.requestDelete(0);
    facade.requestDelete(-1);
    facade.requestDelete(3);

    QCOMPARE(spy.count(), 0);
}

void FoldersFacadeTest::requestDeleteCancelsOpenEdit()
{
    FoldersFacade facade;
    loadDefault(facade);
    facade.beginEdit(1);
    facade.setDraftTitle(QStringLiteral("Office"));
    QSignalSpy deleteSpy(&facade, &FoldersFacade::deleteRequested);

    facade.requestDelete(1);

    QCOMPARE(deleteSpy.count(), 1);
    // The draft cannot outlive the folder it addresses.
    QVERIFY(!facade.editing());
    QCOMPARE(facade.draftTitle(), QString());
}

void FoldersFacadeTest::archiveToggleDefaultsOff()
{
    FoldersFacade facade;

    QVERIFY(!facade.archiveAndMuteNewChats());
}

void FoldersFacadeTest::archiveToggleEmitsChangedOnce()
{
    FoldersFacade facade;
    QSignalSpy spy(&facade, &FoldersFacade::changed);

    facade.setArchiveAndMuteNewChats(true);

    QVERIFY(facade.archiveAndMuteNewChats());
    QCOMPARE(spy.count(), 1);

    facade.setArchiveAndMuteNewChats(false);
    QVERIFY(!facade.archiveAndMuteNewChats());
    QCOMPARE(spy.count(), 2);
}

void FoldersFacadeTest::archiveToggleIsSilentOnSameValue()
{
    FoldersFacade facade;
    QSignalSpy spy(&facade, &FoldersFacade::changed);

    facade.setArchiveAndMuteNewChats(false);
    QCOMPARE(spy.count(), 0);

    facade.setArchiveAndMuteNewChats(true);
    facade.setArchiveAndMuteNewChats(true);
    QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(FoldersFacadeTest)
#include "tst_folders_facade.moc"
