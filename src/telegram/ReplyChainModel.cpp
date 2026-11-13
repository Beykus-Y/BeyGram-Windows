#include "ReplyChainModel.h"

#include <QSet>

#include <algorithm>
#include <utility>

namespace beygram::telegram {
namespace {

bool sameNode(const ReplyNode& left, const ReplyNode& right)
{
    return left.messageId == right.messageId
        && left.parentId == right.parentId
        && left.sender == right.sender
        && left.text == right.text;
}

bool sameNodes(const QHash<qint64, ReplyNode>& left,
               const QHash<qint64, ReplyNode>& right)
{
    if (left.size() != right.size())
        return false;

    for (auto it = left.cbegin(); it != left.cend(); ++it) {
        const auto other = right.constFind(it.key());
        if (other == right.cend() || !sameNode(it.value(), other.value()))
            return false;
    }

    return true;
}

} // namespace

ReplyChainModel::ReplyChainModel(QObject* parent)
    : QObject(parent)
{
}

void ReplyChainModel::setNodes(QVector<ReplyNode> nodes)
{
    QHash<qint64, ReplyNode> next;
    next.reserve(nodes.size());

    // Later duplicates overwrite earlier ones, which is exactly what the
    // arrival order of TDLib updates asks for.
    for (const ReplyNode& node : nodes) {
        if (node.messageId <= 0)
            continue;
        next.insert(node.messageId, node);
    }

    // Reordering the same nodes, or duplicates that resolve to the same
    // value, leave the stored state untouched and must not wake the view.
    if (sameNodes(m_nodes, next))
        return;

    m_nodes = std::move(next);
    emit changed();
}

QVector<ReplyNode> ReplyChainModel::chain(qint64 leafId) const
{
    if (!m_nodes.contains(leafId))
        return {};

    QVector<ReplyNode> path;
    QSet<qint64> visited;
    qint64 currentId = leafId;

    // Walk from the leaf towards the root. A node that is not stored ends
    // the chain, and a repeated id means a cycle: stop before appending it
    // twice so the chain stays finite.
    while (true) {
        const auto it = m_nodes.constFind(currentId);
        if (it == m_nodes.cend() || visited.contains(currentId))
            break;

        visited.insert(currentId);
        path.append(it.value());
        currentId = it.value().parentId;
    }

    std::reverse(path.begin(), path.end());
    return path;
}

int ReplyChainModel::depth(qint64 messageId) const
{
    return static_cast<int>(chain(messageId).size()) - 1;
}

QVector<qint64> ReplyChainModel::children(qint64 parentId) const
{
    QVector<qint64> ids;
    ids.reserve(m_nodes.size());

    for (auto it = m_nodes.cbegin(); it != m_nodes.cend(); ++it) {
        if (it.value().parentId == parentId)
            ids.append(it.key());
    }

    std::sort(ids.begin(), ids.end());
    return ids;
}

void ReplyChainModel::clear()
{
    if (m_nodes.isEmpty())
        return;

    m_nodes.clear();
    emit changed();
}

} // namespace beygram::telegram
