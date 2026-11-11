#include "FolderChats.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <QString>

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kMinChatsLimit = 1;
constexpr int kMaxChatsLimit = 100;

int clampedLimit(int limit)
{
    return std::clamp(limit, kMinChatsLimit, kMaxChatsLimit);
}

// TDLib takes a null chat_list for the main list and chatListFolder for a
// custom folder. chatListArchive carries no identifier, so it is not built
// here: the archive is only reachable through its own chat list object.
QJsonValue chatListForFolder(qint64 folderId)
{
    if (folderId <= 0)
        return QJsonValue(QJsonValue::Null);

    return QJsonValue(QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("chatListFolder")},
        {QStringLiteral("chat_folder_id"), folderId},
    });
}

QJsonObject chatsRequest(const QString& type, qint64 folderId, int limit)
{
    return QJsonObject{
        {QStringLiteral("@type"), type},
        {QStringLiteral("chat_list"), chatListForFolder(folderId)},
        {QStringLiteral("limit"), clampedLimit(limit)},
    };
}

// Chat ids arrive either as JSON numbers or as numeric strings, because TDLib
// int53 values may be serialized as strings by the transport.
bool chatIdFromValue(const QJsonValue& value, qint64* id)
{
    if (value.isDouble()) {
        *id = value.toVariant().toLongLong();
        return true;
    }

    if (value.isString()) {
        bool ok = false;
        const qint64 parsed = value.toString().trimmed().toLongLong(&ok);
        if (!ok)
            return false;
        *id = parsed;
        return true;
    }

    return false;
}

} // namespace

QJsonObject getChatsForFolderRequest(qint64 folderId, int limit)
{
    return chatsRequest(QStringLiteral("getChats"), folderId, limit);
}

QJsonObject loadChatsForFolderRequest(qint64 folderId, int limit)
{
    return chatsRequest(QStringLiteral("loadChats"), folderId, limit);
}

QVector<qint64> chatIdsFromResponse(const QJsonObject& response)
{
    const QJsonArray ids = response.value(QStringLiteral("chat_ids")).toArray();

    QVector<qint64> result;
    result.reserve(ids.size());
    QSet<qint64> seen;
    seen.reserve(ids.size());

    for (const QJsonValue& value : ids) {
        qint64 id = 0;
        if (!chatIdFromValue(value, &id))
            continue;
        if (id == 0)
            continue;
        if (seen.contains(id))
            continue;

        seen.insert(id);
        result.append(id);
    }

    return result;
}

bool responseIsChats(const QJsonObject& response)
{
    return response.value(QStringLiteral("@type")).toString() == QLatin1String("chats");
}

} // namespace beygram::telegram
