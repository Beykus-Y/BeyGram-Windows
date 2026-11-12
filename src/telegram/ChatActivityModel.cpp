#include "ChatActivityModel.h"

#include <algorithm>

namespace beygram::telegram {
namespace {

int nonNegative(int value)
{
    return std::max(0, value);
}

qint64 nonNegativeId(qint64 value)
{
    return std::max<qint64>(0, value);
}

// A record is kept only while it has something a row could paint. Counts are
// already clamped, so zero means "nobody is doing this".
bool isEmptyRecord(const ChatActivity& record)
{
    return record.typingCount == 0 && record.uploadingCount == 0
        && record.recordingCount == 0 && !record.hasDraft;
}

bool sameRecord(const ChatActivity& left, const ChatActivity& right)
{
    return left.chatId == right.chatId
        && left.typingCount == right.typingCount
        && left.uploadingCount == right.uploadingCount
        && left.recordingCount == right.recordingCount
        && left.hasDraft == right.hasDraft
        && left.draftText == right.draftText
        && left.draftReplyTo == right.draftReplyTo;
}

// Writes a normalised record and reports whether the store really moved.
// Empty records are erased instead of stored, and identical ones are skipped,
// so changed() only follows a visible difference.
bool storeRecord(QHash<qint64, ChatActivity>& records, const ChatActivity& record)
{
    const auto existing = records.constFind(record.chatId);
    const bool stored = existing != records.cend();

    if (isEmptyRecord(record)) {
        if (!stored)
            return false;
        records.erase(records.find(record.chatId));
        return true;
    }

    if (stored && sameRecord(*existing, record))
        return false;

    records.insert(record.chatId, record);
    return true;
}

ChatActivity normalise(ChatActivity record)
{
    record.typingCount = nonNegative(record.typingCount);
    record.uploadingCount = nonNegative(record.uploadingCount);
    record.recordingCount = nonNegative(record.recordingCount);

    // The text decides whether a draft exists, so a blank draft can never be
    // reported as present and user text is never dropped because a caller left
    // hasDraft at its default. A stored record always keeps the two in step.
    record.draftText = record.draftText.trimmed();
    record.hasDraft = !record.draftText.isEmpty();
    if (record.hasDraft) {
        record.draftReplyTo = nonNegativeId(record.draftReplyTo);
    } else {
        record.draftText.clear();
        record.draftReplyTo = 0;
    }
    return record;
}

// Shared body of the three per-kind setters: copy the record (or start an empty
// one for this chat), replace one count and store the result.
bool setCount(QHash<qint64, ChatActivity>& records, qint64 chatId,
              int ChatActivity::*field, int count)
{
    if (chatId <= 0)
        return false;

    ChatActivity record = records.value(chatId);
    record.chatId = chatId;
    record.*field = nonNegative(count);
    return storeRecord(records, record);
}

QString countLabel(const QString& label, int count)
{
    if (count <= 1)
        return label;
    // "+N" names the other members, matching "typing… +2" for three typists.
    return QStringLiteral("%1 +%2").arg(label).arg(count - 1);
}

} // namespace

ChatActivityModel::ChatActivityModel(QObject* parent)
    : QObject(parent)
{
}

void ChatActivityModel::setActivity(const ChatActivity& activity)
{
    if (activity.chatId <= 0)
        return;

    if (storeRecord(m_activities, normalise(activity)))
        emit changed();
}

ChatActivity ChatActivityModel::activity(qint64 chatId) const
{
    const auto it = m_activities.constFind(chatId);
    if (it != m_activities.cend())
        return *it;

    ChatActivity empty;
    empty.chatId = chatId;
    return empty;
}

void ChatActivityModel::setTyping(qint64 chatId, int count)
{
    if (setCount(m_activities, chatId, &ChatActivity::typingCount, count))
        emit changed();
}

void ChatActivityModel::setUploading(qint64 chatId, int count)
{
    if (setCount(m_activities, chatId, &ChatActivity::uploadingCount, count))
        emit changed();
}

void ChatActivityModel::setRecording(qint64 chatId, int count)
{
    if (setCount(m_activities, chatId, &ChatActivity::recordingCount, count))
        emit changed();
}

void ChatActivityModel::setDraft(qint64 chatId, const QString& text, qint64 replyTo)
{
    if (chatId <= 0)
        return;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        clearDraft(chatId);
        return;
    }

    ChatActivity record = m_activities.value(chatId);
    record.chatId = chatId;
    record.hasDraft = true;
    record.draftText = trimmed;
    record.draftReplyTo = nonNegativeId(replyTo);
    if (storeRecord(m_activities, record))
        emit changed();
}

void ChatActivityModel::clearDraft(qint64 chatId)
{
    if (chatId <= 0)
        return;

    const auto it = m_activities.find(chatId);
    if (it == m_activities.end() || !it->hasDraft)
        return;

    it->hasDraft = false;
    it->draftText.clear();
    it->draftReplyTo = 0;
    if (isEmptyRecord(*it))
        m_activities.erase(it);
    emit changed();
}

bool ChatActivityModel::hasActivity(qint64 chatId) const
{
    return !isEmptyRecord(activity(chatId));
}

QString ChatActivityModel::statusText(qint64 chatId) const
{
    const ChatActivity record = activity(chatId);

    // Recording wins over uploading, and uploading over typing: a chat shows
    // the strongest ongoing action, not a combination of them.
    if (record.recordingCount > 0)
        return countLabel(QStringLiteral("recording…"), record.recordingCount);
    if (record.uploadingCount > 0)
        return countLabel(QStringLiteral("uploading…"), record.uploadingCount);
    if (record.typingCount > 0)
        return countLabel(QStringLiteral("typing…"), record.typingCount);
    return QString();
}

void ChatActivityModel::removeChat(qint64 chatId)
{
    if (chatId <= 0)
        return;

    if (m_activities.remove(chatId) > 0)
        emit changed();
}

void ChatActivityModel::clear()
{
    if (m_activities.isEmpty())
        return;

    m_activities.clear();
    emit changed();
}

} // namespace beygram::telegram
