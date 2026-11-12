#include "MessageSelectionController.h"

#include <algorithm>

namespace beygram::telegram {

MessageSelectionController::MessageSelectionController(QObject* parent)
    : QObject(parent)
{
}

void MessageSelectionController::toggle(qint64 id)
{
    if (id <= 0)
        return;

    const auto it = std::lower_bound(m_ids.begin(), m_ids.end(), id);
    if (it != m_ids.end() && *it == id) {
        m_ids.erase(it);
        emit changed();
        return;
    }

    m_ids.insert(it, id);
    emit changed();
}

void MessageSelectionController::select(qint64 id)
{
    if (id <= 0)
        return;

    const auto it = std::lower_bound(m_ids.begin(), m_ids.end(), id);
    if (it != m_ids.end() && *it == id)
        return;

    m_ids.insert(it, id);
    emit changed();
}

void MessageSelectionController::clear()
{
    if (m_ids.isEmpty())
        return;

    m_ids.clear();
    emit changed();
}

bool MessageSelectionController::active() const
{
    return !m_ids.isEmpty();
}

QVector<qint64> MessageSelectionController::ids() const
{
    return m_ids;
}

int MessageSelectionController::count() const
{
    return m_ids.size();
}

QVector<qint64> MessageSelectionController::idsForForward() const
{
    return m_ids;
}

} // namespace beygram::telegram
