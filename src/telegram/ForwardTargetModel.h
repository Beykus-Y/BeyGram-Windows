#pragma once

#include "TelegramTypes.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// State for the "forward to..." picker: which messages are being forwarded,
/// which chats can be chosen, the current search text and the chosen target.
///
/// The class holds no transport and no logging. It only keeps the picker state
/// consistent so the UI can bind to it and the send path can ask canSend().
///
/// Invariants:
///  - active() implies messageIds() is non-empty, positive and duplicate free.
///  - chats() only contains positive, unique ids in the order they were given.
///  - selectedChatId() is either 0 or an id present in chats().
///  - changed() is emitted only when observable state actually changed.
class ForwardTargetModel final : public QObject {
    Q_OBJECT

public:
    explicit ForwardTargetModel(QObject* parent = nullptr);

    /// Starts a forward session for the given messages. Non-positive ids are
    /// dropped and duplicates are removed keeping the first occurrence. An input
    /// without a single usable id is rejected and leaves the state untouched.
    /// A previously chosen target chat is forgotten, because it belonged to the
    /// previous session.
    void open(QVector<qint64> messageIds);

    /// Ends the forward session. Closing an inactive model is a no-op.
    void close();

    [[nodiscard]] bool active() const;
    [[nodiscard]] QVector<qint64> messageIds() const;

    /// Replaces the candidate chats. Non-positive ids are dropped and duplicate
    /// ids keep their first entry; the incoming order is preserved. When the
    /// current target chat is no longer among the candidates the selection is
    /// cleared.
    void setChats(QVector<ChatData> chats);
    [[nodiscard]] QVector<ChatData> chats() const;

    /// Case-insensitive substring match on the chat title. An empty filter
    /// returns every chat.
    void setFilter(const QString& filter);
    [[nodiscard]] QString filter() const;
    [[nodiscard]] QVector<ChatData> filteredChats() const;

    /// Selects a target chat. Ids that are not among chats() are ignored, and a
    /// non-positive id clears the selection.
    void setSelectedChat(qint64 chatId);
    [[nodiscard]] qint64 selectedChatId() const;

    /// True when a forward session is open, at least one message id is present
    /// and a target chat is selected.
    [[nodiscard]] bool canSend() const;

    /// Drops the session, the candidates, the filter and the selection.
    void reset();

signals:
    /// Emitted whenever the observable state actually changed.
    void changed();

private:
    QVector<qint64> m_ids;
    QVector<ChatData> m_chats;
    QString m_filter;
    qint64 m_selected = 0;
};

} // namespace beygram::telegram
