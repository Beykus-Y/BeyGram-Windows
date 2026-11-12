#include "telegram/AccessibilityModel.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class AccessibilityModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsAtDefaults();
    void togglesAreIndependent();
    void repeatedWritesAreSilent();
    void textScaleIsClamped();
    void clampedRepeatIsSilent();
    void largeTextDoesNotMoveScale();
    void resetRestoresDefaultsOnce();
    void resetOnDefaultsIsSilent();
    void isDefaultTracksEveryValue();
};

void AccessibilityModelTest::startsAtDefaults()
{
    AccessibilityModel model;

    QVERIFY(model.isDefault());
    QCOMPARE(model.reduceMotion(), false);
    QCOMPARE(model.highContrast(), false);
    QCOMPARE(model.largeText(), false);
    QCOMPARE(model.textScale(), 100);
    QCOMPARE(model.screenReaderHints(), false);
}

void AccessibilityModelTest::togglesAreIndependent()
{
    AccessibilityModel model;
    QSignalSpy spy(&model, &AccessibilityModel::changed);

    model.setReduceMotion(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(model.reduceMotion());
    QCOMPARE(model.highContrast(), false);
    QCOMPARE(model.largeText(), false);
    QCOMPARE(model.textScale(), 100);
    QCOMPARE(model.screenReaderHints(), false);

    model.setHighContrast(true);
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.highContrast());
    QVERIFY(model.reduceMotion());

    model.setLargeText(true);
    QCOMPARE(spy.count(), 3);
    QVERIFY(model.largeText());

    model.setScreenReaderHints(true);
    QCOMPARE(spy.count(), 4);
    QVERIFY(model.screenReaderHints());
    QCOMPARE(model.textScale(), 100);
}

void AccessibilityModelTest::repeatedWritesAreSilent()
{
    AccessibilityModel model;
    QSignalSpy spy(&model, &AccessibilityModel::changed);

    model.setReduceMotion(true);
    model.setReduceMotion(true);
    QCOMPARE(spy.count(), 1);

    model.setHighContrast(true);
    model.setHighContrast(true);
    QCOMPARE(spy.count(), 2);

    model.setTextScale(120);
    model.setTextScale(120);
    QCOMPARE(spy.count(), 3);

    model.setScreenReaderHints(true);
    model.setScreenReaderHints(true);
    QCOMPARE(spy.count(), 4);

    model.setReduceMotion(false);
    QCOMPARE(spy.count(), 5);
    QCOMPARE(model.reduceMotion(), false);
}

void AccessibilityModelTest::textScaleIsClamped()
{
    AccessibilityModel model;
    QSignalSpy spy(&model, &AccessibilityModel::changed);

    model.setTextScale(85);
    QCOMPARE(model.textScale(), 85);
    QCOMPARE(spy.count(), 1);

    model.setTextScale(150);
    QCOMPARE(model.textScale(), 150);
    QCOMPARE(spy.count(), 2);

    model.setTextScale(0);
    QCOMPARE(model.textScale(), 85);
    QCOMPARE(spy.count(), 3);

    model.setTextScale(-40);
    QCOMPARE(model.textScale(), 85);
    QCOMPARE(spy.count(), 3);

    model.setTextScale(1000);
    QCOMPARE(model.textScale(), 150);
    QCOMPARE(spy.count(), 4);
}

void AccessibilityModelTest::clampedRepeatIsSilent()
{
    AccessibilityModel model;

    model.setTextScale(200);
    QCOMPARE(model.textScale(), 150);

    QSignalSpy spy(&model, &AccessibilityModel::changed);
    model.setTextScale(150);
    model.setTextScale(151);
    model.setTextScale(9999);
    QCOMPARE(model.textScale(), 150);
    QCOMPARE(spy.count(), 0);
}

void AccessibilityModelTest::largeTextDoesNotMoveScale()
{
    AccessibilityModel model;

    model.setLargeText(true);
    QVERIFY(model.largeText());
    QCOMPARE(model.textScale(), 100);

    model.setTextScale(90);
    QVERIFY(model.largeText());
    QCOMPARE(model.textScale(), 90);

    model.setLargeText(false);
    QCOMPARE(model.textScale(), 90);
}

void AccessibilityModelTest::resetRestoresDefaultsOnce()
{
    AccessibilityModel model;

    model.setReduceMotion(true);
    model.setHighContrast(true);
    model.setLargeText(true);
    model.setTextScale(140);
    model.setScreenReaderHints(true);
    QVERIFY(!model.isDefault());

    QSignalSpy spy(&model, &AccessibilityModel::changed);
    model.reset();

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.isDefault());
    QCOMPARE(model.reduceMotion(), false);
    QCOMPARE(model.highContrast(), false);
    QCOMPARE(model.largeText(), false);
    QCOMPARE(model.textScale(), 100);
    QCOMPARE(model.screenReaderHints(), false);
}

void AccessibilityModelTest::resetOnDefaultsIsSilent()
{
    AccessibilityModel model;
    QSignalSpy spy(&model, &AccessibilityModel::changed);

    model.reset();
    model.reset();

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.isDefault());
}

void AccessibilityModelTest::isDefaultTracksEveryValue()
{
    AccessibilityModel model;

    model.setTextScale(101);
    QVERIFY(!model.isDefault());
    model.setTextScale(100);
    QVERIFY(model.isDefault());

    model.setReduceMotion(true);
    QVERIFY(!model.isDefault());
    model.setReduceMotion(false);
    QVERIFY(model.isDefault());

    model.setHighContrast(true);
    QVERIFY(!model.isDefault());
    model.setHighContrast(false);
    QVERIFY(model.isDefault());

    model.setLargeText(true);
    QVERIFY(!model.isDefault());
    model.setLargeText(false);
    QVERIFY(model.isDefault());

    model.setScreenReaderHints(true);
    QVERIFY(!model.isDefault());
    model.setScreenReaderHints(false);
    QVERIFY(model.isDefault());
}

QTEST_GUILESS_MAIN(AccessibilityModelTest)
#include "tst_accessibility_model.moc"
