#include "ThreadMessageModel.h"

#include <QHash>
#include <QSet>

#include <algorithm>
#include <tuple>
#include <utility>

namespace beygram::telegram {
namespace {

/// Thread comments are ordered by id: Telegram assigns increasing ids, so this
/// is both chronological and stable when a page repeats a boundary message.
bool olderFirst(const MessageData& left, const MessageData& right)
{
    return left.id < right.id;
}

/// Every field of MessageData takes part, so a media, reaction or edit update
/// is a real change while re-parsing an identical page stays silent.
bool sameMessage(const MessageData& left, const MessageData& right)
{
    return std::tie(left.id, left.chatId, left.date, left.groupedId, left.senderUserId,
                    left.replyToMessageId, left.replyChatId, left.text, left.textHtml,
                    left.replyPreview, left.reactionsJson, left.linkPreviewUrl,
                    left.linkPreviewTitle, left.linkPreviewDescription, left.linkPreviewSite,
                    left.linkPreviewFileId, left.linkPreviewVideo, left.pollQuestion,
                    left.pollOptionsJson, left.pollTotalVoterCount, left.pollClosed,
                    left.pollQuiz, left.pollMultiple, left.senderName, left.contentType,
                    left.outgoing, left.edited, left.failed, left.pending, left.mediaFileId,
                    left.mediaFullFileId, left.mediaWidth, left.mediaHeight, left.mediaReady,
                    left.mediaRevision)
        == std::tie(right.id, right.chatId, right.date, right.groupedId, right.senderUserId,
                    right.replyToMessageId, right.replyChatId, right.text, right.textHtml,
                    right.replyPreview, right.reactionsJson, right.linkPreviewUrl,
                    right.linkPreviewTitle, right.linkPreviewDescription,
                    right.linkPreviewSite, right.linkPreviewFileId, right.linkPreviewVideo,
                    right.pollQuestion, right.pollOptionsJson, right.pollTotalVoterCount,
                    right.pollClosed, right.pollQuiz, right.pollMultiple, right.senderName,
                    right.contentType, right.outgoing, right.edited, right.failed,
                    right.pending, right.mediaFileId, right.mediaFullFileId, right.mediaWidth,
                    right.mediaHeight, right.mediaReady, right.mediaRevision);
}

bool sameMessages(const QVector<MessageData>& left, const QVector<MessageData>& right)
{
    if (left.size() != right.size())
        return false;

    for (qsizetype row = 0; row < left.size(); ++row) {
        if (!sameMessage(left.at(row), right.at(row)))
            return false;
    }
    return true;
}

} // namespace

ThreadMessageModel::ThreadMessageModel(QObject* parent) : QObject(parent)
{
}

void ThreadMessageModel::setThread(qint64 chatId, qint64 messageId, int replyCount)
{
    if (chatId <= 0 || messageId <= 0)
        return;

    const int clampedCount = std::max(0, replyCount);
    const bool sameThread = m_chatId == chatId && m_messageId == messageId;
    if (sameThread && m_replyCount == clampedCount)
        return;

    if (!sameThread)
        m_messages.clear();

    m_chatId = chatId;
    m_messageId = messageId;
    m_replyCount = clampedCount;
    emit changed();
}

void ThreadMessageModel::setMessages(QVector<MessageData> messages)
{
    QVector<MessageData> next = dedupe(std::move(messages));
    sortMessages(next);

    if (sameMessages(m_messages, next))
        return;

    m_messages = std::move(next);
    emit changed();
}

void ThreadMessageModel::appendOlder(QVector<MessageData> messages)
{
    const QVector<MessageData> page = dedupe(std::move(messages));
    if (page.isEmpty())
        return;

    QSet<qint64> loaded;
    loaded.reserve(m_messages.size());
    for (const MessageData& message : m_messages)
        loaded.insert(message.id);

    // A loaded row wins over the page copy: an edit applied while the page was
    // in flight must not be rolled back by the older snapshot.
    bool added = false;
    for (const MessageData& message : page) {
        if (loaded.contains(message.id))
            continue;
        loaded.insert(message.id);
        m_messages.append(message);
        added = true;
    }

    if (!added)
        return;

    sortMessages(m_messages);
    emit changed();
}

QVector<MessageData> ThreadMessageModel::messages() const
{
    return m_messages;
}

int ThreadMessageModel::count() const
{
    return static_cast<int>(m_messages.size());
}

int ThreadMessageModel::replyCount() const
{
    return m_replyCount;
}

bool ThreadMessageModel::hasThread() const
{
    return m_chatId > 0 && m_messageId > 0;
}

qint64 ThreadMessageModel::chatId() const
{
    return m_chatId;
}

qint64 ThreadMessageModel::messageId() const
{
    return m_messageId;
}

void ThreadMessageModel::clear()
{
    if (m_chatId == 0 && m_messageId == 0 && m_replyCount == 0 && m_messages.isEmpty())
        return;

    m_chatId = 0;
    m_messageId = 0;
    m_replyCount = 0;
    m_messages.clear();
    emit changed();
}

void ThreadMessageModel::sortMessages(QVector<MessageData>& messages)
{
    std::sort(messages.begin(), messages.end(), olderFirst);
}

QVector<MessageData> ThreadMessageModel::dedupe(QVector<MessageData> messages)
{
    QHash<qint64, int> positions;
    QVector<MessageData> unique;
    unique.reserve(messages.size());

    for (const MessageData& message : messages) {
        if (message.id <= 0)
            continue;

        const auto it = positions.constFind(message.id);
        if (it != positions.cend()) {
            // The later copy is the live one: it may carry an edit, a reaction
            // or a media update that the earlier snapshot did not have.
            unique[*it] = message;
            continue;
        }

        positions.insert(message.id, static_cast<int>(unique.size()));
        unique.append(message);
    }

    return unique;
}

} // namespace beygram::telegram
