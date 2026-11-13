#include "telegram/ChatSwipeSettings.h"

#include <QSignalSpy>
#include <QtTest>

using namespace beygram::telegram;

class ChatSwipeSettingsModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsWithTheDefaultGestures();
    void startsSilentAndDefault();
    void setLeftStoresTheAction();
    void setLeftIsSilentForTheStoredAction();
    void setLeftSwapsWithTheRightSide();
    void setRightStoresTheAction();
    void setRightIsSilentForTheStoredAction();
    void setRightSwapsWithTheLeftSide();
    void settersAcceptNone();
    void setSettingsInstallsBothSidesAtOnce();
    void setSettingsIsSilentForTheStoredPair();
    void setSettingsRepairsEqualSides();
    void setSettingsEmitsOnceWhenOnlyOneSideDiffers();
    void actionNamesAreLowercase();
    void actionFromNameParsesEveryStoredName();
    void actionFromNameRejectsUnknownNames();
    void unknownActionValuesReadAsNone();
    void namesRoundTrip();
    void sidesAlwaysDiffer();
    void settingsReturnsACopy();
    void isDefaultTracksBothSides();
    void resetRestoresTheDefaults();
    void resetIsSilentWhenAlreadyDefault();
    void resetEmitsOnce();
    void changedTracksOnlyRealChanges();
};

void ChatSwipeSettingsModelTest::startsWithTheDefaultGestures()
{
    const ChatSwipeSettingsModel model;

    QCOMPARE(model.settings().left, SwipeAction::Archive);
    QCOMPARE(model.settings().right, SwipeAction::Read);
    QVERIFY(model.isDefault());
}

void ChatSwipeSettingsModelTest::startsSilentAndDefault()
{
    ChatSwipeSettingsModel model;
    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);

    QCOMPARE(spy.count(), 0);
    QVERIFY(model.isDefault());
}

void ChatSwipeSettingsModelTest::setLeftStoresTheAction()
{
    ChatSwipeSettingsModel model;
    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);

    QVERIFY(model.setLeft(SwipeAction::Mute));
    QCOMPARE(model.settings().left, SwipeAction::Mute);
    QCOMPARE(model.settings().right, SwipeAction::Read);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.isDefault());
}

void ChatSwipeSettingsModelTest::setLeftIsSilentForTheStoredAction()
{
    ChatSwipeSettingsModel model;
    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);

    QVERIFY(!model.setLeft(SwipeAction::Archive));
    QCOMPARE(model.settings().left, SwipeAction::Archive);
    QCOMPARE(model.settings().right, SwipeAction::Read);
    QCOMPARE(spy.count(), 0);
}

void ChatSwipeSettingsModelTest::setLeftSwapsWithTheRightSide()
{
    ChatSwipeSettingsModel model;
    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);

    // Read already belongs to the right side, so the two swap.
    QVERIFY(model.setLeft(SwipeAction::Read));
    QCOMPARE(model.settings().left, SwipeAction::Read);
    QCOMPARE(model.settings().right, SwipeAction::Archive);
    QCOMPARE(spy.count(), 1);

    // The action now on the left is a no-op for the left side.
    QVERIFY(!model.setLeft(SwipeAction::Read));
    QCOMPARE(spy.count(), 1);

    // The old left moved to the right rather than disappearing.
    QVERIFY(model.setRight(SwipeAction::Read));
    QCOMPARE(model.settings().left, SwipeAction::Archive);
    QCOMPARE(model.settings().right, SwipeAction::Read);
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.isDefault());
}

void ChatSwipeSettingsModelTest::setRightStoresTheAction()
{
    ChatSwipeSettingsModel model;
    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);

    QVERIFY(model.setRight(SwipeAction::Pin));
    QCOMPARE(model.settings().left, SwipeAction::Archive);
    QCOMPARE(model.settings().right, SwipeAction::Pin);
    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.isDefault());
}

void ChatSwipeSettingsModelTest::setRightIsSilentForTheStoredAction()
{
    ChatSwipeSettingsModel model;
    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);

    QVERIFY(!model.setRight(SwipeAction::Read));
    QCOMPARE(model.settings().left, SwipeAction::Archive);
    QCOMPARE(model.settings().right, SwipeAction::Read);
    QCOMPARE(spy.count(), 0);
}

void ChatSwipeSettingsModelTest::setRightSwapsWithTheLeftSide()
{
    ChatSwipeSettingsModel model;
    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);

    // Archive already belongs to the left side, so the two swap.
    QVERIFY(model.setRight(SwipeAction::Archive));
    QCOMPARE(model.settings().left, SwipeAction::Read);
    QCOMPARE(model.settings().right, SwipeAction::Archive);
    QCOMPARE(spy.count(), 1);

    QVERIFY(!model.setRight(SwipeAction::Archive));
    QCOMPARE(spy.count(), 1);
}

void ChatSwipeSettingsModelTest::settersAcceptNone()
{
    ChatSwipeSettingsModel leftModel;
    QVERIFY(leftModel.setLeft(SwipeAction::None));
    QCOMPARE(leftModel.settings().left, SwipeAction::None);
    QCOMPARE(leftModel.settings().right, SwipeAction::Read);

    ChatSwipeSettingsModel rightModel;
    QVERIFY(rightModel.setRight(SwipeAction::None));
    QCOMPARE(rightModel.settings().left, SwipeAction::Archive);
    QCOMPARE(rightModel.settings().right, SwipeAction::None);
}

void ChatSwipeSettingsModelTest::setSettingsInstallsBothSidesAtOnce()
{
    ChatSwipeSettingsModel model;
    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);

    model.setSettings({SwipeAction::Pin, SwipeAction::Mute});

    QCOMPARE(model.settings().left, SwipeAction::Pin);
    QCOMPARE(model.settings().right, SwipeAction::Mute);
    QCOMPARE(spy.count(), 1);
}

void ChatSwipeSettingsModelTest::setSettingsIsSilentForTheStoredPair()
{
    ChatSwipeSettingsModel model;
    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);

    // Writing the defaults over the defaults changes nothing.
    model.setSettings(SwipeSettings{});
    QCOMPARE(spy.count(), 0);
    QVERIFY(model.isDefault());

    model.setSettings({SwipeAction::Delete, SwipeAction::None});
    QCOMPARE(spy.count(), 1);

    model.setSettings({SwipeAction::Delete, SwipeAction::None});
    QCOMPARE(spy.count(), 1);
}

void ChatSwipeSettingsModelTest::setSettingsRepairsEqualSides()
{
    ChatSwipeSettingsModel model;
    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);

    // Equal sides are repaired to the default right.
    model.setSettings({SwipeAction::Delete, SwipeAction::Delete});
    QCOMPARE(model.settings().left, SwipeAction::Delete);
    QCOMPARE(model.settings().right, SwipeAction::Read);
    QCOMPARE(spy.count(), 1);

    // When the requested left is the default right, the repair falls back to
    // the default left so the pair still differs.
    model.setSettings({SwipeAction::Read, SwipeAction::Read});
    QCOMPARE(model.settings().left, SwipeAction::Read);
    QCOMPARE(model.settings().right, SwipeAction::Archive);
    QCOMPARE(spy.count(), 2);

    // The same repaired pair again is silent.
    model.setSettings({SwipeAction::Read, SwipeAction::Read});
    QCOMPARE(spy.count(), 2);

    // Archive is the default left, so the right becomes the default right.
    model.setSettings({SwipeAction::Archive, SwipeAction::Archive});
    QCOMPARE(model.settings().left, SwipeAction::Archive);
    QCOMPARE(model.settings().right, SwipeAction::Read);
    QCOMPARE(spy.count(), 3);

    model.setSettings({SwipeAction::Archive, SwipeAction::Archive});
    QCOMPARE(spy.count(), 3);

    model.setSettings({SwipeAction::None, SwipeAction::None});
    QCOMPARE(model.settings().left, SwipeAction::None);
    QCOMPARE(model.settings().right, SwipeAction::Read);
    QCOMPARE(spy.count(), 4);
}

void ChatSwipeSettingsModelTest::setSettingsEmitsOnceWhenOnlyOneSideDiffers()
{
    ChatSwipeSettingsModel model;
    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);

    model.setSettings({SwipeAction::Archive, SwipeAction::Mute});
    QCOMPARE(model.settings().left, SwipeAction::Archive);
    QCOMPARE(model.settings().right, SwipeAction::Mute);
    QCOMPARE(spy.count(), 1);

    model.setSettings({SwipeAction::Archive, SwipeAction::Mute});
    QCOMPARE(spy.count(), 1);

    model.setSettings({SwipeAction::Archive, SwipeAction::Read});
    QCOMPARE(model.settings().left, SwipeAction::Archive);
    QCOMPARE(model.settings().right, SwipeAction::Read);
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.isDefault());
}

void ChatSwipeSettingsModelTest::actionNamesAreLowercase()
{
    const ChatSwipeSettingsModel model;

    QCOMPARE(model.actionName(SwipeAction::None), QStringLiteral("none"));
    QCOMPARE(model.actionName(SwipeAction::Archive), QStringLiteral("archive"));
    QCOMPARE(model.actionName(SwipeAction::Mute), QStringLiteral("mute"));
    QCOMPARE(model.actionName(SwipeAction::Read), QStringLiteral("read"));
    QCOMPARE(model.actionName(SwipeAction::Pin), QStringLiteral("pin"));
    QCOMPARE(model.actionName(SwipeAction::Delete), QStringLiteral("delete"));
}

void ChatSwipeSettingsModelTest::actionFromNameParsesEveryStoredName()
{
    const ChatSwipeSettingsModel model;

    QCOMPARE(model.actionFromName(QStringLiteral("none")), SwipeAction::None);
    QCOMPARE(model.actionFromName(QStringLiteral("archive")), SwipeAction::Archive);
    QCOMPARE(model.actionFromName(QStringLiteral("mute")), SwipeAction::Mute);
    QCOMPARE(model.actionFromName(QStringLiteral("read")), SwipeAction::Read);
    QCOMPARE(model.actionFromName(QStringLiteral("pin")), SwipeAction::Pin);
    QCOMPARE(model.actionFromName(QStringLiteral("delete")), SwipeAction::Delete);
}

void ChatSwipeSettingsModelTest::actionFromNameRejectsUnknownNames()
{
    const ChatSwipeSettingsModel model;

    // Matching is case-sensitive and only the stored lowercase names count.
    const QString unknown[] = {
        QString(),
        QStringLiteral("Archive"),
        QStringLiteral("ARCHIVE"),
        QStringLiteral("more"),
        QStringLiteral("readunread"),
        QStringLiteral(" archive"),
        QStringLiteral("archive "),
        QStringLiteral("unknown"),
    };

    for (const QString& name : unknown) {
        QVERIFY2(model.actionFromName(name) == SwipeAction::None,
                 qPrintable(QStringLiteral("name parsed as an action: \"%1\"").arg(name)));
    }
}

void ChatSwipeSettingsModelTest::unknownActionValuesReadAsNone()
{
    const ChatSwipeSettingsModel model;

    QCOMPARE(model.actionName(static_cast<SwipeAction>(-1)), QStringLiteral("none"));
    QCOMPARE(model.actionName(static_cast<SwipeAction>(99)), QStringLiteral("none"));
}

void ChatSwipeSettingsModelTest::namesRoundTrip()
{
    const ChatSwipeSettingsModel model;
    const SwipeAction actions[] = {
        SwipeAction::None,
        SwipeAction::Archive,
        SwipeAction::Mute,
        SwipeAction::Read,
        SwipeAction::Pin,
        SwipeAction::Delete,
    };

    for (SwipeAction action : actions)
        QCOMPARE(model.actionFromName(model.actionName(action)), action);
}

void ChatSwipeSettingsModelTest::sidesAlwaysDiffer()
{
    const SwipeAction actions[] = {
        SwipeAction::None,
        SwipeAction::Archive,
        SwipeAction::Mute,
        SwipeAction::Read,
        SwipeAction::Pin,
        SwipeAction::Delete,
    };

    // A single setter can never leave both directions on the same action.
    for (SwipeAction action : actions) {
        ChatSwipeSettingsModel leftModel;
        leftModel.setLeft(action);
        QCOMPARE(leftModel.settings().left, action);
        QVERIFY2(leftModel.settings().left != leftModel.settings().right,
                 "setLeft left both sides on the same action");

        ChatSwipeSettingsModel rightModel;
        rightModel.setRight(action);
        QCOMPARE(rightModel.settings().right, action);
        QVERIFY2(rightModel.settings().left != rightModel.settings().right,
                 "setRight left both sides on the same action");
    }

    // A whole pair is either installed as asked or repaired, never duplicated.
    for (SwipeAction left : actions) {
        for (SwipeAction right : actions) {
            ChatSwipeSettingsModel model;
            model.setSettings({left, right});

            QVERIFY2(model.settings().left != model.settings().right,
                     "setSettings left both sides on the same action");
            if (left != right) {
                QCOMPARE(model.settings().left, left);
                QCOMPARE(model.settings().right, right);
            }
        }
    }
}

void ChatSwipeSettingsModelTest::settingsReturnsACopy()
{
    ChatSwipeSettingsModel model;

    SwipeSettings copy = model.settings();
    copy.left = SwipeAction::Delete;

    QCOMPARE(model.settings().left, SwipeAction::Archive);
    QVERIFY(model.isDefault());
}

void ChatSwipeSettingsModelTest::isDefaultTracksBothSides()
{
    ChatSwipeSettingsModel model;
    QVERIFY(model.isDefault());

    model.setLeft(SwipeAction::Mute);
    QVERIFY(!model.isDefault());
    model.setLeft(SwipeAction::Archive);
    QVERIFY(model.isDefault());

    model.setRight(SwipeAction::Pin);
    QVERIFY(!model.isDefault());
    model.setRight(SwipeAction::Read);
    QVERIFY(model.isDefault());

    // Swapping the defaults keeps the pair valid but is not the default order.
    model.setLeft(SwipeAction::Read);
    QVERIFY(!model.isDefault());
    QCOMPARE(model.settings().left, SwipeAction::Read);
    QCOMPARE(model.settings().right, SwipeAction::Archive);

    // None on either side is not a default either.
    model.reset();
    model.setLeft(SwipeAction::None);
    QVERIFY(!model.isDefault());

    model.reset();
    model.setRight(SwipeAction::None);
    QVERIFY(!model.isDefault());
}

void ChatSwipeSettingsModelTest::resetRestoresTheDefaults()
{
    ChatSwipeSettingsModel model;
    model.setSettings({SwipeAction::Delete, SwipeAction::None});
    QVERIFY(!model.isDefault());

    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);
    model.reset();

    QCOMPARE(model.settings().left, SwipeAction::Archive);
    QCOMPARE(model.settings().right, SwipeAction::Read);
    QVERIFY(model.isDefault());
    QCOMPARE(spy.count(), 1);
}

void ChatSwipeSettingsModelTest::resetIsSilentWhenAlreadyDefault()
{
    ChatSwipeSettingsModel model;
    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);

    model.reset();
    QCOMPARE(spy.count(), 0);
    QVERIFY(model.isDefault());

    // Back at the defaults after a round trip, reset() is silent again.
    model.setLeft(SwipeAction::Mute);
    QCOMPARE(spy.count(), 1);

    model.setLeft(SwipeAction::Archive);
    QCOMPARE(spy.count(), 2);

    model.reset();
    QCOMPARE(spy.count(), 2);
    QVERIFY(model.isDefault());
}

void ChatSwipeSettingsModelTest::resetEmitsOnce()
{
    ChatSwipeSettingsModel model;
    model.setSettings({SwipeAction::Delete, SwipeAction::Pin});

    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);
    model.reset();

    QCOMPARE(spy.count(), 1);
    QVERIFY(model.isDefault());
}

void ChatSwipeSettingsModelTest::changedTracksOnlyRealChanges()
{
    ChatSwipeSettingsModel model;
    QSignalSpy spy(&model, &ChatSwipeSettingsModel::changed);

    // Writing the stored defaults is silent.
    QVERIFY(!model.setLeft(SwipeAction::Archive));
    QVERIFY(!model.setRight(SwipeAction::Read));
    QCOMPARE(spy.count(), 0);

    // Swapping the sides is one real change.
    QVERIFY(model.setLeft(SwipeAction::Read));
    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.setLeft(SwipeAction::Read));
    QCOMPARE(spy.count(), 1);

    // Swapping back is another one.
    QVERIFY(model.setRight(SwipeAction::Read));
    QCOMPARE(spy.count(), 2);

    // A full pair equal to the current one and a reset at the defaults add
    // nothing.
    model.setSettings(SwipeSettings{});
    QCOMPARE(spy.count(), 2);
    model.reset();
    QCOMPARE(spy.count(), 2);

    // A repaired equal pair is one change; repeating it is silent.
    model.setSettings({SwipeAction::Mute, SwipeAction::Mute});
    QCOMPARE(spy.count(), 3);
    model.setSettings({SwipeAction::Mute, SwipeAction::Read});
    QCOMPARE(spy.count(), 3);

    model.reset();
    QCOMPARE(spy.count(), 4);
    model.reset();
    QCOMPARE(spy.count(), 4);
}

QTEST_GUILESS_MAIN(ChatSwipeSettingsModelTest)
#include "tst_chat_swipe_settings.moc"
