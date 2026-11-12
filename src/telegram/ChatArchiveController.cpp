#include "ChatArchiveController.h"

#include <utility>

namespace beygram::telegram {
namespace {

// Archive membership is order preserving, so normalisation walks the input once
// and keeps the first occurrence of every positive id.
QVector<qint64> normalizedIds(const QVector<qint64>& ids)
{
    QVector<qint64> result;
    result.reserve(ids.size());
    for (const qint64 id : ids) {
        if (id <= 0 || result.contains(id))
            continue;
        result.append(id);
    }
    return result;
}

} // namespace

ChatArchiveController::ChatArchiveController(QObject* parent)
    : QObject(parent)
{
}

void ChatArchiveController::setArchiveOpen(bool open)
{
    if (m_open == open)
        return;
    m_open = open;
    emit changed();
}

void ChatArchiveController::setArchivedIds(QVector<qint64> ids)
{
    QVector<qint64> next = normalizedIds(ids);
    if (next == m_ids)
        return;
    m_ids = std::move(next);
    emit changed();
}

void ChatArchiveController::addArchived(qint64 chatId)
{
    if (chatId <= 0 || m_ids.contains(chatId))
        return;
    m_ids.append(chatId);
    emit changed();
}

void ChatArchiveController::removeArchived(qint64 chatId)
{
    if (chatId <= 0)
        return;
    const qsizetype index = m_ids.indexOf(chatId);
    if (index < 0)
        return;
    m_ids.remove(index);
    emit changed();
}

void ChatArchiveController::clear()
{
    if (m_ids.isEmpty())
        return;
    m_ids.clear();
    emit changed();
}

} // namespace beygram::telegram
