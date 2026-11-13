#include "ui/SwipeSettingsFacade.h"

#include <QSignalSpy>
#include <QStringList>
#include <QtTest>

using namespace beygram::ui;

namespace {

QStringList expectedActions()
{
    return {QStringLiteral("none"), QStringLiteral("archive"),
            QStringLiteral("mute"), QStringLiteral("read"),
            QStringLiteral("pin"), QStringLiteral("delete")};
}

} // namespace

class SwipeSettingsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsAtTheDefaults();
    void actionsListsEveryGestureInDisplayOrder();
    void actionsAreStableAndUnique();
    void setLeftStoresTheAction();
    void setLeftIsSilentForTheStoredAction();
    void setLeftSwapsWithTheRightSide();
    void setRightStoresTheAction();
    void setRightIsSilentForTheStoredAction();
    void setRightSwapsWithTheLeftSide();
    void settersAcceptNone();
    void unknownNamesAreIgnored();
    void unknownNamesNeverFallBackToNone();
    void everyOfferedActionRoundTrips();
    void sidesAlwaysDiffer();
    void changedTracksOnlyRealChanges();
    void resetRestoresTheDefaults();
    void resetIsSilentAtTheDefaults();
    void resetEmitsOnce();
};

void SwipeSettingsFacadeTest::startsAtTheDefaults()
{
    const SwipeSettingsFacade facade;

    QCOMPARE(facade.leftAction(), QStringLiteral("archive"));
    QCOMPARE(facade.rightAction(), QStringLiteral("read"));
    QCOMPARE(facade.actions(), expectedActions());
}

void SwipeSettingsFacadeTest::actionsListsEveryGestureInDisplayOrder()
{
    const SwipeSettingsFacade facade;

    // The picker's order, with the inert choice first. The names are the stored
    // ones the setters accept, not localized labels.
    QCOMPARE(facade.actions(), expectedActions());
}

void SwipeSettingsFacadeTest::actionsAreStableAndUnique()
{
    SwipeSettingsFacade facade;
    facade.setLeft(QStringLiteral("pin"));

    const QStringList first = facade.actions();
    const QStringList second = facade.actions();

    QCOMPARE(first, second);
    QCOMPARE(first, expectedActions());

    QStringList unique = first;
    unique.removeDuplicates();
    QCOMPARE(unique.size(), first.size());
}

void SwipeSettingsFacadeTest::setLeftStoresTheAction()
{
    SwipeSettingsFacade facade;
    QSignalSpy changed(&facade, &SwipeSettingsFacade::changed);

    facade.setLeft(QStringLiteral("pin"));

    QCOMPARE(facade.leftAction(), QStringLiteral("pin"));
    QCOMPARE(facade.rightAction(), QStringLiteral("read"));
    QCOMPARE(changed.count(), 1);
}

void SwipeSettingsFacadeTest::setLeftIsSilentForTheStoredAction()
{
    SwipeSettingsFacade facade;
    QSignalSpy changed(&facade, &SwipeSettingsFacade::changed);

    facade.setLeft(QStringLiteral("archive"));

    QCOMPARE(facade.leftAction(), QStringLiteral("archive"));
    QCOMPARE(facade.rightAction(), QStringLiteral("read"));
    QCOMPARE(changed.count(), 0);
}

void SwipeSettingsFacadeTest::setLeftSwapsWithTheRightSide()
{
    SwipeSettingsFacade facade;
    QSignalSpy changed(&facade, &SwipeSettingsFacade::changed);

    // Read already belongs to the right side, so the two swap rather than
    // leaving both directions on the same gesture.
    facade.setLeft(QStringLiteral("read"));

    QCOMPARE(facade.leftAction(), QStringLiteral("read"));
    QCOMPARE(facade.rightAction(), QStringLiteral("archive"));
    QCOMPARE(changed.count(), 1);

    // The swapped-in action is now a no-op for the left side.
    facade.setLeft(QStringLiteral("read"));
    QCOMPARE(changed.count(), 1);
}

void SwipeSettingsFacadeTest::setRightStoresTheAction()
{
    SwipeSettingsFacade facade;
    QSignalSpy changed(&facade, &SwipeSettingsFacade::changed);

    facade.setRight(QStringLiteral("mute"));

    QCOMPARE(facade.leftAction(), QStringLiteral("archive"));
    QCOMPARE(facade.rightAction(), QStringLiteral("mute"));
    QCOMPARE(changed.count(), 1);
}

void SwipeSettingsFacadeTest::setRightIsSilentForTheStoredAction()
{
    SwipeSettingsFacade facade;
    QSignalSpy changed(&facade, &SwipeSettingsFacade::changed);

    facade.setRight(QStringLiteral("read"));

    QCOMPARE(facade.leftAction(), QStringLiteral("archive"));
    QCOMPARE(facade.rightAction(), QStringLiteral("read"));
    QCOMPARE(changed.count(), 0);
}

void SwipeSettingsFacadeTest::setRightSwapsWithTheLeftSide()
{
    SwipeSettingsFacade facade;
    QSignalSpy changed(&facade, &SwipeSettingsFacade::changed);

    // Archive already belongs to the left side, so the two swap.
    facade.setRight(QStringLiteral("archive"));

    QCOMPARE(facade.leftAction(), QStringLiteral("read"));
    QCOMPARE(facade.rightAction(), QStringLiteral("archive"));
    QCOMPARE(changed.count(), 1);

    facade.setRight(QStringLiteral("archive"));
    QCOMPARE(changed.count(), 1);
}

void SwipeSettingsFacadeTest::settersAcceptNone()
{
    SwipeSettingsFacade leftFacade;
    QSignalSpy leftChanged(&leftFacade, &SwipeSettingsFacade::changed);

    leftFacade.setLeft(QStringLiteral("none"));

    QCOMPARE(leftFacade.leftAction(), QStringLiteral("none"));
    QCOMPARE(leftFacade.rightAction(), QStringLiteral("read"));
    QCOMPARE(leftChanged.count(), 1);

    SwipeSettingsFacade rightFacade;
    QSignalSpy rightChanged(&rightFacade, &SwipeSettingsFacade::changed);

    rightFacade.setRight(QStringLiteral("none"));

    QCOMPARE(rightFacade.leftAction(), QStringLiteral("archive"));
    QCOMPARE(rightFacade.rightAction(), QStringLiteral("none"));
    QCOMPARE(rightChanged.count(), 1);
}

void SwipeSettingsFacadeTest::unknownNamesAreIgnored()
{
    SwipeSettingsFacade facade;
    QSignalSpy changed(&facade, &SwipeSettingsFacade::changed);

    // Empty, differently-cased, padded and invented names are not gestures the
    // model stores, so neither setter may accept them.
    const QString unknown[] = {
        QString(),
        QStringLiteral("Pin"),
        QStringLiteral("ARCHIVE"),
        QStringLiteral("Read"),
        QStringLiteral(" archive"),
        QStringLiteral("archive "),
        QStringLiteral("markread"),
        QStringLiteral("bogus"),
    };

    for (const QString& name : unknown) {
        facade.setLeft(name);
        facade.setRight(name);
    }

    QCOMPARE(facade.leftAction(), QStringLiteral("archive"));
    QCOMPARE(facade.rightAction(), QStringLiteral("read"));
    QCOMPARE(changed.count(), 0);
}

void SwipeSettingsFacadeTest::unknownNamesNeverFallBackToNone()
{
    SwipeSettingsFacade facade;
    QSignalSpy changed(&facade, &SwipeSettingsFacade::changed);

    // The model reads an unknown name as None. Forwarding that would switch the
    // gesture off on a typo, which is worse than ignoring the write.
    facade.setLeft(QStringLiteral("bogus"));
    facade.setRight(QStringLiteral("also-bogus"));

    QCOMPARE(facade.leftAction(), QStringLiteral("archive"));
    QCOMPARE(facade.rightAction(), QStringLiteral("read"));
    QCOMPARE(changed.count(), 0);
}

void SwipeSettingsFacadeTest::everyOfferedActionRoundTrips()
{
    const QStringList offered = SwipeSettingsFacade().actions();
    QCOMPARE(offered, expectedActions());

    for (const QString& name : offered) {
        SwipeSettingsFacade leftFacade;
        leftFacade.setLeft(name);

        QCOMPARE(leftFacade.leftAction(), name);
        QVERIFY2(leftFacade.leftAction() != leftFacade.rightAction(),
                 "setLeft left both directions on the same action");

        SwipeSettingsFacade rightFacade;
        rightFacade.setRight(name);

        QCOMPARE(rightFacade.rightAction(), name);
        QVERIFY2(rightFacade.leftAction() != rightFacade.rightAction(),
                 "setRight left both directions on the same action");
    }
}

void SwipeSettingsFacadeTest::sidesAlwaysDiffer()
{
    const QStringList offered = SwipeSettingsFacade().actions();

    for (const QString& left : offered) {
        for (const QString& right : offered) {
            SwipeSettingsFacade leftFirst;
            leftFirst.setLeft(left);
            leftFirst.setRight(right);

            QVERIFY2(leftFirst.leftAction() != leftFirst.rightAction(),
                     qPrintable(QStringLiteral("left %1 then right %2 collided")
                                    .arg(left, right)));

            SwipeSettingsFacade rightFirst;
            rightFirst.setRight(right);
            rightFirst.setLeft(left);

            QVERIFY2(rightFirst.leftAction() != rightFirst.rightAction(),
                     qPrintable(QStringLiteral("right %1 then left %2 collided")
                                    .arg(right, left)));
        }
    }
}

void SwipeSettingsFacadeTest::changedTracksOnlyRealChanges()
{
    SwipeSettingsFacade facade;
    QSignalSpy changed(&facade, &SwipeSettingsFacade::changed);

    // Writing the stored defaults is silent.
    facade.setLeft(QStringLiteral("archive"));
    facade.setRight(QStringLiteral("read"));
    QCOMPARE(changed.count(), 0);

    // Swapping the sides is one real change.
    facade.setLeft(QStringLiteral("read"));
    QCOMPARE(changed.count(), 1);
    facade.setLeft(QStringLiteral("read"));
    QCOMPARE(changed.count(), 1);

    // Swapping back is another one.
    facade.setRight(QStringLiteral("read"));
    QCOMPARE(changed.count(), 2);

    // A reset at the defaults adds nothing.
    facade.reset();
    QCOMPARE(changed.count(), 2);

    // A single write and the swap it causes are one signal each.
    facade.setLeft(QStringLiteral("mute"));
    QCOMPARE(changed.count(), 3);
    facade.setRight(QStringLiteral("mute"));
    QCOMPARE(changed.count(), 4);

    facade.reset();
    QCOMPARE(changed.count(), 5);
    facade.reset();
    QCOMPARE(changed.count(), 5);
}

void SwipeSettingsFacadeTest::resetRestoresTheDefaults()
{
    SwipeSettingsFacade facade;
    facade.setLeft(QStringLiteral("delete"));
    facade.setRight(QStringLiteral("pin"));
    QVERIFY(facade.leftAction() != QStringLiteral("archive"));

    QSignalSpy changed(&facade, &SwipeSettingsFacade::changed);
    facade.reset();

    QCOMPARE(facade.leftAction(), QStringLiteral("archive"));
    QCOMPARE(facade.rightAction(), QStringLiteral("read"));
    QCOMPARE(facade.actions(), expectedActions());
    QCOMPARE(changed.count(), 1);
}

void SwipeSettingsFacadeTest::resetIsSilentAtTheDefaults()
{
    SwipeSettingsFacade facade;
    QSignalSpy changed(&facade, &SwipeSettingsFacade::changed);

    facade.reset();
    facade.reset();

    QCOMPARE(facade.leftAction(), QStringLiteral("archive"));
    QCOMPARE(facade.rightAction(), QStringLiteral("read"));
    QCOMPARE(changed.count(), 0);
}

void SwipeSettingsFacadeTest::resetEmitsOnce()
{
    SwipeSettingsFacade facade;
    facade.setLeft(QStringLiteral("none"));
    facade.setRight(QStringLiteral("delete"));

    QSignalSpy changed(&facade, &SwipeSettingsFacade::changed);
    facade.reset();

    QCOMPARE(changed.count(), 1);
    QCOMPARE(facade.leftAction(), QStringLiteral("archive"));
    QCOMPARE(facade.rightAction(), QStringLiteral("read"));
}

QTEST_GUILESS_MAIN(SwipeSettingsFacadeTest)
#include "tst_swipe_settings_facade.moc"
