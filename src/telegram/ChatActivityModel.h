#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QtGlobal>

namespace beygram::telegram {

/// What one chat currently shows under its title, plus the composer draft of
/// that chat. It mirrors the Android model: ChatActivity.kt names the outgoing
/// actions (typing, uploading, recording) and MessageDraft.kt holds the unsent
/// text with its reply target.
///
/// The struct is a plain snapshot. The model normalises it on the way in:
/// counts are never negative, draft text is trimmed, and a stored record keeps
/// hasDraft and draftText in step, so hasDraft is true exactly while the
/// trimmed draft text is non-empty.
struct ChatActivity {
    qint64 chatId = 0;
    int typingCount = 0;
    int uploadingCount = 0;
    int recordingCount = 0;
    bool hasDraft = false;
    QString draftText;
    qint64 draftReplyTo = 0;
};

/// Tracks the transient activity and the draft of every chat, keyed by chat id.
///
/// TDLib delivers typing, uploading and recording as transient events; the
/// transport feeds them here through the per-kind setters, and a view repaints
/// on changed(). Drafts are stored in the same record so a chat row can show
/// both the status line and a draft marker from one lookup.
///
/// Only chats with something to show are stored. When the last count drops to
/// zero and no draft is left, the record is dropped, so the hash never fills
/// with empty entries.
///
/// Every mutator emits changed() only when the stored record really moved, so
/// repeated heartbeats and no-op clears stay silent. Calls with a chat id that
/// is not positive are ignored, because they cannot name a chat.
///
/// The model keeps no clock, sends nothing to TDLib and never logs: it is a
/// pure state holder, which keeps tests deterministic.
class ChatActivityModel final : public QObject {
    Q_OBJECT

public:
    explicit ChatActivityModel(QObject* parent = nullptr);

    /// Replaces the whole record of `activity.chatId` with `activity`. Negative
    /// counts are clamped to zero and the draft text is trimmed. That text
    /// decides whether a draft exists: a blank text clears the draft and its
    /// reply target, while non-empty text is kept even if `hasDraft` was left
    /// false, so user text is never dropped. An activity with nothing to show
    /// removes the chat. Ignored when the chat id is not positive.
    void setActivity(const ChatActivity& activity);

    /// The record of `chatId`, or an empty record whose chatId is `chatId` when
    /// the chat has no activity. This mirrors DraftStore::draft(), so callers
    /// can read a chat without checking for existence first.
    [[nodiscard]] ChatActivity activity(qint64 chatId) const;

    /// Sets how many members are typing, uploading or recording in `chatId`.
    /// Negative counts are clamped to zero, which clears that kind of activity.
    /// Zeroing the last count of a chat that has no draft drops its record.
    void setTyping(qint64 chatId, int count);
    void setUploading(qint64 chatId, int count);
    void setRecording(qint64 chatId, int count);

    /// Stores a composer draft for `chatId`. The text is trimmed; a trimmed
    /// empty text clears the draft instead, exactly like DraftStore. A negative
    /// reply target means "not a reply" and is stored as zero.
    void setDraft(qint64 chatId, const QString& text, qint64 replyTo = 0);

    /// Drops the draft of `chatId` and keeps its counts. Silent when the chat
    /// has no draft; drops the record when nothing else is left.
    void clearDraft(qint64 chatId);

    /// True while the chat has a count above zero or a draft, i.e. while a row
    /// could paint anything from this model.
    [[nodiscard]] bool hasActivity(qint64 chatId) const;

    /// The line shown under the chat title: "recording…", "uploading…" or
    /// "typing…", in that priority order, with " +N" appended when more than
    /// one member is doing it (N is the number of others). Empty when the chat
    /// has no count above zero. Drafts are not part of the status line.
    [[nodiscard]] QString statusText(qint64 chatId) const;

    /// Forgets a chat entirely, e.g. after it was deleted or left. Silent when
    /// the chat has no record.
    void removeChat(qint64 chatId);

    /// Forgets every chat. Silent when the model is already empty.
    void clear();

signals:
    /// Emitted whenever a stored record really changed, i.e. on additions,
    /// edits and removals, but never for an ignored or identical call.
    void changed();

private:
    QHash<qint64, ChatActivity> m_activities;
};

} // namespace beygram::telegram
