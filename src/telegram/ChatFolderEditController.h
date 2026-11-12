#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// Holds the edit state of one chat folder name: which folder is being edited,
/// the draft title and the title the server last confirmed. The folder dialog
/// only forwards what the user typed and asks canSave() whether the Save button
/// does anything; every comparison lives here.
///
/// The draft title is normalised exactly like FolderEdit's payload: whitespace
/// runs (including the line feeds the schema forbids) fold into one space, the
/// ends are trimmed and the text is cut to the 12-character limit in code
/// points. title() is therefore byte-identical to the name that
/// createChatFolderRequest/editChatFolderRequest would send, and dirty() is a
/// plain comparison of that text.
///
/// folderId follows what the two request builders need: 0 means a folder that
/// does not exist on the server yet and will be created, a positive id is an
/// existing custom folder (a ChatFolderInfo::id from ChatFoldersState). The main
/// chat list is not editable and must never be opened here, even though
/// ChatFoldersState also numbers it 0. A negative id is treated as 0.
///
/// Nothing is sent from this class: it builds no request, logs nothing and keeps
/// no network or session state. Once the transport accepted the edit the caller
/// calls markSaved().
class ChatFolderEditController final : public QObject {
    Q_OBJECT

public:
    explicit ChatFolderEditController(QObject* parent = nullptr);

    /// Starts editing one folder. The draft is reset to the normalised title, so
    /// a freshly begun edit is never dirty. Calling begin() while another folder
    /// is open discards that draft instead of merging with it. Silent when the
    /// edit state does not actually change (same folder, same title).
    void begin(qint64 folderId, const QString& title);

    /// Leaves the edit without saving: deactivates and forgets the folder id,
    /// the draft and the original. Silent when no edit is active.
    void cancel();

    /// True between begin() and cancel(). markSaved() does not close the edit,
    /// so saving a rename and then retyping keeps the same session open.
    [[nodiscard]] bool active() const;

    /// The folder being edited, or -1 when inactive. 0 is a folder that still
    /// has to be created.
    [[nodiscard]] qint64 folderId() const;

    /// The draft title as the user would save it, already normalised.
    [[nodiscard]] QString title() const;

    /// Records what the name field shows. Ignored while inactive, because there
    /// is no edit to record it in. Emits changed() only when the normalised
    /// title differs from the current draft, so retyping the same text or adding
    /// only trailing spaces stays silent.
    void setTitle(const QString& title);

    /// True when saving would change the folder name on the server, i.e. when
    /// the draft differs from the title the folder was opened with or last saved
    /// with. A blank draft counts as a change when the original had text,
    /// because that is what the caller typed, even though canSave() rejects it.
    [[nodiscard]] bool dirty() const;

    /// active() && dirty() && the title is not blank && it fits the schema's
    /// 12-character limit. The limit is counted in Unicode code points, the unit
    /// FolderEdit uses, so one emoji is one character and not two.
    [[nodiscard]] bool canSave() const;

    /// Moves the original onto the draft after the server accepted the edit. The
    /// edit stays open and the draft is kept, so the field still shows the saved
    /// name and the next edit is measured against it. Silent when the draft is
    /// already what the original says, or when no edit is active.
    void markSaved();

signals:
    /// Emitted when the edit state changes: begin() opened or replaced an edit,
    /// setTitle() moved the draft, cancel() closed the edit, or markSaved()
    /// accepted the draft.
    void changed();

private:
    qint64 m_folderId = -1;   // -1 while inactive; 0 for a folder to be created
    QString m_title;          // draft, normalised like the FolderEdit payload
    QString m_original;       // title the folder was opened or last saved with
    bool m_active = false;
};

} // namespace beygram::telegram
