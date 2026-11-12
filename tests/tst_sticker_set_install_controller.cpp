#include "telegram/StickerSetInstallController.h"

#include <QSignalSpy>
#include <QtTest>

using beygram::telegram::StickerSetInfo;
using beygram::telegram::StickerSetInstallController;

namespace {

// Only the id, the title and the install flag matter here; the remaining fields
// keep their defaults so a test says exactly what it is about.
StickerSetInfo makeSet(qint64 id, bool installed = false, const QString& title = {})
{
    StickerSetInfo set;
    set.id = id;
    set.title = title;
    set.installed = installed;
    return set;
}

} // namespace

class StickerSetInstallControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setSetsDropsInvalidAndDuplicateIds();
    void setSetsKeepsFirstRowForDuplicateId();
    void setSetsSameIsNoOp();
    void setSetsDetectsRowFieldChange();
    void setSetsPreservesPendingId();
    void installedCountCountsInstalledOnly();
    void isInstalledNeedsKnownAndInstalled();
    void setInstalledFlipsAndSignals();
    void setInstalledIgnoresUnknownId();
    void setInstalledIgnoresInvalidId();
    void setInstalledSameValueIsNoOp();
    void markPendingAcceptsKnownId();
    void markPendingIgnoresUnknownId();
    void markPendingIgnoresNegativeId();
    void markPendingSameIdIsNoOp();
    void markPendingZeroClears();
    void markPendingZeroOnEmptyIsNoOp();
    void clearPendingResetsAndSignals();
    void clearPendingNoOpWhenNotPending();
    void clearResetsEverything();
    void clearOnEmptyStateDoesNotSignal();
    void markPendingAfterClearIsIgnored();
    void setsAreSnapshots();
    void changedCountsAcrossOperations();
};

void StickerSetInstallControllerTest::startsEmpty()
{
    StickerSetInstallController controller;

    QVERIFY(controller.sets().isEmpty());
    QCOMPARE(controller.installedCount(), 0);
    QCOMPARE(controller.pendingSetId(), qint64(0));
    QVERIFY(!controller.isInstalled(1));
}

void StickerSetInstallControllerTest::setSetsDropsInvalidAndDuplicateIds()
{
    StickerSetInstallController controller;

    controller.setSets({makeSet(-3, false, QStringLiteral("negative")),
                        makeSet(5, true, QStringLiteral("cats")),
                        makeSet(5, true, QStringLiteral("cats again")),
                        makeSet(0, true, QStringLiteral("zero")),
                        makeSet(7, false, QStringLiteral("dogs"))});

    QCOMPARE(controller.sets().size(), 2);
    QCOMPARE(controller.sets().at(0).id, qint64(5));
    QCOMPARE(controller.sets().at(1).id, qint64(7));
    QCOMPARE(controller.installedCount(), 1);
}

void StickerSetInstallControllerTest::setSetsKeepsFirstRowForDuplicateId()
{
    StickerSetInstallController controller;

    controller.setSets({makeSet(5, true, QStringLiteral("first")),
                        makeSet(5, false, QStringLiteral("second"))});

    QCOMPARE(controller.sets().size(), 1);
    QCOMPARE(controller.sets().at(0).title, QStringLiteral("first"));
    QVERIFY(controller.isInstalled(5));
}

void StickerSetInstallControllerTest::setSetsSameIsNoOp()
{
    StickerSetInstallController controller;
    const QVector<StickerSetInfo> rows{makeSet(1, false, QStringLiteral("a")),
                                       makeSet(2, true, QStringLiteral("b"))};

    controller.setSets(rows);
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.setSets(rows);
    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.sets().size(), 2);
}

void StickerSetInstallControllerTest::setSetsDetectsRowFieldChange()
{
    StickerSetInstallController controller;
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.setSets({makeSet(1, false, QStringLiteral("a"))});
    controller.setSets({makeSet(1, true, QStringLiteral("a"))});

    QCOMPARE(spy.count(), 2);
    QVERIFY(controller.isInstalled(1));
}

void StickerSetInstallControllerTest::setSetsPreservesPendingId()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5)});
    controller.markPending(5);
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.setSets({makeSet(5), makeSet(9)});

    QCOMPARE(controller.pendingSetId(), qint64(5));
    QCOMPARE(spy.count(), 1);
}

void StickerSetInstallControllerTest::installedCountCountsInstalledOnly()
{
    StickerSetInstallController controller;

    controller.setSets({makeSet(1, true), makeSet(2, false), makeSet(3, true)});

    QCOMPARE(controller.installedCount(), 2);
}

void StickerSetInstallControllerTest::isInstalledNeedsKnownAndInstalled()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5, true), makeSet(7, false)});

    QVERIFY(controller.isInstalled(5));
    QVERIFY(!controller.isInstalled(7));
    QVERIFY(!controller.isInstalled(42));
    QVERIFY(!controller.isInstalled(0));
}

void StickerSetInstallControllerTest::setInstalledFlipsAndSignals()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5, false)});
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    QVERIFY(controller.setInstalled(5, true));
    QVERIFY(controller.isInstalled(5));
    QCOMPARE(controller.installedCount(), 1);
    QCOMPARE(spy.count(), 1);

    QVERIFY(controller.setInstalled(5, false));
    QVERIFY(!controller.isInstalled(5));
    QCOMPARE(controller.installedCount(), 0);
    QCOMPARE(spy.count(), 2);
}

void StickerSetInstallControllerTest::setInstalledIgnoresUnknownId()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5, false)});
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    QVERIFY(!controller.setInstalled(42, true));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.installedCount(), 0);
    QCOMPARE(controller.sets().size(), 1);
}

void StickerSetInstallControllerTest::setInstalledIgnoresInvalidId()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5, false)});
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    QVERIFY(!controller.setInstalled(0, true));
    QVERIFY(!controller.setInstalled(-5, true));

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.installedCount(), 0);
}

void StickerSetInstallControllerTest::setInstalledSameValueIsNoOp()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5, true)});
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    QVERIFY(!controller.setInstalled(5, true));

    QCOMPARE(spy.count(), 0);
    QVERIFY(controller.isInstalled(5));
}

void StickerSetInstallControllerTest::markPendingAcceptsKnownId()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5), makeSet(7)});
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.markPending(7);

    QCOMPARE(controller.pendingSetId(), qint64(7));
    QCOMPARE(spy.count(), 1);

    controller.markPending(5);

    QCOMPARE(controller.pendingSetId(), qint64(5));
    QCOMPARE(spy.count(), 2);
}

void StickerSetInstallControllerTest::markPendingIgnoresUnknownId()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5)});
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.markPending(42);

    QCOMPARE(controller.pendingSetId(), qint64(0));
    QCOMPARE(spy.count(), 0);
}

void StickerSetInstallControllerTest::markPendingIgnoresNegativeId()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5)});
    controller.markPending(5);
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.markPending(-5);

    QCOMPARE(controller.pendingSetId(), qint64(5));
    QCOMPARE(spy.count(), 0);
}

void StickerSetInstallControllerTest::markPendingSameIdIsNoOp()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5)});
    controller.markPending(5);
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.markPending(5);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.pendingSetId(), qint64(5));
}

void StickerSetInstallControllerTest::markPendingZeroClears()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5)});
    controller.markPending(5);
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.markPending(0);

    QCOMPARE(controller.pendingSetId(), qint64(0));
    QCOMPARE(spy.count(), 1);
}

void StickerSetInstallControllerTest::markPendingZeroOnEmptyIsNoOp()
{
    StickerSetInstallController controller;
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.markPending(0);

    QCOMPARE(controller.pendingSetId(), qint64(0));
    QCOMPARE(spy.count(), 0);
}

void StickerSetInstallControllerTest::clearPendingResetsAndSignals()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5, true)});
    controller.markPending(5);
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.clearPending();

    QCOMPARE(controller.pendingSetId(), qint64(0));
    QCOMPARE(spy.count(), 1);
    // Clearing the pending set must not touch the install state.
    QVERIFY(controller.isInstalled(5));
}

void StickerSetInstallControllerTest::clearPendingNoOpWhenNotPending()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5)});
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.clearPending();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.pendingSetId(), qint64(0));
}

void StickerSetInstallControllerTest::clearResetsEverything()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5, true), makeSet(7, false)});
    controller.markPending(7);
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.clear();

    QCOMPARE(spy.count(), 1);
    QVERIFY(controller.sets().isEmpty());
    QCOMPARE(controller.installedCount(), 0);
    QCOMPARE(controller.pendingSetId(), qint64(0));
}

void StickerSetInstallControllerTest::clearOnEmptyStateDoesNotSignal()
{
    StickerSetInstallController controller;
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.clear();

    QCOMPARE(spy.count(), 0);
}

void StickerSetInstallControllerTest::markPendingAfterClearIsIgnored()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5)});
    controller.markPending(5);
    controller.clear();
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.markPending(5);

    QCOMPARE(spy.count(), 0);
    QCOMPARE(controller.pendingSetId(), qint64(0));
}

void StickerSetInstallControllerTest::setsAreSnapshots()
{
    StickerSetInstallController controller;
    controller.setSets({makeSet(5, true)});

    QVector<StickerSetInfo> rows = controller.sets();
    rows.clear();

    QCOMPARE(controller.sets().size(), 1);
    QCOMPARE(controller.installedCount(), 1);
}

void StickerSetInstallControllerTest::changedCountsAcrossOperations()
{
    StickerSetInstallController controller;
    QSignalSpy spy(&controller, &StickerSetInstallController::changed);

    controller.setSets({makeSet(1, false, QStringLiteral("a")),
                        makeSet(2, true, QStringLiteral("b"))}); // 1
    controller.setSets({makeSet(1, false, QStringLiteral("a")),
                        makeSet(2, true, QStringLiteral("b"))}); // no-op
    QVERIFY(controller.setInstalled(1, true));                   // 2
    QVERIFY(!controller.setInstalled(1, true));                  // no-op
    QVERIFY(!controller.setInstalled(9, true));                  // unknown, no-op
    controller.markPending(2);                                   // 3
    controller.markPending(2);                                   // no-op
    controller.markPending(9);                                   // unknown, no-op
    controller.clearPending();                                   // 4
    controller.clearPending();                                   // no-op
    controller.clear();                                          // 5
    controller.clear();                                          // no-op

    QCOMPARE(spy.count(), 5);
    QVERIFY(controller.sets().isEmpty());
    QCOMPARE(controller.pendingSetId(), qint64(0));
}

QTEST_GUILESS_MAIN(StickerSetInstallControllerTest)
#include "tst_sticker_set_install_controller.moc"
