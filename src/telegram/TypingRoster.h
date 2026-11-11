#pragma once

#include <QHash>
#include <QObject>
#include <QString>

namespace beygram::telegram {

/// Tracks who is currently typing in one chat.
///
/// TDLib delivers updateChatAction as a transient event: every typing action is
/// a heartbeat that refreshes that user's deadline, and chatActionCancel
/// withdraws it. Nothing here is persisted; a roster belongs to an open chat.
///
/// The class is time-driven but never reads the clock itself. applyAction() and
/// expire() take an explicit `nowMs`, so the caller advances time with a timer
/// and tests stay deterministic.
///
/// Only chatActionTyping keeps a user in the roster. chatActionCancel and every
/// other action (uploading a photo, recording a video, ...) remove the user,
/// because the line under the chat title only ever describes typing.
class TypingRoster final : public QObject {
    Q_OBJECT

public:
    explicit TypingRoster(QObject* parent = nullptr);

    /// Records one updateChatAction. A typing action sets the user's deadline to
    /// `nowMs` plus 5 seconds and refreshes a stale name; any other action drops
    /// the user. changed() is emitted only when the label or count really
    /// changes, so someone who keeps typing does not repaint the chat list.
    void applyAction(qint64 userId, const QString& userName,
                     const QString& actionType, qint64 nowMs);

    /// Drops every user whose deadline has passed, i.e. `expiresAt <= nowMs`.
    void expire(qint64 nowMs);

    /// The line shown under the chat title: one name, "A and B",
    /// "A, B and C", or "4 people". Empty when nobody is typing. Names are
    /// ordered alphabetically, so the text does not depend on arrival order.
    [[nodiscard]] QString label() const;

    [[nodiscard]] int count() const;

    /// Forgets every typist, e.g. when the chat is closed or the list is reset.
    void clear();

signals:
    void changed();

private:
    struct Entry {
        QString name;
        qint64 expiresAt = 0;
    };
    QHash<qint64, Entry> m_entries;
};

} // namespace beygram::telegram
