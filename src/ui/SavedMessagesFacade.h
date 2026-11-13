#pragma once

#include "telegram/SavedMessages.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace beygram::ui {

/// The Saved Messages screen as one QML-facing object.
///
/// This is a thin presentation facade: the entries, their ordering, the
/// duplicate rule and the changed() suppression all live in the
/// telegram::SavedMessagesModel this class owns. The facade only translates
/// between the string ids QML uses and the qint64 pair the model speaks, and
/// publishes the rows as parallel QStringLists so a repeater can index all of
/// them with the same position.
///
/// ids() is the stable key of each row and has the form "chatId:messageId",
/// both in decimal. A pair is what identifies an entry, so the same message id
/// in two chats produces two different keys. open() and removeAt() take the
/// row index, which is why QML never has to split a key itself.
///
/// previews(), ids() and dates() are parallel and in display order; entry i of
/// each describes the same row. dates() holds savedAt as decimal Unix seconds
/// rather than a formatted label, because the page formats it with the
/// application's localization singleton and the facade stays locale-free.
///
/// Invariants:
///  - count() == previews().size() == ids().size() == dates().size().
///  - rows are newest first, as the model defines: savedAt descending, then
///    messageId descending, with ties keeping their arrival order.
///  - add() and addAt() drop a non-positive or non-numeric id, and a pair that
///    is already stored keeps the copy it has.
///  - remove() and removeAt() of an absent row, clear() of an empty list and a
///    repeated add of a stored pair are silent: no changed() is emitted.
///
/// The facade never sends anything, reads the clock only to stamp add(), and
/// never logs. Opening a message is only announced through openRequested().
class SavedMessagesFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList previews READ previews NOTIFY changed)
    Q_PROPERTY(QStringList ids READ ids NOTIFY changed)
    Q_PROPERTY(QStringList dates READ dates NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)

public:
    explicit SavedMessagesFacade(QObject* parent = nullptr);

    /// The preview text of each row, in display order.
    [[nodiscard]] QStringList previews() const;

    /// The "chatId:messageId" key of each row, same order as previews().
    [[nodiscard]] QStringList ids() const;

    /// The savedAt of each row as decimal Unix seconds, same order as
    /// previews(). The page turns it into a label.
    [[nodiscard]] QStringList dates() const;

    [[nodiscard]] int count() const;

    /// Saves one message, stamped with the current Unix time.
    ///
    /// chatId and messageId must be positive decimal strings, as QML carries
    /// them; anything else is ignored. preview is stored verbatim, so the
    /// caller decides how much of the message it quotes. Saving a pair that is
    /// already stored is a no-op, exactly as SavedMessagesModel::append()
    /// defines it.
    Q_INVOKABLE void add(const QString& chatId, const QString& messageId,
                         const QString& preview);

    /// Saves one message with an explicit savedAt.
    ///
    /// This is the import/restore variant of add(): a list loaded from storage
    /// keeps the time it was saved instead of pretending it was saved now.
    /// Ids are validated and duplicates are handled exactly like add().
    Q_INVOKABLE void addAt(const QString& chatId, const QString& messageId,
                           const QString& preview, qint64 savedAt);

    /// Removes the entry with exactly this pair. Invalid ids and an absent
    /// pair are silent.
    Q_INVOKABLE void remove(const QString& chatId, const QString& messageId);

    /// Removes the row at `index` in previews()/ids()/dates(). An index outside
    /// the list is silent.
    Q_INVOKABLE void removeAt(int index);

    /// Drops every entry. Clearing an empty list is a no-op.
    Q_INVOKABLE void clear();

    /// Announces the row at `index` through openRequested() with the row's
    /// chatId and messageId, both in decimal. An index outside the list is
    /// silent.
    Q_INVOKABLE void open(int index);

signals:
    /// Emitted whenever previews(), ids(), dates() or count() actually
    /// changed. The model already suppresses no-op updates, so forwarding it
    /// is enough.
    void changed();

    /// Emitted by open() with the message the caller should show.
    void openRequested(const QString& chatId, const QString& messageId);

private:
    telegram::SavedMessagesModel m_model;
};

} // namespace beygram::ui
