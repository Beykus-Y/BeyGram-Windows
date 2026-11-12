#include "MessageStatisticsModel.h"

#include <algorithm>

namespace beygram::telegram {

MessageStatisticsModel::MessageStatisticsModel(QObject* parent) : QObject(parent)
{
}

void MessageStatisticsModel::setMessage(qint64 chatId, qint64 messageId)
{
    if (chatId <= 0 || messageId <= 0)
        return;

    if (m_chatId == chatId && m_messageId == messageId && m_views == 0 && m_forwards == 0
        && m_reactions == 0 && !m_loading) {
        return;
    }

    m_chatId = chatId;
    m_messageId = messageId;
    m_views = 0;
    m_forwards = 0;
    m_reactions = 0;
    m_loading = false;
    emit changed();
}

bool MessageStatisticsModel::active() const
{
    return m_chatId > 0 && m_messageId > 0;
}

qint64 MessageStatisticsModel::chatId() const
{
    return m_chatId;
}

qint64 MessageStatisticsModel::messageId() const
{
    return m_messageId;
}

void MessageStatisticsModel::setCounts(int views, int forwards, int reactions)
{
    if (!active())
        return;

    const int clampedViews = std::max(0, views);
    const int clampedForwards = std::max(0, forwards);
    const int clampedReactions = std::max(0, reactions);

    if (m_views == clampedViews && m_forwards == clampedForwards
        && m_reactions == clampedReactions) {
        return;
    }

    m_views = clampedViews;
    m_forwards = clampedForwards;
    m_reactions = clampedReactions;
    emit changed();
}

int MessageStatisticsModel::views() const
{
    return m_views;
}

int MessageStatisticsModel::forwards() const
{
    return m_forwards;
}

int MessageStatisticsModel::reactions() const
{
    return m_reactions;
}

void MessageStatisticsModel::setLoading(bool loading)
{
    if (!active())
        return;

    if (m_loading == loading)
        return;

    m_loading = loading;
    emit changed();
}

bool MessageStatisticsModel::loading() const
{
    return m_loading;
}

void MessageStatisticsModel::clear()
{
    if (!active())
        return;

    m_chatId = 0;
    m_messageId = 0;
    m_views = 0;
    m_forwards = 0;
    m_reactions = 0;
    m_loading = false;
    emit changed();
}

} // namespace beygram::telegram
