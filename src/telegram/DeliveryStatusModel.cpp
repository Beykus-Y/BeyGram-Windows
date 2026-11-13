#include "DeliveryStatusModel.h"

namespace beygram::telegram {

namespace {

int countInState(const QHash<qint64, QPair<DeliveryState, qint64>>& states,
                 DeliveryState state)
{
    int count = 0;
    for (const auto& entry : states) {
        if (entry.first == state)
            ++count;
    }
    return count;
}

bool isReadable(DeliveryState state)
{
    return state == DeliveryState::Pending || state == DeliveryState::Sent;
}

} // namespace

DeliveryStatusModel::DeliveryStatusModel(QObject* parent)
    : QObject(parent)
{
}

void DeliveryStatusModel::setState(qint64 chatId, qint64 messageId,
                                   DeliveryState state)
{
    if (messageId <= 0)
        return;

    // None means "no delivery state", so forget the message instead of storing
    // a value that every reader would have to treat as absent anyway.
    if (state == DeliveryState::None) {
        if (m_states.remove(messageId) > 0)
            emit changed();
        return;
    }

    const auto it = m_states.constFind(messageId);
    if (it != m_states.cend() && it->first == state && it->second == chatId)
        return;

    // A changed chat alone matters: markChatRead must upgrade the message for
    // its current chat, not for the one it was first seen in.
    m_states.insert(messageId, qMakePair(state, chatId));
    emit changed();
}

DeliveryState DeliveryStatusModel::state(qint64 messageId) const
{
    const auto it = m_states.constFind(messageId);
    return it == m_states.cend() ? DeliveryState::None : it->first;
}

QString DeliveryStatusModel::glyph(qint64 messageId) const
{
    switch (state(messageId)) {
    case DeliveryState::Pending:
        return QStringLiteral("…");
    case DeliveryState::Sent:
        return QStringLiteral("✓");
    case DeliveryState::Read:
        return QStringLiteral("✓✓");
    case DeliveryState::Failed:
        return QStringLiteral("!");
    case DeliveryState::None:
        break;
    }
    return {};
}

int DeliveryStatusModel::pendingCount() const
{
    return countInState(m_states, DeliveryState::Pending);
}

int DeliveryStatusModel::failedCount() const
{
    return countInState(m_states, DeliveryState::Failed);
}

void DeliveryStatusModel::markChatRead(qint64 chatId,
                                       const QVector<qint64>& messageIds)
{
    bool upgraded = false;
    for (const qint64 messageId : messageIds) {
        if (messageId <= 0)
            continue;

        auto it = m_states.find(messageId);
        if (it == m_states.end() || it->second != chatId)
            continue;
        if (!isReadable(it->first))
            continue;

        it->first = DeliveryState::Read;
        upgraded = true;
    }

    // The whole batch is one visible change for the chat.
    if (upgraded)
        emit changed();
}

void DeliveryStatusModel::remove(qint64 messageId)
{
    if (m_states.remove(messageId) > 0)
        emit changed();
}

void DeliveryStatusModel::clear()
{
    if (m_states.isEmpty())
        return;
    m_states.clear();
    emit changed();
}

} // namespace beygram::telegram
