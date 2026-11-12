#include "telegram/MediaDownloadService.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

namespace {

constexpr qint64 MiB = 1024 * 1024;

MediaCachePolicy limits(qint64 wifi, qint64 mobile, qint64 roaming)
{
    MediaCachePolicy policy;
    policy.wifiBytes = wifi;
    policy.mobileBytes = mobile;
    policy.roamingBytes = roaming;
    return policy;
}

} // namespace

class MediaDownloadServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void policyLimitBytesAreNeverNegative();
    void defaultPolicyUsesConservativeNetworkCeilings();
    void setPolicyEmitsOnlyOnRealChange();
    void setPolicyNormalisesNegativeCeilings();
    void requestRejectsNonPositiveIds();
    void requestRejectsSizeAboveNetworkLimit();
    void requestAcceptsSizeAtTheLimit();
    void requestAcceptsUnknownSizeWhileNetworkIsEnabled();
    void requestRejectsDuplicatesPendingAndActive();
    void requestEmitsDownloadRequestedWithStoredPriority();
    void requestOrdersPendingByPriorityThenFifo();
    void onStartedMovesOnlyTheRequestedPendingEntry();
    void onStartedIsIdempotent();
    void onStartedIgnoresUnknownAndNonPositiveIds();
    void activeReportsStartOrder();
    void onFinishedSuccessCountsAndRemoves();
    void onFinishedFailureCountsAndDoesNotRequeue();
    void onFinishedIgnoresNonActiveAndUnknownIds();
    void cancelPendingEmitsCancelled();
    void cancelActiveEmitsCancelledWithoutCountingFailure();
    void cancelUnknownIsSilent();
    void clearResetsListsAndCounters();
    void clearIsNoOpWhenAlreadyEmpty();
    void policyChangeKeepsQueuedRequestsAndGatesNewOnes();
    void requestAfterFinishCanReuseId();
    void changedTracksOnlyRealChanges();
};

void MediaDownloadServiceTest::startsEmpty()
{
    const MediaDownloadService service;

    QVERIFY(service.pending().isEmpty());
    QVERIFY(service.active().isEmpty());
    QCOMPARE(service.completedCount(), 0);
    QCOMPARE(service.failedCount(), 0);
}

void MediaDownloadServiceTest::policyLimitBytesAreNeverNegative()
{
    const MediaCachePolicy policy = limits(-5, 7, 0);

    QCOMPARE(policy.limitBytes(NetworkKind::WiFi), qint64(0));
    QCOMPARE(policy.limitBytes(NetworkKind::Mobile), qint64(7));
    QCOMPARE(policy.limitBytes(NetworkKind::Roaming), qint64(0));
}

void MediaDownloadServiceTest::defaultPolicyUsesConservativeNetworkCeilings()
{
    MediaDownloadService service;

    QVERIFY(service.request(1, 8, 50 * MiB, NetworkKind::WiFi));
    QVERIFY(!service.request(2, 8, 50 * MiB + 1, NetworkKind::WiFi));
    QVERIFY(service.request(3, 8, 5 * MiB, NetworkKind::Mobile));
    QVERIFY(!service.request(4, 8, 5 * MiB + 1, NetworkKind::Mobile));
    QVERIFY(!service.request(5, 8, 0, NetworkKind::Roaming));

    QCOMPARE(service.pending(), QVector<int>({1, 3}));
}

void MediaDownloadServiceTest::setPolicyEmitsOnlyOnRealChange()
{
    MediaDownloadService service;
    QSignalSpy spy(&service, &MediaDownloadService::changed);

    service.setPolicy(limits(1 * MiB, 2 * MiB, 3 * MiB));
    QCOMPARE(spy.count(), 1);

    service.setPolicy(limits(1 * MiB, 2 * MiB, 3 * MiB));
    QCOMPARE(spy.count(), 1);

    service.setPolicy(limits(4 * MiB, 2 * MiB, 3 * MiB));
    QCOMPARE(spy.count(), 2);
}

void MediaDownloadServiceTest::setPolicyNormalisesNegativeCeilings()
{
    MediaDownloadService service;
    QSignalSpy spy(&service, &MediaDownloadService::changed);

    service.setPolicy(limits(-1, -2, -3));
    QCOMPARE(spy.count(), 1);

    // A negative ceiling is stored as zero, so this writes the same policy.
    service.setPolicy(limits(0, 0, 0));
    QCOMPARE(spy.count(), 1);

    QVERIFY(!service.request(1, 8, 0, NetworkKind::WiFi));
    QVERIFY(!service.request(2, 8, 1, NetworkKind::Mobile));
    QVERIFY(!service.request(3, 8, 1, NetworkKind::Roaming));
}

void MediaDownloadServiceTest::requestRejectsNonPositiveIds()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));

    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);
    QSignalSpy requestSpy(&service, &MediaDownloadService::downloadRequested);

    QVERIFY(!service.request(0, 8, 1, NetworkKind::WiFi));
    QVERIFY(!service.request(-1, 8, 1, NetworkKind::WiFi));
    QVERIFY(!service.request(-1000, 32, 0, NetworkKind::WiFi));

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(requestSpy.count(), 0);
    QVERIFY(service.pending().isEmpty());
}

void MediaDownloadServiceTest::requestRejectsSizeAboveNetworkLimit()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 5 * MiB, 1 * MiB));

    QSignalSpy requestSpy(&service, &MediaDownloadService::downloadRequested);

    QVERIFY(!service.request(1, 8, 10 * MiB + 1, NetworkKind::WiFi));
    QVERIFY(!service.request(2, 8, 5 * MiB + 1, NetworkKind::Mobile));
    QVERIFY(!service.request(3, 8, 1 * MiB + 1, NetworkKind::Roaming));

    QVERIFY(service.pending().isEmpty());
    QCOMPARE(requestSpy.count(), 0);
}

void MediaDownloadServiceTest::requestAcceptsSizeAtTheLimit()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 5 * MiB, 1 * MiB));

    QVERIFY(service.request(1, 8, 10 * MiB, NetworkKind::WiFi));
    QVERIFY(service.request(2, 8, 5 * MiB, NetworkKind::Mobile));
    QVERIFY(service.request(3, 8, 1 * MiB, NetworkKind::Roaming));

    QCOMPARE(service.pending(), QVector<int>({1, 2, 3}));
}

void MediaDownloadServiceTest::requestAcceptsUnknownSizeWhileNetworkIsEnabled()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 0, 0));

    // A non-positive size means TDLib has not reported it yet.
    QVERIFY(service.request(1, 8, 0, NetworkKind::WiFi));
    QVERIFY(service.request(2, 8, -1, NetworkKind::WiFi));

    // A zero ceiling disables the network even for an unknown size.
    QVERIFY(!service.request(3, 8, 0, NetworkKind::Mobile));
    QVERIFY(!service.request(4, 8, 0, NetworkKind::Roaming));

    QCOMPARE(service.pending(), QVector<int>({1, 2}));
}

void MediaDownloadServiceTest::requestRejectsDuplicatesPendingAndActive()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(7, 8, 1, NetworkKind::WiFi));

    QSignalSpy requestSpy(&service, &MediaDownloadService::downloadRequested);
    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);

    QVERIFY(!service.request(7, 32, 1, NetworkKind::WiFi));
    QCOMPARE(requestSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);

    service.onStarted(7);
    QVERIFY(!service.request(7, 8, 1, NetworkKind::WiFi));
    QCOMPARE(requestSpy.count(), 0);

    QCOMPARE(service.active(), QVector<int>({7}));
    QVERIFY(service.pending().isEmpty());
}

void MediaDownloadServiceTest::requestEmitsDownloadRequestedWithStoredPriority()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));

    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);
    QSignalSpy requestSpy(&service, &MediaDownloadService::downloadRequested);

    QVERIFY(service.request(5, 16, 1, NetworkKind::WiFi));

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(requestSpy.count(), 1);
    QCOMPARE(requestSpy.at(0).at(0).toInt(), 5);
    QCOMPARE(requestSpy.at(0).at(1).toInt(), 16);
}

void MediaDownloadServiceTest::requestOrdersPendingByPriorityThenFifo()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));

    QVERIFY(service.request(10, 8, 1, NetworkKind::WiFi));
    QVERIFY(service.request(11, 32, 1, NetworkKind::WiFi));
    QVERIFY(service.request(12, 16, 1, NetworkKind::WiFi));
    QVERIFY(service.request(13, 32, 1, NetworkKind::WiFi));
    QVERIFY(service.request(14, 1, 1, NetworkKind::WiFi));

    QCOMPARE(service.pending(), QVector<int>({11, 13, 12, 10, 14}));
}

void MediaDownloadServiceTest::onStartedMovesOnlyTheRequestedPendingEntry()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(1, 8, 1, NetworkKind::WiFi));
    QVERIFY(service.request(2, 32, 1, NetworkKind::WiFi));
    QVERIFY(service.request(3, 16, 1, NetworkKind::WiFi));

    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);
    service.onStarted(3);

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(service.pending(), QVector<int>({2, 1}));
    QCOMPARE(service.active(), QVector<int>({3}));
}

void MediaDownloadServiceTest::onStartedIsIdempotent()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(1, 8, 1, NetworkKind::WiFi));
    service.onStarted(1);

    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);
    service.onStarted(1);

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(service.active(), QVector<int>({1}));
    QVERIFY(service.pending().isEmpty());
}

void MediaDownloadServiceTest::onStartedIgnoresUnknownAndNonPositiveIds()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(1, 8, 1, NetworkKind::WiFi));

    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);
    service.onStarted(99);
    service.onStarted(0);
    service.onStarted(-1);

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(service.pending(), QVector<int>({1}));
    QVERIFY(service.active().isEmpty());
}

void MediaDownloadServiceTest::activeReportsStartOrder()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(1, 8, 1, NetworkKind::WiFi));
    QVERIFY(service.request(2, 32, 1, NetworkKind::WiFi));
    QVERIFY(service.request(3, 16, 1, NetworkKind::WiFi));

    service.onStarted(2);
    service.onStarted(3);
    service.onStarted(1);

    QCOMPARE(service.active(), QVector<int>({2, 3, 1}));
    QVERIFY(service.pending().isEmpty());
}

void MediaDownloadServiceTest::onFinishedSuccessCountsAndRemoves()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(1, 8, 1, NetworkKind::WiFi));
    service.onStarted(1);

    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);
    service.onFinished(1, true);

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(service.completedCount(), 1);
    QCOMPARE(service.failedCount(), 0);
    QVERIFY(service.active().isEmpty());

    // A second finish for the same id is not a second completion.
    service.onFinished(1, true);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(service.completedCount(), 1);
}

void MediaDownloadServiceTest::onFinishedFailureCountsAndDoesNotRequeue()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(1, 8, 1, NetworkKind::WiFi));
    QVERIFY(service.request(2, 8, 1, NetworkKind::WiFi));
    service.onStarted(1);

    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);
    service.onFinished(1, false);

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(service.failedCount(), 1);
    QCOMPARE(service.completedCount(), 0);
    QVERIFY(service.active().isEmpty());
    // A failure is counted, not re-queued automatically.
    QCOMPARE(service.pending(), QVector<int>({2}));

    // The owner may request the failed id again explicitly.
    QVERIFY(service.request(1, 8, 1, NetworkKind::WiFi));
    QCOMPARE(service.pending(), QVector<int>({2, 1}));
}

void MediaDownloadServiceTest::onFinishedIgnoresNonActiveAndUnknownIds()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(1, 8, 1, NetworkKind::WiFi));

    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);
    service.onFinished(1, true); // still pending, not active
    service.onFinished(99, true);
    service.onFinished(0, false);
    service.onFinished(-1, false);

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(service.completedCount(), 0);
    QCOMPARE(service.failedCount(), 0);
    QCOMPARE(service.pending(), QVector<int>({1}));
    QVERIFY(service.active().isEmpty());
}

void MediaDownloadServiceTest::cancelPendingEmitsCancelled()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(1, 8, 1, NetworkKind::WiFi));
    QVERIFY(service.request(2, 16, 1, NetworkKind::WiFi));

    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);
    QSignalSpy cancelledSpy(&service, &MediaDownloadService::cancelled);

    service.cancel(1);

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 1);
    QCOMPARE(cancelledSpy.at(0).at(0).toInt(), 1);
    QCOMPARE(service.pending(), QVector<int>({2}));
    QVERIFY(service.active().isEmpty());
}

void MediaDownloadServiceTest::cancelActiveEmitsCancelledWithoutCountingFailure()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(1, 8, 1, NetworkKind::WiFi));
    QVERIFY(service.request(2, 8, 1, NetworkKind::WiFi));
    service.onStarted(1);

    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);
    QSignalSpy cancelledSpy(&service, &MediaDownloadService::cancelled);

    service.cancel(1);

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 1);
    QCOMPARE(cancelledSpy.at(0).at(0).toInt(), 1);
    QVERIFY(service.active().isEmpty());
    QCOMPARE(service.pending(), QVector<int>({2}));
    QCOMPARE(service.completedCount(), 0);
    QCOMPARE(service.failedCount(), 0);
}

void MediaDownloadServiceTest::cancelUnknownIsSilent()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(1, 8, 1, NetworkKind::WiFi));

    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);
    QSignalSpy cancelledSpy(&service, &MediaDownloadService::cancelled);

    service.cancel(2);
    service.cancel(0);
    service.cancel(-1);
    service.cancel(99);

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(cancelledSpy.count(), 0);
    QCOMPARE(service.pending(), QVector<int>({1}));
}

void MediaDownloadServiceTest::clearResetsListsAndCounters()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(1, 8, 1, NetworkKind::WiFi));
    QVERIFY(service.request(2, 8, 1, NetworkKind::WiFi));
    QVERIFY(service.request(3, 8, 1, NetworkKind::WiFi));

    service.onStarted(1);
    service.onFinished(1, true);
    QCOMPARE(service.completedCount(), 1);

    QVERIFY(service.request(4, 8, 1, NetworkKind::WiFi));
    service.onStarted(4);
    service.onFinished(4, false);
    QCOMPARE(service.failedCount(), 1);

    service.onStarted(2);
    QCOMPARE(service.pending(), QVector<int>({3}));
    QCOMPARE(service.active(), QVector<int>({2}));

    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);
    QSignalSpy cancelledSpy(&service, &MediaDownloadService::cancelled);

    service.clear();

    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(cancelledSpy.count(), 0);
    QVERIFY(service.pending().isEmpty());
    QVERIFY(service.active().isEmpty());
    QCOMPARE(service.completedCount(), 0);
    QCOMPARE(service.failedCount(), 0);
}

void MediaDownloadServiceTest::clearIsNoOpWhenAlreadyEmpty()
{
    MediaDownloadService service;
    QSignalSpy changedSpy(&service, &MediaDownloadService::changed);

    service.clear();
    QCOMPARE(changedSpy.count(), 0);

    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QCOMPARE(changedSpy.count(), 1);

    QVERIFY(service.request(1, 8, 1, NetworkKind::WiFi)); // 2
    service.onStarted(1);                                 // 3
    service.onFinished(1, true);                          // 4
    QCOMPARE(service.completedCount(), 1);

    // Clearing the counter is a real change even though both lists are empty.
    service.clear();
    QCOMPARE(changedSpy.count(), 5);
    QCOMPARE(service.completedCount(), 0);

    service.clear();
    QCOMPARE(changedSpy.count(), 5);
}

void MediaDownloadServiceTest::policyChangeKeepsQueuedRequestsAndGatesNewOnes()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(1, 8, 10 * MiB, NetworkKind::WiFi));

    service.setPolicy(limits(1 * MiB, 1 * MiB, 1 * MiB));

    QCOMPARE(service.pending(), QVector<int>({1}));
    QVERIFY(!service.request(2, 8, 10 * MiB, NetworkKind::WiFi));
    QVERIFY(service.request(3, 8, 1 * MiB, NetworkKind::WiFi));
    QCOMPARE(service.pending(), QVector<int>({1, 3}));
}

void MediaDownloadServiceTest::requestAfterFinishCanReuseId()
{
    MediaDownloadService service;
    service.setPolicy(limits(10 * MiB, 10 * MiB, 10 * MiB));
    QVERIFY(service.request(7, 8, 1, NetworkKind::WiFi));
    service.onStarted(7);
    service.onFinished(7, true);

    QVERIFY(service.request(7, 8, 1, NetworkKind::WiFi));

    QCOMPARE(service.pending(), QVector<int>({7}));
    QVERIFY(service.active().isEmpty());
    QCOMPARE(service.completedCount(), 1);
}

void MediaDownloadServiceTest::changedTracksOnlyRealChanges()
{
    MediaDownloadService service;
    QSignalSpy spy(&service, &MediaDownloadService::changed);

    QVERIFY(!service.request(0, 8, 1, NetworkKind::WiFi)); // no-op
    service.setPolicy(limits(100 * MiB, 100 * MiB, 100 * MiB)); // 1
    service.setPolicy(limits(100 * MiB, 100 * MiB, 100 * MiB)); // no-op
    QVERIFY(!service.request(1, 8, 100 * MiB + 1, NetworkKind::WiFi)); // no-op
    QVERIFY(service.request(1, 8, 100 * MiB, NetworkKind::WiFi)); // 2
    QVERIFY(!service.request(1, 32, 1, NetworkKind::WiFi)); // no-op
    service.onStarted(1); // 3
    service.onStarted(1); // no-op
    service.onFinished(1, true); // 4
    service.onFinished(1, true); // no-op
    QVERIFY(service.request(2, 8, 1, NetworkKind::WiFi)); // 5
    service.cancel(2); // 6
    service.cancel(2); // no-op
    service.clear(); // 7, resets the completed counter
    service.clear(); // no-op

    QCOMPARE(spy.count(), 7);
}

QTEST_GUILESS_MAIN(MediaDownloadServiceTest)
#include "tst_media_download_service.moc"
