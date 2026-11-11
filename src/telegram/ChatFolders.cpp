#include "ChatFolders.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>

namespace beygram::telegram {
namespace {

constexpr int kChatFoldersLimit = 100;

QString mainListTitle()
{
    return QStringLiteral("All chats");
}

// A folder title may arrive as chatFolderName { text: formattedText }, as a
// bare formattedText object, as a plain string or as a plain "title" field.
QString folderTitleFromValue(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    if (!value.isObject())
        return QString();

    const QJsonObject object = value.toObject();
    const QJsonValue text = object.value(QStringLiteral("text"));
    if (text.isString())
        return text.toString();
    if (text.isObject()) {
        const QString formatted = text.toObject().value(QStringLiteral("text")).toString();
        if (!formatted.isEmpty())
            return formatted;
    }
    return object.value(QStringLiteral("title")).toString();
}

// The custom emoji identifier lives in icon.custom_emoji_id in some TDLib
// versions and in icon.name in others; both are normalized to a string.
QString folderIconFromValue(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    if (!value.isObject())
        return QString();

    const QJsonObject object = value.toObject();
    const QJsonValue emojiId = object.value(QStringLiteral("custom_emoji_id"));
    if (emojiId.isString())
        return emojiId.toString();
    if (emojiId.isDouble())
        return QString::number(jsonInt64(emojiId));
    return object.value(QStringLiteral("name")).toString();
}

int folderPositionFromObject(const QJsonObject& object, int fallback)
{
    const QJsonValue position = object.value(QStringLiteral("position"));
    if (position.isString() || position.isDouble())
        return static_cast<int>(jsonInt64(position));
    return fallback;
}

bool hasArchiveChatList(const QJsonValue& value)
{
    if (value.isString())
        return value.toString() == QLatin1String("chatListArchive");
    return value.toObject().value(QStringLiteral("@type")).toString()
        == QLatin1String("chatListArchive");
}

ChatFolderInfo folderFromObject(const QJsonObject& object, int index)
{
    ChatFolderInfo folder;
    folder.id = jsonInt64(object.value(QStringLiteral("id")));
    folder.title = folderTitleFromValue(object.value(QStringLiteral("title")));
    if (folder.title.isEmpty())
        folder.title = folderTitleFromValue(object.value(QStringLiteral("name")));
    folder.icon = folderIconFromValue(object.value(QStringLiteral("icon")));
    folder.position = folderPositionFromObject(object, index);
    folder.unreadCount = static_cast<int>(
        jsonInt64(object.value(QStringLiteral("unread_count"))));

    const QJsonValue isMain = object.value(QStringLiteral("is_main"));
    folder.isMain = isMain.isBool() ? isMain.toBool() : folder.id == 0;
    if (folder.isMain) {
        // The main list is always id 0 and always carries a title.
        folder.id = 0;
        if (folder.title.isEmpty())
            folder.title = mainListTitle();
    }
    return folder;
}

} // namespace

ChatFoldersState chatFoldersFromUpdate(const QJsonObject& update)
{
    ChatFoldersState state;
    if (update.value(QStringLiteral("@type")).toString() != QLatin1String("updateChatFolders"))
        return state;

    const QJsonArray items = update.value(QStringLiteral("chat_folders")).toArray();
    bool mainFound = false;
    QVector<ChatFolderInfo> customFolders;
    customFolders.reserve(items.size());

    int index = 0;
    for (const QJsonValue& value : items) {
        if (!value.isObject()) {
            ++index;
            continue;
        }

        ChatFolderInfo folder = folderFromObject(value.toObject(), index);
        if (folder.isMain) {
            if (!mainFound) {
                state.folders.append(folder);
                mainFound = true;
            }
        } else {
            customFolders.append(folder);
        }
        ++index;
    }

    if (!mainFound) {
        ChatFolderInfo main;
        main.id = 0;
        main.title = mainListTitle();
        main.isMain = true;
        main.position = static_cast<int>(
            jsonInt64(update.value(QStringLiteral("main_chat_list_position"))));
        state.folders.prepend(main);
    }

    std::stable_sort(customFolders.begin(), customFolders.end(),
                     [](const ChatFolderInfo& left, const ChatFolderInfo& right) {
                         return left.position < right.position;
                     });
    for (const ChatFolderInfo& folder : customFolders)
        state.folders.append(folder);

    state.hasArchive = update.value(QStringLiteral("has_archive")).toBool(false);
    const QJsonValue archiveList = update.value(QStringLiteral("archive_chat_list"));
    if (hasArchiveChatList(archiveList))
        state.hasArchive = true;
    // chatListArchive carries no identifier, so the archive list is always 0.
    state.archiveChatListId = 0;

    if (state.hasArchive) {
        for (ChatFolderInfo& folder : state.folders)
            folder.hasArchive = true;
    }
    return state;
}

QJsonObject getChatFoldersRequest()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatFolders")},
        {QStringLiteral("chat_list"), QJsonValue::Null},
        {QStringLiteral("limit"), kChatFoldersLimit},
    };
}

QJsonObject getChatFolderRequest(qint64 folderId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getChatFolder")},
        {QStringLiteral("chat_folder_id"), folderId},
    };
}

} // namespace beygram::telegram
