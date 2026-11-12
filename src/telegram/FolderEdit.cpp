#include "FolderEdit.h"

#include <QJsonArray>
#include <QJsonValue>

namespace beygram::telegram {
namespace {

constexpr int kMaxFolderTitleLength = 12;
constexpr int kNoFolderColor = -1;

/// Plain formattedText without entities. The folder name is sent as text, never
/// as markup; TDLib strips every entity except custom emoji from a folder name.
QJsonObject formattedText(const QString& text)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("formattedText")},
        {QStringLiteral("text"), text},
        {QStringLiteral("entities"), QJsonArray{}},
    };
}

/// Keeps at most maxCodePoints Unicode code points. QString is UTF-16, so a
/// surrogate pair is advanced as one character instead of being cut in half.
QString truncateToCodePoints(const QString& text, int maxCodePoints)
{
    int index = 0;
    int codePoints = 0;
    while (index < text.size() && codePoints < maxCodePoints) {
        const bool surrogatePair = text.at(index).isHighSurrogate()
            && index + 1 < text.size() && text.at(index + 1).isLowSurrogate();
        index += surrogatePair ? 2 : 1;
        ++codePoints;
    }
    return text.left(index);
}

/// Folds every whitespace run (including the line feeds the schema forbids)
/// into one space, trims the ends and applies the 12-character limit. An empty
/// result stays empty so TDLib rejects it instead of a placeholder name being
/// invented here.
QString folderTitle(const QString& title)
{
    QString folded;
    folded.reserve(title.size());
    bool pendingSpace = false;
    for (const QChar character : title) {
        if (character.isSpace()) {
            pendingSpace = true;
            continue;
        }
        if (pendingSpace && !folded.isEmpty())
            folded.append(QLatin1Char(' '));
        pendingSpace = false;
        folded.append(character);
    }
    return truncateToCodePoints(folded, kMaxFolderTitleLength).trimmed();
}

QJsonObject chatFolderName(const QString& title)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("chatFolderName")},
        {QStringLiteral("text"), formattedText(folderTitle(title))},
        // The title is plain text, so it cannot contain custom emoji to animate.
        {QStringLiteral("animate_custom_emoji"), false},
    };
}

QJsonObject chatFolder(const QString& title)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("chatFolder")},
        {QStringLiteral("name"), chatFolderName(title)},
        // A null icon lets Telegram choose the default icon for the folder.
        {QStringLiteral("icon"), QJsonValue::Null},
        {QStringLiteral("color_id"), kNoFolderColor},
        {QStringLiteral("is_shareable"), false},
        {QStringLiteral("pinned_chat_ids"), QJsonArray{}},
        {QStringLiteral("included_chat_ids"), QJsonArray{}},
        {QStringLiteral("excluded_chat_ids"), QJsonArray{}},
        {QStringLiteral("exclude_muted"), false},
        {QStringLiteral("exclude_read"), false},
        {QStringLiteral("exclude_archived"), false},
        {QStringLiteral("include_contacts"), false},
        {QStringLiteral("include_non_contacts"), false},
        {QStringLiteral("include_bots"), false},
        {QStringLiteral("include_groups"), false},
        {QStringLiteral("include_channels"), false},
    };
}

} // namespace

QJsonObject createChatFolderRequest(const QString& title)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("createChatFolder")},
        {QStringLiteral("folder"), chatFolder(title)},
    };
}

QJsonObject editChatFolderRequest(qint64 folderId, const QString& title)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("editChatFolder")},
        {QStringLiteral("chat_folder_id"), folderId},
        {QStringLiteral("folder"), chatFolder(title)},
    };
}

QJsonObject deleteChatFolderRequest(qint64 folderId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("deleteChatFolder")},
        {QStringLiteral("chat_folder_id"), folderId},
        // Deleting a folder never leaves its chats; an empty list is required
        // by the schema and keeps the folder removal non-destructive.
        {QStringLiteral("leave_chat_ids"), QJsonArray{}},
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
