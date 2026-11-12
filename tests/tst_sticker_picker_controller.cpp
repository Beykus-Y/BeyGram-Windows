#include "telegram/StickerPickerController.h"

#include <QSignalSpy>
#include <QtTest>

using beygram::telegram::StickerPickerController;
using beygram::telegram::StickerSetInfo;

namespace {

// Only the id and the title matter for the strip; the remaining fields keep
// their defaults so a test says exactly what it is about.
StickerSetInfo makeSet(qint64 id, const QString& title = {})
{
    StickerSetInfo set;
    set.id = id;
    set.title = title;
    return set;
}

} // namespace

class StickerPickerControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setSetsDropsInvalidAndDuplicateIds();
    void setSetsKeepsFirstRowForDuplicateId();
    void setSetsSameIsNoOp();
    void setSetsDetectsRowFieldChange();
    void setSetsPreservesCurrentSelection();
    void selectSetAcceptsKnownAndRecent();
    void selectSetIgnoresUnknownId();
    void selectSetSameIdIsNoOp();
    void selectSetChangeResetsStickersAndLoading();
    void selectSetRecentClearsBack();
    void setStickersDropsInvalidAndDuplicates();
    void setStickersWorksWhileLoading();
    void setStickersSameIsNoOp();
    void setStickersEmptyListClears();
    void setLoadingTogglesAndSignals();
    void setLoadingNoOpWhenUnchanged();
    void clearResetsEverything();
    void clearOnEmptyStateDoesNotSignal();
    void selectSetAfterClearIsIgnored();
    void setsAndStickersAreSnapshots();
    void changedCountsAcrossOperations();
};

void StickerPickerControllerTest::startsEmpty()
{
    StickerPickerController controller;

    QVERIFY(controller.sets().isEmpty());
    QCOMPARE(controller.setCount(), 0);
    QCOMPARE(controller.currentSetId(), qint64(0));
    QVERIFY(controller.stickers().isEmpty());
    QVERIFY(!controller.loading());
}

void StickerPickerControllerTest::setSetsDropsInvalidAndDuplicateIds()
{
    StickerPickerController controller;

    controller.setSets({makeSet(-3, QStringLiteral("negative")),
                        makeSet(5, QStringLiteral("cats")),
                        makeSet(5, QStringLiteral("cats again")),
                        makeSet(0, QStringLiteral("zero")),
                        makeSet(7, QStringLiteral("dogs"))});

    QCOMPARE(controller.setCount(), 2);
    QCOMPARE(controller.sets().at(0).id, qint64(5));
    QCOMPARE(controller.sets().at(1).id, qint64(7));
}

void StickerPickerControllerTest::setSetsKeepsFirstRowForDuplicateId()
{
    StickerPickerController controller;

    controller.setSets({makeSet(5, QStringLiteral("first")),
                        makeSet(5, QStringLiteral("second"))});

    QCOMPARE(controller.setCount(), 1);
    QCOMPARE(controller.sets().at(0).title, QStringLiteral("first"));
}

void StickerPickerControllerTest::setSetsSameIsNoOp()
{
    StickerPickerController controller;
    const QVector<StickerSetInfo> rows{makeSet(1, QStringLiteral("a")),
                                       makeSet(2, QStringLiteral("b"))};

    controller.setSets(rows);
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.setSets(rows);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.setCount(), 2);
}

void StickerPickerControllerTest::setSetsDetectsRowFieldChange()
{
    StickerPickerController controller;
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.setSets({makeSet(1, QStringLiteral("a"))});
    controller.setSets({makeSet(1, QStringLiteral("b"))});

    QCOMPARE(spy.count(), 2);
    QCOMPARE(controller.sets().at(0).title, QStringLiteral("b"));
}

void StickerPickerControllerTest::setSetsPreservesCurrentSelection()
{
    StickerPickerController controller;
    controller.setSets({makeSet(5, QStringLiteral("cats"))});
    controller.selectSet(5);
    controller.setStickers({11, 12});

    controller.setSets({makeSet(5, QStringLiteral("cats")),
                        makeSet(9, QStringLiteral("dogs"))});

    QCOMPARE(controller.currentSetId(), qint64(5));
    QCOMPARE(controller.stickers(), QVector<qint64>({11, 12}));
}

void StickerPickerControllerTest::selectSetAcceptsKnownAndRecent()
{
    StickerPickerController controller;
    controller.setSets({makeSet(5, QStringLiteral("cats"))});
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.selectSet(5);
    QCOMPARE(controller.currentSetId(), qint64(5));
    QCOMPARE(spy.count(), 1);

    controller.selectSet(0);
    QCOMPARE(controller.currentSetId(), qint64(0));
    QCOMPARE(spy.count(), 2);
}

void StickerPickerControllerTest::selectSetIgnoresUnknownId()
{
    StickerPickerController controller;
    controller.setSets({makeSet(5, QStringLiteral("cats"))});
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.selectSet(42);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.currentSetId(), qint64(0));
}

void StickerPickerControllerTest::selectSetSameIdIsNoOp()
{
    StickerPickerController controller;
    controller.setSets({makeSet(5, QStringLiteral("cats"))});
    controller.selectSet(5);
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.selectSet(5);

    QCOMPARE(spy.count(), 0);
}

void StickerPickerControllerTest::selectSetChangeResetsStickersAndLoading()
{
    StickerPickerController controller;
    controller.setSets({makeSet(5, QStringLiteral("cats")),
                        makeSet(7, QStringLiteral("dogs"))});
    controller.selectSet(5);
    controller.setStickers({11, 12});
    controller.setLoading(true);
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.selectSet(7);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(controller.currentSetId(), qint64(7));
    QVERIFY(controller.stickers().isEmpty());
    QVERIFY(!controller.loading());
}

void StickerPickerControllerTest::selectSetRecentClearsBack()
{
    StickerPickerController controller;
    controller.setSets({makeSet(5, QStringLiteral("cats"))});
    controller.selectSet(5);
    controller.setStickers({11});
    controller.setLoading(true);

    controller.selectSet(0);

    QCOMPARE(controller.currentSetId(), qint64(0));
    QVERIFY(controller.stickers().isEmpty());
    QVERIFY(!controller.loading());
}

void StickerPickerControllerTest::setStickersDropsInvalidAndDuplicates()
{
    StickerPickerController controller;

    controller.setStickers({0, -1, 4, 4, 2});

    QCOMPARE(controller.stickers(), QVector<qint64>({4, 2}));
}

void StickerPickerControllerTest::setStickersWorksWhileLoading()
{
    StickerPickerController controller;
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.setLoading(true);
    controller.setStickers({3});

    QCOMPARE(spy.count(), 2);
    QCOMPARE(controller.stickers(), QVector<qint64>({3}));
    QVERIFY(controller.loading());
}

void StickerPickerControllerTest::setStickersSameIsNoOp()
{
    StickerPickerController controller;
    controller.setStickers({4, 2});
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    // Same ids after normalisation: the repeated id collapses onto the first.
    controller.setStickers({4, 4, 2});

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.stickers(), QVector<qint64>({4, 2}));
}

void StickerPickerControllerTest::setStickersEmptyListClears()
{
    StickerPickerController controller;
    controller.setStickers({3});
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.setStickers({});

    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.stickers().isEmpty());
}

void StickerPickerControllerTest::setLoadingTogglesAndSignals()
{
    StickerPickerController controller;
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.setLoading(true);
    QVERIFY(controller.loading());
    QCOMPARE(spy.count(), 1);

    controller.setLoading(false);
    QVERIFY(!controller.loading());
    QCOMPARE(spy.count(), 2);
}

void StickerPickerControllerTest::setLoadingNoOpWhenUnchanged()
{
    StickerPickerController controller;
    controller.setLoading(true);
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.setLoading(true);

    QCOMPARE(spy.count(), 0);
    QVERIFY(controller.loading());
}

void StickerPickerControllerTest::clearResetsEverything()
{
    StickerPickerController controller;
    controller.setSets({makeSet(5, QStringLiteral("cats"))});
    controller.selectSet(5);
    controller.setStickers({11, 12});
    controller.setLoading(true);
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.clear();

    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.sets().isEmpty());
    QCOMPARE(controller.setCount(), 0);
    QCOMPARE(controller.currentSetId(), qint64(0));
    QVERIFY(controller.stickers().isEmpty());
    QVERIFY(!controller.loading());
}

void StickerPickerControllerTest::clearOnEmptyStateDoesNotSignal()
{
    StickerPickerController controller;
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.clear();

    QCOMPARE(spy.count(), 0);
}

void StickerPickerControllerTest::selectSetAfterClearIsIgnored()
{
    StickerPickerController controller;
    controller.setSets({makeSet(5, QStringLiteral("cats"))});
    controller.selectSet(5);
    controller.clear();
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.selectSet(5);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.currentSetId(), qint64(0));
}

void StickerPickerControllerTest::setsAndStickersAreSnapshots()
{
    StickerPickerController controller;
    controller.setSets({makeSet(5, QStringLiteral("cats"))});
    controller.setStickers({11, 12});

    QVector<StickerSetInfo> rows = controller.sets();
    rows.clear();
    QVector<qint64> ids = controller.stickers();
    ids.clear();

    QCOMPARE(controller.setCount(), 1);
    QCOMPARE(controller.stickers(), QVector<qint64>({11, 12}));
}

void StickerPickerControllerTest::changedCountsAcrossOperations()
{
    StickerPickerController controller;
    QSignalSpy spy(&controller, &StickerPickerController::changed);

    controller.setSets({makeSet(1, QStringLiteral("a")),
                        makeSet(2, QStringLiteral("b"))}); // 1
    controller.setSets({makeSet(1, QStringLiteral("a")),
                        makeSet(2, QStringLiteral("b"))}); // no-op
    controller.selectSet(1);                               // 2
    controller.selectSet(1);                               // no-op
    controller.setStickers({10, 11});                      // 3
    controller.setStickers({10, 11, 11});                  // no-op
    controller.setLoading(true);                           // 4
    controller.setLoading(true);                           // no-op
    controller.selectSet(2);                               // 5
    controller.clear();                                    // 6
    controller.clear();                                    // no-op

    QCOMPARE(spy.count(), 6);
    QVERIFY(controller.sets().isEmpty());
    QVERIFY(controller.stickers().isEmpty());
    QCOMPARE(controller.currentSetId(), qint64(0));
    QVERIFY(!controller.loading());
}

QTEST_GUILESS_MAIN(StickerPickerControllerTest)
#include "tst_sticker_picker_controller.moc"
