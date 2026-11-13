#include "telegram/MessageActionAvailability.h"

#include <QtTest>

#include <array>

using namespace beygram::telegram;

namespace {

constexpr int Reply = static_cast<int>(MessageAction::Reply);
constexpr int Forward = static_cast<int>(MessageAction::Forward);
constexpr int Copy = static_cast<int>(MessageAction::Copy);
constexpr int Edit = static_cast<int>(MessageAction::Edit);
constexpr int Delete = static_cast<int>(MessageAction::Delete);
constexpr int Pin = static_cast<int>(MessageAction::Pin);
constexpr int Save = static_cast<int>(MessageAction::Save);
constexpr int React = static_cast<int>(MessageAction::React);
constexpr int Translate = static_cast<int>(MessageAction::Translate);
constexpr int Report = static_cast<int>(MessageAction::Report);

constexpr std::array<MessageAction, 10> kEveryAction{
    MessageAction::Reply, MessageAction::Forward, MessageAction::Copy,
    MessageAction::Edit, MessageAction::Delete, MessageAction::Pin,
    MessageAction::Save, MessageAction::React, MessageAction::Translate,
    MessageAction::Report,
};

QVector<int> actionIds(const QVector<MessageAction>& actions)
{
    QVector<int> ids;
    ids.reserve(actions.size());
    for (const MessageAction action : actions)
        ids.append(static_cast<int>(action));
    return ids;
}

} // namespace

class MessageActionAvailabilityTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultContextOffersIncomingActions();
    void editNeedsOutgoingEditPermissionAndText();
    void onlyDeleteForUnsettledMessages_data();
    void onlyDeleteForUnsettledMessages();
    void protectedContentWithdrawsCopySaveForwardTranslate();
    void paidMediaWithdrawsSaveAndForward();
    void secretChatWithdrawsSaveAndForward();
    void channelSubscriberCannotReplyOrEdit();
    void channelAdministratorKeepsReplyAndEdit();
    void pinAndDeleteFollowPermissions();
    void reportOnlyForIncomingMessages();
    void combinedRestrictionsKeepOnlyPinAndReact();
    void labelsAreEnglish();
    void isActionAvailableMatchesAvailableActions();
};

void MessageActionAvailabilityTest::defaultContextOffersIncomingActions()
{
    const ActionContext context;

    QCOMPARE(actionIds(availableActions(context)),
             (QVector<int>{Reply, Forward, Copy, Delete, Save, React,
                           Translate, Report}));
    QVERIFY(isActionAvailable(MessageAction::Reply, context));
    QVERIFY(isActionAvailable(MessageAction::Report, context));
    QVERIFY(!isActionAvailable(MessageAction::Edit, context));
    QVERIFY(!isActionAvailable(MessageAction::Pin, context));
}

void MessageActionAvailabilityTest::editNeedsOutgoingEditPermissionAndText()
{
    ActionContext context;
    context.canEdit = true;
    context.hasText = true;

    // Incoming: the account does not own the message.
    QVERIFY(!isActionAvailable(MessageAction::Edit, context));

    context.outgoing = true;
    QCOMPARE(actionIds(availableActions(context)),
             (QVector<int>{Reply, Forward, Copy, Edit, Delete, Save, React,
                           Translate}));
    QVERIFY(!isActionAvailable(MessageAction::Report, context));

    context.hasText = false;
    QVERIFY(!isActionAvailable(MessageAction::Edit, context));

    context.hasText = true;
    context.canEdit = false;
    QVERIFY(!isActionAvailable(MessageAction::Edit, context));
}

void MessageActionAvailabilityTest::onlyDeleteForUnsettledMessages_data()
{
    QTest::addColumn<int>("kind");

    QTest::newRow("service") << 0;
    QTest::newRow("pending") << 1;
    QTest::newRow("failed") << 2;
}

void MessageActionAvailabilityTest::onlyDeleteForUnsettledMessages()
{
    QFETCH(int, kind);

    ActionContext context;
    context.isService = kind == 0;
    context.pending = kind == 1;
    context.failed = kind == 2;

    // Other grants must not leak through an unsettled message.
    context.outgoing = true;
    context.canEdit = true;
    context.hasText = true;
    context.canPin = true;

    QCOMPARE(actionIds(availableActions(context)), (QVector<int>{Delete}));
    QVERIFY(isActionAvailable(MessageAction::Delete, context));
    for (const MessageAction action : kEveryAction) {
        if (action != MessageAction::Delete)
            QVERIFY(!isActionAvailable(action, context));
    }

    context.canDelete = false;
    QVERIFY(availableActions(context).isEmpty());
    QVERIFY(!isActionAvailable(MessageAction::Delete, context));
}

void MessageActionAvailabilityTest::protectedContentWithdrawsCopySaveForwardTranslate()
{
    ActionContext context;
    context.protectedContent = true;

    QCOMPARE(actionIds(availableActions(context)),
             (QVector<int>{Reply, Delete, React, Report}));
    QVERIFY(!isActionAvailable(MessageAction::Copy, context));
    QVERIFY(!isActionAvailable(MessageAction::Save, context));
    QVERIFY(!isActionAvailable(MessageAction::Forward, context));
    QVERIFY(!isActionAvailable(MessageAction::Translate, context));
    QVERIFY(isActionAvailable(MessageAction::Reply, context));
    QVERIFY(isActionAvailable(MessageAction::React, context));
}

void MessageActionAvailabilityTest::paidMediaWithdrawsSaveAndForward()
{
    ActionContext context;
    context.paidMedia = true;

    QCOMPARE(actionIds(availableActions(context)),
             (QVector<int>{Reply, Copy, Delete, React, Translate, Report}));
    QVERIFY(!isActionAvailable(MessageAction::Save, context));
    QVERIFY(!isActionAvailable(MessageAction::Forward, context));
    QVERIFY(isActionAvailable(MessageAction::Copy, context));
    QVERIFY(isActionAvailable(MessageAction::Translate, context));
}

void MessageActionAvailabilityTest::secretChatWithdrawsSaveAndForward()
{
    ActionContext context;
    context.isSecret = true;

    QCOMPARE(actionIds(availableActions(context)),
             (QVector<int>{Reply, Copy, Delete, React, Translate, Report}));
    QVERIFY(!isActionAvailable(MessageAction::Save, context));
    QVERIFY(!isActionAvailable(MessageAction::Forward, context));
    QVERIFY(isActionAvailable(MessageAction::Copy, context));
    QVERIFY(isActionAvailable(MessageAction::Translate, context));
}

void MessageActionAvailabilityTest::channelSubscriberCannotReplyOrEdit()
{
    ActionContext context;
    context.isChannel = true;
    context.canSend = false;
    context.outgoing = true;
    context.canEdit = true;
    context.hasText = true;

    QCOMPARE(actionIds(availableActions(context)),
             (QVector<int>{Forward, Copy, Delete, Save, React, Translate}));
    QVERIFY(!isActionAvailable(MessageAction::Reply, context));
    QVERIFY(!isActionAvailable(MessageAction::Edit, context));
    QVERIFY(isActionAvailable(MessageAction::Forward, context));
    QVERIFY(isActionAvailable(MessageAction::Delete, context));
}

void MessageActionAvailabilityTest::channelAdministratorKeepsReplyAndEdit()
{
    ActionContext context;
    context.isChannel = true;
    context.canSend = true;
    context.outgoing = true;
    context.canEdit = true;
    context.hasText = true;

    QCOMPARE(actionIds(availableActions(context)),
             (QVector<int>{Reply, Forward, Copy, Edit, Delete, Save, React,
                           Translate}));
    QVERIFY(isActionAvailable(MessageAction::Reply, context));
    QVERIFY(isActionAvailable(MessageAction::Edit, context));
}

void MessageActionAvailabilityTest::pinAndDeleteFollowPermissions()
{
    ActionContext context;
    QVERIFY(!isActionAvailable(MessageAction::Pin, context));

    context.canPin = true;
    QCOMPARE(actionIds(availableActions(context)),
             (QVector<int>{Reply, Forward, Copy, Delete, Pin, Save, React,
                           Translate, Report}));

    context.canDelete = false;
    QVERIFY(!isActionAvailable(MessageAction::Delete, context));
    QVERIFY(isActionAvailable(MessageAction::Pin, context));
}

void MessageActionAvailabilityTest::reportOnlyForIncomingMessages()
{
    ActionContext context;
    QVERIFY(isActionAvailable(MessageAction::Report, context));

    context.outgoing = true;
    QVERIFY(!isActionAvailable(MessageAction::Report, context));
}

void MessageActionAvailabilityTest::combinedRestrictionsKeepOnlyPinAndReact()
{
    ActionContext context;
    context.outgoing = true;
    context.canSend = false;
    context.isChannel = true;
    context.isSecret = true;
    context.protectedContent = true;
    context.paidMedia = true;
    context.canPin = true;
    context.canDelete = false;
    context.canEdit = true;
    context.hasText = true;

    QCOMPARE(actionIds(availableActions(context)),
             (QVector<int>{Pin, React}));
}

void MessageActionAvailabilityTest::labelsAreEnglish()
{
    QCOMPARE(actionLabel(MessageAction::Reply), QStringLiteral("Reply"));
    QCOMPARE(actionLabel(MessageAction::Forward), QStringLiteral("Forward"));
    QCOMPARE(actionLabel(MessageAction::Copy), QStringLiteral("Copy"));
    QCOMPARE(actionLabel(MessageAction::Edit), QStringLiteral("Edit"));
    QCOMPARE(actionLabel(MessageAction::Delete), QStringLiteral("Delete"));
    QCOMPARE(actionLabel(MessageAction::Pin), QStringLiteral("Pin"));
    QCOMPARE(actionLabel(MessageAction::Save), QStringLiteral("Save"));
    QCOMPARE(actionLabel(MessageAction::React), QStringLiteral("React"));
    QCOMPARE(actionLabel(MessageAction::Translate), QStringLiteral("Translate"));
    QCOMPARE(actionLabel(MessageAction::Report), QStringLiteral("Report"));
}

void MessageActionAvailabilityTest::isActionAvailableMatchesAvailableActions()
{
    ActionContext incoming;
    incoming.canPin = true;

    ActionContext outgoing;
    outgoing.outgoing = true;
    outgoing.canEdit = true;
    outgoing.hasText = true;

    ActionContext restricted;
    restricted.isChannel = true;
    restricted.canSend = false;
    restricted.isSecret = true;
    restricted.protectedContent = true;
    restricted.paidMedia = true;
    restricted.canPin = true;
    restricted.canDelete = false;

    const std::array<ActionContext, 3> contexts{incoming, outgoing, restricted};
    for (const ActionContext& context : contexts) {
        const QVector<MessageAction> actions = availableActions(context);
        for (const MessageAction action : kEveryAction) {
            QCOMPARE(isActionAvailable(action, context),
                     actions.contains(action));
        }
    }
}

QTEST_GUILESS_MAIN(MessageActionAvailabilityTest)
#include "tst_message_action_availability.moc"
