#include "NotificationScopes.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>

#include <cmath>

namespace beygram::telegram {
namespace {

// Scope ids are int64 in the wire shape but arrive through QJsonValue, which
// stores numbers as doubles. Accept only values that name an integer exactly,
// and also accept numeric strings because the transport may serialize int64
// values that way.
bool scopeIdFromValue(const QJsonValue& value, qint64* scopeId)
{
    if (value.isDouble()) {
        const double number = value.toDouble();
        if (!std::isfinite(number) || std::floor(number) != number)
            return false;
        if (number < -9223372036854775808.0 || number >= 9223372036854775808.0)
            return false;

        *scopeId = static_cast<qint64>(number);
        return true;
    }

    if (value.isString()) {
        bool ok = false;
        const qint64 parsed = value.toString().trimmed().toLongLong(&ok);
        if (!ok)
            return false;

        *scopeId = parsed;
        return true;
    }

    return false;
}

// A field that is absent keeps the struct default. A field that is present with
// another type makes the whole entry malformed instead of silently reading as
// false or empty.
bool boolField(const QJsonObject& object, const QString& key, bool fallback, bool* result)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined()) {
        *result = fallback;
        return true;
    }

    if (!value.isBool())
        return false;

    *result = value.toBool();
    return true;
}

bool titleField(const QJsonObject& object, QString* title)
{
    const QJsonValue value = object.value(QStringLiteral("title"));
    if (value.isUndefined()) {
        title->clear();
        return true;
    }

    if (!value.isString())
        return false;

    *title = value.toString().trimmed();
    return true;
}

bool scopeFromObject(const QJsonObject& object, NotificationScope* scope)
{
    qint64 scopeId = 0;
    if (!scopeIdFromValue(object.value(QStringLiteral("scope_id")), &scopeId))
        return false;
    if (scopeId < 0)
        return false;

    NotificationScope parsed;
    parsed.scopeId = scopeId;

    bool muted = false;
    bool showPreview = true;
    bool sound = true;
    if (!titleField(object, &parsed.title)
        || !boolField(object, QStringLiteral("muted"), false, &muted)
        || !boolField(object, QStringLiteral("show_preview"), true, &showPreview)
        || !boolField(object, QStringLiteral("sound"), true, &sound)) {
        return false;
    }

    parsed.muted = muted;
    parsed.showPreview = showPreview;
    parsed.sound = sound;
    *scope = parsed;
    return true;
}

} // namespace

QJsonObject getNotificationScopeRequest(qint64 scopeId)
{
    if (scopeId < 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getNotificationScope")},
        {QStringLiteral("scope_id"), scopeId},
    };
}

QJsonObject setNotificationScopeRequest(const NotificationScope& scope)
{
    if (scope.scopeId < 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setNotificationScope")},
        {QStringLiteral("scope_id"), scope.scopeId},
        {QStringLiteral("title"), scope.title.trimmed()},
        {QStringLiteral("muted"), scope.muted},
        {QStringLiteral("show_preview"), scope.showPreview},
        {QStringLiteral("sound"), scope.sound},
    };
}

QJsonObject setScopeChatRequest(qint64 scopeId, qint64 chatId, bool enabled)
{
    // Chat ids are int53 and supergroups/channels are negative, so only zero is
    // invalid.
    if (scopeId < 0 || chatId == 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setScopeChat")},
        {QStringLiteral("scope_id"), scopeId},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("enabled"), enabled},
    };
}

QVector<NotificationScope> notificationScopesFromResponse(const QJsonObject& response)
{
    const QJsonValue scopesValue = response.value(QStringLiteral("scopes"));
    if (!scopesValue.isArray())
        return {};

    const QJsonArray entries = scopesValue.toArray();

    QVector<NotificationScope> scopes;
    scopes.reserve(entries.size());

    QSet<qint64> seen;
    seen.reserve(entries.size());

    for (const QJsonValue& entry : entries) {
        if (!entry.isObject())
            continue;

        NotificationScope scope;
        if (!scopeFromObject(entry.toObject(), &scope))
            continue;
        if (seen.contains(scope.scopeId))
            continue;

        seen.insert(scope.scopeId);
        scopes.append(scope);
    }

    return scopes;
}

} // namespace beygram::telegram
