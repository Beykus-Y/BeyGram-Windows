#include "ui/MessageActionsFacade.h"

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtTest>

#include <initializer_list>

using beygram::ui::MessageActionsFacade;

namespace {

/// The configure() parameters in one object, mirroring
/// beygram::telegram::ActionContext field for field. Defaults describe an
/// unrestricted private chat holding a received message.
struct Flags {
    bool outgoing = false;
    bool canSend = true;
    bool isChannel = false;
    bool isSecret = false;
    bool protectedContent = false;
    bool paidMedia = false;
    bool pending = false;
    bool failed = false;
    bool hasText = false;
    bool isService = false;
    bool canPin = false;
    bool canDelete = true;
    bool canEdit = false;
};

void configure(MessageActionsFacade& facade, const Flags& flags)
{
    facade.configure(flags.outgoing, flags.canSend, flags.isChannel, flags.isSecret,
                     flags.protectedContent, flags.paidMedia, flags.pending, flags.failed,
                     flags.hasText, flags.isService, flags.canPin, flags.canDelete,
                     flags.canEdit);
}

QStringList ids(std::initializer_list<const char*> values)
{
    QStringList result;
    for (const char* value : values)
        result.append(QString::fromLatin1(value));
    return result;
}

} // namespace

class MessageActionsFacadeTest final : public QObject {
    Q_OBJECT

private slots:
    void startsEmpty();
    void triggerBeforeConfigureIsIgnored();

    void receivedPrivateMessageOffersOrdinaryActions();
    void outgoingTextMessageAddsEdit();
    void channelSubscriberLosesReplyAndEdit();
    void channelAdministratorKeepsReplyAndEdit();
    void protectedContentWithdrawsForwardCopySaveTranslate();
    void paidMediaWithdrawsForwardAndSave();
    void secretChatWithdrawsForwardAndSave();
    void serviceMessageOffersOnlyDelete();
    void pendingMessageOffersOnlyDelete();
    void failedMessageOffersOnlyDelete();
    void unsettledMessageWithoutDeletePermissionOffersNothing();
    void deleteFollowsCanDelete();
    void pinFollowsCanPin();
    void editRequiresOutgoingEditableText();

    void configureEmitsChangedOnlyWhenTheSetChanges();
    void triggerEmitsTheMatchingSignal();
    void triggerIgnoresWithheldActions();
    void triggerIgnoresUnknownAndEmptyActions();
    void triggerDoesNotEmitChanged();
    void triggerUsesTheLatestConfiguration();
};

void MessageActionsFacadeTest::startsEmpty()
{
    const MessageActionsFacade facade;

    QVERIFY2(facade.actions().isEmpty(),
             "the menu must not guess availability before configure()");
}

void MessageActionsFacadeTest::triggerBeforeConfigureIsIgnored()
{
    MessageActionsFacade facade;

    QSignalSpy replySpy(&facade, &MessageActionsFacade::replyRequested);
    QSignalSpy deleteSpy(&facade, &MessageActionsFacade::deleteRequested);

    facade.trigger(QStringLiteral("reply"));
    facade.trigger(QStringLiteral("delete"));

    QCOMPARE(replySpy.count(), 0);
    QCOMPARE(deleteSpy.count(), 0);
}

void MessageActionsFacadeTest::receivedPrivateMessageOffersOrdinaryActions()
{
    MessageActionsFacade facade;
    configure(facade, Flags{});

    // Reply, Forward, Copy, Delete, Save, React, Translate and Report are
    // offered; Edit needs an outgoing message, Pin needs canPin.
    QCOMPARE(facade.actions(),
             ids({"reply", "forward", "copy", "delete", "save", "react", "translate",
                  "report"}));
}

void MessageActionsFacadeTest::outgoingTextMessageAddsEdit()
{
    MessageActionsFacade facade;

    Flags flags;
    flags.outgoing = true;
    flags.canEdit = true;
    flags.hasText = true;
    flags.canPin = true;
    configure(facade, flags);

    // Report is withheld for the account's own message; every other action is
    // offered in MessageAction declaration order.
    QCOMPARE(facade.actions(),
             ids({"reply", "forward", "copy", "edit", "delete", "pin", "save", "react",
                  "translate"}));
}

void MessageActionsFacadeTest::channelSubscriberLosesReplyAndEdit()
{
    MessageActionsFacade facade;

    // A subscriber is the account that cannot post, matching InteractionPolicy.
    Flags flags;
    flags.isChannel = true;
    flags.canSend = false;
    flags.outgoing = true;
    flags.canEdit = true;
    flags.hasText = true;
    configure(facade, flags);

    QCOMPARE(facade.actions(),
             ids({"forward", "copy", "delete", "save", "react", "translate"}));
}

void MessageActionsFacadeTest::channelAdministratorKeepsReplyAndEdit()
{
    MessageActionsFacade facade;

    Flags flags;
    flags.isChannel = true;
    flags.canSend = true;
    flags.outgoing = true;
    flags.canEdit = true;
    flags.hasText = true;
    configure(facade, flags);

    QCOMPARE(facade.actions(),
             ids({"reply", "forward", "copy", "edit", "delete", "save", "react",
                  "translate"}));
}

void MessageActionsFacadeTest::protectedContentWithdrawsForwardCopySaveTranslate()
{
    MessageActionsFacade facade;

    Flags flags;
    flags.protectedContent = true;
    flags.canPin = true;
    configure(facade, flags);

    QCOMPARE(facade.actions(), ids({"reply", "delete", "pin", "react", "report"}));
}

void MessageActionsFacadeTest::paidMediaWithdrawsForwardAndSave()
{
    MessageActionsFacade facade;

    Flags flags;
    flags.paidMedia = true;
    configure(facade, flags);

    // Copy and Translate stay: only Forward and Save reveal the paid media.
    QCOMPARE(facade.actions(),
             ids({"reply", "copy", "delete", "react", "translate", "report"}));
}

void MessageActionsFacadeTest::secretChatWithdrawsForwardAndSave()
{
    MessageActionsFacade facade;

    Flags flags;
    flags.isSecret = true;
    configure(facade, flags);

    QCOMPARE(facade.actions(),
             ids({"reply", "copy", "delete", "react", "translate", "report"}));
}

void MessageActionsFacadeTest::serviceMessageOffersOnlyDelete()
{
    MessageActionsFacade facade;

    Flags flags;
    flags.isService = true;
    flags.canPin = true;
    configure(facade, flags);

    QCOMPARE(facade.actions(), ids({"delete"}));
}

void MessageActionsFacadeTest::pendingMessageOffersOnlyDelete()
{
    MessageActionsFacade facade;

    Flags flags;
    flags.pending = true;
    flags.canPin = true;
    configure(facade, flags);

    QCOMPARE(facade.actions(), ids({"delete"}));
}

void MessageActionsFacadeTest::failedMessageOffersOnlyDelete()
{
    MessageActionsFacade facade;

    Flags flags;
    flags.failed = true;
    configure(facade, flags);

    QCOMPARE(facade.actions(), ids({"delete"}));
}

void MessageActionsFacadeTest::unsettledMessageWithoutDeletePermissionOffersNothing()
{
    MessageActionsFacade facade;

    Flags flags;
    flags.failed = true;
    flags.canDelete = false;
    configure(facade, flags);

    QVERIFY2(facade.actions().isEmpty(),
             "an unsettled message offers nothing once Delete is withdrawn");
}

void MessageActionsFacadeTest::deleteFollowsCanDelete()
{
    MessageActionsFacade facade;

    Flags flags;
    flags.canDelete = false;
    configure(facade, flags);

    QCOMPARE(facade.actions(),
             ids({"reply", "forward", "copy", "save", "react", "translate", "report"}));
}

void MessageActionsFacadeTest::pinFollowsCanPin()
{
    MessageActionsFacade facade;

    configure(facade, Flags{});
    QVERIFY(!facade.actions().contains(QStringLiteral("pin")));

    Flags flags;
    flags.canPin = true;
    configure(facade, flags);
    QVERIFY(facade.actions().contains(QStringLiteral("pin")));
}

void MessageActionsFacadeTest::editRequiresOutgoingEditableText()
{
    MessageActionsFacade facade;

    Flags flags;
    flags.outgoing = true;
    flags.canEdit = true;
    flags.hasText = true;
    configure(facade, flags);
    QVERIFY2(facade.actions().contains(QStringLiteral("edit")),
             "an own editable text message can be edited");

    flags.outgoing = false;
    configure(facade, flags);
    QVERIFY2(!facade.actions().contains(QStringLiteral("edit")),
             "a received message cannot be edited");

    flags.outgoing = true;
    flags.canEdit = false;
    configure(facade, flags);
    QVERIFY2(!facade.actions().contains(QStringLiteral("edit")),
             "the chat must still permit editing");

    flags.canEdit = true;
    flags.hasText = false;
    configure(facade, flags);
    QVERIFY2(!facade.actions().contains(QStringLiteral("edit")),
             "media without text has nothing to edit");
}

void MessageActionsFacadeTest::configureEmitsChangedOnlyWhenTheSetChanges()
{
    MessageActionsFacade facade;
    QSignalSpy spy(&facade, &MessageActionsFacade::changed);

    configure(facade, Flags{});
    QCOMPARE(spy.count(), 1);

    // Re-applying the identical context is not a change.
    configure(facade, Flags{});
    QCOMPARE(spy.count(), 1);

    // hasText only matters when Edit can be offered, so this changes nothing.
    Flags flags;
    flags.hasText = true;
    configure(facade, flags);
    QCOMPARE(spy.count(), 1);

    flags.canPin = true;
    configure(facade, flags);
    QCOMPARE(spy.count(), 2);

    // Edit appears and Report disappears: still a single change notification.
    flags.outgoing = true;
    flags.canEdit = true;
    configure(facade, flags);
    QCOMPARE(spy.count(), 3);

    flags.pending = true;
    configure(facade, flags);
    QCOMPARE(spy.count(), 4);

    configure(facade, flags);
    QCOMPARE(spy.count(), 4);
}

void MessageActionsFacadeTest::triggerEmitsTheMatchingSignal()
{
    MessageActionsFacade facade;

    // An own editable text message offers nine of the ten actions.
    Flags flags;
    flags.outgoing = true;
    flags.canEdit = true;
    flags.hasText = true;
    flags.canPin = true;
    configure(facade, flags);

    QSignalSpy replySpy(&facade, &MessageActionsFacade::replyRequested);
    QSignalSpy forwardSpy(&facade, &MessageActionsFacade::forwardRequested);
    QSignalSpy copySpy(&facade, &MessageActionsFacade::copyRequested);
    QSignalSpy editSpy(&facade, &MessageActionsFacade::editRequested);
    QSignalSpy deleteSpy(&facade, &MessageActionsFacade::deleteRequested);
    QSignalSpy pinSpy(&facade, &MessageActionsFacade::pinRequested);
    QSignalSpy saveSpy(&facade, &MessageActionsFacade::saveRequested);
    QSignalSpy reactSpy(&facade, &MessageActionsFacade::reactRequested);
    QSignalSpy translateSpy(&facade, &MessageActionsFacade::translateRequested);
    QSignalSpy reportSpy(&facade, &MessageActionsFacade::reportRequested);

    facade.trigger(QStringLiteral("reply"));
    facade.trigger(QStringLiteral("forward"));
    facade.trigger(QStringLiteral("copy"));
    facade.trigger(QStringLiteral("edit"));
    facade.trigger(QStringLiteral("delete"));
    facade.trigger(QStringLiteral("pin"));
    facade.trigger(QStringLiteral("save"));
    facade.trigger(QStringLiteral("react"));
    facade.trigger(QStringLiteral("translate"));

    // Exactly one signal per chosen action, so a misrouted identifier shows up
    // as a zero on its own spy and a surplus elsewhere.
    QCOMPARE(replySpy.count(), 1);
    QCOMPARE(forwardSpy.count(), 1);
    QCOMPARE(copySpy.count(), 1);
    QCOMPARE(editSpy.count(), 1);
    QCOMPARE(deleteSpy.count(), 1);
    QCOMPARE(pinSpy.count(), 1);
    QCOMPARE(saveSpy.count(), 1);
    QCOMPARE(reactSpy.count(), 1);
    QCOMPARE(translateSpy.count(), 1);
    QCOMPARE(reportSpy.count(), 0);

    // Report needs a received message, so the same facade must offer it after
    // reconfiguration.
    configure(facade, Flags{});
    facade.trigger(QStringLiteral("report"));

    QCOMPARE(reportSpy.count(), 1);
    QCOMPARE(replySpy.count(), 1);
    QCOMPARE(deleteSpy.count(), 1);
}

void MessageActionsFacadeTest::triggerIgnoresWithheldActions()
{
    MessageActionsFacade facade;
    configure(facade, Flags{});

    QSignalSpy editSpy(&facade, &MessageActionsFacade::editRequested);
    QSignalSpy pinSpy(&facade, &MessageActionsFacade::pinRequested);

    facade.trigger(QStringLiteral("edit"));
    facade.trigger(QStringLiteral("pin"));

    QCOMPARE(editSpy.count(), 0);
    QCOMPARE(pinSpy.count(), 0);
}

void MessageActionsFacadeTest::triggerIgnoresUnknownAndEmptyActions()
{
    MessageActionsFacade facade;
    configure(facade, Flags{});

    QSignalSpy replySpy(&facade, &MessageActionsFacade::replyRequested);
    QSignalSpy changedSpy(&facade, &MessageActionsFacade::changed);

    facade.trigger(QString());
    facade.trigger(QStringLiteral("Reply"));
    facade.trigger(QStringLiteral("REPLY"));
    facade.trigger(QStringLiteral("reply "));
    facade.trigger(QStringLiteral("bogus"));

    QCOMPARE(replySpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
}

void MessageActionsFacadeTest::triggerDoesNotEmitChanged()
{
    MessageActionsFacade facade;
    configure(facade, Flags{});

    QSignalSpy changedSpy(&facade, &MessageActionsFacade::changed);
    QSignalSpy replySpy(&facade, &MessageActionsFacade::replyRequested);

    facade.trigger(QStringLiteral("reply"));

    // Choosing an action is a request, not a change to the offered set.
    QCOMPARE(replySpy.count(), 1);
    QCOMPARE(changedSpy.count(), 0);
}

void MessageActionsFacadeTest::triggerUsesTheLatestConfiguration()
{
    MessageActionsFacade facade;

    Flags flags;
    flags.outgoing = true;
    flags.canEdit = true;
    flags.hasText = true;
    configure(facade, flags);

    QSignalSpy editSpy(&facade, &MessageActionsFacade::editRequested);

    facade.trigger(QStringLiteral("edit"));
    QCOMPARE(editSpy.count(), 1);

    // Editing is withdrawn while the menu could still be open.
    flags.canEdit = false;
    configure(facade, flags);
    facade.trigger(QStringLiteral("edit"));
    QCOMPARE(editSpy.count(), 1);

    flags.canEdit = true;
    configure(facade, flags);
    facade.trigger(QStringLiteral("edit"));
    QCOMPARE(editSpy.count(), 2);
}

QTEST_GUILESS_MAIN(MessageActionsFacadeTest)
#include "tst_message_actions_facade.moc"
