#include "WebAppInfo.h"

#include "TelegramTypes.h"

#include <QJsonValue>
#include <QStringList>

namespace beygram::telegram {
namespace {

// The same cap the deep-link parser uses; a Web App URL longer than this is not
// something Telegram produces.
constexpr qsizetype kMaxWebAppUrlLength = 2048;
constexpr int kMaxHostLabelLength = 63;

bool isAsciiDigit(QChar character)
{
    const char16_t code = character.unicode();
    return code >= u'0' && code <= u'9';
}

bool isAsciiLetter(QChar character)
{
    const char16_t code = character.unicode();
    return (code >= u'a' && code <= u'z') || (code >= u'A' && code <= u'Z');
}

bool isAsciiAlnum(QChar character)
{
    return isAsciiLetter(character) || isAsciiDigit(character);
}

bool isHexDigit(QChar character)
{
    const char16_t code = character.unicode();
    return isAsciiDigit(character) || (code >= u'a' && code <= u'f')
        || (code >= u'A' && code <= u'F');
}

// Control and format characters can hide a second value behind the visible one,
// so they are rejected before the URL is looked at.
bool isControlCharacter(QChar character)
{
    const QChar::Category category = character.category();
    return category == QChar::Other_Control || category == QChar::Other_Format;
}

// A port must be a plain decimal number inside the TCP range. An empty port
// ("host:") and a non-numeric one are malformed, not "no port".
bool isValidPort(const QString& text)
{
    if (text.isEmpty() || text.size() > 5)
        return false;
    for (const QChar character : text) {
        if (!isAsciiDigit(character))
            return false;
    }

    bool ok = false;
    const int port = text.toInt(&ok);
    return ok && port >= 1 && port <= 65535;
}

// Hosts are ASCII on the wire: labels of letters, digits and '-', joined by
// '.', or a bracketed IPv6 literal. Underscores, raw IDN characters and empty
// labels ("example..test") are rejected rather than normalised.
bool isValidHost(const QString& host)
{
    if (host.isEmpty())
        return false;

    if (host.startsWith(QLatin1Char('['))) {
        if (!host.endsWith(QLatin1Char(']')) || host.size() <= 2)
            return false;
        const QString literal = host.mid(1, host.size() - 2);
        for (const QChar character : literal) {
            if (!isHexDigit(character) && character != QLatin1Char(':')
                && character != QLatin1Char('.')) {
                return false;
            }
        }
        return true;
    }

    if (host.startsWith(QLatin1Char('.')) || host.endsWith(QLatin1Char('.')))
        return false;

    const QStringList labels = host.split(QLatin1Char('.'));
    for (const QString& label : labels) {
        if (label.isEmpty() || label.size() > kMaxHostLabelLength)
            return false;
        if (label.startsWith(QLatin1Char('-')) || label.endsWith(QLatin1Char('-')))
            return false;
        for (const QChar character : label) {
            if (!isAsciiAlnum(character) && character != QLatin1Char('-'))
                return false;
        }
    }
    return true;
}

// Splits "host[:port]" and validates both halves. Userinfo is rejected before
// the split: "https://user:pass@host/" must never look like a plain host.
bool isSafeAuthority(const QString& authority)
{
    if (authority.isEmpty() || authority.contains(QLatin1Char('@')))
        return false;

    QString host = authority;
    QString port;
    bool hasPort = false;

    if (authority.startsWith(QLatin1Char('['))) {
        const qsizetype close = authority.indexOf(QLatin1Char(']'));
        if (close < 0)
            return false;
        host = authority.left(close + 1);
        const QString rest = authority.mid(close + 1);
        if (!rest.isEmpty()) {
            if (!rest.startsWith(QLatin1Char(':')))
                return false;
            hasPort = true;
            port = rest.mid(1);
        }
    } else {
        const qsizetype colon = authority.indexOf(QLatin1Char(':'));
        if (colon >= 0) {
            host = authority.left(colon);
            hasPort = true;
            port = authority.mid(colon + 1);
            // A second colon without brackets is a malformed IPv6 literal.
            if (port.contains(QLatin1Char(':')))
                return false;
        }
    }

    if (!isValidHost(host))
        return false;
    return !hasPort || isValidPort(port);
}

// The description is a plain string in TDLib, but a formattedText object is
// accepted too, so an older or richer payload still yields its text.
QString textFromFormatted(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    if (value.isObject())
        return value.toObject().value(QStringLiteral("text")).toString();
    return {};
}

// "web_app" is the TDLib spelling; "webApp" is accepted as well because the
// same description travels through camelCase JSON in some payloads.
QJsonObject webAppField(const QJsonObject& object)
{
    const QJsonValue snake = object.value(QStringLiteral("web_app"));
    if (snake.isObject())
        return snake.toObject();

    const QJsonValue camel = object.value(QStringLiteral("webApp"));
    if (camel.isObject())
        return camel.toObject();

    return {};
}

// The description sits on the message itself or on its content. An empty
// object carries nothing, so it falls through to the next place instead of
// shadowing a complete one.
QJsonObject webAppObject(const QJsonObject& message)
{
    QJsonObject object = webAppField(message);
    if (!object.isEmpty())
        return object;
    return webAppField(message.value(QStringLiteral("content")).toObject());
}

QJsonObject webAppOpenParameters()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("webAppOpenParameters")},
        // The palette is applied by the UI layer; an unknown theme is null.
        {QStringLiteral("theme"), QJsonValue::Null},
        {QStringLiteral("application_name"), QStringLiteral("BeyGram")},
        {QStringLiteral("mode"),
         QJsonObject{{QStringLiteral("@type"), QStringLiteral("webAppOpenModeFullSize")}}},
    };
}

} // namespace

WebAppInfo webAppFromMessage(const QJsonObject& message)
{
    WebAppInfo info;

    const QJsonObject object = webAppObject(message);
    if (object.isEmpty())
        return info;

    info.url = object.value(QStringLiteral("url")).toString();
    info.title = object.value(QStringLiteral("title")).toString();
    info.description =
        textFromFormatted(object.value(QStringLiteral("description")));

    info.botUserId = jsonInt64(object.value(QStringLiteral("bot_user_id")));
    if (info.botUserId <= 0) {
        // A Web App belongs to the bot that attached it, and via_bot_id is the
        // bot id a message carries when the button itself omits one. The sender
        // is not assumed to be a bot, so it is not used here.
        info.botUserId = jsonInt64(message.value(QStringLiteral("via_bot_id")));
    }

    info.isSecure = isSafeWebAppUrl(info.url);
    return info;
}

QJsonObject openWebAppRequest(qint64 chatId, qint64 botUserId, const QString& url)
{
    if (chatId <= 0 || botUserId <= 0)
        return {};

    const QString target = url.trimmed();
    if (!isSafeWebAppUrl(target))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("openWebApp")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("bot_user_id"), botUserId},
        {QStringLiteral("url"), target},
        // No topic and no reply: the launch belongs to the chat itself.
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("reply_to"), QJsonValue::Null},
        {QStringLiteral("parameters"), webAppOpenParameters()},
    };
}

bool isSafeWebAppUrl(const QString& url)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > kMaxWebAppUrlLength)
        return false;
    for (const QChar character : trimmed) {
        if (character.isSpace() || isControlCharacter(character))
            return false;
    }

    // The scheme is read by hand instead of through QUrl, which normalises and
    // guesses; "HTTPS://host" is accepted, "https:host" is not a URL Telegram
    // ever produces and is rejected.
    const qsizetype colon = trimmed.indexOf(QLatin1Char(':'));
    if (colon <= 0)
        return false;
    if (trimmed.left(colon).toLower() != QLatin1String("https"))
        return false;

    QString rest = trimmed.mid(colon + 1);
    if (!rest.startsWith(QLatin1String("//")))
        return false;
    rest = rest.mid(2);

    // The authority ends at the path, query or fragment.
    qsizetype authorityEnd = rest.size();
    for (qsizetype index = 0; index < rest.size(); ++index) {
        const QChar character = rest.at(index);
        if (character == QLatin1Char('/') || character == QLatin1Char('?')
            || character == QLatin1Char('#')) {
            authorityEnd = index;
            break;
        }
    }

    return isSafeAuthority(rest.left(authorityEnd));
}

} // namespace beygram::telegram
