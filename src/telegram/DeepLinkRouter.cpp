#include "DeepLinkRouter.h"

#include "DeepLinks.h"

#include <QStringList>

namespace beygram::telegram {
namespace {

/// What the UI is asked to do, plus the fields that go with it. Kept apart from
/// the router so the mapping can be read as one table.
struct Route {
    DeepLinkAction action = DeepLinkAction::None;
    QString target;
    qint64 messageId = 0;
};

// The parser only accepts an explicit "https://" on a Telegram host. The shell
// is also handed "http://t.me/..." links copied from web pages and bare
// "t.me/..." links copied from chat clients, so those two forms are rewritten
// to the canonical one first.
//
// This is a prefix rewrite, not a second parser: nothing about the host, path
// or query is interpreted here. parseDeepLink() still decides every route, still
// rejects foreign hosts, reserved routes and malformed queries, and still never
// fills a field from unvalidated input. Accepting "http" costs nothing because
// the router never fetches the URL - it only names a local action.
QString normalizeLink(const QString& link)
{
    const QString lower = link.toLower();
    if (lower.startsWith(QLatin1String("http://")))
        return QStringLiteral("https://") + link.mid(7);

    // Only the exact Telegram hosts with a path. "t.me.evil.com/x" matches none
    // of these and is rejected by the parser as a foreign host.
    static const QStringList barePrefixes{
        QStringLiteral("t.me/"),
        QStringLiteral("www.t.me/"),
        QStringLiteral("telegram.me/"),
        QStringLiteral("www.telegram.me/"),
    };
    for (const QString& prefix : barePrefixes) {
        if (lower.startsWith(prefix))
            return QStringLiteral("https://") + link;
    }

    return link;
}

// One row of the mapping documented in DeepLinkRouter.h. The parser stays the
// single source of truth: this function only names the action and normalises
// the case of the payloads that are case-insensitive.
Route routeFor(const DeepLink& link)
{
    switch (link.kind) {
    case DeepLink::Kind::PublicChat:
        // Telegram usernames are case-insensitive; lower-casing makes two
        // spellings of the same chat compare equal.
        return {DeepLinkAction::OpenChat, link.value.toLower(), 0};
    case DeepLink::Kind::Message:
        return {DeepLinkAction::OpenMessage, link.value.toLower(), link.messageId};
    case DeepLink::Kind::BotStart:
        // The /start payload is intentionally dropped: the router only says
        // which bot to open, and the caller can read the payload from
        // parseDeepLink() when it wants to offer it.
        return {DeepLinkAction::OpenBot, link.value.toLower(), 0};
    case DeepLink::Kind::Invite:
        // Invite hashes are base64url: 'A' and 'a' are different hashes, so the
        // hash is kept exactly as the parser produced it.
        return {DeepLinkAction::JoinChat, link.value, 0};
    case DeepLink::Kind::Phone:
        // The parser already returns "+<digits>"; the leading '+' matters.
        return {DeepLinkAction::ConfirmPhone, link.value, 0};
    case DeepLink::Kind::PrivateChat:
        // A numeric user id, which is all a tg://user?id= link carries.
        return {DeepLinkAction::OpenProfile, link.value, 0};
    case DeepLink::Kind::Settings:
        return {DeepLinkAction::OpenSettings, QString(), 0};
    case DeepLink::Kind::Unknown:
        break;
    }

    // Unrecognised: no action and no target, so a rejected link can never be
    // turned into a lookup.
    return {};
}

} // namespace

DeepLinkRouter::DeepLinkRouter(QObject* parent)
    : QObject(parent)
{
}

void DeepLinkRouter::setLink(const QString& url)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty()) {
        reset();
        return;
    }

    const Route route = routeFor(parseDeepLink(normalizeLink(trimmed)));

    if (m_link == trimmed && m_action == route.action && m_target == route.target
        && m_messageId == route.messageId) {
        return;
    }

    m_link = trimmed;
    m_action = route.action;
    m_target = route.target;
    m_messageId = route.messageId;
    emit changed();
}

void DeepLinkRouter::reset()
{
    if (m_link.isEmpty() && m_action == DeepLinkAction::None && m_target.isEmpty()
        && m_messageId == 0) {
        return;
    }

    m_link.clear();
    m_action = DeepLinkAction::None;
    m_target.clear();
    m_messageId = 0;
    emit changed();
}

} // namespace beygram::telegram
