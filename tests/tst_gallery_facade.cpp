#include "ui/GalleryFacade.h"

#include <QSignalSpy>
#include <QtTest>

#include <initializer_list>
#include <limits>

using namespace beygram::ui;

namespace {

/// QML hands the facade string ids, so the tests build the same shape.
QStringList strIds(std::initializer_list<qint64> values)
{
    QStringList ids;
    ids.reserve(static_cast<qsizetype>(values.size()));
    for (const qint64 value : values)
        ids.append(QString::number(value));
    return ids;
}

} // namespace

class GalleryFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsClosed();
    void openGalleryOpensAtCurrentItem();
    void openGalleryDropsUnparsableAndNonPositiveIds();
    void openGalleryDedupesKeepingFirstCopy();
    void openGalleryFallsBackToFirstForUnknownCurrent();
    void openGalleryRejectsEmptyGallery();
    void openGalleryRejectedEmptyKeepsOpenGallery();
    void closeClearsGallery();
    void closeIsSilentWhileClosed();
    void nextAdvancesAndStopsAtLastItem();
    void previousStepsBackAndStopsAtFirstItem();
    void navigationIsIgnoredWhileClosed();
    void singleItemGalleryCannotNavigate();
    void navigationResetsZoom();
    void setZoomClampsToRange();
    void setZoomIgnoresNonFiniteValues();
    void resetZoomRestoresDefault();
    void currentChangedFiresWhenIndexMoves();
    void changedFiresOnlyOnRealChanges();
    void setAlbumsGroupsMessages();
    void setAlbumsKeepsWithinAlbumOrder();
    void setAlbumsIgnoresMismatchedLists();
    void setAlbumsClearsWithEmptyLists();
    void setAlbumsDropsIdsThatCannotAddressAMessage();
    void setAlbumsLeavesTheGalleryAlone();
    void albumMessageIdsIgnoresUnknownIds();
};

void GalleryFacadeTest::startsClosed()
{
    const GalleryFacade facade;

    QVERIFY(!facade.open());
    QVERIFY(facade.currentId().isEmpty());
    QCOMPARE(facade.index(), -1);
    QCOMPARE(facade.count(), 0);
    QCOMPARE(facade.zoom(), 1.0);
    QVERIFY(facade.albumMessageIds(QStringLiteral("1")).isEmpty());
}

void GalleryFacadeTest::openGalleryOpensAtCurrentItem()
{
    GalleryFacade facade;

    facade.openGallery(strIds({101, 102, 103}), QStringLiteral("102"));

    QVERIFY(facade.open());
    QCOMPARE(facade.count(), 3);
    QCOMPARE(facade.index(), 1);
    QCOMPARE(facade.currentId(), QStringLiteral("102"));
    QCOMPARE(facade.zoom(), 1.0);
}

void GalleryFacadeTest::openGalleryDropsUnparsableAndNonPositiveIds()
{
    GalleryFacade facade;

    // "-4" and "0" cannot address a message, "abc" is not a number at all.
    facade.openGallery(
        QStringList{QStringLiteral("-4"), QStringLiteral("abc"), QStringLiteral("5"),
                    QStringLiteral("0"), QStringLiteral("12")},
        QStringLiteral("12"));

    QVERIFY(facade.open());
    QCOMPARE(facade.count(), 2);
    QCOMPARE(facade.index(), 1);
    QCOMPARE(facade.currentId(), QStringLiteral("12"));
}

void GalleryFacadeTest::openGalleryDedupesKeepingFirstCopy()
{
    GalleryFacade facade;

    facade.openGallery(strIds({7, 3, 7, 3, 9}), QStringLiteral("3"));

    QCOMPARE(facade.count(), 3);
    QCOMPARE(facade.index(), 1);
    QCOMPARE(facade.currentId(), QStringLiteral("3"));
}

void GalleryFacadeTest::openGalleryFallsBackToFirstForUnknownCurrent()
{
    GalleryFacade facade;

    // An id that is not part of the gallery opens it at the first item instead
    // of leaving the viewer blank.
    facade.openGallery(strIds({4, 8, 15}), QStringLiteral("99"));
    QCOMPARE(facade.index(), 0);
    QCOMPARE(facade.currentId(), QStringLiteral("4"));

    // An id that cannot address a message falls back the same way.
    facade.openGallery(strIds({4, 8, 15}), QString());
    QCOMPARE(facade.index(), 0);
    QCOMPARE(facade.currentId(), QStringLiteral("4"));
}

void GalleryFacadeTest::openGalleryRejectsEmptyGallery()
{
    GalleryFacade facade;
    QSignalSpy spy(&facade, &GalleryFacade::changed);
    QVERIFY(spy.isValid());

    facade.openGallery(QStringList{QStringLiteral("abc"), QString()}, QStringLiteral("5"));

    QVERIFY(!facade.open());
    QCOMPARE(facade.count(), 0);
    QCOMPARE(facade.index(), -1);
    QCOMPARE(spy.count(), 0);
}

void GalleryFacadeTest::openGalleryRejectedEmptyKeepsOpenGallery()
{
    GalleryFacade facade;
    facade.openGallery(strIds({21, 22}), QStringLiteral("21"));
    QSignalSpy spy(&facade, &GalleryFacade::changed);

    // Only ids the cleaning drops, so there is nothing to show.
    facade.openGallery(strIds({0, -3}), QStringLiteral("22"));

    QVERIFY(facade.open());
    QCOMPARE(facade.count(), 2);
    QCOMPARE(facade.index(), 0);
    QCOMPARE(facade.currentId(), QStringLiteral("21"));
    QCOMPARE(spy.count(), 0);
}

void GalleryFacadeTest::closeClearsGallery()
{
    GalleryFacade facade;
    facade.openGallery(strIds({80, 81}), QStringLiteral("81"));
    facade.setZoom(2.0);

    facade.close();

    QVERIFY(!facade.open());
    QCOMPARE(facade.count(), 0);
    QCOMPARE(facade.index(), -1);
    QVERIFY(facade.currentId().isEmpty());
    QCOMPARE(facade.zoom(), 1.0);
}

void GalleryFacadeTest::closeIsSilentWhileClosed()
{
    GalleryFacade facade;
    QSignalSpy spy(&facade, &GalleryFacade::changed);
    QSignalSpy currentSpy(&facade, &GalleryFacade::currentChanged);

    facade.close();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(currentSpy.count(), 0);
}

void GalleryFacadeTest::nextAdvancesAndStopsAtLastItem()
{
    GalleryFacade facade;
    facade.openGallery(strIds({40, 41, 42}), QStringLiteral("40"));

    facade.next();
    QCOMPARE(facade.index(), 1);
    QCOMPARE(facade.currentId(), QStringLiteral("41"));

    facade.next();
    QCOMPARE(facade.index(), 2);
    QCOMPARE(facade.currentId(), QStringLiteral("42"));

    // A gallery has a last item, so navigation never wraps.
    facade.next();
    QCOMPARE(facade.index(), 2);
    QCOMPARE(facade.currentId(), QStringLiteral("42"));
}

void GalleryFacadeTest::previousStepsBackAndStopsAtFirstItem()
{
    GalleryFacade facade;
    facade.openGallery(strIds({50, 51, 52}), QStringLiteral("52"));

    facade.previous();
    QCOMPARE(facade.index(), 1);
    QCOMPARE(facade.currentId(), QStringLiteral("51"));

    facade.previous();
    QCOMPARE(facade.index(), 0);
    QCOMPARE(facade.currentId(), QStringLiteral("50"));

    facade.previous();
    QCOMPARE(facade.index(), 0);
    QCOMPARE(facade.currentId(), QStringLiteral("50"));
}

void GalleryFacadeTest::navigationIsIgnoredWhileClosed()
{
    GalleryFacade facade;

    facade.next();
    facade.previous();

    QVERIFY(!facade.open());
    QCOMPARE(facade.index(), -1);
    QVERIFY(facade.currentId().isEmpty());
}

void GalleryFacadeTest::singleItemGalleryCannotNavigate()
{
    GalleryFacade facade;
    facade.openGallery(strIds({70}), QStringLiteral("70"));

    facade.next();
    facade.previous();

    QCOMPARE(facade.count(), 1);
    QCOMPARE(facade.index(), 0);
    QCOMPARE(facade.currentId(), QStringLiteral("70"));
}

void GalleryFacadeTest::navigationResetsZoom()
{
    GalleryFacade facade;
    facade.openGallery(strIds({60, 61}), QStringLiteral("60"));

    facade.setZoom(3.0);
    facade.next();

    // The zoom belongs to the item that was on screen.
    QCOMPARE(facade.zoom(), 1.0);

    facade.setZoom(4.0);
    facade.previous();
    QCOMPARE(facade.zoom(), 1.0);
}

void GalleryFacadeTest::setZoomClampsToRange()
{
    GalleryFacade facade;

    facade.setZoom(2.5);
    QCOMPARE(facade.zoom(), 2.5);

    facade.setZoom(0.25);
    QCOMPARE(facade.zoom(), 1.0);

    facade.setZoom(42.0);
    QCOMPARE(facade.zoom(), 5.0);

    facade.setZoom(-3.0);
    QCOMPARE(facade.zoom(), 1.0);
}

void GalleryFacadeTest::setZoomIgnoresNonFiniteValues()
{
    GalleryFacade facade;
    facade.setZoom(3.0);

    facade.setZoom(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(facade.zoom(), 3.0);

    facade.setZoom(std::numeric_limits<double>::infinity());
    QCOMPARE(facade.zoom(), 3.0);

    facade.setZoom(-std::numeric_limits<double>::infinity());
    QCOMPARE(facade.zoom(), 3.0);
}

void GalleryFacadeTest::resetZoomRestoresDefault()
{
    GalleryFacade facade;
    facade.openGallery(strIds({1, 2}), QStringLiteral("1"));
    facade.setZoom(4.5);

    facade.resetZoom();

    QCOMPARE(facade.zoom(), 1.0);
    QCOMPARE(facade.currentId(), QStringLiteral("1"));
}

void GalleryFacadeTest::currentChangedFiresWhenIndexMoves()
{
    GalleryFacade facade;
    QSignalSpy spy(&facade, &GalleryFacade::currentChanged);
    QVERIFY(spy.isValid());

    facade.openGallery(strIds({10, 11}), QStringLiteral("10"));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("10"));

    facade.next();
    QCOMPARE(spy.count(), 2);
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("11"));

    // No successor, so the item on screen did not move.
    facade.next();
    QCOMPARE(spy.count(), 2);

    facade.previous();
    QCOMPARE(spy.count(), 3);
    QCOMPARE(spy.at(2).at(0).toString(), QStringLiteral("10"));

    // Reopening the same gallery at the same item keeps the same item on screen.
    facade.openGallery(strIds({10, 11}), QStringLiteral("10"));
    QCOMPARE(spy.count(), 3);

    // Closing moves to no item at all.
    facade.close();
    QCOMPARE(spy.count(), 4);
    QVERIFY(spy.at(3).at(0).toString().isEmpty());
}

void GalleryFacadeTest::changedFiresOnlyOnRealChanges()
{
    GalleryFacade facade;
    QSignalSpy spy(&facade, &GalleryFacade::changed);
    QVERIFY(spy.isValid());

    // Nothing is open, and the defaults are already in place.
    facade.close();
    facade.next();
    facade.previous();
    facade.resetZoom();
    facade.setZoom(1.0);
    QCOMPARE(spy.count(), 0);

    facade.openGallery(strIds({5, 6, 7}), QStringLiteral("5"));
    QCOMPARE(spy.count(), 1);

    // Same gallery, same item, untouched zoom: nothing moved.
    facade.openGallery(strIds({5, 6, 7}), QStringLiteral("5"));
    QCOMPARE(spy.count(), 1);

    facade.openGallery(strIds({5, 6, 7}), QStringLiteral("6"));
    QCOMPARE(spy.count(), 2);

    facade.next();
    QCOMPARE(spy.count(), 3);

    // The last item has no successor.
    facade.next();
    QCOMPARE(spy.count(), 3);

    facade.previous();
    QCOMPARE(spy.count(), 4);

    facade.setZoom(2.0);
    QCOMPARE(spy.count(), 5);

    facade.setZoom(2.0);
    QCOMPARE(spy.count(), 5);

    facade.setZoom(99.0);
    QCOMPARE(spy.count(), 6);

    facade.resetZoom();
    QCOMPARE(spy.count(), 7);

    facade.resetZoom();
    QCOMPARE(spy.count(), 7);

    facade.close();
    QCOMPARE(spy.count(), 8);

    facade.close();
    QCOMPARE(spy.count(), 8);
}

void GalleryFacadeTest::setAlbumsGroupsMessages()
{
    GalleryFacade facade;

    facade.setAlbums(strIds({100, 100, 0, 200, 200}), strIds({10, 11, 12, 13, 14}));

    QCOMPARE(facade.albumMessageIds(QStringLiteral("10")), strIds({10, 11}));
    QCOMPARE(facade.albumMessageIds(QStringLiteral("11")), strIds({10, 11}));
    // Ungrouped messages never share an album, even with each other.
    QCOMPARE(facade.albumMessageIds(QStringLiteral("12")), strIds({12}));
    QCOMPARE(facade.albumMessageIds(QStringLiteral("13")), strIds({13, 14}));
    QCOMPARE(facade.albumMessageIds(QStringLiteral("14")), strIds({13, 14}));
}

void GalleryFacadeTest::setAlbumsKeepsWithinAlbumOrder()
{
    GalleryFacade facade;

    // The album keeps the supplied order, and a grouped_id that reappears after
    // an unrelated message still joins its earlier album.
    facade.setAlbums(strIds({7, 0, 7}), strIds({30, 20, 10}));

    QCOMPARE(facade.albumMessageIds(QStringLiteral("30")), strIds({30, 10}));
    QCOMPARE(facade.albumMessageIds(QStringLiteral("10")), strIds({30, 10}));
    QCOMPARE(facade.albumMessageIds(QStringLiteral("20")), strIds({20}));
}

void GalleryFacadeTest::setAlbumsIgnoresMismatchedLists()
{
    GalleryFacade facade;
    facade.setAlbums(strIds({5, 5}), strIds({1, 2}));

    QSignalSpy spy(&facade, &GalleryFacade::changed);

    // A shorter grouping list describes inconsistent data, so the previous
    // grouping stays instead of being half replaced.
    facade.setAlbums(strIds({9}), strIds({1, 2}));
    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.albumMessageIds(QStringLiteral("1")), strIds({1, 2}));

    facade.setAlbums(strIds({9, 9, 9}), strIds({1}));
    QCOMPARE(spy.count(), 0);
    QCOMPARE(facade.albumMessageIds(QStringLiteral("1")), strIds({1, 2}));
}

void GalleryFacadeTest::setAlbumsClearsWithEmptyLists()
{
    GalleryFacade facade;
    facade.setAlbums(strIds({5}), strIds({1}));
    QCOMPARE(facade.albumMessageIds(QStringLiteral("1")), strIds({1}));

    QSignalSpy spy(&facade, &GalleryFacade::changed);

    facade.setAlbums({}, {});
    QCOMPARE(spy.count(), 1);
    QVERIFY(facade.albumMessageIds(QStringLiteral("1")).isEmpty());

    // Already empty, so a second clear changes nothing.
    facade.setAlbums({}, {});
    QCOMPARE(spy.count(), 1);
}

void GalleryFacadeTest::setAlbumsDropsIdsThatCannotAddressAMessage()
{
    GalleryFacade facade;

    facade.setAlbums(strIds({5, 5, 0}),
                     QStringList{QStringLiteral("0"), QStringLiteral("7"),
                                 QStringLiteral("8")});

    // "0" cannot address a message, so it never enters the grouping.
    QVERIFY(facade.albumMessageIds(QStringLiteral("0")).isEmpty());
    QCOMPARE(facade.albumMessageIds(QStringLiteral("7")), strIds({7}));
    QCOMPARE(facade.albumMessageIds(QStringLiteral("8")), strIds({8}));
}

void GalleryFacadeTest::setAlbumsLeavesTheGalleryAlone()
{
    GalleryFacade facade;
    facade.openGallery(strIds({40, 41, 42}), QStringLiteral("41"));

    QSignalSpy currentSpy(&facade, &GalleryFacade::currentChanged);

    // Albums belong to the message list, not to the gallery that is on screen.
    facade.setAlbums(strIds({9, 9}), strIds({40, 42}));

    QVERIFY(facade.open());
    QCOMPARE(facade.count(), 3);
    QCOMPARE(facade.index(), 1);
    QCOMPARE(facade.currentId(), QStringLiteral("41"));
    QCOMPARE(currentSpy.count(), 0);

    facade.next();
    QCOMPARE(facade.currentId(), QStringLiteral("42"));
    QCOMPARE(currentSpy.count(), 1);
}

void GalleryFacadeTest::albumMessageIdsIgnoresUnknownIds()
{
    GalleryFacade facade;
    facade.setAlbums(strIds({3}), strIds({1}));

    QVERIFY(facade.albumMessageIds(QStringLiteral("999")).isEmpty());
    QVERIFY(facade.albumMessageIds(QStringLiteral("abc")).isEmpty());
    QVERIFY(facade.albumMessageIds(QString()).isEmpty());
    QVERIFY(facade.albumMessageIds(QStringLiteral("-1")).isEmpty());
}

QTEST_GUILESS_MAIN(GalleryFacadeTest)
#include "tst_gallery_facade.moc"
