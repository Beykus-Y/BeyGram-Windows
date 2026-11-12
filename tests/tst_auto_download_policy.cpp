#include "telegram/AutoDownloadPolicy.h"

#include <QSignalSpy>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

constexpr int MiB = 1024 * 1024;

DownloadLimits uniformLimits(int bytes)
{
    return DownloadLimits{bytes, bytes, bytes};
}

DownloadLimits wifiDefaults()
{
    return DownloadLimits{10 * MiB, 50 * MiB, 20 * MiB};
}

DownloadLimits mobileDefaults()
{
    return DownloadLimits{1 * MiB, 5 * MiB, 2 * MiB};
}

} // namespace

class AutoDownloadPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultsAreTheAndroidBuckets();
    void defaultsAllowWiFiAndRefuseRoaming();
    void setLimitsIsPerNetwork();
    void negativeLimitsAreClampedToZero();
    void setLimitsEmitsChangedOnlyForRealChange();
    void autoDownloadOffRefusesEverything();
    void allowsAcceptsTheCeilingAndRefusesOneByteMore();
    void allowsRefusesNegativeSize();
    void allowsRefusesHugeSize();
    void zeroCeilingRefusesEvenZeroBytes();
    void setPreloadStoriesEmitsChangedOnlyForRealChange();
    void resetToDefaultsRestoresLimitsAndFlags();
    void resetToDefaultsIsSilentWhenAlreadyDefault();
};

void AutoDownloadPolicyTest::defaultsAreTheAndroidBuckets()
{
    const AutoDownloadPolicy policy;

    QVERIFY2(policy.autoDownload(), "auto-download is on by default");
    QVERIFY2(!policy.preloadStories(), "stories are not preloaded by default");

    const DownloadLimits wifi = policy.limits(NetworkKind::WiFi);
    QCOMPARE(wifi.photoBytes, 10 * MiB);
    QCOMPARE(wifi.videoBytes, 50 * MiB);
    QCOMPARE(wifi.fileBytes, 20 * MiB);

    const DownloadLimits mobile = policy.limits(NetworkKind::Mobile);
    QCOMPARE(mobile.photoBytes, 1 * MiB);
    QCOMPARE(mobile.videoBytes, 5 * MiB);
    QCOMPARE(mobile.fileBytes, 2 * MiB);

    // Roaming is off: every ceiling is zero, which means "never".
    const DownloadLimits roaming = policy.limits(NetworkKind::Roaming);
    QCOMPARE(roaming.photoBytes, 0);
    QCOMPARE(roaming.videoBytes, 0);
    QCOMPARE(roaming.fileBytes, 0);
}

void AutoDownloadPolicyTest::defaultsAllowWiFiAndRefuseRoaming()
{
    const AutoDownloadPolicy policy;

    QVERIFY2(policy.allows(NetworkKind::WiFi, 0), "a zero-byte transfer fits any budget");
    QVERIFY(policy.allows(NetworkKind::WiFi, 1));
    QVERIFY(policy.allows(NetworkKind::WiFi, 10 * MiB));
    QVERIFY(policy.allows(NetworkKind::WiFi, 50 * MiB));
    QVERIFY2(!policy.allows(NetworkKind::WiFi, 50 * MiB + 1), "one byte over the widest ceiling");

    QVERIFY(policy.allows(NetworkKind::Mobile, 1 * MiB));
    QVERIFY(policy.allows(NetworkKind::Mobile, 5 * MiB));
    QVERIFY(!policy.allows(NetworkKind::Mobile, 5 * MiB + 1));

    // Zero ceilings are not "unlimited": nothing is fetched on roaming.
    QVERIFY(!policy.allows(NetworkKind::Roaming, 0));
    QVERIFY(!policy.allows(NetworkKind::Roaming, 1));
}

void AutoDownloadPolicyTest::setLimitsIsPerNetwork()
{
    AutoDownloadPolicy policy;
    policy.setLimits(NetworkKind::Mobile, DownloadLimits{3 * MiB, 4 * MiB, 5 * MiB});

    const DownloadLimits mobile = policy.limits(NetworkKind::Mobile);
    QCOMPARE(mobile.photoBytes, 3 * MiB);
    QCOMPARE(mobile.videoBytes, 4 * MiB);
    QCOMPARE(mobile.fileBytes, 5 * MiB);

    // Writing one network leaves the other buckets alone.
    QVERIFY2(policy.limits(NetworkKind::WiFi) == wifiDefaults(), "WiFi keeps its defaults");
    QVERIFY2(policy.limits(NetworkKind::Roaming) == DownloadLimits{}, "roaming stays off");
}

void AutoDownloadPolicyTest::negativeLimitsAreClampedToZero()
{
    AutoDownloadPolicy policy;
    policy.setLimits(NetworkKind::WiFi,
                     DownloadLimits{-1, -5 * MiB, std::numeric_limits<int>::min()});

    const DownloadLimits stored = policy.limits(NetworkKind::WiFi);
    QCOMPARE(stored.photoBytes, 0);
    QCOMPARE(stored.videoBytes, 0);
    QCOMPARE(stored.fileBytes, 0);

    // A clamped ceiling means "never", not "unlimited".
    QVERIFY(!policy.allows(NetworkKind::WiFi, 0));
    QVERIFY(!policy.allows(NetworkKind::WiFi, 1));
}

void AutoDownloadPolicyTest::setLimitsEmitsChangedOnlyForRealChange()
{
    AutoDownloadPolicy policy;
    QSignalSpy spy(&policy, &AutoDownloadPolicy::changed);

    policy.setLimits(NetworkKind::WiFi, DownloadLimits{0, 2, 3});
    QCOMPARE(spy.count(), 1);

    // Writing the stored values again is silent.
    policy.setLimits(NetworkKind::WiFi, DownloadLimits{0, 2, 3});
    QCOMPARE(spy.count(), 1);

    // So is a write that clamps back to what is already stored.
    policy.setLimits(NetworkKind::WiFi, DownloadLimits{-1, 2, 3});
    QCOMPARE(spy.count(), 1);

    // Only a field that really differs counts as a change.
    policy.setLimits(NetworkKind::WiFi, DownloadLimits{0, 2, 4});
    QCOMPARE(spy.count(), 2);

    // A different network is a different bucket, so it is a change too.
    policy.setLimits(NetworkKind::Roaming, DownloadLimits{1, 2, 4});
    QCOMPARE(spy.count(), 3);
}

void AutoDownloadPolicyTest::autoDownloadOffRefusesEverything()
{
    AutoDownloadPolicy policy;
    QSignalSpy spy(&policy, &AutoDownloadPolicy::changed);

    policy.setAutoDownload(false);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!policy.autoDownload());
    QVERIFY2(!policy.allows(NetworkKind::WiFi, 0), "disabled policy refuses even zero bytes");
    QVERIFY(!policy.allows(NetworkKind::WiFi, 1));
    QVERIFY(!policy.allows(NetworkKind::Mobile, 1));

    // Turning it off again is not a change.
    policy.setAutoDownload(false);
    QCOMPARE(spy.count(), 1);

    policy.setAutoDownload(true);
    QCOMPARE(spy.count(), 2);
    QVERIFY(policy.allows(NetworkKind::WiFi, 1));
}

void AutoDownloadPolicyTest::allowsAcceptsTheCeilingAndRefusesOneByteMore()
{
    AutoDownloadPolicy policy;
    policy.setLimits(NetworkKind::Mobile, uniformLimits(2 * MiB));

    QVERIFY(policy.allows(NetworkKind::Mobile, 0));
    QVERIFY(policy.allows(NetworkKind::Mobile, 1));
    QVERIFY(policy.allows(NetworkKind::Mobile, 2 * MiB));
    QVERIFY(!policy.allows(NetworkKind::Mobile, 2 * MiB + 1));

    // Raising the ceiling admits the larger transfer.
    policy.setLimits(NetworkKind::Mobile, uniformLimits(3 * MiB));
    QVERIFY(policy.allows(NetworkKind::Mobile, 2 * MiB + 1));
}

void AutoDownloadPolicyTest::allowsRefusesNegativeSize()
{
    const AutoDownloadPolicy policy;

    QVERIFY2(!policy.allows(NetworkKind::WiFi, -1), "an unknown size is not a budget fit");
    QVERIFY(!policy.allows(NetworkKind::WiFi, -10 * MiB));
    QVERIFY(!policy.allows(NetworkKind::WiFi, std::numeric_limits<qint64>::min()));
}

void AutoDownloadPolicyTest::allowsRefusesHugeSize()
{
    const AutoDownloadPolicy policy;

    QVERIFY(!policy.allows(NetworkKind::WiFi, std::numeric_limits<qint64>::max()));
    QVERIFY(!policy.allows(NetworkKind::Mobile, std::numeric_limits<qint64>::max()));
    QVERIFY(!policy.allows(NetworkKind::Roaming, std::numeric_limits<qint64>::max()));
}

void AutoDownloadPolicyTest::zeroCeilingRefusesEvenZeroBytes()
{
    AutoDownloadPolicy policy;
    policy.setLimits(NetworkKind::WiFi, uniformLimits(0));

    QVERIFY2(!policy.allows(NetworkKind::WiFi, 0), "a zero ceiling means never, not unlimited");
    QVERIFY(!policy.allows(NetworkKind::WiFi, 1));
}

void AutoDownloadPolicyTest::setPreloadStoriesEmitsChangedOnlyForRealChange()
{
    AutoDownloadPolicy policy;
    QSignalSpy spy(&policy, &AutoDownloadPolicy::changed);

    policy.setPreloadStories(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(policy.preloadStories());

    policy.setPreloadStories(true);
    QCOMPARE(spy.count(), 1);

    policy.setPreloadStories(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!policy.preloadStories());

    // The story flag is independent of the auto-download flag.
    QVERIFY(policy.autoDownload());
}

void AutoDownloadPolicyTest::resetToDefaultsRestoresLimitsAndFlags()
{
    AutoDownloadPolicy policy;
    policy.setLimits(NetworkKind::WiFi, uniformLimits(1));
    policy.setLimits(NetworkKind::Roaming, uniformLimits(1));
    policy.setAutoDownload(false);
    policy.setPreloadStories(true);

    QSignalSpy spy(&policy, &AutoDownloadPolicy::changed);
    policy.resetToDefaults();
    QCOMPARE(spy.count(), 1);

    QVERIFY2(policy.limits(NetworkKind::WiFi) == wifiDefaults(), "WiFi defaults restored");
    QVERIFY2(policy.limits(NetworkKind::Mobile) == mobileDefaults(), "Mobile defaults restored");
    QVERIFY2(policy.limits(NetworkKind::Roaming) == DownloadLimits{}, "roaming switched off");
    QVERIFY(policy.autoDownload());
    QVERIFY(!policy.preloadStories());

    // The restored policy behaves like a fresh one.
    QVERIFY(policy.allows(NetworkKind::WiFi, 50 * MiB));
    QVERIFY(!policy.allows(NetworkKind::WiFi, 50 * MiB + 1));
    QVERIFY(!policy.allows(NetworkKind::Roaming, 1));
}

void AutoDownloadPolicyTest::resetToDefaultsIsSilentWhenAlreadyDefault()
{
    AutoDownloadPolicy policy;
    QSignalSpy spy(&policy, &AutoDownloadPolicy::changed);

    // A fresh policy is already at the defaults.
    policy.resetToDefaults();
    QCOMPARE(spy.count(), 0);

    // Writing the default values explicitly does not change anything either.
    policy.setLimits(NetworkKind::WiFi, wifiDefaults());
    policy.setLimits(NetworkKind::Mobile, mobileDefaults());
    policy.setLimits(NetworkKind::Roaming, DownloadLimits{});
    QCOMPARE(spy.count(), 0);

    policy.resetToDefaults();
    QCOMPARE(spy.count(), 0);
}

QTEST_GUILESS_MAIN(AutoDownloadPolicyTest)
#include "tst_auto_download_policy.moc"
