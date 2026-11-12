#include "PollVoteModel.h"

#include <algorithm>

namespace beygram::telegram {

PollVoteModel::PollVoteModel(QObject* parent) : QObject(parent)
{
}

void PollVoteModel::setPoll(qint64 chatId, qint64 messageId, int optionCount, bool multiple)
{
    // An unparsed poll has no usable ids or no answers; storing it would make
    // every index out of range and hide the poll the user is looking at.
    if (chatId <= 0 || messageId <= 0 || optionCount <= 0)
        return;

    // A fresh answer may carry a different vote, so an accepted setPoll() always
    // drops the selection; only a call that changes nothing at all is silent.
    if (chatId == m_chatId && messageId == m_messageId && optionCount == m_count
        && multiple == m_multiple && m_selected.isEmpty()) {
        return;
    }

    m_chatId = chatId;
    m_messageId = messageId;
    m_count = optionCount;
    m_multiple = multiple;
    m_selected.clear();

    emit changed();
}

bool PollVoteModel::active() const
{
    return m_chatId > 0 && m_messageId > 0 && m_count > 0;
}

qint64 PollVoteModel::chatId() const
{
    return m_chatId;
}

qint64 PollVoteModel::messageId() const
{
    return m_messageId;
}

int PollVoteModel::optionCount() const
{
    return m_count;
}

bool PollVoteModel::multiple() const
{
    return m_multiple;
}

bool PollVoteModel::toggle(int optionIndex)
{
    // Out of range covers the inactive model too: without a poll, m_count is 0
    // and no index can be valid.
    if (optionIndex < 0 || optionIndex >= m_count)
        return false;

    // The selection stays sorted, so the position of the index (or the place it
    // would take) is one lookup.
    const auto position = std::lower_bound(m_selected.begin(), m_selected.end(), optionIndex);
    const bool wasSelected = position != m_selected.end() && *position == optionIndex;

    if (wasSelected) {
        m_selected.erase(position);
        emit changed();
        return true;
    }

    if (m_multiple)
        m_selected.insert(position, optionIndex);
    else
        m_selected = QVector<int>{optionIndex};

    emit changed();
    return true;
}

QVector<int> PollVoteModel::selected() const
{
    return m_selected;
}

bool PollVoteModel::hasSelection() const
{
    return !m_selected.isEmpty();
}

void PollVoteModel::clearSelection()
{
    if (m_selected.isEmpty())
        return;

    m_selected.clear();
    emit changed();
}

void PollVoteModel::reset()
{
    if (m_chatId == 0 && m_messageId == 0 && m_count == 0 && !m_multiple
        && m_selected.isEmpty()) {
        return;
    }

    m_chatId = 0;
    m_messageId = 0;
    m_count = 0;
    m_multiple = false;
    m_selected.clear();

    emit changed();
}

} // namespace beygram::telegram
