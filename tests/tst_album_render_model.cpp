#include "telegram/AlbumRenderModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

MessageData message(qint64 id, qint64 groupedId = 0)
{
    MessageData data;
    data.id = id;
    data.chatId = 42;
    data.groupedId = groupedId;
    return data;
}

QVector<qint64> anchorsOf(const QVector<AlbumRenderRow>& rows)
{
    QVector<qint64> anchors;
    anchors.reserve(rows.size());
    for (const AlbumRenderRow& row : rows)
        anchors.append(row.anchorMessageId);
    return anchors;
}

QVector<QVector<qint64>> messageIdsOf(const QVector<AlbumRenderRow>& rows)
{
    QVector<QVector<qint64>> ids;
    ids.reserve(rows.size());
    for (const AlbumRenderRow& row : rows)
        ids.append(row.messageIds);
    return ids;
}

QVector<bool> albumFlagsOf(const QVector<AlbumRenderRow>& rows)
{
    QVector<bool> flags;
    flags.reserve(rows.size());
    for (const AlbumRenderRow& row : rows)
        flags.append(row.isAlbum);
    return flags;
}

} // namespace

class AlbumRenderModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void groupsConsecutiveGroupedMessages();
    void keepsUngroupedMessagesSeparate();
    void preservesDisplayOrder();
    void splitsWhenGroupedIdChanges();
    void reopensGroupedIdAsNewRow();
    void loneGroupedMessageIsStillAnAlbum();
    void dropsNonPositiveIds();
    void droppedIdsDoNotBreakAdjacency();
    void rowForMessageFindsAlbumAndSingle();
    void rowForMessageReturnsDefaultWhenAbsent();
    void setMessagesReplacesPreviousRows();
    void albumCountCountsOnlyAlbums();
    void clearResetsRows();
    void changedSignalFiresOnlyOnRealChanges();
};

void AlbumRenderModelTest::startsEmpty()
{
    const AlbumRenderModel model;

    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.albumCount(), 0);
    QVERIFY(model.rows().isEmpty());
}

void AlbumRenderModelTest::groupsConsecutiveGroupedMessages()
{
    AlbumRenderModel model;
    model.setMessages({message(10, 100), message(11, 100), message(12, 100)});

    const QVector<AlbumRenderRow> rows = model.rows();

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.albumCount(), 1);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.constFirst().anchorMessageId, qint64(10));
    QCOMPARE(rows.constFirst().messageIds, (QVector<qint64>{10, 11, 12}));
    QVERIFY(rows.constFirst().isAlbum);
}

void AlbumRenderModelTest::keepsUngroupedMessagesSeparate()
{
    AlbumRenderModel model;
    // A zero grouped_id never joins anything, not even another zero.
    model.setMessages({message(1), message(2), message(3)});

    const QVector<AlbumRenderRow> rows = model.rows();

    QCOMPARE(rows.size(), 3);
    QCOMPARE(anchorsOf(rows), (QVector<qint64>{1, 2, 3}));
    QCOMPARE(messageIdsOf(rows), (QVector<QVector<qint64>>{{1}, {2}, {3}}));
    QCOMPARE(albumFlagsOf(rows), (QVector<bool>{false, false, false}));
    QCOMPARE(model.albumCount(), 0);
}

void AlbumRenderModelTest::preservesDisplayOrder()
{
    AlbumRenderModel model;
    // Supplied newest-first: the model must not sort, and the album keeps the
    // supplied order with its first supplied message as the anchor.
    model.setMessages({message(30, 7), message(20, 7), message(10)});

    const QVector<AlbumRenderRow> rows = model.rows();

    QCOMPARE(rows.size(), 2);
    QCOMPARE(anchorsOf(rows), (QVector<qint64>{30, 10}));
    QCOMPARE(messageIdsOf(rows), (QVector<QVector<qint64>>{{30, 20}, {10}}));
}

void AlbumRenderModelTest::splitsWhenGroupedIdChanges()
{
    AlbumRenderModel model;
    model.setMessages(
        {message(1, 5), message(2, 5), message(3, 6), message(4, 6), message(5)});

    const QVector<AlbumRenderRow> rows = model.rows();

    QCOMPARE(rows.size(), 3);
    QCOMPARE(anchorsOf(rows), (QVector<qint64>{1, 3, 5}));
    QCOMPARE(messageIdsOf(rows), (QVector<QVector<qint64>>{{1, 2}, {3, 4}, {5}}));
    QCOMPARE(albumFlagsOf(rows), (QVector<bool>{true, true, false}));
    QCOMPARE(model.albumCount(), 2);
}

void AlbumRenderModelTest::reopensGroupedIdAsNewRow()
{
    AlbumRenderModel model;
    // The grouped_id reappears after an unrelated message, so it does not join
    // its earlier album and opens a second album row instead.
    model.setMessages({message(1, 50), message(2, 50), message(3), message(4, 50)});

    const QVector<AlbumRenderRow> rows = model.rows();

    QCOMPARE(rows.size(), 3);
    QCOMPARE(anchorsOf(rows), (QVector<qint64>{1, 3, 4}));
    QCOMPARE(messageIdsOf(rows), (QVector<QVector<qint64>>{{1, 2}, {3}, {4}}));
    QCOMPARE(albumFlagsOf(rows), (QVector<bool>{true, false, true}));
    QCOMPARE(model.albumCount(), 2);
}

void AlbumRenderModelTest::loneGroupedMessageIsStillAnAlbum()
{
    AlbumRenderModel model;
    // Telegram can leave a single visible message with a grouped_id behind;
    // it is still an album member, just the only one currently loaded.
    model.setMessages({message(9, 77)});

    const QVector<AlbumRenderRow> rows = model.rows();

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.constFirst().anchorMessageId, qint64(9));
    QCOMPARE(rows.constFirst().messageIds, (QVector<qint64>{9}));
    QVERIFY(rows.constFirst().isAlbum);
    QCOMPARE(model.albumCount(), 1);
}

void AlbumRenderModelTest::dropsNonPositiveIds()
{
    AlbumRenderModel model;
    model.setMessages({message(0, 7), message(-4, 7), message(8)});

    const QVector<AlbumRenderRow> rows = model.rows();

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.constFirst().anchorMessageId, qint64(8));
    QCOMPARE(rows.constFirst().messageIds, (QVector<qint64>{8}));
    QVERIFY(!rows.constFirst().isAlbum);
}

void AlbumRenderModelTest::droppedIdsDoNotBreakAdjacency()
{
    AlbumRenderModel model;
    // The invalid message cannot be rendered, so 1 and 2 are adjacent and
    // share the album they were sent in.
    model.setMessages({message(1, 5), message(0, 5), message(-1, 9), message(2, 5)});

    const QVector<AlbumRenderRow> rows = model.rows();

    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.constFirst().anchorMessageId, qint64(1));
    QCOMPARE(rows.constFirst().messageIds, (QVector<qint64>{1, 2}));
    QVERIFY(rows.constFirst().isAlbum);
}

void AlbumRenderModelTest::rowForMessageFindsAlbumAndSingle()
{
    AlbumRenderModel model;
    model.setMessages({message(1, 100), message(2, 100), message(3)});

    const AlbumRenderRow albumRow = model.rowForMessage(2);
    QCOMPARE(albumRow.anchorMessageId, qint64(1));
    QCOMPARE(albumRow.messageIds, (QVector<qint64>{1, 2}));
    QVERIFY(albumRow.isAlbum);

    const AlbumRenderRow firstOfAlbum = model.rowForMessage(1);
    QCOMPARE(firstOfAlbum.anchorMessageId, qint64(1));
    QCOMPARE(firstOfAlbum.messageIds, (QVector<qint64>{1, 2}));

    const AlbumRenderRow singleRow = model.rowForMessage(3);
    QCOMPARE(singleRow.anchorMessageId, qint64(3));
    QCOMPARE(singleRow.messageIds, (QVector<qint64>{3}));
    QVERIFY(!singleRow.isAlbum);
}

void AlbumRenderModelTest::rowForMessageReturnsDefaultWhenAbsent()
{
    AlbumRenderModel model;
    model.setMessages({message(1, 100), message(2, 100)});

    for (const qint64 id : {qint64(99), qint64(0), qint64(-3)}) {
        const AlbumRenderRow row = model.rowForMessage(id);
        QCOMPARE(row.anchorMessageId, qint64(0));
        QVERIFY(row.messageIds.isEmpty());
        QVERIFY(!row.isAlbum);
    }
}

void AlbumRenderModelTest::setMessagesReplacesPreviousRows()
{
    AlbumRenderModel model;
    model.setMessages({message(1, 100), message(2, 100)});
    QCOMPARE(model.albumCount(), 1);

    model.setMessages({message(3), message(4)});

    const QVector<AlbumRenderRow> rows = model.rows();

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.albumCount(), 0);
    QCOMPARE(messageIdsOf(rows), (QVector<QVector<qint64>>{{3}, {4}}));
}

void AlbumRenderModelTest::albumCountCountsOnlyAlbums()
{
    AlbumRenderModel model;
    model.setMessages(
        {message(1, 5), message(2, 5), message(3), message(4, 6), message(5, 6)});

    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.albumCount(), 2);
    QCOMPARE(model.albumCount(), static_cast<int>(albumFlagsOf(model.rows()).count(true)));
}

void AlbumRenderModelTest::clearResetsRows()
{
    AlbumRenderModel model;
    model.setMessages({message(1, 100), message(2, 100)});
    QCOMPARE(model.rowCount(), 1);

    model.clear();

    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.albumCount(), 0);
    QVERIFY(model.rows().isEmpty());
}

void AlbumRenderModelTest::changedSignalFiresOnlyOnRealChanges()
{
    AlbumRenderModel model;
    QSignalSpy spy(&model, &AlbumRenderModel::changed);
    QVERIFY(spy.isValid());

    model.setMessages({});
    QCOMPARE(spy.count(), 0);

    model.setMessages({message(1, 7), message(2, 7)});
    QCOMPARE(spy.count(), 1);

    // Identical rows stay silent.
    model.setMessages({message(1, 7), message(2, 7)});
    QCOMPARE(spy.count(), 1);

    // A payload edit that does not change the rows stays silent.
    MessageData edited = message(1, 7);
    edited.text = QStringLiteral("edited");
    model.setMessages({edited, message(2, 7)});
    QCOMPARE(spy.count(), 1);

    // An invalid message adds no row, so nothing changed.
    model.setMessages({message(1, 7), message(2, 7), message(0, 9)});
    QCOMPARE(spy.count(), 1);

    // Breaking the adjacency turns the album into two singles.
    model.setMessages({message(1, 7), message(2)});
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 3);

    // Already empty: clearing again is a no-op.
    model.clear();
    QCOMPARE(spy.count(), 3);

    model.setMessages({});
    QCOMPARE(spy.count(), 3);
}

QTEST_GUILESS_MAIN(AlbumRenderModelTest)
#include "tst_album_render_model.moc"
