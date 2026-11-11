#include "telegram/GlobalSearchModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

SearchHit makeHit(qint64 chatId, qint64 messageId, const QString& title = {})
{
    SearchHit hit;
    hit.chatId = chatId;
    hit.messageId = messageId;
    hit.date = 1700000000 + messageId;
    hit.chatTitle = title;
    hit.senderName = QStringLiteral("Sender");
    hit.text = QStringLiteral("body");
    hit.textHtml = QStringLiteral("body");
    return hit;
}

} // namespace

class GlobalSearchModelTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesHitRoles();
    void ignoresInvalidIndex();
    void fallsBackToCachedChatTitle();
    void setChatTitlesAnnouncesChangedRows();
    void appendsAndSkipsDuplicates();
    void reportsHasMoreThroughState();
    void clearsHitsAndState();
};

void GlobalSearchModelTest::exposesHitRoles()
{
    GlobalSearchModel model;
    SearchHit hit;
    hit.chatId = 42;
    hit.messageId = 900;
    hit.date = 1700000123;
    hit.chatTitle = QStringLiteral("Alice");
    hit.senderName = QStringLiteral("Bob");
    hit.text = QStringLiteral("plain");
    hit.textHtml = QStringLiteral("<b>rich</b>");

    model.setHits({hit}, false);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.rowCount(), 1);

    const QModelIndex index = model.index(0);
    QCOMPARE(index.data(GlobalSearchModel::ChatIdRole).toLongLong(), qint64(42));
    QCOMPARE(index.data(GlobalSearchModel::MessageIdRole).toLongLong(), qint64(900));
    QCOMPARE(index.data(GlobalSearchModel::DateRole).toLongLong(), qint64(1700000123));
    QCOMPARE(index.data(GlobalSearchModel::ChatTitleRole).toString(), QStringLiteral("Alice"));
    QCOMPARE(index.data(GlobalSearchModel::SenderNameRole).toString(), QStringLiteral("Bob"));
    QCOMPARE(index.data(GlobalSearchModel::TextRole).toString(), QStringLiteral("plain"));
    QCOMPARE(index.data(GlobalSearchModel::TextHtmlRole).toString(), QStringLiteral("<b>rich</b>"));

    const QHash<int, QByteArray> roles = model.roleNames();
    QCOMPARE(roles.value(GlobalSearchModel::ChatIdRole), QByteArray("chatId"));
    QCOMPARE(roles.value(GlobalSearchModel::MessageIdRole), QByteArray("messageId"));
    QCOMPARE(roles.value(GlobalSearchModel::DateRole), QByteArray("date"));
    QCOMPARE(roles.value(GlobalSearchModel::ChatTitleRole), QByteArray("chatTitle"));
    QCOMPARE(roles.value(GlobalSearchModel::SenderNameRole), QByteArray("senderName"));
    QCOMPARE(roles.value(GlobalSearchModel::TextRole), QByteArray("text"));
    QCOMPARE(roles.value(GlobalSearchModel::TextHtmlRole), QByteArray("textHtml"));
}

void GlobalSearchModelTest::ignoresInvalidIndex()
{
    GlobalSearchModel model;
    QVERIFY(!model.index(0).isValid());
    QVERIFY(!model.index(0).data(GlobalSearchModel::TextRole).isValid());

    model.setHits({makeHit(1, 2)}, false);

    QVERIFY(!model.index(1).isValid());
    QVERIFY(!model.index(-1).isValid());
    QVERIFY(!model.index(0, 0).sibling(5, 0).isValid());
    QVERIFY(!model.index(0).data(Qt::DisplayRole).isValid());
    QCOMPARE(model.rowCount(model.index(0)), 0);
}

void GlobalSearchModelTest::fallsBackToCachedChatTitle()
{
    GlobalSearchModel model;

    SearchHit owned = makeHit(1, 10, QStringLiteral("Own Title"));
    SearchHit joined = makeHit(2, 11);
    SearchHit unknown = makeHit(3, 12);
    model.setHits({owned, joined, unknown}, false);

    // No cache yet: hits without their own title stay empty, never invented.
    QCOMPARE(model.index(0).data(GlobalSearchModel::ChatTitleRole).toString(),
             QStringLiteral("Own Title"));
    QVERIFY(model.index(1).data(GlobalSearchModel::ChatTitleRole).toString().isEmpty());
    QVERIFY(model.index(2).data(GlobalSearchModel::ChatTitleRole).toString().isEmpty());

    model.setChatTitles({{2, QStringLiteral("Joined Chat")},
                         {3, QStringLiteral("")},
                         {1, QStringLiteral("Ignored Cache Title")}});

    QCOMPARE(model.index(0).data(GlobalSearchModel::ChatTitleRole).toString(),
             QStringLiteral("Own Title"));
    QCOMPARE(model.index(1).data(GlobalSearchModel::ChatTitleRole).toString(),
             QStringLiteral("Joined Chat"));
    QVERIFY(model.index(2).data(GlobalSearchModel::ChatTitleRole).toString().isEmpty());
}

void GlobalSearchModelTest::setChatTitlesAnnouncesChangedRows()
{
    GlobalSearchModel model;
    model.setHits({makeHit(1, 10, QStringLiteral("Own Title")), makeHit(2, 11),
                   makeHit(3, 12)},
                  false);

    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);

    model.setChatTitles({{1, QStringLiteral("Cache One")},
                         {2, QStringLiteral("Cache Two")},
                         {3, QStringLiteral("Cache Three")}});

    QCOMPARE(changedSpy.count(), 1);
    const QList<QVariant> args = changedSpy.takeFirst();
    QCOMPARE(args.at(0).toModelIndex().row(), 1);
    QCOMPARE(args.at(1).toModelIndex().row(), 2);

    // A hit that carries its own title is unaffected by the cache.
    QCOMPARE(model.index(0).data(GlobalSearchModel::ChatTitleRole).toString(),
             QStringLiteral("Own Title"));

    // Re-applying the same cache changes nothing, so nothing is announced.
    model.setChatTitles({{1, QStringLiteral("Cache One")},
                         {2, QStringLiteral("Cache Two")},
                         {3, QStringLiteral("Cache Three")}});
    QCOMPARE(changedSpy.count(), 0);
}

void GlobalSearchModelTest::appendsAndSkipsDuplicates()
{
    GlobalSearchModel model;
    QSignalSpy countSpy(&model, &GlobalSearchModel::countChanged);

    model.setHits({makeHit(1, 10)}, true);
    QCOMPARE(model.count(), 1);
    QCOMPARE(countSpy.count(), 1);

    // One hit repeats an existing pair, one repeats a pair inside the batch,
    // and one is genuinely new.
    model.appendHits({makeHit(1, 10), makeHit(2, 20), makeHit(2, 20), makeHit(3, 30)},
                     false);
    QCOMPARE(model.count(), 3);
    QCOMPARE(countSpy.count(), 2);
    QCOMPARE(model.index(1).data(GlobalSearchModel::ChatIdRole).toLongLong(), qint64(2));
    QCOMPARE(model.index(1).data(GlobalSearchModel::MessageIdRole).toLongLong(), qint64(20));
    QCOMPARE(model.index(2).data(GlobalSearchModel::ChatIdRole).toLongLong(), qint64(3));
    QCOMPARE(model.index(2).data(GlobalSearchModel::MessageIdRole).toLongLong(), qint64(30));

    // The same messageId in a different chat is a different hit.
    model.appendHits({makeHit(3, 20)}, false);
    QCOMPARE(model.count(), 4);

    // A page made only of duplicates inserts nothing and emits no count change.
    countSpy.clear();
    model.appendHits({makeHit(1, 10), makeHit(2, 20)}, false);
    QCOMPARE(model.count(), 4);
    QCOMPARE(countSpy.count(), 0);
}

void GlobalSearchModelTest::reportsHasMoreThroughState()
{
    GlobalSearchModel model;
    QSignalSpy stateSpy(&model, &GlobalSearchModel::stateChanged);

    QVERIFY(!model.hasMore());
    model.setHits({makeHit(1, 10)}, true);
    QVERIFY(model.hasMore());
    QCOMPARE(stateSpy.count(), 1);

    // An unchanged flag is not announced again.
    model.setHits({makeHit(1, 10)}, true);
    QCOMPARE(stateSpy.count(), 1);

    model.appendHits({makeHit(2, 20)}, true);
    QCOMPARE(stateSpy.count(), 1);
    QVERIFY(model.hasMore());

    model.appendHits({}, false);
    QCOMPARE(stateSpy.count(), 2);
    QVERIFY(!model.hasMore());
    QCOMPARE(model.count(), 2);
}

void GlobalSearchModelTest::clearsHitsAndState()
{
    GlobalSearchModel model;
    model.setHits({makeHit(1, 10), makeHit(2, 20)}, true);
    model.setChatTitles({{1, QStringLiteral("One")}});

    QSignalSpy countSpy(&model, &GlobalSearchModel::countChanged);
    QSignalSpy stateSpy(&model, &GlobalSearchModel::stateChanged);

    model.clear();
    QCOMPARE(model.count(), 0);
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(!model.hasMore());
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 1);
    QVERIFY(!model.index(0).isValid());

    // Clearing an already empty model is a no-op.
    model.clear();
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(stateSpy.count(), 1);

    // The model is reusable afterwards.
    model.setHits({makeHit(4, 40)}, false);
    QCOMPARE(model.count(), 1);
    QVERIFY(model.index(0).data(GlobalSearchModel::ChatTitleRole).toString().isEmpty());
}

QTEST_GUILESS_MAIN(GlobalSearchModelTest)
#include "tst_global_search_model.moc"
