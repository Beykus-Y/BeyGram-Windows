#include "TypingStatus.h"

#include <QJsonValue>

namespace beygram::telegram {
namespace {

// ChatAction uploading variants carry progress:int32. The client only
// announces the activity, so the reported percentage stays at 0.
constexpr int kNoProgress = 0;

QJsonObject chatAction(const QString& actionType, bool carriesProgress)
{
    QJsonObject action{
        {QStringLiteral("@type"), actionType},
    };
    if (carriesProgress)
        action.insert(QStringLiteral("progress"), kNoProgress);
    return action;
}

QJsonObject sendChatActionRequest(qint64 chatId, const QString& actionType,
                                  bool carriesProgress)
{
    // This schema expresses a thread with the nullable topic_id (there is no
    // message_thread_id in sendChatAction) and identifies the business account
    // with business_connection_id, empty for the user's own account.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("sendChatAction")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("topic_id"), QJsonValue::Null},
        {QStringLiteral("business_connection_id"), QString()},
        {QStringLiteral("action"), chatAction(actionType, carriesProgress)},
    };
}

} // namespace

QJsonObject sendTypingRequest(qint64 chatId)
{
    return sendChatActionRequest(chatId, QStringLiteral("chatActionTyping"), false);
}

QJsonObject sendUploadPhotoRequest(qint64 chatId)
{
    return sendChatActionRequest(chatId, QStringLiteral("chatActionUploadingPhoto"), true);
}

QJsonObject sendUploadDocumentRequest(qint64 chatId)
{
    return sendChatActionRequest(chatId, QStringLiteral("chatActionUploadingDocument"), true);
}

QString chatActionLabel(const QString& actionType)
{
    if (actionType == QLatin1String("chatActionTyping"))
        return QStringLiteral("typing…");
    if (actionType == QLatin1String("chatActionUploadingPhoto"))
        return QStringLiteral("sending a photo…");
    if (actionType == QLatin1String("chatActionUploadingDocument"))
        return QStringLiteral("sending a file…");
    return QString();
}

} // namespace beygram::telegram
