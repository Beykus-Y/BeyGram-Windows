#include "telegram/ReplyChainModel.h"

#include <QSignalSpy>
#include <QString>
#include <QVector>
#include <QtTest>

using namespace beygram::telegram;

namespace {

ReplyNode node(qint64 messageId, qint64 parentId = 0,
               const QString& sender = {}, const QString& text = {})
{
    ReplyNode value;
    value.messageId = messageId;
    value.parentId = parentId;
    value.sender = sender;
    value.text = text;
    return value;
}

QVector<qint64> idsOf(const QVector<ReplyNode>& nodes)
{
    QVector<qint64> ids;
    ids.reserve(nodes.size());
    for (const ReplyNode& entry : nodes)
        ids.append(entry.messageId);
    return ids;
}

} // namespace

class ReplyChainModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setNodesBuildsChainFromRootToLeaf();
    void chainOfRootIsSingleNode();
    void dropsNonPositiveIds();
    void laterDuplicateWins();
    void unknownLeafHasEmptyChain();
    void chainStopsAtMissingParent();
    void chainBreaksCycleAtRepeatedId();
    void chainHandlesSelfParent();
    void depthCountsEdges();
    void depthOfUnknownIsMinusOne();
    void childrenAreSortedAscending();
    void childrenIncludeOrphansUnderMissingParent();
    void childrenExcludeDroppedIds();
    void setNodesReplacesPreviousNodes();
    void setNodesEmitsOnlyOnRealChange();
    void setNodesSilentForReorderedNodes();
    void duplicateThatKeepsSameValueIsSilent();
    void setNodesWithOnlyInvalidIdsIsSilentWhenEmpty();
    void clearResetsEverything();
    void clearIsNoOpWhenEmpty();
    void changedTracksOnlyRealChanges();
};

void ReplyChainModelTest::startsEmpty()
{
    ReplyChainModel model;
    QSignalSpy spy(&model, &ReplyChainModel::changed);

    QVERIFY(model.chain(1).isEmpty());
    QCOMPARE(model.depth(1), -1);
    QVERIFY(model.children(0).isEmpty());
    QVERIFY(model.children(1).isEmpty());

    model.clear();
    QCOMPARE(spy.count(), 0);
}

void ReplyChainModelTest::setNodesBuildsChainFromRootToLeaf()
{
    ReplyChainModel model;

    // Deliberately out of order: the parent links, not the input order,
    // decide the chain.
    model.setNodes({
        node(12, 11, QStringLiteral("carol"), QStringLiteral("third")),
        node(10, 0, QStringLiteral("alice"), QStringLiteral("first")),
        node(11, 10, QStringLiteral("bob"), QStringLiteral("second")),
    });

    const QVector<ReplyNode> chain = model.chain(12);
    QCOMPARE(idsOf(chain), (QVector<qint64>{10, 11, 12}));
    QCOMPARE(chain.at(0).sender, QStringLiteral("alice"));
    QCOMPARE(chain.at(1).sender, QStringLiteral("bob"));
    QCOMPARE(chain.at(2).sender, QStringLiteral("carol"));
    QCOMPARE(chain.at(0).text, QStringLiteral("first"));
    QCOMPARE(chain.at(2).text, QStringLiteral("third"));
}

void ReplyChainModelTest::chainOfRootIsSingleNode()
{
    ReplyChainModel model;
    model.setNodes({node(10, 0, QStringLiteral("alice"), QStringLiteral("root"))});

    const QVector<ReplyNode> chain = model.chain(10);
    QCOMPARE(idsOf(chain), (QVector<qint64>{10}));
    QCOMPARE(chain.first().sender, QStringLiteral("alice"));
    QCOMPARE(model.depth(10), 0);
}

void ReplyChainModelTest::dropsNonPositiveIds()
{
    ReplyChainModel model;
    model.setNodes({
        node(0, 5, QStringLiteral("zero")),
        node(-1, 2, QStringLiteral("negative")),
        node(3, 0, QStringLiteral("kept")),
        node(-9, -9, QStringLiteral("negative root")),
    });

    QVERIFY(model.chain(0).isEmpty());
    QVERIFY(model.chain(-1).isEmpty());
    QCOMPARE(model.depth(0), -1);
    QCOMPARE(model.depth(-1), -1);

    QCOMPARE(idsOf(model.chain(3)), (QVector<qint64>{3}));
    QVERIFY(model.children(5).isEmpty());
    QCOMPARE(model.children(0), (QVector<qint64>{3}));
}

void ReplyChainModelTest::laterDuplicateWins()
{
    ReplyChainModel model;
    model.setNodes({
        node(7, 1, QStringLiteral("old sender"), QStringLiteral("old text")),
        node(7, 2, QStringLiteral("new sender"), QStringLiteral("new text")),
    });

    const QVector<ReplyNode> chain = model.chain(7);
    QCOMPARE(chain.size(), 1);
    QCOMPARE(chain.first().messageId, qint64(7));
    QCOMPARE(chain.first().parentId, qint64(2));
    QCOMPARE(chain.first().sender, QStringLiteral("new sender"));
    QCOMPARE(chain.first().text, QStringLiteral("new text"));

    QVERIFY(model.children(1).isEmpty());
    QCOMPARE(model.children(2), (QVector<qint64>{7}));
}

void ReplyChainModelTest::unknownLeafHasEmptyChain()
{
    ReplyChainModel model;
    model.setNodes({node(1, 0), node(2, 1)});

    QVERIFY(model.chain(999).isEmpty());
    QCOMPARE(model.depth(999), -1);
    QVERIFY(model.children(999).isEmpty());
}

void ReplyChainModelTest::chainStopsAtMissingParent()
{
    ReplyChainModel model;

    // 5 points at 99, which was never stored: the chain ends at 5.
    model.setNodes({node(5, 99, QStringLiteral("orphan")), node(4, 5)});

    QCOMPARE(idsOf(model.chain(4)), (QVector<qint64>{5, 4}));
    QCOMPARE(model.depth(4), 1);
    QCOMPARE(model.depth(5), 0);
}

void ReplyChainModelTest::chainBreaksCycleAtRepeatedId()
{
    ReplyChainModel model;

    // 1 -> 2 -> 3 -> 2: the second visit of 2 ends the walk.
    model.setNodes({node(1, 2), node(2, 3), node(3, 2)});

    QCOMPARE(idsOf(model.chain(1)), (QVector<qint64>{3, 2, 1}));
    QCOMPARE(model.depth(1), 2);

    QCOMPARE(idsOf(model.chain(2)), (QVector<qint64>{3, 2}));
    QCOMPARE(model.depth(2), 1);
}

void ReplyChainModelTest::chainHandlesSelfParent()
{
    ReplyChainModel model;
    model.setNodes({node(7, 7, QStringLiteral("self")), node(8, 7)});

    QCOMPARE(idsOf(model.chain(7)), (QVector<qint64>{7}));
    QCOMPARE(model.depth(7), 0);
    QCOMPARE(idsOf(model.chain(8)), (QVector<qint64>{7, 8}));
    QCOMPARE(model.depth(8), 1);
}

void ReplyChainModelTest::depthCountsEdges()
{
    ReplyChainModel model;
    model.setNodes({node(1, 0), node(2, 1), node(3, 2), node(4, 3)});

    QCOMPARE(model.depth(1), 0);
    QCOMPARE(model.depth(2), 1);
    QCOMPARE(model.depth(3), 2);
    QCOMPARE(model.depth(4), 3);

    for (const qint64 id : {qint64(1), qint64(2), qint64(3), qint64(4)})
        QCOMPARE(model.depth(id), static_cast<int>(model.chain(id).size()) - 1);
}

void ReplyChainModelTest::depthOfUnknownIsMinusOne()
{
    ReplyChainModel model;

    QCOMPARE(model.depth(42), -1);

    model.setNodes({node(1, 0), node(2, 1)});
    QCOMPARE(model.depth(42), -1);
    QCOMPARE(model.depth(0), -1);
}

void ReplyChainModelTest::childrenAreSortedAscending()
{
    ReplyChainModel model;

    // Numeric order, not the order of insertion or string order.
    model.setNodes({
        node(12, 10),
        node(10, 0),
        node(2, 100),
        node(10, 100),
        node(1, 100),
        node(13, 10),
        node(11, 10),
    });

    QCOMPARE(model.children(100), (QVector<qint64>{1, 2, 10}));
    QCOMPARE(model.children(10), (QVector<qint64>{11, 12, 13}));
}

void ReplyChainModelTest::childrenIncludeOrphansUnderMissingParent()
{
    ReplyChainModel model;
    model.setNodes({node(21, 50), node(22, 50)});

    QCOMPARE(model.children(50), (QVector<qint64>{21, 22}));
    QVERIFY(model.children(21).isEmpty());
    QCOMPARE(idsOf(model.chain(21)), (QVector<qint64>{21}));
}

void ReplyChainModelTest::childrenExcludeDroppedIds()
{
    ReplyChainModel model;
    model.setNodes({
        node(0, 10),
        node(-2, 10),
        node(11, 10),
        node(12, 10),
    });

    QCOMPARE(model.children(10), (QVector<qint64>{11, 12}));
}

void ReplyChainModelTest::setNodesReplacesPreviousNodes()
{
    ReplyChainModel model;
    model.setNodes({node(1, 0), node(2, 1)});

    model.setNodes({node(3, 0)});

    QVERIFY(model.chain(1).isEmpty());
    QVERIFY(model.chain(2).isEmpty());
    QCOMPARE(model.depth(2), -1);
    QCOMPARE(model.children(0), (QVector<qint64>{3}));
    QVERIFY(model.children(1).isEmpty());
    QCOMPARE(idsOf(model.chain(3)), (QVector<qint64>{3}));
}

void ReplyChainModelTest::setNodesEmitsOnlyOnRealChange()
{
    ReplyChainModel model;
    QSignalSpy spy(&model, &ReplyChainModel::changed);

    model.setNodes({node(1, 0, QStringLiteral("alice"), QStringLiteral("one"))});
    QCOMPARE(spy.count(), 1);

    // Identical input is not a change.
    model.setNodes({node(1, 0, QStringLiteral("alice"), QStringLiteral("one"))});
    QCOMPARE(spy.count(), 1);

    model.setNodes({node(1, 0, QStringLiteral("bob"), QStringLiteral("one"))});
    QCOMPARE(spy.count(), 2);

    model.setNodes({node(1, 0, QStringLiteral("bob"), QStringLiteral("two"))});
    QCOMPARE(spy.count(), 3);

    model.setNodes({node(1, 5, QStringLiteral("bob"), QStringLiteral("two"))});
    QCOMPARE(spy.count(), 4);

    model.setNodes({node(1, 5, QStringLiteral("bob"), QStringLiteral("two")), node(2, 1)});
    QCOMPARE(spy.count(), 5);

    model.setNodes({node(1, 5, QStringLiteral("bob"), QStringLiteral("two"))});
    QCOMPARE(spy.count(), 6);

    model.setNodes({});
    QCOMPARE(spy.count(), 7);

    model.setNodes({});
    QCOMPARE(spy.count(), 7);
}

void ReplyChainModelTest::setNodesSilentForReorderedNodes()
{
    ReplyChainModel model;
    model.setNodes({node(1, 0), node(2, 1), node(3, 2)});
    QSignalSpy spy(&model, &ReplyChainModel::changed);

    model.setNodes({node(3, 2), node(1, 0), node(2, 1)});

    QCOMPARE(spy.count(), 0);
    QCOMPARE(idsOf(model.chain(3)), (QVector<qint64>{1, 2, 3}));
}

void ReplyChainModelTest::duplicateThatKeepsSameValueIsSilent()
{
    ReplyChainModel model;
    model.setNodes({node(1, 0, QStringLiteral("alice"), QStringLiteral("one"))});
    QSignalSpy spy(&model, &ReplyChainModel::changed);

    // The later duplicate wins but carries the value that is already stored.
    model.setNodes({
        node(1, 0, QStringLiteral("stale"), QStringLiteral("stale")),
        node(1, 0, QStringLiteral("alice"), QStringLiteral("one")),
    });

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.chain(1).first().sender, QStringLiteral("alice"));
}

void ReplyChainModelTest::setNodesWithOnlyInvalidIdsIsSilentWhenEmpty()
{
    ReplyChainModel model;
    QSignalSpy spy(&model, &ReplyChainModel::changed);

    model.setNodes({node(0, 1), node(-3, 0)});

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.chain(0).isEmpty());
    QVERIFY(model.chain(-3).isEmpty());
}

void ReplyChainModelTest::clearResetsEverything()
{
    ReplyChainModel model;
    model.setNodes({node(1, 0), node(2, 1), node(3, 1)});
    QSignalSpy spy(&model, &ReplyChainModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.chain(1).isEmpty());
    QVERIFY(model.chain(2).isEmpty());
    QCOMPARE(model.depth(1), -1);
    QVERIFY(model.children(0).isEmpty());
    QVERIFY(model.children(1).isEmpty());

    model.clear();
    QCOMPARE(spy.count(), 1);
}

void ReplyChainModelTest::clearIsNoOpWhenEmpty()
{
    ReplyChainModel model;
    QSignalSpy spy(&model, &ReplyChainModel::changed);

    model.clear();
    model.clear();

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.chain(1).isEmpty());
}

void ReplyChainModelTest::changedTracksOnlyRealChanges()
{
    ReplyChainModel model;
    QSignalSpy spy(&model, &ReplyChainModel::changed);

    model.clear();                                          // already empty
    model.setNodes({});                                     // already empty
    model.setNodes({node(0, 1), node(-1, 0)});              // all dropped
    QCOMPARE(spy.count(), 0);

    model.setNodes({node(1, 0, QStringLiteral("alice"), QStringLiteral("root"))});
    QCOMPARE(spy.count(), 1);

    model.setNodes({node(1, 0, QStringLiteral("alice"), QStringLiteral("root"))});
    QCOMPARE(spy.count(), 1);

    model.setNodes({node(1, 0, QStringLiteral("alice"), QStringLiteral("root")),
                    node(2, 1, QStringLiteral("bob"), QStringLiteral("reply"))});
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.depth(2), 1);

    model.setNodes({node(2, 1, QStringLiteral("bob"), QStringLiteral("reply"))});
    QCOMPARE(spy.count(), 3);
    QCOMPARE(model.depth(1), -1);
    QCOMPARE(model.depth(2), 0);

    model.clear();
    QCOMPARE(spy.count(), 4);
    model.clear();
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(ReplyChainModelTest)
#include "tst_reply_chain_model.moc"
