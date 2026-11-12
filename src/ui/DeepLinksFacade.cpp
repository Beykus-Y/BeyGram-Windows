#include "DeepLinksFacade.h"

#include <QtGlobal>

namespace beygram::ui {

namespace {

/// The QML-facing name of one router action. The names are written out rather
/// than derived from the enumerators so a rename in C++ cannot silently change
/// what QML compares against.
QString actionName(telegram::DeepLinkAction action)
{
    switch (action) {
    case telegram::DeepLinkAction::None:
        return QStringLiteral("none");
    case telegram::DeepLinkAction::OpenChat:
        return QStringLiteral("openChat");
    case telegram::DeepLinkAction::JoinChat:
        return QStringLiteral("joinChat");
    case telegram::DeepLinkAction::OpenMessage:
        return QStringLiteral("openMessage");
    case telegram::DeepLinkAction::OpenProfile:
        return QStringLiteral("openProfile");
    case telegram::DeepLinkAction::OpenStickerSet:
        return QStringLiteral("openStickerSet");
    case telegram::DeepLinkAction::OpenBot:
        return QStringLiteral("openBot");
    case telegram::DeepLinkAction::ConfirmPhone:
        return QStringLiteral("confirmPhone");
    case telegram::DeepLinkAction::OpenSettings:
        return QStringLiteral("openSettings");
    }

    return QStringLiteral("none");
}

} // namespace

DeepLinksFacade::DeepLinksFacade(QObject* parent)
    : QObject(parent)
{
    // The router already suppresses no-op changes, so its signal can be
    // forwarded as-is: a repeated link causes no repaint.
    connect(&m_router, &telegram::DeepLinkRouter::changed,
            this, &DeepLinksFacade::changed);
}

QString DeepLinksFacade::link() const
{
    return m_router.link();
}

QString DeepLinksFacade::action() const
{
    return actionName(m_router.action());
}

QString DeepLinksFacade::target() const
{
    return m_router.target();
}

QString DeepLinksFacade::messageId() const
{
    const qint64 id = m_router.messageId();
    return id > 0 ? QString::number(id) : QString();
}

bool DeepLinksFacade::valid() const
{
    return m_router.valid();
}

void DeepLinksFacade::setLink(const QString& url)
{
    m_router.setLink(url);
}

void DeepLinksFacade::reset()
{
    m_router.reset();
}

bool DeepLinksFacade::confirm()
{
    if (!valid())
        return false;

    // Snapshot before emitting: the receiver may re-enter the facade and set a
    // different link, and the announcement must describe the link that was
    // confirmed rather than whatever replaced it.
    const QString confirmedAction = action();
    const QString confirmedTarget = target();
    const QString confirmedMessage = messageId();
    emit navigateRequested(confirmedAction, confirmedTarget, confirmedMessage);
    return true;
}

} // namespace beygram::ui
