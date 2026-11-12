#include "MessageProperties.h"

#include <QJsonValue>
#include <QString>

namespace beygram::telegram {
namespace {

/// Copies a boolean field only when the response carries one. An absent or
/// malformed field leaves the documented default in place instead of turning
/// an unknown into a denial (or a permission).
void applyBool(const QJsonObject& response, const QString& key, bool& target)
{
    const QJsonValue value = response.value(key);
    if (value.isBool())
        target = value.toBool();
}

bool flag(const QJsonObject& response, const QString& key)
{
    return response.value(key).toBool();
}

} // namespace

QJsonObject getMessagePropertiesRequest(qint64 chatId, qint64 messageId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getMessageProperties")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
    };
}

MessageProperties propertiesFromResponse(const QJsonObject& response)
{
    MessageProperties properties;

    // A failed lookup must not grant anything. canBeCopied stays true because
    // copying text to the local clipboard is not a server-side action.
    if (response.value(QStringLiteral("@type")).toString() == QLatin1String("error"))
        return properties;

    applyBool(response, QStringLiteral("can_be_forwarded"), properties.canBeForwarded);
    applyBool(response, QStringLiteral("can_be_saved"), properties.canBeSaved);
    applyBool(response, QStringLiteral("can_be_copied"), properties.canBeCopied);
    applyBool(response, QStringLiteral("can_be_deleted_only_for_self"),
              properties.canBeDeletedOnlyForSelf);
    applyBool(response, QStringLiteral("can_be_deleted_for_all_users"),
              properties.canBeDeletedForAllUsers);
    applyBool(response, QStringLiteral("can_get_added_reactions"),
              properties.canGetAddedReactions);

    // Newer TDLib revisions report protected content through these flags; a
    // message flagged here can never be saved, whatever can_be_saved says.
    if (flag(response, QStringLiteral("has_protected_content_by_current_user"))
        || flag(response, QStringLiteral("has_protected_content_by_other_user"))) {
        properties.canBeSaved = false;
    }

    return properties;
}

} // namespace beygram::telegram
