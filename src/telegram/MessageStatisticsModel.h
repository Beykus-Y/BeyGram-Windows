#pragma once

#include <QObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Holds the statistics of one message: its view, forward and reaction counts,
/// plus whether those numbers are still being fetched.
///
/// The model is a plain value holder. It never talks to TDLib and does not own
/// the message; it only remembers the (chatId, messageId) pair the numbers
/// belong to. Counts and the loading flag only apply while a message is set, so
/// a response that arrives after clear() cannot revive a stale panel. changed()
/// is emitted only when a stored value really changes, which keeps a view bound
/// to it cheap to repaint.
class MessageStatisticsModel final : public QObject {
    Q_OBJECT

public:
    explicit MessageStatisticsModel(QObject* parent = nullptr);

    /// Selects the message the statistics belong to. Both ids must be positive;
    /// anything else is ignored. A valid pair resets the counts and the loading
    /// flag, because they described the previous message.
    void setMessage(qint64 chatId, qint64 messageId);

    /// Whether a message is selected.
    [[nodiscard]] bool active() const;

    [[nodiscard]] qint64 chatId() const;
    [[nodiscard]] qint64 messageId() const;

    /// Replaces the three counters. Negative values are stored as 0. Ignored
    /// while no message is selected, so a late response is dropped.
    void setCounts(int views, int forwards, int reactions);

    [[nodiscard]] int views() const;
    [[nodiscard]] int forwards() const;
    [[nodiscard]] int reactions() const;

    /// Marks whether the numbers are still being fetched. Ignored while no
    /// message is selected.
    void setLoading(bool loading);

    [[nodiscard]] bool loading() const;

    /// Drops the message, the counts and the loading flag. A no-op while no
    /// message is selected.
    void clear();

signals:
    void changed();

private:
    qint64 m_chatId = 0;
    qint64 m_messageId = 0;
    int m_views = 0;
    int m_forwards = 0;
    int m_reactions = 0;
    bool m_loading = false;
};

} // namespace beygram::telegram
