#include "telegram/AlbumGrouper.h"

#include <QtTest>

using namespace beygram::telegram;

namespace {

QVector<qint64> groupedIdsOf(const QVector<AlbumGroup>& groups)
{
    QVector<qint64> ids;
    ids.reserve(groups.size());
    for (const AlbumGroup& group : groups)
        ids.append(group.groupedId);
    return ids;
}

} // namespace

class AlbumGrouperTest final : public QObject {
    Q_OBJECT

private slots:
    void groupsTwoAlbumsAndSingleton();
    void preservesInputOrder();
    void ignoresDuplicateAndNonPositiveIds();
    void handlesEmptyInput();
    void mergesNonContiguousGroupedId();
};

void AlbumGrouperTest::groupsTwoAlbumsAndSingleton()
{
    const QVector<QPair<qint64, qint64>> input{
        {10, 100}, {11, 100}, {12, 0}, {13, 200}, {14, 200}};

    const QVector<AlbumGroup> groups = groupAlbums(input);

    QCOMPARE(groups.size(), 3);
    QCOMPARE(groupedIdsOf(groups), (QVector<qint64>{100, 0, 200}));
    QCOMPARE(groups.at(0).messageIds, (QVector<qint64>{10, 11}));
    QCOMPARE(groups.at(1).messageIds, (QVector<qint64>{12}));
    QCOMPARE(groups.at(2).messageIds, (QVector<qint64>{13, 14}));
}

void AlbumGrouperTest::preservesInputOrder()
{
    // Deliberately not sorted by message id: groups keep first-seen order and
    // ids inside a group keep the order they were supplied in.
    const QVector<QPair<qint64, qint64>> input{
        {30, 5}, {10, 0}, {20, 0}, {40, 5}};

    const QVector<AlbumGroup> groups = groupAlbums(input);

    QCOMPARE(groups.size(), 3);
    QCOMPARE(groupedIdsOf(groups), (QVector<qint64>{5, 0, 0}));
    QCOMPARE(groups.at(0).messageIds, (QVector<qint64>{30, 40}));
    QCOMPARE(groups.at(1).messageIds, (QVector<qint64>{10}));
    QCOMPARE(groups.at(2).messageIds, (QVector<qint64>{20}));
}

void AlbumGrouperTest::ignoresDuplicateAndNonPositiveIds()
{
    const QVector<QPair<qint64, qint64>> input{
        {0, 7}, {-4, 7}, {8, 0}, {8, 9}, {-1, 0}, {9, 0}, {9, 9}};

    const QVector<AlbumGroup> groups = groupAlbums(input);

    QCOMPARE(groups.size(), 2);
    QCOMPARE(groupedIdsOf(groups), (QVector<qint64>{0, 0}));
    // The first occurrence of id 8 wins, so it stays an ungrouped singleton.
    QCOMPARE(groups.at(0).messageIds, (QVector<qint64>{8}));
    QCOMPARE(groups.at(1).messageIds, (QVector<qint64>{9}));
}

void AlbumGrouperTest::handlesEmptyInput()
{
    const QVector<QPair<qint64, qint64>> input;
    QVERIFY(groupAlbums(input).isEmpty());
}

void AlbumGrouperTest::mergesNonContiguousGroupedId()
{
    const QVector<QPair<qint64, qint64>> input{
        {1, 50}, {2, 0}, {3, 50}, {4, 60}, {5, 50}};

    const QVector<AlbumGroup> groups = groupAlbums(input);

    QCOMPARE(groups.size(), 3);
    QCOMPARE(groupedIdsOf(groups), (QVector<qint64>{50, 0, 60}));
    QCOMPARE(groups.at(0).messageIds, (QVector<qint64>{1, 3, 5}));
    QCOMPARE(groups.at(1).messageIds, (QVector<qint64>{2}));
    QCOMPARE(groups.at(2).messageIds, (QVector<qint64>{4}));
}

QTEST_GUILESS_MAIN(AlbumGrouperTest)
#include "tst_album_grouper.moc"
