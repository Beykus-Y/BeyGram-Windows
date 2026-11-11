#include "ArchiveChats.h"

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

// chatListArchive carries no identifier, so the archive is always this exact
// object; it is never a null chat_list (that one means the main list).
QJsonObject archiveChatList()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("chatListArchive")},
    };
}

QJsonObject mainChatList()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("chatListMain")},
    };
}

QJsonObject chatsRequest(const QString& type, int limit)
{
    return QJsonObject{
        {QStringLiteral("@type"), type},
        {QStringLiteral("chat_list"), archiveChatList()},
        {QStringLiteral("limit"), clampedLimit(limit)},
    };
}

} // namespace

QJsonObject getArchivedChatsRequest(int limit)
{
    return chatsRequest(QStringLiteral("getChats"), limit);
}

QJsonObject loadArchivedChatsRequest(int limit)
{
    return chatsRequest(QStringLiteral("loadChats"), limit);
}

QJsonObject setChatArchivedRequest(qint64 chatId, bool archived)
{
    // The schema used by this client has no toggleChatIsArchived and no
    // setChatArchived. Moving the chat between the Main and Archive chat lists
    // with addChatToList is the supported way to archive it: TDLib drops the
    // chat from the other list because a chat can't be in both at once.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("addChatToList")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("chat_list"), archived ? archiveChatList() : mainChatList()},
    };
}

bool responseIsArchivedChats(const QJsonObject& response)
{
    return response.value(QStringLiteral("@type")).toString() == QLatin1String("chats");
}

} // namespace beygram::telegram
