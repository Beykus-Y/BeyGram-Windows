#include "telegram/MediaGalleryController.h"

#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

class MediaGalleryControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInactive();
    void openActivatesGalleryWithCurrentItem();
    void openDropsNonPositiveIds();
    void openDedupesKeepingFirstCopy();
    void openFallsBackToFirstForUnknownCurrent();
    void openRejectsEmptyGallery();
    void openRejectedEmptyKeepsOpenGallery();
    void openResetsZoomAndPan();
    void messageIdsReturnsACopy();
    void nextAdvancesThroughGallery();
    void nextAtEndFailsWithoutWrapping();
    void previousStepsBack();
    void previousAtStartFailsWithoutWrapping();
    void navigationResetsZoomAndPan();
    void singleItemGalleryCannotNavigate();
    void navigationFailsWhileInactive();
    void setZoomClampsToRange();
    void setZoomIgnoresNonFiniteValues();
    void setPanClampsEachAxis();
    void setPanIgnoresNonFiniteValues();
    void resetZoomRestoresDefaults();
    void closeClearsGallery();
    void closeIsSilentWhileInactive();
    void changedFiresOnlyOnRealChanges();
};

void MediaGalleryControllerTest::startsInactive()
{
    const MediaGalleryController controller;

    QVERIFY(!controller.active());
    QVERIFY(controller.messageIds().isEmpty());
    QCOMPARE(controller.currentIndex(), -1);
    QCOMPARE(controller.currentMessageId(), qint64(0));
    QCOMPARE(controller.zoom(), 1.0);
    QCOMPARE(controller.panX(), 0.0);
    QCOMPARE(controller.panY(), 0.0);
}

void MediaGalleryControllerTest::openActivatesGalleryWithCurrentItem()
{
    MediaGalleryController controller;

    controller.open({101, 102, 103}, 102);

    QVERIFY(controller.active());
    QCOMPARE(controller.messageIds(), (QVector<qint64>{101, 102, 103}));
    QCOMPARE(controller.currentIndex(), 1);
    QCOMPARE(controller.currentMessageId(), qint64(102));
}

void MediaGalleryControllerTest::openDropsNonPositiveIds()
{
    MediaGalleryController controller;

    controller.open({-4, 5, 0, 12}, 12);

    QCOMPARE(controller.messageIds(), (QVector<qint64>{5, 12}));
    QCOMPARE(controller.currentIndex(), 1);
    QCOMPARE(controller.currentMessageId(), qint64(12));
}

void MediaGalleryControllerTest::openDedupesKeepingFirstCopy()
{
    MediaGalleryController controller;

    controller.open({7, 3, 7, 3, 9}, 3);

    // The first copy of each id defines its place in the gallery order.
    QCOMPARE(controller.messageIds(), (QVector<qint64>{7, 3, 9}));
    QCOMPARE(controller.currentIndex(), 1);
    QCOMPARE(controller.currentMessageId(), qint64(3));
}

void MediaGalleryControllerTest::openFallsBackToFirstForUnknownCurrent()
{
    MediaGalleryController controller;

    // A message that is not part of the gallery still opens it, at its first
    // item, so clicking an unknown id never leaves the viewer blank.
    controller.open({4, 8, 15}, 99);

    QVERIFY(controller.active());
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.currentMessageId(), qint64(4));

    // Ids that cannot address a message fall back the same way.
    controller.open({4, 8, 15}, 0);
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.currentMessageId(), qint64(4));
}

void MediaGalleryControllerTest::openRejectsEmptyGallery()
{
    MediaGalleryController controller;
    QSignalSpy spy(&controller, &MediaGalleryController::changed);

    controller.open({}, 5);

    QVERIFY(!controller.active());
    QVERIFY(controller.messageIds().isEmpty());
    QCOMPARE(controller.currentIndex(), -1);
    QCOMPARE(spy.count(), 0);
}

void MediaGalleryControllerTest::openRejectedEmptyKeepsOpenGallery()
{
    MediaGalleryController controller;
    controller.open({21, 22}, 21);
    QSignalSpy spy(&controller, &MediaGalleryController::changed);

    // Only ids that the cleaning drops, so there is nothing to show.
    controller.open({0, -3}, 22);

    QVERIFY(controller.active());
    QCOMPARE(controller.messageIds(), (QVector<qint64>{21, 22}));
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.currentMessageId(), qint64(21));
    QCOMPARE(spy.count(), 0);
}

void MediaGalleryControllerTest::openResetsZoomAndPan()
{
    MediaGalleryController controller;
    controller.open({1, 2}, 1);
    controller.setZoom(3.5);
    controller.setPan(0.5, -0.5);
    QSignalSpy spy(&controller, &MediaGalleryController::changed);

    controller.open({1, 2}, 1);

    QCOMPARE(controller.zoom(), 1.0);
    QCOMPARE(controller.panX(), 0.0);
    QCOMPARE(controller.panY(), 0.0);
    QCOMPARE(spy.count(), 1);
}

void MediaGalleryControllerTest::messageIdsReturnsACopy()
{
    MediaGalleryController controller;
    controller.open({30, 31}, 30);

    QVector<qint64> ids = controller.messageIds();
    ids.append(32);
    ids[0] = 99;

    QCOMPARE(controller.messageIds(), (QVector<qint64>{30, 31}));
    QCOMPARE(controller.currentMessageId(), qint64(30));
}

void MediaGalleryControllerTest::nextAdvancesThroughGallery()
{
    MediaGalleryController controller;
    controller.open({40, 41, 42}, 40);

    QVERIFY(controller.next());
    QCOMPARE(controller.currentIndex(), 1);
    QCOMPARE(controller.currentMessageId(), qint64(41));

    QVERIFY(controller.next());
    QCOMPARE(controller.currentIndex(), 2);
    QCOMPARE(controller.currentMessageId(), qint64(42));
}

void MediaGalleryControllerTest::nextAtEndFailsWithoutWrapping()
{
    MediaGalleryController controller;
    controller.open({40, 41}, 41);

    QVERIFY(!controller.next());
    QCOMPARE(controller.currentIndex(), 1);
    QCOMPARE(controller.currentMessageId(), qint64(41));
}

void MediaGalleryControllerTest::previousStepsBack()
{
    MediaGalleryController controller;
    controller.open({50, 51, 52}, 52);

    QVERIFY(controller.previous());
    QCOMPARE(controller.currentIndex(), 1);
    QCOMPARE(controller.currentMessageId(), qint64(51));

    QVERIFY(controller.previous());
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.currentMessageId(), qint64(50));
}

void MediaGalleryControllerTest::previousAtStartFailsWithoutWrapping()
{
    MediaGalleryController controller;
    controller.open({50, 51}, 50);

    QVERIFY(!controller.previous());
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.currentMessageId(), qint64(50));
}

void MediaGalleryControllerTest::navigationResetsZoomAndPan()
{
    MediaGalleryController controller;
    controller.open({60, 61, 62}, 61);

    controller.setZoom(4.0);
    controller.setPan(-1.0, 1.0);
    QVERIFY(controller.next());

    // The view state belongs to the item that was on screen.
    QCOMPARE(controller.zoom(), 1.0);
    QCOMPARE(controller.panX(), 0.0);
    QCOMPARE(controller.panY(), 0.0);

    controller.setZoom(2.0);
    controller.setPan(0.25, 0.75);
    QVERIFY(controller.previous());
    QCOMPARE(controller.zoom(), 1.0);
    QCOMPARE(controller.panX(), 0.0);
    QCOMPARE(controller.panY(), 0.0);
}

void MediaGalleryControllerTest::singleItemGalleryCannotNavigate()
{
    MediaGalleryController controller;
    controller.open({70}, 70);

    QVERIFY(!controller.next());
    QVERIFY(!controller.previous());
    QCOMPARE(controller.currentIndex(), 0);
    QCOMPARE(controller.currentMessageId(), qint64(70));
}

void MediaGalleryControllerTest::navigationFailsWhileInactive()
{
    MediaGalleryController controller;

    QVERIFY(!controller.next());
    QVERIFY(!controller.previous());
    QCOMPARE(controller.currentIndex(), -1);
    QCOMPARE(controller.currentMessageId(), qint64(0));
}

void MediaGalleryControllerTest::setZoomClampsToRange()
{
    MediaGalleryController controller;

    controller.setZoom(2.5);
    QCOMPARE(controller.zoom(), 2.5);

    controller.setZoom(1.0);
    QCOMPARE(controller.zoom(), 1.0);

    controller.setZoom(5.0);
    QCOMPARE(controller.zoom(), 5.0);

    // Values outside the range are clamped, not rejected.
    controller.setZoom(0.25);
    QCOMPARE(controller.zoom(), 1.0);

    controller.setZoom(42.0);
    QCOMPARE(controller.zoom(), 5.0);

    controller.setZoom(-3.0);
    QCOMPARE(controller.zoom(), 1.0);
}

void MediaGalleryControllerTest::setZoomIgnoresNonFiniteValues()
{
    MediaGalleryController controller;
    controller.setZoom(3.0);

    controller.setZoom(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(controller.zoom(), 3.0);

    controller.setZoom(std::numeric_limits<double>::infinity());
    QCOMPARE(controller.zoom(), 3.0);

    controller.setZoom(-std::numeric_limits<double>::infinity());
    QCOMPARE(controller.zoom(), 3.0);
}

void MediaGalleryControllerTest::setPanClampsEachAxis()
{
    MediaGalleryController controller;

    controller.setPan(0.5, -0.5);
    QCOMPARE(controller.panX(), 0.5);
    QCOMPARE(controller.panY(), -0.5);

    // Each axis is clamped on its own.
    controller.setPan(3.0, -0.25);
    QCOMPARE(controller.panX(), 1.0);
    QCOMPARE(controller.panY(), -0.25);

    controller.setPan(0.25, -7.0);
    QCOMPARE(controller.panX(), 0.25);
    QCOMPARE(controller.panY(), -1.0);

    controller.setPan(0.0, 0.0);
    QCOMPARE(controller.panX(), 0.0);
    QCOMPARE(controller.panY(), 0.0);
}

void MediaGalleryControllerTest::setPanIgnoresNonFiniteValues()
{
    MediaGalleryController controller;
    controller.setPan(0.5, 0.5);

    controller.setPan(std::numeric_limits<double>::quiet_NaN(), 0.0);
    QCOMPARE(controller.panX(), 0.5);
    QCOMPARE(controller.panY(), 0.5);

    controller.setPan(0.0, std::numeric_limits<double>::infinity());
    QCOMPARE(controller.panX(), 0.5);
    QCOMPARE(controller.panY(), 0.5);
}

void MediaGalleryControllerTest::resetZoomRestoresDefaults()
{
    MediaGalleryController controller;

    controller.resetZoom();
    QCOMPARE(controller.zoom(), 1.0);

    controller.setZoom(4.5);
    controller.setPan(-0.75, 0.25);

    controller.resetZoom();
    QCOMPARE(controller.zoom(), 1.0);
    QCOMPARE(controller.panX(), 0.0);
    QCOMPARE(controller.panY(), 0.0);
}

void MediaGalleryControllerTest::closeClearsGallery()
{
    MediaGalleryController controller;
    controller.open({80, 81}, 81);
    controller.setZoom(2.0);
    controller.setPan(0.5, 0.5);
    QSignalSpy spy(&controller, &MediaGalleryController::changed);

    controller.close();

    QVERIFY(!controller.active());
    QVERIFY(controller.messageIds().isEmpty());
    QCOMPARE(controller.currentIndex(), -1);
    QCOMPARE(controller.currentMessageId(), qint64(0));
    QCOMPARE(controller.zoom(), 1.0);
    QCOMPARE(controller.panX(), 0.0);
    QCOMPARE(controller.panY(), 0.0);
    QCOMPARE(spy.count(), 1);
}

void MediaGalleryControllerTest::closeIsSilentWhileInactive()
{
    MediaGalleryController controller;
    QSignalSpy spy(&controller, &MediaGalleryController::changed);

    controller.close();

    QVERIFY(!controller.active());
    QCOMPARE(spy.count(), 0);

    controller.open({90}, 90);
    controller.close();
    QCOMPARE(spy.count(), 2);

    // Already closed, so a second close has nothing to change.
    controller.close();
    QCOMPARE(spy.count(), 2);
}

void MediaGalleryControllerTest::changedFiresOnlyOnRealChanges()
{
    MediaGalleryController controller;
    QSignalSpy spy(&controller, &MediaGalleryController::changed);

    // Nothing is open, and the defaults are already in place.
    controller.setZoom(1.0);
    controller.setPan(0.0, 0.0);
    controller.resetZoom();
    controller.close();
    QCOMPARE(spy.count(), 0);

    controller.next();
    controller.previous();
    QCOMPARE(spy.count(), 0);

    controller.open({5, 6, 7}, 5);
    QCOMPARE(spy.count(), 1);

    // Same gallery, same item, untouched view: nothing moved.
    controller.open({5, 6, 7}, 5);
    QCOMPARE(spy.count(), 1);

    controller.open({5, 6, 7}, 6);
    QCOMPARE(spy.count(), 2);

    controller.next();
    QCOMPARE(spy.count(), 3);

    // The last item has no successor.
    QVERIFY(!controller.next());
    QCOMPARE(spy.count(), 3);

    controller.previous();
    QCOMPARE(spy.count(), 4);

    controller.setZoom(2.0);
    controller.setZoom(2.0);
    QCOMPARE(spy.count(), 5);

    controller.setZoom(99.0);
    controller.setZoom(99.0);
    QCOMPARE(spy.count(), 6);

    controller.setPan(0.5, -0.5);
    controller.setPan(0.5, -0.5);
    QCOMPARE(spy.count(), 7);

    controller.setPan(2.0, 2.0);
    QCOMPARE(spy.count(), 8);

    controller.resetZoom();
    controller.resetZoom();
    QCOMPARE(spy.count(), 9);

    controller.close();
    controller.close();
    QCOMPARE(spy.count(), 10);

    // An empty gallery is rejected even when nothing is open.
    controller.open({}, 0);
    QCOMPARE(spy.count(), 10);
}

QTEST_GUILESS_MAIN(MediaGalleryControllerTest)
#include "tst_media_gallery_controller.moc"
