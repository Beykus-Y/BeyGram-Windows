#pragma once

#include <QHash>
#include <QObject>
#include <QPair>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// Delivery state of an outgoing message, as shown in the message bubble.
enum class DeliveryState {
    None,    ///< No state recorded for the message.
    Pending, ///< Queued locally, not accepted by the server yet.
    Sent,    ///< Accepted by the server, not read by the peer yet.
    Read,    ///< Read by the peer.
    Failed,  ///< Sending failed; the user can retry.
};

/// Tracks the delivery state of outgoing messages and maps it to the glyph the
/// bubble shows.
///
/// States are keyed by message id and remember the chat the message belongs to,
/// so markChatRead can upgrade only the messages of the chat that was actually
/// read. Incoming messages carry no delivery state and are simply absent, which
/// state() reports as DeliveryState::None.
class DeliveryStatusModel final : public QObject {
    Q_OBJECT

public:
    explicit DeliveryStatusModel(QObject* parent = nullptr);

    /// Records the state of messageId, which belongs to chatId. A non-positive
    /// message id is ignored. DeliveryState::None forgets the message.
    /// Re-recording the same state for the same chat is a no-op; any other
    /// transition, including a change of chatId, emits changed().
    void setState(qint64 chatId, qint64 messageId, DeliveryState state);

    /// The recorded state, or DeliveryState::None when the message is unknown.
    [[nodiscard]] DeliveryState state(qint64 messageId) const;

    /// The glyph for messageId: "" for None, "…" for Pending, "✓" for Sent,
    /// "✓✓" for Read and "!" for Failed.
    [[nodiscard]] QString glyph(qint64 messageId) const;

    /// Number of messages still in DeliveryState::Pending.
    [[nodiscard]] int pendingCount() const;

    /// Number of messages in DeliveryState::Failed.
    [[nodiscard]] int failedCount() const;

    /// Marks the listed messages of chatId as read. Only Pending and Sent
    /// messages owned by chatId are upgraded; Read, Failed, unknown and
    /// foreign-chat messages are left alone. Emits changed() once when at least
    /// one message was upgraded.
    void markChatRead(qint64 chatId, const QVector<qint64>& messageIds);

    /// Forgets messageId. Removing an unknown message is a no-op.
    void remove(qint64 messageId);

    /// Forgets every message. Emits changed() only when something was stored.
    void clear();

signals:
    /// Emitted whenever a stored state actually changed.
    void changed();

private:
    QHash<qint64, QPair<DeliveryState, qint64>> m_states; // message -> (state, chatId)
};

} // namespace beygram::telegram
