#include "telegram/ChatActionBarModel.h"

#include <QSignalSpy>
#include <QString>
#include <QtTest>

using namespace beygram::telegram;

class ChatActionBarModelTest final : public QObject {
    Q_OBJECT

private slots:
    void startsInvisible();
    void storesKindAndText();
    void trimsText();
    void allowsEmptyTextForVisibleKind();
    void noneClearsTextAndBusy();
    void noneOnEmptyStateIsSilent();
    void setActionEmitsOnlyOnRealChange();
    void keepsBusyWhenKindChanges();
    void everyKindIsVisible();
    void busyTracksVisibleBar();
    void busyIgnoredWhileInvisible();
    void busyEmitsOnlyOnRealChange();
    void clearResetsEverything();
    void clearIsNoOpWhenAlreadyEmpty();
    void clearEmitsOnceFromBusyState();
    void changedTracksOnlyRealChanges();
};

void ChatActionBarModelTest::startsInvisible()
{
    ChatActionBarModel model;
    QSignalSpy spy(&model, &ChatActionBarModel::changed);

    QCOMPARE(model.kind(), ActionBarKind::None);
    QVERIFY(model.text().isEmpty());
    QVERIFY(!model.visible());
    QVERIFY(!model.busy());
    QCOMPARE(spy.count(), 0);
}

void ChatActionBarModelTest::storesKindAndText()
{
    ChatActionBarModel model;

    model.setAction(ActionBarKind::ReportSpam, QStringLiteral("Report spam"));

    QCOMPARE(model.kind(), ActionBarKind::ReportSpam);
    QCOMPARE(model.text(), QStringLiteral("Report spam"));
    QVERIFY(model.visible());
    QVERIFY(!model.busy());
}

void ChatActionBarModelTest::trimsText()
{
    ChatActionBarModel model;

    model.setAction(ActionBarKind::AddContact, QStringLiteral("  Add to contacts \n"));

    QCOMPARE(model.text(), QStringLiteral("Add to contacts"));

    // Inner whitespace is content and survives the trim.
    model.setAction(ActionBarKind::AddContact, QStringLiteral("  Add  to contacts  "));
    QCOMPARE(model.text(), QStringLiteral("Add  to contacts"));
}

void ChatActionBarModelTest::allowsEmptyTextForVisibleKind()
{
    ChatActionBarModel model;

    // A bar may be shown before Telegram's localised label arrives.
    model.setAction(ActionBarKind::SharePhone, QString());

    QCOMPARE(model.kind(), ActionBarKind::SharePhone);
    QVERIFY(model.text().isEmpty());
    QVERIFY(model.visible());

    // Whitespace-only text is stored as empty too, not as blanks.
    model.setAction(ActionBarKind::SharePhone, QStringLiteral("   \t "));
    QVERIFY(model.text().isEmpty());
    QVERIFY(model.visible());
}

void ChatActionBarModelTest::noneClearsTextAndBusy()
{
    ChatActionBarModel model;
    model.setAction(ActionBarKind::BlockUser, QStringLiteral("Block"));
    model.setBusy(true);

    model.setAction(ActionBarKind::None, QStringLiteral("stale label"));

    QCOMPARE(model.kind(), ActionBarKind::None);
    QVERIFY(model.text().isEmpty());
    QVERIFY(!model.visible());
    QVERIFY(!model.busy());
}

void ChatActionBarModelTest::noneOnEmptyStateIsSilent()
{
    ChatActionBarModel model;
    QSignalSpy spy(&model, &ChatActionBarModel::changed);

    model.setAction(ActionBarKind::None, QStringLiteral("ignored"));

    QCOMPARE(spy.count(), 0);
    QVERIFY(!model.visible());
}

void ChatActionBarModelTest::setActionEmitsOnlyOnRealChange()
{
    ChatActionBarModel model;
    QSignalSpy spy(&model, &ChatActionBarModel::changed);

    model.setAction(ActionBarKind::UnblockUser, QStringLiteral("Unblock"));
    QCOMPARE(spy.count(), 1);

    // Same kind and text after trimming is not a change.
    model.setAction(ActionBarKind::UnblockUser, QStringLiteral("  Unblock  "));
    QCOMPARE(spy.count(), 1);

    model.setAction(ActionBarKind::UnblockUser, QStringLiteral("Unblock user"));
    QCOMPARE(spy.count(), 2);

    model.setAction(ActionBarKind::BlockUser, QStringLiteral("Unblock user"));
    QCOMPARE(spy.count(), 3);

    model.setAction(ActionBarKind::None, QString());
    QCOMPARE(spy.count(), 4);

    model.setAction(ActionBarKind::None, QString());
    QCOMPARE(spy.count(), 4);
}

void ChatActionBarModelTest::keepsBusyWhenKindChanges()
{
    ChatActionBarModel model;
    model.setAction(ActionBarKind::ReportSpam, QStringLiteral("Report spam"));
    model.setBusy(true);

    // The request already in flight still belongs to this bar, so a new kind
    // does not silently drop the disabled state.
    model.setAction(ActionBarKind::AddContact, QStringLiteral("Add contact"));

    QCOMPARE(model.kind(), ActionBarKind::AddContact);
    QCOMPARE(model.text(), QStringLiteral("Add contact"));
    QVERIFY(model.busy());
}

void ChatActionBarModelTest::everyKindIsVisible()
{
    ChatActionBarModel model;

    const ActionBarKind kinds[] = {
        ActionBarKind::ReportSpam, ActionBarKind::AddContact,
        ActionBarKind::SharePhone, ActionBarKind::BlockUser,
        ActionBarKind::UnblockUser, ActionBarKind::JoinGroup,
        ActionBarKind::RestoreChat, ActionBarKind::ReportLocation,
    };

    for (const ActionBarKind kind : kinds) {
        model.setAction(kind, QStringLiteral("label"));
        QCOMPARE(model.kind(), kind);
        QVERIFY(model.visible());
        QCOMPARE(model.text(), QStringLiteral("label"));
    }
}

void ChatActionBarModelTest::busyTracksVisibleBar()
{
    ChatActionBarModel model;
    model.setAction(ActionBarKind::JoinGroup, QStringLiteral("Join"));

    model.setBusy(true);
    QVERIFY(model.busy());

    model.setBusy(false);
    QVERIFY(!model.busy());
}

void ChatActionBarModelTest::busyIgnoredWhileInvisible()
{
    ChatActionBarModel model;
    QSignalSpy spy(&model, &ChatActionBarModel::changed);

    model.setBusy(true);

    QVERIFY(!model.busy());
    QCOMPARE(spy.count(), 0);

    // Clearing back to None drops the flag, and busy cannot come back after it.
    model.setAction(ActionBarKind::RestoreChat, QStringLiteral("Restore"));
    model.setBusy(true);
    model.clear();
    model.setBusy(true);

    QVERIFY(!model.visible());
    QVERIFY(!model.busy());
}

void ChatActionBarModelTest::busyEmitsOnlyOnRealChange()
{
    ChatActionBarModel model;
    model.setAction(ActionBarKind::ReportLocation, QStringLiteral("Report"));
    QSignalSpy spy(&model, &ChatActionBarModel::changed);

    model.setBusy(true);
    QCOMPARE(spy.count(), 1);

    model.setBusy(true);
    QCOMPARE(spy.count(), 1);

    model.setBusy(false);
    QCOMPARE(spy.count(), 2);

    model.setBusy(false);
    QCOMPARE(spy.count(), 2);
}

void ChatActionBarModelTest::clearResetsEverything()
{
    ChatActionBarModel model;
    model.setAction(ActionBarKind::SharePhone, QStringLiteral("Share phone"));
    model.setBusy(true);
    QSignalSpy spy(&model, &ChatActionBarModel::changed);

    model.clear();

    QCOMPARE(model.kind(), ActionBarKind::None);
    QVERIFY(model.text().isEmpty());
    QVERIFY(!model.visible());
    QVERIFY(!model.busy());
    QCOMPARE(spy.count(), 1);
}

void ChatActionBarModelTest::clearIsNoOpWhenAlreadyEmpty()
{
    ChatActionBarModel model;
    QSignalSpy spy(&model, &ChatActionBarModel::changed);

    model.clear();
    model.clear();

    QCOMPARE(spy.count(), 0);
    QCOMPARE(model.kind(), ActionBarKind::None);
}

void ChatActionBarModelTest::clearEmitsOnceFromBusyState()
{
    ChatActionBarModel model;
    model.setAction(ActionBarKind::BlockUser, QStringLiteral("Block"));
    model.setBusy(true);
    QSignalSpy spy(&model, &ChatActionBarModel::changed);

    model.clear();

    QCOMPARE(spy.count(), 1);
    QVERIFY(!model.busy());

    // A second clear has nothing left to drop.
    model.clear();
    QCOMPARE(spy.count(), 1);
}

void ChatActionBarModelTest::changedTracksOnlyRealChanges()
{
    ChatActionBarModel model;
    QSignalSpy spy(&model, &ChatActionBarModel::changed);

    model.setBusy(true);                                                  // invisible
    model.clear();                                                        // already empty
    model.setAction(ActionBarKind::None, QStringLiteral("ignored"));      // invisible
    QCOMPARE(spy.count(), 0);

    model.setAction(ActionBarKind::AddContact, QString());                // visible
    QCOMPARE(spy.count(), 1);

    model.setBusy(true);                                                  // visible
    QCOMPARE(spy.count(), 2);

    model.setAction(ActionBarKind::AddContact, QStringLiteral("Add"));    // text only
    QCOMPARE(spy.count(), 3);

    model.setAction(ActionBarKind::AddContact, QStringLiteral("Add "));   // trim no-op
    QCOMPARE(spy.count(), 3);

    model.setAction(ActionBarKind::None, QString());                      // hide
    QCOMPARE(spy.count(), 4);
    QVERIFY(!model.visible());
    QVERIFY(!model.busy());
}

QTEST_GUILESS_MAIN(ChatActionBarModelTest)
#include "tst_chat_action_bar_model.moc"
