#include "telegram/PremiumStatusModel.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

#include <limits>

using namespace beygram::telegram;

namespace {

constexpr qint64 kDay = 24 * 60 * 60;

/// A fixed "now" so every daysLeft() expectation is a literal rather than a
/// clock read.
constexpr qint64 kNow = 1'700'000'000;

} // namespace

class PremiumStatusModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();

    void setPremiumStoresFlagAndExpiry();
    void setPremiumDropsExpiryWhenNotPremium();
    void setPremiumClampsNegativeExpiry();
    void setPremiumIgnoresEquivalentValues();

    void setFeaturesTrimsAndDropsEmpty();
    void setFeaturesDedupesKeepingFirstInOrder();
    void setFeaturesDedupesCaseInsensitively();
    void setFeaturesReplacesPrevious();
    void setFeaturesIgnoresEquivalentInput();
    void setFeaturesAcceptsEmptyList();

    void hasFeatureIsCaseInsensitive();
    void hasFeatureTrimsQuery();
    void hasFeatureRejectsEmptyQuery();
    void hasFeatureSurvivesStatusChange();

    void daysLeftRoundsUp();
    void daysLeftCountsWholeDays();
    void daysLeftIsZeroWhenExpired();
    void daysLeftIsZeroWithoutExpiry();
    void daysLeftIsZeroWhenNotPremium();

    void badgeTextFollowsPremium();

    void clearResetsEverything();
    void clearIsNoOpWhenEmpty();
    void clearKeepsFeaturesOutOfBadge();

    void changedSignalFiresOnlyOnRealChanges();
};

void PremiumStatusModelTest::startsEmpty()
{
    PremiumStatusModel model;

    QVERIFY(!model.premium());
    QCOMPARE(model.expiresAt(), qint64(0));
    QVERIFY(model.features().isEmpty());
    QVERIFY(model.badgeText().isEmpty());
    QCOMPARE(model.daysLeft(kNow), 0);
    QVERIFY(!model.hasFeature(QStringLiteral("premiumFeatureDisabledAds")));
}

void PremiumStatusModelTest::setPremiumStoresFlagAndExpiry()
{
    PremiumStatusModel model;
    model.setPremium(true, kNow + 30 * kDay);

    QVERIFY(model.premium());
    QCOMPARE(model.expiresAt(), kNow + 30 * kDay);
}

void PremiumStatusModelTest::setPremiumDropsExpiryWhenNotPremium()
{
    PremiumStatusModel model;
    model.setPremium(true, kNow + kDay);
    QCOMPARE(model.expiresAt(), kNow + kDay);

    // A non-Premium account must never carry a leftover date.
    model.setPremium(false, kNow + kDay);
    QVERIFY(!model.premium());
    QCOMPARE(model.expiresAt(), qint64(0));

    model.setPremium(false);
    QCOMPARE(model.expiresAt(), qint64(0));
}

void PremiumStatusModelTest::setPremiumClampsNegativeExpiry()
{
    PremiumStatusModel model;
    model.setPremium(true, -1);

    QVERIFY(model.premium());
    QCOMPARE(model.expiresAt(), qint64(0));
    QCOMPARE(model.daysLeft(kNow), 0);

    model.setPremium(true, std::numeric_limits<qint64>::min());
    QCOMPARE(model.expiresAt(), qint64(0));
}

void PremiumStatusModelTest::setPremiumIgnoresEquivalentValues()
{
    PremiumStatusModel model;
    QSignalSpy spy(&model, &PremiumStatusModel::changed);

    model.setPremium(true, kNow + kDay);
    QCOMPARE(spy.count(), 1);

    model.setPremium(true, kNow + kDay);
    QCOMPARE(spy.count(), 1);

    // Dropping the expiry is a real change.
    model.setPremium(true, 0);
    QCOMPARE(spy.count(), 2);

    // Clamping a negative expiry to the already-stored 0 is not.
    model.setPremium(true, -5);
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.expiresAt(), qint64(0));
}

void PremiumStatusModelTest::setFeaturesTrimsAndDropsEmpty()
{
    PremiumStatusModel model;
    model.setFeatures({QStringLiteral("  premiumFeatureDisabledAds  "),
                       QString(),
                       QStringLiteral("   "),
                       QStringLiteral("premiumFeatureIncreasedLimits")});

    const QStringList expected{QStringLiteral("premiumFeatureDisabledAds"),
                               QStringLiteral("premiumFeatureIncreasedLimits")};
    QCOMPARE(model.features(), expected);
}

void PremiumStatusModelTest::setFeaturesDedupesKeepingFirstInOrder()
{
    PremiumStatusModel model;
    model.setFeatures({QStringLiteral("c"), QStringLiteral("a"),
                       QStringLiteral("b"), QStringLiteral("a"),
                       QStringLiteral("c")});

    const QStringList expected{QStringLiteral("c"), QStringLiteral("a"),
                               QStringLiteral("b")};
    QCOMPARE(model.features(), expected);
}

void PremiumStatusModelTest::setFeaturesDedupesCaseInsensitively()
{
    PremiumStatusModel model;
    model.setFeatures({QStringLiteral("premiumFeatureDisabledAds"),
                       QStringLiteral("PREMIUMFEATUREDISABLEDADS"),
                       QStringLiteral("PremiumFeatureDisabledAds")});

    // hasFeature() is case-insensitive, so these three are one key: the first
    // spelling wins.
    QCOMPARE(model.features(),
             QStringList{QStringLiteral("premiumFeatureDisabledAds")});
}

void PremiumStatusModelTest::setFeaturesReplacesPrevious()
{
    PremiumStatusModel model;
    model.setFeatures({QStringLiteral("a"), QStringLiteral("b")});
    model.setFeatures({QStringLiteral("c")});

    QCOMPARE(model.features(), QStringList{QStringLiteral("c")});
    QVERIFY(!model.hasFeature(QStringLiteral("a")));
    QVERIFY(model.hasFeature(QStringLiteral("c")));
}

void PremiumStatusModelTest::setFeaturesIgnoresEquivalentInput()
{
    PremiumStatusModel model;
    QSignalSpy spy(&model, &PremiumStatusModel::changed);

    model.setFeatures({QStringLiteral(" a "), QStringLiteral("b")});
    QCOMPARE(spy.count(), 1);
    QCOMPARE(model.features(),
             (QStringList{QStringLiteral("a"), QStringLiteral("b")}));

    // Same keys after sanitising: no repaint.
    model.setFeatures({QStringLiteral("a"), QStringLiteral("b"),
                       QStringLiteral("a")});
    QCOMPARE(spy.count(), 1);

    // A different spelling is a real change to the stored list.
    model.setFeatures({QStringLiteral("A"), QStringLiteral("b")});
    QCOMPARE(spy.count(), 2);
    QCOMPARE(model.features(),
             (QStringList{QStringLiteral("A"), QStringLiteral("b")}));
}

void PremiumStatusModelTest::setFeaturesAcceptsEmptyList()
{
    PremiumStatusModel model;
    model.setFeatures({QStringLiteral("a")});
    QCOMPARE(model.features().size(), qsizetype(1));

    model.setFeatures(QStringList());
    QVERIFY(model.features().isEmpty());
}

void PremiumStatusModelTest::hasFeatureIsCaseInsensitive()
{
    PremiumStatusModel model;
    model.setFeatures({QStringLiteral("premiumFeatureDisabledAds"),
                       QStringLiteral("premiumFeatureIncreasedLimits")});

    QVERIFY(model.hasFeature(QStringLiteral("premiumFeatureDisabledAds")));
    QVERIFY(model.hasFeature(QStringLiteral("PREMIUMFEATUREDISABLEDADS")));
    QVERIFY(model.hasFeature(QStringLiteral("PremiumFeatureIncreasedLimits")));
    QVERIFY(!model.hasFeature(QStringLiteral("premiumFeatureStories")));
}

void PremiumStatusModelTest::hasFeatureTrimsQuery()
{
    PremiumStatusModel model;
    model.setFeatures({QStringLiteral("premiumFeatureDisabledAds")});

    QVERIFY(model.hasFeature(QStringLiteral("  premiumFeatureDisabledAds  ")));
}

void PremiumStatusModelTest::hasFeatureRejectsEmptyQuery()
{
    PremiumStatusModel model;
    model.setFeatures({QStringLiteral("premiumFeatureDisabledAds")});

    QVERIFY(!model.hasFeature(QString()));
    QVERIFY(!model.hasFeature(QStringLiteral("   ")));
}

void PremiumStatusModelTest::hasFeatureSurvivesStatusChange()
{
    PremiumStatusModel model;
    model.setFeatures({QStringLiteral("premiumFeatureDisabledAds")});

    // Documented behaviour: setPremium() never touches the feature list, so a
    // caller that only counts features on a paying account checks premium().
    model.setPremium(false);
    QVERIFY(model.hasFeature(QStringLiteral("premiumFeatureDisabledAds")));

    model.setPremium(true, kNow + kDay);
    QVERIFY(model.hasFeature(QStringLiteral("premiumFeatureDisabledAds")));

    model.clear();
    QVERIFY(!model.hasFeature(QStringLiteral("premiumFeatureDisabledAds")));
}

void PremiumStatusModelTest::daysLeftRoundsUp()
{
    PremiumStatusModel model;

    model.setPremium(true, kNow + kDay);
    QCOMPARE(model.daysLeft(kNow), 1);

    // One second past a whole day is still part of the next day.
    model.setPremium(true, kNow + kDay + 1);
    QCOMPARE(model.daysLeft(kNow), 2);

    // An hour left reports a whole day rather than zero.
    model.setPremium(true, kNow + 3600);
    QCOMPARE(model.daysLeft(kNow), 1);

    // A single second left rounds up to one day too.
    model.setPremium(true, kNow + 1);
    QCOMPARE(model.daysLeft(kNow), 1);
}

void PremiumStatusModelTest::daysLeftCountsWholeDays()
{
    PremiumStatusModel model;
    model.setPremium(true, kNow + 30 * kDay);

    QCOMPARE(model.daysLeft(kNow), 30);
    QCOMPARE(model.daysLeft(kNow + 10 * kDay), 20);
    QCOMPARE(model.daysLeft(kNow + 29 * kDay), 1);
    QCOMPARE(model.daysLeft(kNow + 30 * kDay - 1), 1);
}

void PremiumStatusModelTest::daysLeftIsZeroWhenExpired()
{
    PremiumStatusModel model;
    model.setPremium(true, kNow + kDay);

    // Exactly at the expiry and after it: no negative countdown.
    QCOMPARE(model.daysLeft(kNow + kDay), 0);
    QCOMPARE(model.daysLeft(kNow + 2 * kDay), 0);
    QCOMPARE(model.daysLeft(kNow + 365 * kDay), 0);
}

void PremiumStatusModelTest::daysLeftIsZeroWithoutExpiry()
{
    PremiumStatusModel model;
    model.setPremium(true);

    // 0 expiry means "no known expiry"; the countdown reports 0 rather than -1
    // or a bogus number.
    QCOMPARE(model.expiresAt(), qint64(0));
    QCOMPARE(model.daysLeft(kNow), 0);

    model.setPremium(true, -100);
    QCOMPARE(model.daysLeft(kNow), 0);
}

void PremiumStatusModelTest::daysLeftIsZeroWhenNotPremium()
{
    PremiumStatusModel model;
    model.setPremium(false, kNow + 30 * kDay);

    QCOMPARE(model.expiresAt(), qint64(0));
    QCOMPARE(model.daysLeft(kNow), 0);
}

void PremiumStatusModelTest::badgeTextFollowsPremium()
{
    PremiumStatusModel model;
    QVERIFY(model.badgeText().isEmpty());

    model.setPremium(true, kNow + kDay);
    QCOMPARE(model.badgeText(), QStringLiteral("Premium"));

    model.setPremium(false);
    QVERIFY(model.badgeText().isEmpty());
}

void PremiumStatusModelTest::clearResetsEverything()
{
    PremiumStatusModel model;
    model.setPremium(true, kNow + kDay);
    model.setFeatures({QStringLiteral("a"), QStringLiteral("b")});

    model.clear();

    QVERIFY(!model.premium());
    QCOMPARE(model.expiresAt(), qint64(0));
    QVERIFY(model.features().isEmpty());
    QVERIFY(model.badgeText().isEmpty());
    QCOMPARE(model.daysLeft(kNow), 0);
    QVERIFY(!model.hasFeature(QStringLiteral("a")));
}

void PremiumStatusModelTest::clearIsNoOpWhenEmpty()
{
    PremiumStatusModel model;
    QSignalSpy spy(&model, &PremiumStatusModel::changed);

    model.clear();
    QCOMPARE(spy.count(), 0);

    model.clear();
    QCOMPARE(spy.count(), 0);
}

void PremiumStatusModelTest::clearKeepsFeaturesOutOfBadge()
{
    PremiumStatusModel model;
    QSignalSpy spy(&model, &PremiumStatusModel::changed);

    // Features alone do not make the account Premium.
    model.setFeatures({QStringLiteral("premiumFeatureDisabledAds")});
    QCOMPARE(spy.count(), 1);
    QVERIFY(model.badgeText().isEmpty());
    QVERIFY(!model.premium());
}

void PremiumStatusModelTest::changedSignalFiresOnlyOnRealChanges()
{
    PremiumStatusModel model;
    QSignalSpy spy(&model, &PremiumStatusModel::changed);

    model.setPremium(true, kNow + kDay);
    QCOMPARE(spy.count(), 1);

    model.setPremium(true, kNow + kDay);
    QCOMPARE(spy.count(), 1);

    model.setFeatures({QStringLiteral("a")});
    QCOMPARE(spy.count(), 2);

    model.setFeatures({QStringLiteral(" a ")});
    QCOMPARE(spy.count(), 2);

    // Turning Premium off while it already has no expiry is still a change of
    // the flag itself.
    model.setPremium(false);
    QCOMPARE(spy.count(), 3);

    model.setPremium(false);
    QCOMPARE(spy.count(), 3);

    model.clear();
    QCOMPARE(spy.count(), 4);

    model.clear();
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(PremiumStatusModelTest)
#include "tst_premium_status_model.moc"
