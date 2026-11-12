#include "telegram/CacheCleanupController.h"

#include <QSignalSpy>
#include <QtTest>

#include <initializer_list>
#include <utility>

using namespace beygram::telegram;

class CacheCleanupControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void setCandidatesDropsEmptyKinds();
    void setCandidatesClampsNegativeBytes();
    void setCandidatesEmitsOnlyOnRealChange();
    void setCandidatesPrunesSelectionOfDroppedKinds();
    void setSelectedKeepsKnownKindsOnly();
    void setSelectedDedupesAndSorts();
    void setSelectedEmitsOnlyOnRealChange();
    void selectedBytesSumsSelectedKinds();
    void selectedBytesIsZeroWithoutSelection();
    void canCleanNeedsBytesAndSelection();
    void canCleanIsFalseWhileRunning();
    void setRunningEmitsOnlyOnRealChange();
    void clearResetsEverything();
    void clearIsSilentWhenAlreadyEmpty();
};

namespace {

QHash<QString, qint64> candidatesOf(std::initializer_list<std::pair<QString, qint64>> pairs)
{
    QHash<QString, qint64> result;
    for (const auto& pair : pairs)
        result.insert(pair.first, pair.second);
    return result;
}

} // namespace

void CacheCleanupControllerTest::startsEmpty()
{
    CacheCleanupController controller;
    QSignalSpy spy(&controller, &CacheCleanupController::changed);

    QVERIFY(controller.candidates().isEmpty());
    QVERIFY(controller.selected().isEmpty());
    QCOMPARE(controller.selectedBytes(), qint64(0));
    QVERIFY(!controller.canClean());
    QVERIFY(!controller.running());
    QCOMPARE(spy.count(), 0);
}

void CacheCleanupControllerTest::setCandidatesDropsEmptyKinds()
{
    CacheCleanupController controller;

    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), 100},
        {QString(), 200},
        {QStringLiteral("   "), 300},
        {QStringLiteral("video"), 0},
    }));

    const QHash<QString, qint64> expected = candidatesOf({
        {QStringLiteral("photo"), 100},
        {QStringLiteral("video"), 0},
    });
    QVERIFY(controller.candidates() == expected);
}

void CacheCleanupControllerTest::setCandidatesClampsNegativeBytes()
{
    CacheCleanupController controller;

    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), -1},
        {QStringLiteral("audio"), -1024},
    }));

    const QHash<QString, qint64> expected = candidatesOf({
        {QStringLiteral("photo"), 0},
        {QStringLiteral("audio"), 0},
    });
    QVERIFY(controller.candidates() == expected);
    QCOMPARE(controller.candidates().value(QStringLiteral("audio")), qint64(0));
}

void CacheCleanupControllerTest::setCandidatesEmitsOnlyOnRealChange()
{
    CacheCleanupController controller;
    QSignalSpy spy(&controller, &CacheCleanupController::changed);

    const QHash<QString, qint64> bytes = candidatesOf({
        {QStringLiteral("photo"), 100},
        {QStringLiteral("document"), 20},
    });
    controller.setCandidates(bytes);
    QCOMPARE(spy.count(), 1);

    controller.setCandidates(bytes);
    QCOMPARE(spy.count(), 1);

    // A negative size clamps to 0, which differs from the stored 20.
    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), 100},
        {QStringLiteral("document"), -20},
    }));
    QCOMPARE(spy.count(), 2);

    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), 100},
        {QStringLiteral("document"), 21},
    }));
    QCOMPARE(spy.count(), 3);
}

void CacheCleanupControllerTest::setCandidatesPrunesSelectionOfDroppedKinds()
{
    CacheCleanupController controller;
    QSignalSpy spy(&controller, &CacheCleanupController::changed);

    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), 100},
        {QStringLiteral("video"), 200},
    }));
    controller.setSelected({QStringLiteral("photo"), QStringLiteral("video")});
    QCOMPARE(spy.count(), 2);
    QCOMPARE(controller.selected().size(), 2);

    // video is gone, so it cannot stay selected.
    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), 100},
    }));
    QCOMPARE(spy.count(), 3);

    const QStringList expectedSelection = {QStringLiteral("photo")};
    QCOMPARE(controller.selected(), expectedSelection);
    QCOMPARE(controller.selectedBytes(), qint64(100));

    // Bytes moved but the pruned selection did not: still exactly one signal.
    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), 150},
    }));
    QCOMPARE(spy.count(), 4);
    QCOMPARE(controller.selected(), expectedSelection);
}

void CacheCleanupControllerTest::setSelectedKeepsKnownKindsOnly()
{
    CacheCleanupController controller;
    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), 100},
        {QStringLiteral("video"), 200},
    }));

    controller.setSelected({
        QStringLiteral("video"),
        QStringLiteral("audio"),
        QStringLiteral("photo"),
        QStringLiteral("nope"),
    });

    const QStringList expected = {QStringLiteral("photo"), QStringLiteral("video")};
    QCOMPARE(controller.selected(), expected);
}

void CacheCleanupControllerTest::setSelectedDedupesAndSorts()
{
    CacheCleanupController controller;
    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), 100},
        {QStringLiteral("video"), 200},
        {QStringLiteral("audio"), 50},
    }));

    controller.setSelected({
        QStringLiteral("video"),
        QStringLiteral("photo"),
        QStringLiteral("video"),
        QStringLiteral("audio"),
        QStringLiteral("photo"),
    });

    const QStringList expected = {
        QStringLiteral("audio"),
        QStringLiteral("photo"),
        QStringLiteral("video"),
    };
    QCOMPARE(controller.selected(), expected);
}

void CacheCleanupControllerTest::setSelectedEmitsOnlyOnRealChange()
{
    CacheCleanupController controller;
    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), 100},
        {QStringLiteral("video"), 200},
    }));

    QSignalSpy spy(&controller, &CacheCleanupController::changed);

    controller.setSelected({QStringLiteral("video"), QStringLiteral("photo")});
    QCOMPARE(spy.count(), 1);

    // Same set, different order and a duplicate: no real change.
    controller.setSelected({
        QStringLiteral("photo"),
        QStringLiteral("video"),
        QStringLiteral("photo"),
    });
    QCOMPARE(spy.count(), 1);

    // Clearing is a real change.
    controller.setSelected({});
    QCOMPARE(spy.count(), 2);
    QVERIFY(controller.selected().isEmpty());

    // Unknown kinds alone do not change an empty selection.
    controller.setSelected({QStringLiteral("nope")});
    QCOMPARE(spy.count(), 2);
}

void CacheCleanupControllerTest::selectedBytesSumsSelectedKinds()
{
    CacheCleanupController controller;
    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), 100},
        {QStringLiteral("video"), 250},
        {QStringLiteral("document"), 3},
    }));

    controller.setSelected({QStringLiteral("photo"), QStringLiteral("document")});
    QCOMPARE(controller.selectedBytes(), qint64(103));

    controller.setSelected({
        QStringLiteral("photo"),
        QStringLiteral("video"),
        QStringLiteral("document"),
    });
    QCOMPARE(controller.selectedBytes(), qint64(353));
}

void CacheCleanupControllerTest::selectedBytesIsZeroWithoutSelection()
{
    CacheCleanupController controller;
    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), 100},
    }));

    QCOMPARE(controller.selectedBytes(), qint64(0));
    QVERIFY(!controller.canClean());

    controller.setSelected({QStringLiteral("photo")});
    QCOMPARE(controller.selectedBytes(), qint64(100));

    controller.setSelected({});
    QCOMPARE(controller.selectedBytes(), qint64(0));
}

void CacheCleanupControllerTest::canCleanNeedsBytesAndSelection()
{
    CacheCleanupController controller;
    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), 0},
        {QStringLiteral("video"), 10},
    }));

    QVERIFY(!controller.canClean());

    // A selected kind worth nothing is not a cleanup.
    controller.setSelected({QStringLiteral("photo")});
    QVERIFY(!controller.canClean());

    controller.setSelected({QStringLiteral("video")});
    QVERIFY(controller.canClean());

    controller.setSelected({QStringLiteral("photo"), QStringLiteral("video")});
    QVERIFY(controller.canClean());

    controller.setSelected({});
    QVERIFY(!controller.canClean());
}

void CacheCleanupControllerTest::canCleanIsFalseWhileRunning()
{
    CacheCleanupController controller;
    controller.setCandidates(candidatesOf({
        {QStringLiteral("video"), 10},
    }));
    controller.setSelected({QStringLiteral("video")});
    QVERIFY(controller.canClean());

    controller.setRunning(true);
    QVERIFY(controller.running());
    QVERIFY(!controller.canClean());

    controller.setRunning(false);
    QVERIFY(controller.canClean());
}

void CacheCleanupControllerTest::setRunningEmitsOnlyOnRealChange()
{
    CacheCleanupController controller;
    QSignalSpy spy(&controller, &CacheCleanupController::changed);

    controller.setRunning(false);
    QCOMPARE(spy.count(), 0);

    controller.setRunning(true);
    QCOMPARE(spy.count(), 1);

    controller.setRunning(true);
    QCOMPARE(spy.count(), 1);

    controller.setRunning(false);
    QCOMPARE(spy.count(), 2);
}

void CacheCleanupControllerTest::clearResetsEverything()
{
    CacheCleanupController controller;
    controller.setCandidates(candidatesOf({
        {QStringLiteral("photo"), 100},
        {QStringLiteral("video"), 200},
    }));
    controller.setSelected({QStringLiteral("photo")});
    controller.setRunning(true);

    QSignalSpy spy(&controller, &CacheCleanupController::changed);

    controller.clear();
    QCOMPARE(spy.count(), 1);

    QVERIFY(controller.candidates().isEmpty());
    QVERIFY(controller.selected().isEmpty());
    QCOMPARE(controller.selectedBytes(), qint64(0));
    QVERIFY(!controller.running());
    QVERIFY(!controller.canClean());
}

void CacheCleanupControllerTest::clearIsSilentWhenAlreadyEmpty()
{
    CacheCleanupController controller;
    QSignalSpy spy(&controller, &CacheCleanupController::changed);

    controller.clear();
    QCOMPARE(spy.count(), 0);

    // Running alone still counts as state worth clearing.
    controller.setRunning(true);
    QCOMPARE(spy.count(), 1);

    controller.clear();
    QCOMPARE(spy.count(), 2);
    QVERIFY(!controller.running());
}

QTEST_GUILESS_MAIN(CacheCleanupControllerTest)
#include "tst_cache_cleanup_controller.moc"
