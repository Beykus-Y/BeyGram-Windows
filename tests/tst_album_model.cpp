#include "telegram/AlbumModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

AlbumMediaItem makeItem(qint64 messageId, qint64 groupedId = 0, const QString& caption = {})
{
    AlbumMediaItem item;
    item.messageId = messageId;
    item.groupedId = groupedId;
    item.caption = caption;
    item.thumbnailFileId = static_cast<int>(messageId * 10);
    item.fullFileId = static_cast<int>(messageId * 10 + 1);
    item.video = (messageId % 2) != 0;
    return item;
}

QVector<qint64> groupedIdsOf(const QVector<AlbumEntry>& albums)
{
    QVector<qint64> ids;
    ids.reserve(albums.size());
    for (const AlbumEntry& album : albums)
        ids.append(album.groupedId);
    return ids;
}

QVector<qint64> messageIdsOf(const AlbumEntry& album)
{
    QVector<qint64> ids;
    ids.reserve(album.items.size());
    for (const AlbumMediaItem& item : album.items)
        ids.append(item.messageId);
    return ids;
}

QVector<QVector<qint64>> messageIdsOf(const QVector<AlbumEntry>& albums)
{
    QVector<QVector<qint64>> ids;
    ids.reserve(albums.size());
    for (const AlbumEntry& album : albums)
        ids.append(messageIdsOf(album));
    return ids;
}

} // namespace

class AlbumModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void groupsByGroupedIdAndKeepsSinglesSeparate();
    void preservesItemOrderWithinAlbum();
    void mergesNonContiguousGroupedId();
    void sortsAlbumsByFirstMessageId();
    void dedupesMessageIdsKeepingTheFirstCopy();
    void ignoresNonPositiveMessageIds();
    void keepsPayloadFields();
    void setItemsReplacesPreviousItems();
    void albumCountMatchesAlbums();
    void clearResetsAlbums();
    void changedSignalFiresOnlyOnMutation();
};

void AlbumModelTest::startsEmpty()
{
    AlbumModel model;

    QCOMPARE(model.albumCount(), 0);
    QVERIFY(model.albums().isEmpty());
}

void AlbumModelTest::groupsByGroupedIdAndKeepsSinglesSeparate()
{
    AlbumModel model;
    model.setItems({makeItem(10, 100), makeItem(11, 100), makeItem(12, 0), makeItem(13, 200),
                    makeItem(14, 200), makeItem(15, 0)});

    const QVector<AlbumEntry> albums = model.albums();

    QCOMPARE(model.albumCount(), 4);
    QCOMPARE(albums.size(), 4);
    // Ungrouped messages never share a row, even with each other.
    QCOMPARE(groupedIdsOf(albums), (QVector<qint64>{100, 0, 200, 0}));
    QCOMPARE(messageIdsOf(albums),
             (QVector<QVector<qint64>>{{10, 11}, {12}, {13, 14}, {15}}));
}

void AlbumModelTest::preservesItemOrderWithinAlbum()
{
    AlbumModel model;
    // Supplied out of id order: the album keeps the supplied order.
    model.setItems({makeItem(30, 5), makeItem(20, 5), makeItem(10, 5)});

    const QVector<AlbumEntry> albums = model.albums();

    QCOMPARE(albums.size(), 1);
    QCOMPARE(albums.constFirst().groupedId, qint64(5));
    QCOMPARE(messageIdsOf(albums.constFirst()), (QVector<qint64>{30, 20, 10}));
}

void AlbumModelTest::mergesNonContiguousGroupedId()
{
    AlbumModel model;
    // A grouped_id that reappears after an unrelated singleton still joins its
    // earlier album instead of opening a second one.
    model.setItems({makeItem(1, 50), makeItem(2, 0), makeItem(3, 50), makeItem(4, 60),
                    makeItem(5, 50)});

    const QVector<AlbumEntry> albums = model.albums();

    QCOMPARE(albums.size(), 3);
    QCOMPARE(groupedIdsOf(albums), (QVector<qint64>{50, 0, 60}));
    QCOMPARE(messageIdsOf(albums), (QVector<QVector<qint64>>{{1, 3, 5}, {2}, {4}}));
}

void AlbumModelTest::sortsAlbumsByFirstMessageId()
{
    AlbumModel model;
    // Album 900 opens with 40 (its smallest id is 25, so sorting by the first
    // item rather than the smallest id is what puts it after the singleton 30).
    model.setItems({makeItem(40, 900), makeItem(25, 900), makeItem(30, 0)});

    const QVector<AlbumEntry> albums = model.albums();

    QCOMPARE(groupedIdsOf(albums), (QVector<qint64>{0, 900}));
    QCOMPARE(messageIdsOf(albums), (QVector<QVector<qint64>>{{30}, {40, 25}}));
}

void AlbumModelTest::dedupesMessageIdsKeepingTheFirstCopy()
{
    AlbumModel model;
    // The first copy of 8 is the ungrouped one, so the later grouped copy is
    // dropped and 8 stays a singleton.
    model.setItems({makeItem(8, 0, QStringLiteral("first")), makeItem(8, 9),
                    makeItem(9, 9), makeItem(9, 9)});

    const QVector<AlbumEntry> albums = model.albums();

    QCOMPARE(albums.size(), 2);
    QCOMPARE(groupedIdsOf(albums), (QVector<qint64>{0, 9}));
    QCOMPARE(messageIdsOf(albums), (QVector<QVector<qint64>>{{8}, {9}}));
    QCOMPARE(albums.constFirst().items.constFirst().caption, QStringLiteral("first"));
}

void AlbumModelTest::ignoresNonPositiveMessageIds()
{
    AlbumModel model;
    model.setItems({makeItem(0, 7), makeItem(-4, 7), makeItem(8, 0)});

    const QVector<AlbumEntry> albums = model.albums();

    QCOMPARE(albums.size(), 1);
    QCOMPARE(groupedIdsOf(albums), (QVector<qint64>{0}));
    QCOMPARE(messageIdsOf(albums.constFirst()), (QVector<qint64>{8}));
}

void AlbumModelTest::keepsPayloadFields()
{
    AlbumModel model;
    model.setItems({makeItem(1, 77, QStringLiteral("caption"))});

    const QVector<AlbumEntry> albums = model.albums();
    QCOMPARE(albums.size(), 1);

    const AlbumMediaItem item = albums.constFirst().items.constFirst();
    QCOMPARE(item.messageId, qint64(1));
    QCOMPARE(item.groupedId, qint64(77));
    QCOMPARE(item.caption, QStringLiteral("caption"));
    QCOMPARE(item.thumbnailFileId, 10);
    QCOMPARE(item.fullFileId, 11);
    QCOMPARE(item.video, true);
}

void AlbumModelTest::setItemsReplacesPreviousItems()
{
    AlbumModel model;
    model.setItems({makeItem(1, 100), makeItem(2, 100)});
    model.setItems({makeItem(3, 0)});

    const QVector<AlbumEntry> albums = model.albums();

    QCOMPARE(model.albumCount(), 1);
    QCOMPARE(groupedIdsOf(albums), (QVector<qint64>{0}));
    QCOMPARE(messageIdsOf(albums.constFirst()), (QVector<qint64>{3}));
}

void AlbumModelTest::albumCountMatchesAlbums()
{
    AlbumModel model;
    model.setItems({makeItem(1, 100), makeItem(2, 0), makeItem(3, 100), makeItem(4, 200)});

    QCOMPARE(model.albumCount(), model.albums().size());
    QCOMPARE(model.albumCount(), 3);
}

void AlbumModelTest::clearResetsAlbums()
{
    AlbumModel model;
    model.setItems({makeItem(1, 100), makeItem(2, 0)});
    QCOMPARE(model.albumCount(), 2);

    model.clear();

    QCOMPARE(model.albumCount(), 0);
    QVERIFY(model.albums().isEmpty());
}

void AlbumModelTest::changedSignalFiresOnlyOnMutation()
{
    AlbumModel model;
    QSignalSpy spy(&model, &AlbumModel::changed);
    QVERIFY(spy.isValid());

    model.setItems({makeItem(1, 0), makeItem(2, 0)});
    QCOMPARE(spy.count(), 1);

    // Same items again, including a duplicate that de-duplicates to the same
    // list: nothing changed, so no signal.
    model.setItems({makeItem(1, 0), makeItem(2, 0)});
    QCOMPARE(spy.count(), 1);
    model.setItems({makeItem(1, 0), makeItem(1, 0), makeItem(2, 0)});
    QCOMPARE(spy.count(), 1);

    // A payload edit is a real change.
    model.setItems({makeItem(1, 0), makeItem(2, 0, QStringLiteral("edited"))});
    QCOMPARE(spy.count(), 2);

    model.clear();
    QCOMPARE(spy.count(), 3);

    model.clear();
    QCOMPARE(spy.count(), 3);

    model.setItems({});
    QCOMPARE(spy.count(), 3);
}

QTEST_GUILESS_MAIN(AlbumModelTest)
#include "tst_album_model.moc"
