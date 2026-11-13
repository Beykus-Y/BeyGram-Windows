#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace beygram::ui {

/// QML-facing state for forwarding one source message set to several chats.
///
/// The facade is presentation state only: it owns the candidate chats, the
/// search text and the multi-selection, and announces the finished choice
/// through forwardRequested(). It performs no I/O and writes no logs.
///
/// Ids cross the QML boundary as strings because QML numbers are doubles and a
/// 64-bit TDLib id would lose precision. The source chat may be negative (a
/// group or channel id); message ids and target chat ids must be positive,
/// which is the same rule ForwardBatchPlanner applies. Blank or non-numeric
/// values are ignored.
///
/// Invariants:
///  - chatTitles() and chatIdAt() describe the same filtered chat order.
///  - selectedIds() only contains ids from the current candidate chats, in
///    candidate order rather than click order.
///  - canForward() is true exactly when begin() accepted a source and at least
///    one message, at least one target is selected, and both limits hold
///    (100 messages, 20 targets).
///  - error() explains the last failed action; it is cleared once the state is
///    usable again or the problem has been fixed.
///  - changed() is emitted only when observable state actually changed.
///
/// A failed begin() clears the previous session instead of leaving it armed:
/// the dialog must never forward the messages of an older selection. The
/// candidate chat list is kept because it is not session state.
class MultiForwardFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList chatTitles READ chatTitles NOTIFY changed)
    Q_PROPERTY(QStringList selectedIds READ selectedIds NOTIFY changed)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY changed)
    Q_PROPERTY(bool canForward READ canForward NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)

public:
    explicit MultiForwardFacade(QObject* parent = nullptr);

    /// Titles of the candidate chats matching filter(), in candidate order.
    [[nodiscard]] QStringList chatTitles() const;

    /// The id of the chat at `index` in chatTitles(), or an empty string when
    /// the index is out of range.
    Q_INVOKABLE QString chatIdAt(int index) const;

    /// The selected target ids, in candidate order.
    [[nodiscard]] QStringList selectedIds() const;

    /// Case-insensitive substring match on the chat title. An empty filter
    /// shows every chat.
    [[nodiscard]] QString filter() const;
    Q_INVOKABLE void setFilter(const QString& filter);

    /// True when begin() accepted the session, at least one target is selected
    /// and both batch limits hold.
    [[nodiscard]] bool canForward() const;

    /// The user-facing explanation of the last failed action, or an empty
    /// string.
    [[nodiscard]] QString error() const;

    /// Starts a session for `messageIds` taken from `fromChatId`. Message ids
    /// are parsed, filtered to positive unique values (first occurrence wins),
    /// and the session is rejected above 100 messages. A previously selected
    /// target, the filter and the error are cleared. A rejected input clears
    /// the session and stores the reason in error().
    Q_INVOKABLE void begin(const QString& fromChatId, const QStringList& messageIds);

    /// Replaces the candidate chats. `ids` and `titles` are paired by position;
    /// non-positive or duplicate ids are dropped (first occurrence wins), and a
    /// missing or blank title falls back to the id so the row stays
    /// identifiable. Targets that are no longer candidates are deselected.
    Q_INVOKABLE void setChats(const QStringList& ids, const QStringList& titles);

    /// Adds the chat to the targets or removes it when it is already selected.
    /// Ids that are not current candidates are ignored. Adding a 21st target is
    /// refused and reported through error().
    Q_INVOKABLE void toggleTarget(const QString& chatId);

    /// Validates the session and, when it is usable, emits forwardRequested()
    /// with the source, the message ids and the target ids. Returns false and
    /// stores the reason in error() when nothing can be forwarded. The session
    /// is kept after a successful confirm() so a caller can retry or inspect
    /// it; begin() starts the next one.
    Q_INVOKABLE bool confirm();

signals:
    /// Emitted whenever any observable state of the facade changed.
    void changed();

    /// Emitted by a successful confirm(). The payload is a snapshot, so a slot
    /// may start a new session without changing the ids it received.
    void forwardRequested(const QString& fromChatId, const QStringList& messageIds,
                          const QStringList& targetIds);

private:
    enum class Issue {
        None,
        NoSource,
        NoMessages,
        NoTargets,
        TooManyMessages,
        TooManyTargets,
    };

    struct ChatEntry {
        qint64 id = 0;
        QString title;
    };

    [[nodiscard]] static Issue issueFor(const QString& validationCode);
    [[nodiscard]] bool matchesFilter(const QString& title) const;
    [[nodiscard]] QVector<qint64> targetIds() const;
    [[nodiscard]] QString errorText() const;

    qint64 m_fromChatId = 0;
    QVector<qint64> m_messageIds;
    QVector<ChatEntry> m_chats;
    QSet<qint64> m_targets;
    QString m_filter;
    Issue m_issue = Issue::None;
};

} // namespace beygram::ui
