#include "DeepLinks.h"

#include <QByteArray>
#include <QList>
#include <QStringDecoder>
#include <QStringList>

namespace beygram::telegram {
namespace {

// Android caps the raw link at 2048 characters before parsing.
constexpr int kMaxDeepLinkLength = 2048;
constexpr int kMaxUsernameLength = 32;
constexpr int kMaxInviteHashLength = 128;
constexpr int kMinPhoneDigits = 3;
constexpr int kMaxPhoneDigits = 15; // E.164
constexpr qint64 kMaxInt53 = 9007199254740991LL;

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

// Control and format characters can hide a second value behind the visible one,
// so they are rejected before and after percent decoding.
bool isControlCharacter(QChar character)
{
    const QChar::Category category = character.category();
    return category == QChar::Other_Control || category == QChar::Other_Format;
}

int hexValue(QChar character)
{
    const char16_t code = character.unicode();
    if (code >= u'0' && code <= u'9')
        return code - u'0';
    if (code >= u'a' && code <= u'f')
        return code - u'a' + 10;
    if (code >= u'A' && code <= u'F')
        return code - u'A' + 10;
    return -1;
}

// Percent-decodes one path segment or query component. A '%' that is not
// followed by two hex digits, a raw byte above ASCII (Telegram links are ASCII
// on the wire) and a byte sequence that is not valid UTF-8 all fail, so a
// mangled escape can never smuggle a different value through.
bool decodeStrict(const QString& encoded, QString* decoded)
{
    QByteArray bytes;
    bytes.reserve(encoded.size());

    for (qsizetype index = 0; index < encoded.size(); ++index) {
        const QChar character = encoded.at(index);
        if (character == QLatin1Char('%')) {
            if (index + 2 >= encoded.size())
                return false;
            const int high = hexValue(encoded.at(index + 1));
            const int low = hexValue(encoded.at(index + 2));
            if (high < 0 || low < 0)
                return false;
            bytes.append(static_cast<char>((high << 4) | low));
            index += 2;
            continue;
        }
        if (character.unicode() > 0x7F)
            return false;
        bytes.append(static_cast<char>(character.unicode()));
    }

    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder.decode(bytes);
    if (decoder.hasError())
        return false;
    // A decoded control character would end up in a request field verbatim.
    for (const QChar character : text) {
        if (isControlCharacter(character))
            return false;
    }

    *decoded = text;
    return true;
}

struct QueryParameter {
    QString key;
    QString value;
};

using Query = QList<QueryParameter>;

// Parses the query in one pass: duplicate keys are rejected because the parser
// cannot know which one the sender meant, and a part without '=' is rejected
// because it has no value to read.
bool parseQuery(const QString& raw, Query* query)
{
    if (raw.isEmpty())
        return true;

    const QStringList parts = raw.split(QLatin1Char('&'));
    for (const QString& part : parts) {
        if (part.isEmpty())
            return false;

        const qsizetype equals = part.indexOf(QLatin1Char('='));
        if (equals <= 0)
            return false;

        QString key;
        QString value;
        if (!decodeStrict(part.left(equals), &key))
            return false;
        if (!decodeStrict(part.mid(equals + 1), &value))
            return false;
        if (key.isEmpty())
            return false;

        for (const QueryParameter& existing : *query) {
            if (existing.key == key)
                return false;
        }
        query->append(QueryParameter{key, value});
    }
    return true;
}

QString queryValue(const Query& query, const QString& key, bool* present = nullptr)
{
    for (const QueryParameter& parameter : query) {
        if (parameter.key == key) {
            if (present)
                *present = true;
            return parameter.value;
        }
    }
    if (present)
        *present = false;
    return {};
}

// Routes accept only the parameters they understand; an extra one is a sign the
// link was not produced by Telegram and would otherwise be silently dropped.
bool hasOnlyKeys(const Query& query, const QStringList& allowed)
{
    for (const QueryParameter& parameter : query) {
        if (!allowed.contains(parameter.key))
            return false;
    }
    return true;
}

// TDLib int53 ids: digits only, positive and inside the JSON-safe range.
bool parsePositiveId(const QString& text, qint64* id)
{
    if (text.isEmpty() || text.size() > 18)
        return false;
    for (const QChar character : text) {
        if (!isAsciiDigit(character))
            return false;
    }

    bool ok = false;
    const qint64 value = text.toLongLong(&ok);
    if (!ok || value <= 0 || value > kMaxInt53)
        return false;

    *id = value;
    return true;
}

// Telegram public usernames are ASCII: 1..32 characters, starting with a letter
// or a digit, otherwise letters, digits and '_'. Legacy short usernames such as
// "@gif" are allowed, but a purely numeric name is not a username.
bool isPublicUsername(const QString& username)
{
    if (username.isEmpty() || username.size() > kMaxUsernameLength)
        return false;
    if (!isAsciiAlnum(username.at(0)))
        return false;

    bool hasLetter = false;
    for (const QChar character : username) {
        if (isAsciiLetter(character)) {
            hasLetter = true;
            continue;
        }
        if (!isAsciiDigit(character) && character != QLatin1Char('_'))
            return false;
    }
    return hasLetter;
}

// Invite hashes are base64url-like: letters, digits, '_' and '-'.
bool isInviteHash(const QString& hash)
{
    if (hash.isEmpty() || hash.size() > kMaxInviteHashLength)
        return false;
    for (const QChar character : hash) {
        if (!isAsciiAlnum(character) && character != QLatin1Char('_')
            && character != QLatin1Char('-')) {
            return false;
        }
    }
    return true;
}

// tg://resolve?phone= and t.me/+<digits> both carry a bare number. Only digits
// with an optional leading '+' are accepted; the result always has the '+'.
bool normalizePhone(const QString& raw, QString* phone)
{
    QString digits = raw;
    if (digits.startsWith(QLatin1Char('+')))
        digits.remove(0, 1);
    if (digits.size() < kMinPhoneDigits || digits.size() > kMaxPhoneDigits)
        return false;
    for (const QChar character : digits) {
        if (!isAsciiDigit(character))
            return false;
    }

    *phone = QStringLiteral("+") + digits;
    return true;
}

struct RawLink {
    bool valid = false;
    bool telegramScheme = false;
    QString authority;    // lower-cased tg:// action or https host
    QStringList segments; // https path segments, still percent-encoded
    Query query;
};

// Splits the link into authority, path and query without QUrl, which is
// deliberately not used: QUrl normalises hosts, tolerates junk and would make
// "t.me.evil.com" or a userinfo host look plausible.
RawLink splitUrl(const QString& url)
{
    RawLink link;

    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > kMaxDeepLinkLength)
        return link;
    for (const QChar character : trimmed) {
        if (character.isSpace() || isControlCharacter(character))
            return link;
    }

    const qsizetype colon = trimmed.indexOf(QLatin1Char(':'));
    if (colon <= 0)
        return link;

    const QString scheme = trimmed.left(colon).toLower();
    const bool telegramScheme = scheme == QLatin1String("tg");
    const bool https = scheme == QLatin1String("https");
    if (!telegramScheme && !https)
        return link;

    QString rest = trimmed.mid(colon + 1);
    // "tg:resolve" and "https:t.me" are not links Telegram ever produces.
    if (!rest.startsWith(QLatin1String("//")))
        return link;
    rest = rest.mid(2);

    // Fragments never select a route; "#comment-123" is dropped, not parsed.
    const qsizetype fragment = rest.indexOf(QLatin1Char('#'));
    if (fragment >= 0)
        rest.truncate(fragment);

    QString rawQuery;
    const qsizetype question = rest.indexOf(QLatin1Char('?'));
    if (question >= 0) {
        rawQuery = rest.mid(question + 1);
        rest.truncate(question);
    }

    QString authority = rest;
    QString path;
    const qsizetype slash = rest.indexOf(QLatin1Char('/'));
    if (slash >= 0) {
        authority = rest.left(slash);
        path = rest.mid(slash);
    }

    // No userinfo and no port: Telegram links use the bare host or action.
    if (authority.isEmpty() || authority.contains(QLatin1Char('@'))
        || authority.contains(QLatin1Char(':'))) {
        return link;
    }
    if (!parseQuery(rawQuery, &link.query))
        return link;

    if (telegramScheme) {
        // Extra path segments are not part of any tg:// route.
        while (path.endsWith(QLatin1Char('/')))
            path.chop(1);
        if (!path.isEmpty())
            return link;
        link.authority = authority.toLower();
        link.telegramScheme = true;
        link.valid = true;
        return link;
    }

    static const QStringList hosts{
        QStringLiteral("t.me"),
        QStringLiteral("www.t.me"),
        QStringLiteral("telegram.me"),
        QStringLiteral("www.telegram.me"),
    };
    link.authority = authority.toLower();
    if (!hosts.contains(link.authority))
        return link;

    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    if (path.isEmpty())
        return link; // "https://t.me/" has no route

    link.segments = path.mid(1).split(QLatin1Char('/'));
    for (const QString& segment : link.segments) {
        // "t.me/name//123" is malformed, not a link to message 123.
        if (segment.isEmpty())
            return link;
    }

    link.valid = true;
    return link;
}

QStringList decodeSegments(const QStringList& encoded, bool* ok)
{
    QStringList decoded;
    decoded.reserve(encoded.size());
    for (const QString& segment : encoded) {
        QString text;
        if (!decodeStrict(segment, &text)) {
            *ok = false;
            return {};
        }
        decoded.append(text);
    }
    *ok = true;
    return decoded;
}

// Routes whose first https path segment is a Telegram feature, never a
// username. "s" is the web-preview prefix, "c" addresses a private channel by
// id (which DeepLink cannot express), and the rest are app entry points.
const QStringList& reservedUsernames()
{
    static const QStringList reserved{
        QStringLiteral("login"),
        QStringLiteral("addstickers"),
        QStringLiteral("addemoji"),
        QStringLiteral("addtheme"),
        QStringLiteral("addlist"),
        QStringLiteral("share"),
        QStringLiteral("invoice"),
        QStringLiteral("giftcode"),
        QStringLiteral("boost"),
        QStringLiteral("confirmphone"),
        QStringLiteral("setlanguage"),
        QStringLiteral("s"),
        QStringLiteral("c"),
        QStringLiteral("bg"),
        QStringLiteral("contact"),
        QStringLiteral("iv"),
        QStringLiteral("proxy"),
        QStringLiteral("socks"),
    };
    return reserved;
}

DeepLink parseResolve(const RawLink& link)
{
    DeepLink result;

    static const QStringList allowed{
        QStringLiteral("domain"),
        QStringLiteral("phone"),
        QStringLiteral("post"),
        QStringLiteral("thread"),
        QStringLiteral("start"),
        QStringLiteral("startgroup"),
    };
    if (!hasOnlyKeys(link.query, allowed))
        return result;

    bool hasDomain = false;
    bool hasPhone = false;
    const QString domain = queryValue(link.query, QStringLiteral("domain"), &hasDomain);
    const QString phone = queryValue(link.query, QStringLiteral("phone"), &hasPhone);
    // Exactly one of the two; "resolve" with neither has no target.
    if (hasDomain == hasPhone)
        return result;

    if (hasPhone) {
        // post/thread/start do not apply to a phone link.
        if (link.query.size() != 1)
            return result;
        QString normalized;
        if (!normalizePhone(phone, &normalized))
            return result;
        result.kind = DeepLink::Kind::Phone;
        result.value = normalized;
        return result;
    }

    if (!isPublicUsername(domain))
        return result;

    bool hasPost = false;
    bool hasThread = false;
    bool hasStart = false;
    bool hasStartGroup = false;
    const QString post = queryValue(link.query, QStringLiteral("post"), &hasPost);
    const QString thread = queryValue(link.query, QStringLiteral("thread"), &hasThread);
    const QString start = queryValue(link.query, QStringLiteral("start"), &hasStart);
    const QString startGroup =
        queryValue(link.query, QStringLiteral("startgroup"), &hasStartGroup);

    if (hasStart && hasStartGroup)
        return result;
    if (hasThread && !hasPost)
        return result;

    if (hasPost) {
        qint64 postId = 0;
        if (!parsePositiveId(post, &postId))
            return result;
        if (hasThread) {
            qint64 threadId = 0;
            if (!parsePositiveId(thread, &threadId))
                return result;
        }
        // A post link has no start payload; mixing them is not a Telegram link.
        if (hasStart || hasStartGroup)
            return result;

        result.kind = DeepLink::Kind::Message;
        result.value = domain;
        result.messageId = postId;
        return result;
    }

    if (hasStart || hasStartGroup) {
        // "startgroup" is reported as BotStart too: the caller only opens the
        // bot, so the payload is informational either way.
        const QString payload = hasStart ? start : startGroup;
        if (payload.isEmpty()) {
            result.kind = DeepLink::Kind::PublicChat;
            result.value = domain;
            return result;
        }
        result.kind = DeepLink::Kind::BotStart;
        result.value = domain;
        result.botStartParameter = payload;
        return result;
    }

    result.kind = DeepLink::Kind::PublicChat;
    result.value = domain;
    return result;
}

DeepLink parseJoin(const RawLink& link)
{
    DeepLink result;

    if (!hasOnlyKeys(link.query, QStringList{QStringLiteral("invite")}))
        return result;

    bool hasInvite = false;
    QString invite = queryValue(link.query, QStringLiteral("invite"), &hasInvite);
    if (!hasInvite)
        return result;
    if (invite.startsWith(QLatin1Char('+')))
        invite.remove(0, 1);
    if (!isInviteHash(invite))
        return result;

    result.kind = DeepLink::Kind::Invite;
    result.value = invite;
    return result;
}

DeepLink parseUser(const RawLink& link)
{
    DeepLink result;

    if (!hasOnlyKeys(link.query, QStringList{QStringLiteral("id")}))
        return result;

    bool hasId = false;
    const QString id = queryValue(link.query, QStringLiteral("id"), &hasId);
    if (!hasId)
        return result;

    qint64 userId = 0;
    if (!parsePositiveId(id, &userId))
        return result;

    result.kind = DeepLink::Kind::PrivateChat;
    result.value = QString::number(userId);
    return result;
}

DeepLink parseTelegramScheme(const RawLink& link)
{
    const QString& action = link.authority;
    if (action == QLatin1String("resolve"))
        return parseResolve(link);
    if (action == QLatin1String("join"))
        return parseJoin(link);
    if (action == QLatin1String("user"))
        return parseUser(link);
    if (action == QLatin1String("settings")) {
        if (!link.query.isEmpty())
            return {};
        DeepLink result;
        result.kind = DeepLink::Kind::Settings;
        return result;
    }

    // Every other action stays Unknown on purpose. In particular tg://login
    // carries a one-time token that is recognised only to be ignored, so the
    // token never reaches a field, a log or a lookup.
    return {};
}

DeepLink parseHttpsLink(const QStringList& segments, const Query& query)
{
    DeepLink result;
    if (segments.isEmpty())
        return result;

    const QString& first = segments.at(0);
    const qsizetype count = segments.size();

    if (first.compare(QLatin1String("joinchat"), Qt::CaseInsensitive) == 0) {
        if (count != 2 || !query.isEmpty())
            return result;
        if (!isInviteHash(segments.at(1)))
            return result;

        result.kind = DeepLink::Kind::Invite;
        result.value = segments.at(1);
        return result;
    }

    // t.me/+<value> is either an invite or a phone number; Telegram itself tells
    // them apart by "all digits means phone".
    if (first.startsWith(QLatin1Char('+'))) {
        if (count != 1 || !query.isEmpty())
            return result;

        const QString value = first.mid(1);
        QString normalized;
        if (normalizePhone(value, &normalized)) {
            result.kind = DeepLink::Kind::Phone;
            result.value = normalized;
            return result;
        }
        if (isInviteHash(value)) {
            result.kind = DeepLink::Kind::Invite;
            result.value = value;
        }
        return result;
    }

    if (reservedUsernames().contains(first.toLower()))
        return result;
    if (!isPublicUsername(first))
        return result;

    // Only these parameters are meaningful on a username link. "single" is the
    // web view flag and changes nothing here; "comment" would point at a
    // different message, so it is rejected rather than opened wrongly.
    static const QStringList allowed{
        QStringLiteral("start"),
        QStringLiteral("startgroup"),
        QStringLiteral("single"),
    };
    if (!hasOnlyKeys(query, allowed))
        return result;

    bool hasStart = false;
    bool hasStartGroup = false;
    bool hasSingle = false;
    const QString start = queryValue(query, QStringLiteral("start"), &hasStart);
    const QString startGroup = queryValue(query, QStringLiteral("startgroup"), &hasStartGroup);
    queryValue(query, QStringLiteral("single"), &hasSingle);

    if (hasStart && hasStartGroup)
        return result;

    if (count == 1) {
        if (hasSingle)
            return result;
        if (hasStart || hasStartGroup) {
            const QString payload = hasStart ? start : startGroup;
            if (payload.isEmpty()) {
                result.kind = DeepLink::Kind::PublicChat;
                result.value = first;
                return result;
            }
            result.kind = DeepLink::Kind::BotStart;
            result.value = first;
            result.botStartParameter = payload;
            return result;
        }

        result.kind = DeepLink::Kind::PublicChat;
        result.value = first;
        return result;
    }

    // A start payload belongs to the chat itself, not to one of its messages.
    if (hasStart || hasStartGroup)
        return result;

    if (count == 2) {
        qint64 postId = 0;
        if (!parsePositiveId(segments.at(1), &postId))
            return result;

        result.kind = DeepLink::Kind::Message;
        result.value = first;
        result.messageId = postId;
        return result;
    }

    if (count == 3) {
        qint64 topicId = 0;
        qint64 postId = 0;
        if (!parsePositiveId(segments.at(1), &topicId))
            return result;
        if (!parsePositiveId(segments.at(2), &postId))
            return result;

        result.kind = DeepLink::Kind::Message;
        result.value = first;
        result.messageId = postId;
        return result;
    }

    // /c/<id>/<post> and any other four-segment form are out of scope.
    return result;
}

} // namespace

DeepLink parseDeepLink(const QString& url)
{
    const RawLink link = splitUrl(url);
    if (!link.valid)
        return {};
    if (link.telegramScheme)
        return parseTelegramScheme(link);
    if (link.segments.isEmpty())
        return {};

    bool ok = false;
    const QStringList segments = decodeSegments(link.segments, &ok);
    if (!ok)
        return {};
    return parseHttpsLink(segments, link.query);
}

QJsonObject resolvePublicChatRequest(const QString& username)
{
    QString name = username.trimmed();
    if (name.startsWith(QLatin1Char('@')))
        name.remove(0, 1);
    if (!isPublicUsername(name))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("searchPublicChat")},
        {QStringLiteral("username"), name},
    };
}

QJsonObject joinChatRequest(const QString& inviteLink)
{
    const QString input = inviteLink.trimmed();
    if (input.isEmpty())
        return {};

    QString hash;
    if (input.startsWith(QLatin1Char('+'))) {
        hash = input.mid(1);
    } else if (input.contains(QLatin1Char('/'))) {
        const DeepLink link = parseDeepLink(input);
        if (link.kind == DeepLink::Kind::Invite)
            hash = link.value;
    } else {
        // A bare hash, the form a user may paste from a chat message.
        hash = input;
    }

    if (!isInviteHash(hash))
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("joinChatByInviteLink")},
        {QStringLiteral("invite_link"), QStringLiteral("https://t.me/+") + hash},
    };
}

} // namespace beygram::telegram
