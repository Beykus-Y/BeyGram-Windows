#include "MessageActionAvailability.h"

#include <array>

namespace beygram::telegram {
namespace {

constexpr std::array<MessageAction, 10> kAllActions{
    MessageAction::Reply,
    MessageAction::Forward,
    MessageAction::Copy,
    MessageAction::Edit,
    MessageAction::Delete,
    MessageAction::Pin,
    MessageAction::Save,
    MessageAction::React,
    MessageAction::Translate,
    MessageAction::Report,
};

} // namespace

bool isActionAvailable(MessageAction action, const ActionContext& context)
{
    // An unsettled message is not ordinary content: a service notice has no
    // author to reply to, and a pending or failed send cannot be forwarded,
    // copied or edited. Delete stays available while the account may delete.
    if (context.isService || context.pending || context.failed)
        return action == MessageAction::Delete && context.canDelete;

    // A channel subscriber is the account that cannot post (canSend false).
    // An administrator keeps canSend true, matching InteractionPolicy.
    const bool channelSubscriber = context.isChannel && !context.canSend;

    switch (action) {
    case MessageAction::Reply:
        // In a channel this is exactly the administrator/subscriber split.
        return context.canSend;
    case MessageAction::Forward:
        return !context.protectedContent && !context.paidMedia && !context.isSecret;
    case MessageAction::Copy:
        return !context.protectedContent;
    case MessageAction::Edit:
        return context.outgoing && context.canEdit && context.hasText
            && !channelSubscriber;
    case MessageAction::Delete:
        return context.canDelete;
    case MessageAction::Pin:
        return context.canPin;
    case MessageAction::Save:
        return !context.protectedContent && !context.paidMedia && !context.isSecret;
    case MessageAction::React:
        return true;
    case MessageAction::Translate:
        return !context.protectedContent;
    case MessageAction::Report:
        return !context.outgoing;
    }

    return false;
}

QVector<MessageAction> availableActions(const ActionContext& context)
{
    QVector<MessageAction> actions;
    actions.reserve(static_cast<qsizetype>(kAllActions.size()));
    for (const MessageAction action : kAllActions) {
        if (isActionAvailable(action, context))
            actions.append(action);
    }
    return actions;
}

QString actionLabel(MessageAction action)
{
    switch (action) {
    case MessageAction::Reply:
        return QStringLiteral("Reply");
    case MessageAction::Forward:
        return QStringLiteral("Forward");
    case MessageAction::Copy:
        return QStringLiteral("Copy");
    case MessageAction::Edit:
        return QStringLiteral("Edit");
    case MessageAction::Delete:
        return QStringLiteral("Delete");
    case MessageAction::Pin:
        return QStringLiteral("Pin");
    case MessageAction::Save:
        return QStringLiteral("Save");
    case MessageAction::React:
        return QStringLiteral("React");
    case MessageAction::Translate:
        return QStringLiteral("Translate");
    case MessageAction::Report:
        return QStringLiteral("Report");
    }

    return {};
}

} // namespace beygram::telegram
