#include "MessageActionsFacade.h"

#include "telegram/MessageActionAvailability.h"

#include <array>
#include <optional>
#include <utility>

namespace beygram::ui {
namespace {

using telegram::MessageAction;

/// Every action in MessageAction declaration order, so the identifier and
/// availability helpers cannot drift from the enum.
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

/// The stable identifier QML uses for the action. MessageActionsMenu.qml maps
/// the same strings to translated labels and glyphs.
QString actionId(MessageAction action)
{
    switch (action) {
    case MessageAction::Reply:
        return QStringLiteral("reply");
    case MessageAction::Forward:
        return QStringLiteral("forward");
    case MessageAction::Copy:
        return QStringLiteral("copy");
    case MessageAction::Edit:
        return QStringLiteral("edit");
    case MessageAction::Delete:
        return QStringLiteral("delete");
    case MessageAction::Pin:
        return QStringLiteral("pin");
    case MessageAction::Save:
        return QStringLiteral("save");
    case MessageAction::React:
        return QStringLiteral("react");
    case MessageAction::Translate:
        return QStringLiteral("translate");
    case MessageAction::Report:
        return QStringLiteral("report");
    }

    return {};
}

/// Resolves an identifier produced by actionId(). Unknown strings yield no
/// action, which trigger() treats as a no-op.
std::optional<MessageAction> actionFromId(const QString& id)
{
    for (const MessageAction action : kAllActions) {
        if (actionId(action) == id)
            return action;
    }
    return std::nullopt;
}

/// Converts the availability result into the QML-facing identifier list while
/// keeping its order.
QStringList actionIds(const QVector<MessageAction>& actions)
{
    QStringList ids;
    ids.reserve(actions.size());
    for (const MessageAction action : actions)
        ids.append(actionId(action));
    return ids;
}

} // namespace

MessageActionsFacade::MessageActionsFacade(QObject* parent)
    : QObject(parent)
{
}

QStringList MessageActionsFacade::actions() const
{
    return m_actions;
}

void MessageActionsFacade::configure(bool outgoing, bool canSend, bool isChannel, bool isSecret,
                                    bool protectedContent, bool paidMedia, bool pending,
                                    bool failed, bool hasText, bool isService, bool canPin,
                                    bool canDelete, bool canEdit)
{
    telegram::ActionContext context;
    context.outgoing = outgoing;
    context.canSend = canSend;
    context.isChannel = isChannel;
    context.isSecret = isSecret;
    context.protectedContent = protectedContent;
    context.paidMedia = paidMedia;
    context.pending = pending;
    context.failed = failed;
    context.hasText = hasText;
    context.isService = isService;
    context.canPin = canPin;
    context.canDelete = canDelete;
    context.canEdit = canEdit;

    QStringList next = actionIds(telegram::availableActions(context));
    if (next == m_actions)
        return;

    m_actions = std::move(next);
    emit changed();
}

void MessageActionsFacade::trigger(const QString& action)
{
    // The configured set, not the enum, decides what may be requested: the
    // caller may have reconfigured the facade while the menu stayed open.
    const std::optional<MessageAction> parsed = actionFromId(action);
    if (!parsed || !m_actions.contains(action))
        return;

    switch (*parsed) {
    case MessageAction::Reply:
        emit replyRequested();
        break;
    case MessageAction::Forward:
        emit forwardRequested();
        break;
    case MessageAction::Copy:
        emit copyRequested();
        break;
    case MessageAction::Edit:
        emit editRequested();
        break;
    case MessageAction::Delete:
        emit deleteRequested();
        break;
    case MessageAction::Pin:
        emit pinRequested();
        break;
    case MessageAction::Save:
        emit saveRequested();
        break;
    case MessageAction::React:
        emit reactRequested();
        break;
    case MessageAction::Translate:
        emit translateRequested();
        break;
    case MessageAction::Report:
        emit reportRequested();
        break;
    }
}

} // namespace beygram::ui
