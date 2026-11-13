#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One message in a reply chain. `messageId` identifies the message,
/// `parentId` is the message it replies to (0 or any unknown id means the
/// chain ends there), and `sender`/`text` are the already formatted strings
/// the transport received from TDLib.
struct ReplyNode {
    qint64 messageId = 0;
    qint64 parentId = 0;
    QString sender;
    QString text;
};

/// Stores the messages that take part in reply chains and resolves a chain
/// from a leaf message back to its root.
///
/// The model holds no transport state: setNodes() replaces the whole node
/// set, and the getters derive chains, depths and children from it. Nodes
/// with messageId <= 0 are dropped, and a later duplicate of the same id
/// replaces the earlier one, so a caller can pass TDLib updates in arrival
/// order.
///
/// chain() walks parentId until it reaches a node that is not stored or an id
/// that was already visited. The walk is reversed, so the result runs from
/// root to leaf. A cycle is therefore cut at the first repeated id instead of
/// looping forever, and an unknown leaf yields an empty chain.
///
/// depth() is the number of edges in the chain, i.e. chain().size() - 1, and
/// is -1 for an unknown message. children() returns the ids of the nodes
/// whose parentId matches, in ascending id order.
///
/// Every mutator emits changed() only when the stored state really moved, so
/// a view can repaint unconditionally and repeated updates stay silent.
class ReplyChainModel final : public QObject {
    Q_OBJECT

public:
    explicit ReplyChainModel(QObject* parent = nullptr);

    /// Replaces the stored nodes. Non-positive ids are dropped and a later
    /// duplicate id wins over an earlier one. Emits changed() only when the
    /// resulting set differs from the current one, so reordering the same
    /// nodes is silent.
    void setNodes(QVector<ReplyNode> nodes);

    /// The chain ending at `leafId`, ordered root -> leaf. Empty when
    /// `leafId` is not stored. A missing parent or a repeated id ends the
    /// walk, so a cycle is cut rather than followed forever.
    [[nodiscard]] QVector<ReplyNode> chain(qint64 leafId) const;

    /// Number of edges from the root to `messageId`, i.e. chain().size() - 1.
    /// -1 when the message is unknown.
    [[nodiscard]] int depth(qint64 messageId) const;

    /// Ids of the stored nodes whose parentId is `parentId`, ascending. A
    /// parent does not have to be stored itself, so orphans are still listed
    /// under the id they point at.
    [[nodiscard]] QVector<qint64> children(qint64 parentId) const;

    /// Drops every node. No-op, including no signal, when already empty.
    void clear();

signals:
    /// Emitted whenever the stored node set really changed. Ignored nodes,
    /// duplicates that resolve to the same value, and no-op calls stay silent.
    void changed();

private:
    QHash<qint64, ReplyNode> m_nodes;
};

} // namespace beygram::telegram
