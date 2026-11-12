#include "ui/PremiumFacade.h"

#include <QDateTime>
#include <QMetaObject>
#include <QMetaProperty>
#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using namespace beygram::ui;

namespace {

constexpr qint64 kDay = 24 * 60 * 60;

/// The countdown is formatted against the wall clock, so every expectation is
/// built from this read rather than a fixed timestamp.
qint64 now()
{
    return QDateTime::currentSecsSinceEpoch();
}

} // namespace

class PremiumFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsNotPremium();

    void setStatusStoresPremiumAndBadge();
    void setStatusNotPremiumDropsBadgeAndExpiry();

    void expiryTextIsEmptyWhenNotPremium();
    void expiryTextWithoutExpiry();
    void expiryTextCountsWholeDaysLeft();
    void expiryTextTodayWhenExpired();

    void setFeaturesStoresSanitizedKeys();
    void setFeatureCatalogFormatsPairs();
    void setFeatureCatalogHandlesMissingDescriptions();
    void setFeatureCatalogSkipsUntitledPairs();
    void setFeaturesDropsStaleCatalog();
    void setFeatureCatalogDoesNotImplyPremium();

    void loadingReflectsState();
    void upgradeEmitsRequest();
    void clearResetsEverything();

    void changedSignalFiresOnlyOnRealChanges();
    void propertiesNotifyThroughChanged();
};

void PremiumFacadeTest::startsNotPremium()
{
    PremiumFacade facade;

    QVERIFY(!facade.premium());
    QVERIFY(facade.badge().isEmpty());
    QVERIFY(facade.expiryText().isEmpty());
    QVERIFY(facade.features().isEmpty());
    QVERIFY(!facade.loading());
}

void PremiumFacadeTest::setStatusStoresPremiumAndBadge()
{
    PremiumFacade facade;
    facade.setStatus(true, now() + 30 * kDay);

    QVERIFY(facade.premium());
    QCOMPARE(facade.badge(), QStringLiteral("Premium"));
    QCOMPARE(facade.expiryText(), QStringLiteral("30 days left"));
}

void PremiumFacadeTest::setStatusNotPremiumDropsBadgeAndExpiry()
{
    PremiumFacade facade;
    facade.setStatus(true, now() + kDay);
    QCOMPARE(facade.badge(), QStringLiteral("Premium"));

    // A non-Premium account must never keep the badge or a leftover countdown,
    // even when the caller passes a future timestamp with it.
    facade.setStatus(false, now() + kDay);

    QVERIFY(!facade.premium());
    QVERIFY(facade.badge().isEmpty());
    QVERIFY(facade.expiryText().isEmpty());
}

void PremiumFacadeTest::expiryTextIsEmptyWhenNotPremium()
{
    PremiumFacade facade;
    facade.setStatus(false, 0);

    // Empty, not "No expiry": there is no subscription to describe.
    QVERIFY(facade.expiryText().isEmpty());
}

void PremiumFacadeTest::expiryTextWithoutExpiry()
{
    PremiumFacade facade;

    // 0 means "no known expiry" (a lifetime subscription or a status without
    // one), and the model clamps a negative timestamp to the same value.
    facade.setStatus(true, 0);
    QCOMPARE(facade.expiryText(), QStringLiteral("No expiry"));

    facade.setStatus(true, -5);
    QCOMPARE(facade.expiryText(), QStringLiteral("No expiry"));
}

void PremiumFacadeTest::expiryTextCountsWholeDaysLeft()
{
    PremiumFacade facade;

    facade.setStatus(true, now() + 30 * kDay);
    QCOMPARE(facade.expiryText(), QStringLiteral("30 days left"));

    // One hour past a whole day is part of the next day, as daysLeft() rounds
    // up; the card renders the model's number rather than truncating it.
    facade.setStatus(true, now() + kDay + 3600);
    QCOMPARE(facade.expiryText(), QStringLiteral("2 days left"));

    facade.setStatus(true, now() + 3600);
    QCOMPARE(facade.expiryText(), QStringLiteral("1 days left"));
}

void PremiumFacadeTest::expiryTextTodayWhenExpired()
{
    PremiumFacade facade;

    // daysLeft() never reports a negative count, so an expiry that has already
    // passed reads as the last day rather than "-3 days left".
    facade.setStatus(true, now() - 60);
    QCOMPARE(facade.expiryText(), QStringLiteral("Expires today"));

    // The flag itself is not re-evaluated against the clock.
    QVERIFY(facade.premium());
}

void PremiumFacadeTest::setFeaturesStoresSanitizedKeys()
{
    PremiumFacade facade;
    facade.setFeatures({QStringLiteral("  premiumFeatureDisabledAds  "),
                        QString(),
                        QStringLiteral("   "),
                        QStringLiteral("premiumFeatureIncreasedLimits"),
                        QStringLiteral("PREMIUMFEATUREDISABLEDADS")});

    // The model trims, drops empty keys and removes later duplicates
    // case-insensitively, keeping the first spelling and the caller's order.
    const QStringList expected{QStringLiteral("premiumFeatureDisabledAds"),
                               QStringLiteral("premiumFeatureIncreasedLimits")};
    QCOMPARE(facade.features(), expected);
}

void PremiumFacadeTest::setFeatureCatalogFormatsPairs()
{
    PremiumFacade facade;
    facade.setFeatureCatalog({QStringLiteral("Disabled Ads"), QStringLiteral("Bigger uploads")},
                             {QStringLiteral("No ads anywhere."), QStringLiteral("4 GB files")});

    const QStringList expected{QStringLiteral("Disabled Ads — No ads anywhere."),
                               QStringLiteral("Bigger uploads — 4 GB files")};
    QCOMPARE(facade.features(), expected);
}

void PremiumFacadeTest::setFeatureCatalogHandlesMissingDescriptions()
{
    PremiumFacade facade;

    // The description list may be shorter than the title list: an extra title
    // renders alone rather than with a dangling separator.
    facade.setFeatureCatalog({QStringLiteral("Bigger uploads"),
                              QStringLiteral("Faster downloads"),
                              QStringLiteral("Stories")},
                             {QStringLiteral("4 GB")});

    const QStringList expected{QStringLiteral("Bigger uploads — 4 GB"),
                               QStringLiteral("Faster downloads"),
                               QStringLiteral("Stories")};
    QCOMPARE(facade.features(), expected);
}

void PremiumFacadeTest::setFeatureCatalogSkipsUntitledPairs()
{
    PremiumFacade facade;

    // A description without a title has no row to label, so it is skipped
    // instead of rendering as a bare "— description".
    facade.setFeatureCatalog({QString(), QStringLiteral("   "), QStringLiteral("Stories")},
                             {QStringLiteral("one"), QStringLiteral("two"),
                              QStringLiteral("Priority support")});

    QCOMPARE(facade.features(),
             QStringList{QStringLiteral("Stories — Priority support")});
}

void PremiumFacadeTest::setFeaturesDropsStaleCatalog()
{
    PremiumFacade facade;
    facade.setFeatures({QStringLiteral("premiumFeatureDisabledAds")});
    facade.setFeatureCatalog({QStringLiteral("Disabled Ads")}, {QStringLiteral("No ads.")});
    QCOMPARE(facade.features(), QStringList{QStringLiteral("Disabled Ads — No ads.")});

    // A new server list must not be shadowed by the titles built for the old
    // one, so the catalog is dropped with it.
    facade.setFeatures({QStringLiteral("premiumFeatureIncreasedLimits")});
    QCOMPARE(facade.features(), QStringList{QStringLiteral("premiumFeatureIncreasedLimits")});
}

void PremiumFacadeTest::setFeatureCatalogDoesNotImplyPremium()
{
    PremiumFacade facade;
    facade.setFeatureCatalog({QStringLiteral("Disabled Ads")}, {QStringLiteral("No ads.")});

    QVERIFY(!facade.premium());
    QVERIFY(facade.badge().isEmpty());
    QVERIFY(facade.expiryText().isEmpty());
}

void PremiumFacadeTest::loadingReflectsState()
{
    PremiumFacade facade;
    QSignalSpy spy(&facade, &PremiumFacade::changed);
    QVERIFY(spy.isValid());

    QVERIFY(!facade.loading());

    facade.setLoading(true);
    QVERIFY(facade.loading());
    QCOMPARE(spy.count(), 1);

    // Re-sending the current value is not a change.
    facade.setLoading(true);
    QCOMPARE(spy.count(), 1);

    facade.setLoading(false);
    QVERIFY(!facade.loading());
    QCOMPARE(spy.count(), 2);
}

void PremiumFacadeTest::upgradeEmitsRequest()
{
    PremiumFacade facade;
    QSignalSpy spy(&facade, &PremiumFacade::upgradeRequested);
    QVERIFY(spy.isValid());

    facade.upgrade();

    QCOMPARE(spy.count(), 1);
    // upgrade() only announces the intent; it starts nothing and changes no
    // state, so a failed purchase leaves the card as it was.
    QVERIFY(!facade.premium());
    QVERIFY(!facade.loading());
}

void PremiumFacadeTest::clearResetsEverything()
{
    PremiumFacade facade;
    facade.setStatus(true, now() + 30 * kDay);
    facade.setFeatures({QStringLiteral("premiumFeatureDisabledAds")});
    facade.setFeatureCatalog({QStringLiteral("Disabled Ads")}, {QStringLiteral("No ads.")});
    facade.setLoading(true);

    facade.clear();

    QVERIFY(!facade.premium());
    QVERIFY(facade.badge().isEmpty());
    QVERIFY(facade.expiryText().isEmpty());
    QVERIFY(facade.features().isEmpty());
    QVERIFY(!facade.loading());
}

void PremiumFacadeTest::changedSignalFiresOnlyOnRealChanges()
{
    PremiumFacade facade;
    QSignalSpy spy(&facade, &PremiumFacade::changed);
    QVERIFY(spy.isValid());

    const qint64 base = now();

    facade.setStatus(true, base + 30 * kDay);
    QCOMPARE(spy.count(), 1);
    facade.setStatus(true, base + 30 * kDay);
    QCOMPARE(spy.count(), 1);

    facade.setFeatures({QStringLiteral("a"), QStringLiteral("b")});
    QCOMPARE(spy.count(), 2);
    // Same keys after sanitising: no repaint.
    facade.setFeatures({QStringLiteral(" a "), QStringLiteral("b"), QStringLiteral("a")});
    QCOMPARE(spy.count(), 2);

    facade.setFeatureCatalog({QStringLiteral("A")}, {});
    QCOMPARE(spy.count(), 3);
    facade.setFeatureCatalog({QStringLiteral(" A ")}, {QStringLiteral("")});
    QCOMPARE(spy.count(), 3);

    facade.setLoading(false);
    QCOMPARE(spy.count(), 3);
    facade.setLoading(true);
    QCOMPARE(spy.count(), 4);

    // clear() reports once even though it drops the status, the catalog and
    // the loading flag together.
    facade.clear();
    QCOMPARE(spy.count(), 5);
    facade.clear();
    QCOMPARE(spy.count(), 5);
}

void PremiumFacadeTest::propertiesNotifyThroughChanged()
{
    PremiumFacade facade;
    const QMetaObject* meta = facade.metaObject();

    const QStringList names{QStringLiteral("premium"), QStringLiteral("badge"),
                            QStringLiteral("expiryText"), QStringLiteral("features"),
                            QStringLiteral("loading")};
    for (const QString& name : names) {
        const int index = meta->indexOfProperty(name.toLatin1().constData());
        QVERIFY2(index >= 0, qPrintable(name));

        const QMetaProperty property = meta->property(index);
        QVERIFY2(property.hasNotifySignal(), qPrintable(name));
        QCOMPARE(QByteArray(property.notifySignal().name()), QByteArrayLiteral("changed"));
    }

    QVERIFY(meta->indexOfSignal("changed()") >= 0);
    QVERIFY(meta->indexOfSignal("upgradeRequested()") >= 0);
    QVERIFY(meta->indexOfMethod("upgrade()") >= 0);
}

QTEST_GUILESS_MAIN(PremiumFacadeTest)
#include "tst_premium_facade.moc"
