#pragma once

#include <QObject>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Holds the client-side view of the Archive: whether the archive folder is
/// open and which chat ids it currently lists.
///
/// The archived ids are kept exactly as TDLib reports them: only positive ids
/// are accepted, duplicates are dropped and the incoming order is preserved.
/// Every mutator is a no-op when it would not change the stored state, so
/// changed() is emitted once per real transition and views never reload for
/// nothing.
class ChatArchiveController final : public QObject {
    Q_OBJECT

public:
    explicit ChatArchiveController(QObject* parent = nullptr);

    /// Opens or closes the archive folder. Closing it keeps the id list, so a
    /// later reopen does not have to wait for a fresh getChats.
    void setArchiveOpen(bool open);
    [[nodiscard]] bool isArchiveOpen() const { return m_open; }

    /// Replaces the whole list, typically from a getChats response. Non-positive
    /// ids are discarded and repeated ids keep their first position.
    void setArchivedIds(QVector<qint64> ids);
    [[nodiscard]] QVector<qint64> archivedIds() const { return m_ids; }

    /// Appends a chat to the end of the list. An id that is already archived is
    /// ignored; a non-positive id is ignored.
    void addArchived(qint64 chatId);

    /// Removes a chat from the list. Unknown and non-positive ids are ignored.
    void removeArchived(qint64 chatId);

    /// Empties the list. The open flag is untouched.
    void clear();

signals:
    /// Emitted whenever the open flag or the id list actually changed.
    void changed();

private:
    bool m_open = false;
    QVector<qint64> m_ids;
};

} // namespace beygram::telegram
