#include "ChatCache.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace beygram::telegram {
namespace {

constexpr int CacheVersion = 1;

QJsonObject chatToJson(const ChatData& chat)
{
    return QJsonObject{
        {QStringLiteral("id"), chat.id},
        {QStringLiteral("title"), chat.title},
        {QStringLiteral("type"), chat.type},
        {QStringLiteral("preview"), chat.preview},
        {QStringLiteral("order"), chat.order},
        {QStringLiteral("lastMessageDate"), chat.lastMessageDate},
        {QStringLiteral("lastMessageId"), chat.lastMessageId},
        {QStringLiteral("unreadCount"), chat.unreadCount},
        {QStringLiteral("pinned"), chat.pinned},
        {QStringLiteral("muted"), chat.muted},
        {QStringLiteral("inMainList"), chat.inMainList},
        {QStringLiteral("archived"), chat.archived},
        {QStringLiteral("archiveOrder"), chat.archiveOrder},
        {QStringLiteral("isChannel"), chat.isChannel},
        {QStringLiteral("photoFileId"), chat.photoFileId},
        {QStringLiteral("initials"), chat.initials},
        {QStringLiteral("accentIndex"), chat.accentIndex},
    };
}

ChatData chatFromCacheJson(const QJsonObject& object)
{
    ChatData chat;
    chat.id = static_cast<qint64>(object.value(QStringLiteral("id")).toDouble());
    if (chat.id == 0)
        return {};
    chat.title = object.value(QStringLiteral("title")).toString();
    chat.type = object.value(QStringLiteral("type")).toString();
    chat.preview = object.value(QStringLiteral("preview")).toString();
    chat.order = static_cast<qint64>(object.value(QStringLiteral("order")).toDouble());
    chat.lastMessageDate = static_cast<qint64>(
        object.value(QStringLiteral("lastMessageDate")).toDouble());
    chat.lastMessageId = static_cast<qint64>(
        object.value(QStringLiteral("lastMessageId")).toDouble());
    chat.unreadCount = object.value(QStringLiteral("unreadCount")).toInt();
    chat.pinned = object.value(QStringLiteral("pinned")).toBool();
    chat.muted = object.value(QStringLiteral("muted")).toBool();
    chat.inMainList = object.value(QStringLiteral("inMainList")).toBool();
    chat.archived = object.value(QStringLiteral("archived")).toBool();
    chat.archiveOrder = static_cast<qint64>(
        object.value(QStringLiteral("archiveOrder")).toDouble());
    chat.isChannel = object.value(QStringLiteral("isChannel")).toBool();
    chat.photoFileId = object.value(QStringLiteral("photoFileId")).toInt();
    chat.initials = object.value(QStringLiteral("initials")).toString();
    chat.accentIndex = object.value(QStringLiteral("accentIndex")).toInt();
    return chat;
}

} // namespace

ChatCacheSnapshot loadChatCache(const QString& path)
{
    ChatCacheSnapshot snapshot;
    if (path.isEmpty())
        return snapshot;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return snapshot;

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return snapshot;

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != CacheVersion)
        return snapshot;

    snapshot.savedAt = static_cast<qint64>(
        root.value(QStringLiteral("savedAt")).toDouble());
    const QJsonArray chats = root.value(QStringLiteral("chats")).toArray();
    snapshot.chats.reserve(chats.size());
    for (const QJsonValue& value : chats) {
        const ChatData chat = chatFromCacheJson(value.toObject());
        if (chat.id != 0)
            snapshot.chats.append(chat);
    }
    return snapshot;
}

bool saveChatCache(const QString& path, const QVector<ChatData>& chats)
{
    if (path.isEmpty())
        return false;

    QJsonArray array;
    for (const ChatData& chat : chats) {
        if (chat.id != 0)
            array.append(chatToJson(chat));
    }

    QJsonObject root{
        {QStringLiteral("version"), CacheVersion},
        {QStringLiteral("savedAt"),
         static_cast<double>(QDateTime::currentSecsSinceEpoch())},
        {QStringLiteral("chats"), array},
    };

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return file.commit();
}

} // namespace beygram::telegram
