#pragma once

#include <QObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Tracks the read markers of one chat so the client neither sends a redundant
/// viewMessages request nor moves a marker backwards.
///
/// Telegram keeps two markers per chat: the inbox marker (newest incoming
/// message the user has read) and the outbox marker (newest outgoing message
/// the peer has read). Both are message ids and only ever grow. TDLib reports
/// the committed values through updateChatReadInbox/updateChatReadOutbox, which
/// applyInbox/applyOutbox record; commit records the id this client just marked
/// read.
class ReadMarkerTracker final : public QObject {
    Q_OBJECT

public:
    explicit ReadMarkerTracker(QObject* parent = nullptr);

    /// Drops both markers back to zero, e.g. when the chat is closed and the
    /// next open has to re-evaluate what is unread.
    void reset();

    [[nodiscard]] qint64 inbox() const { return m_inbox; }
    [[nodiscard]] qint64 outbox() const { return m_outbox; }

    void applyInbox(qint64 messageId);
    void applyOutbox(qint64 messageId);

    /// True when the newest visible message is past the committed inbox marker,
    /// i.e. a viewMessages request would actually change the server state.
    [[nodiscard]] bool shouldMark(qint64 visibleNewestId) const;

    /// Records that everything up to messageId was marked read locally.
    void commit(qint64 messageId);

signals:
    /// Emitted whenever a marker actually moved.
    void changed();

private:
    qint64 m_inbox = 0;
    qint64 m_outbox = 0;
};

} // namespace beygram::telegram
