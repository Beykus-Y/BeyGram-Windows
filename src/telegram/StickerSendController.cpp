#include "StickerSendController.h"

#include "StickerSend.h"

#include <utility>

namespace beygram::telegram {

StickerSendController::StickerSendController(QObject* parent)
    : QObject(parent)
{
}

void StickerSendController::setTarget(qint64 chatId, qint64 replyTo)
{
    if (chatId == 0)
        return;

    const qint64 normalizedReply = replyTo > 0 ? replyTo : 0;
    if (m_chatId == chatId && m_replyTo == normalizedReply)
        return;

    m_chatId = chatId;
    m_replyTo = normalizedReply;
    emit changed();
}

bool StickerSendController::canSend(qint64 stickerId, int fileId) const
{
    return m_chatId != 0 && stickerId > 0 && fileId >= 0;
}

QJsonObject StickerSendController::request(qint64 stickerId, int fileId) const
{
    if (!canSend(stickerId, fileId))
        return {};
    return sendStickerRequest(m_chatId, stickerId, fileId, m_replyTo);
}

void StickerSendController::addRecent(qint64 stickerId, int maxRecent)
{
    if (stickerId <= 0 || maxRecent <= 0)
        return;

    QVector<qint64> next;
    next.reserve(qMin<qsizetype>(m_recent.size() + 1, maxRecent));
    next.append(stickerId);
    for (const qint64 id : m_recent) {
        if (id == stickerId)
            continue;
        if (next.size() >= maxRecent)
            break;
        next.append(id);
    }

    if (next == m_recent)
        return;

    m_recent = std::move(next);
    emit changed();
}

QVector<qint64> StickerSendController::recent() const
{
    return m_recent;
}

} // namespace beygram::telegram
