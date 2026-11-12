#pragma once

#include "telegram/ArchiveSettingsModel.h"
#include "telegram/ChatFolderEditController.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// The folder editor's view model: the loaded folder strip, the folder being
/// edited and the archive switch that belongs to the same screen.
///
/// The facade owns no transport. setFolders() is how the caller publishes what
/// updateChatFolders/chat folder lists reported, beginCreate()/beginEdit() open
/// an edit, and save() only emits createRequested/renameRequested with the exact
/// payload the transport should send before marking the draft saved. Nothing
/// here logs, reads the clock or performs I/O, so the dialog can be driven from
/// a test without a session.
///
/// Ids are strings because they travel to QML as a plain list; the value is the
/// decimal ChatFolderInfo::id. The main chat list is id "0" and is never
/// editable or deletable, matching FolderEdit's contract.
///
/// Selection is an index into the lists published by setFolders(), or -1 when
/// nothing is selected. setFolders() keeps a still-valid index, drops it when
/// the new list is shorter, and truncates mismatched inputs to the shorter
/// length instead of inventing folders. changed() is emitted only for real
/// changes, so a refresh that reports the same folders does not repaint.
class FoldersFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList folderTitles READ folderTitles NOTIFY changed)
    Q_PROPERTY(QStringList folderIds READ folderIds NOTIFY changed)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY changed)
    Q_PROPERTY(bool editing READ editing NOTIFY changed)
    Q_PROPERTY(QString draftTitle READ draftTitle NOTIFY changed)
    Q_PROPERTY(bool canSave READ canSave NOTIFY changed)
    Q_PROPERTY(bool archiveAndMuteNewChats READ archiveAndMuteNewChats WRITE
                   setArchiveAndMuteNewChats NOTIFY changed)

public:
    explicit FoldersFacade(QObject* parent = nullptr);

    [[nodiscard]] QStringList folderTitles() const;
    [[nodiscard]] QStringList folderIds() const;

    /// Publishes the folder strip. Entries beyond the shorter list are ignored.
    /// The selection survives while its index is still in range and becomes -1
    /// otherwise. Silent when the same ids and titles are already loaded.
    Q_INVOKABLE void setFolders(const QStringList& ids, const QStringList& titles);

    /// The selected row, or -1. selectFolder() accepts -1 to clear the
    /// selection; any other out-of-range index is ignored. Selecting a real row
    /// emits folderSelected() with its id, so the parent can switch the chat
    /// list it shows.
    [[nodiscard]] int currentIndex() const;
    Q_INVOKABLE void selectFolder(int index);

    /// True between beginCreate()/beginEdit() and cancelEdit(). Saving does not
    /// close the edit: the draft stays open so the field can keep showing the
    /// saved name, exactly like ChatFolderEditController.
    [[nodiscard]] bool editing() const;

    /// The draft as the transport would send it: normalised, trimmed and cut to
    /// the schema's 12 code points.
    [[nodiscard]] QString draftTitle() const;

    /// True when save() would actually send something: an edit is open, the
    /// draft differs from the stored title and it is not blank.
    [[nodiscard]] bool canSave() const;

    /// Opens an edit for a folder that does not exist yet (id 0). Saving emits
    /// createRequested().
    Q_INVOKABLE void beginCreate();

    /// Opens an edit for the folder at `index`, loading its current title. The
    /// main list ("0") and malformed or out-of-range rows are ignored, because
    /// the main chat list is not a folder.
    Q_INVOKABLE void beginEdit(int index);

    /// Records what the name field shows. Ignored while no edit is open; the
    /// draft is normalised by the controller, so retyping an equivalent title
    /// stays silent.
    Q_INVOKABLE void setDraftTitle(const QString& title);

    /// Leaves the edit without sending anything.
    Q_INVOKABLE void cancelEdit();

    /// Sends the pending edit: createRequested() for a new folder,
    /// renameRequested() for an existing one. Returns false and emits nothing
    /// when canSave() is false. On success the draft is marked saved, so
    /// canSave() turns false while editing() stays true.
    Q_INVOKABLE bool save();

    /// Asks the parent to delete the folder at `index` and emits
    /// deleteRequested() with its id. The main list and invalid rows are
    /// ignored. An open edit of that same folder is cancelled first.
    Q_INVOKABLE void requestDelete(int index);

    [[nodiscard]] bool archiveAndMuteNewChats() const;
    void setArchiveAndMuteNewChats(bool on);

signals:
    /// Any observable state of this facade moved: folders, selection, edit
    /// state, draft, canSave or the archive switch.
    void changed();

    /// A row was selected; folderId is its decimal id.
    void folderSelected(const QString& folderId);

    /// save() accepted a new folder. The transport should send
    /// createChatFolderRequest(title).
    void createRequested(const QString& title);

    /// save() accepted a rename. The transport should send
    /// editChatFolderRequest(folderId.toLongLong(), title).
    void renameRequested(const QString& folderId, const QString& title);

    /// requestDelete() accepted a folder. The transport should send
    /// deleteChatFolderRequest(folderId.toLongLong()).
    void deleteRequested(const QString& folderId);

private:
    telegram::ChatFolderEditController m_edit;
    telegram::ArchiveSettingsModel m_archive;
    QStringList m_ids;
    QStringList m_titles;
    int m_currentIndex = -1;
};

} // namespace beygram::ui
