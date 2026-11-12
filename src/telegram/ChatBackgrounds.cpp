#include "ChatBackgrounds.h"

#include "TelegramTypes.h"

#include <QJsonValue>

#include <limits>

namespace beygram::telegram {
namespace {

// The four kinds the renderer knows, shared with BackgroundKind and the
// appearance layer. The background protocol spells them as lower-case strings.
QString canonicalBackgroundType(const QString& type)
{
    const QString normalized = type.trimmed().toLower();
    if (normalized == QLatin1String("wallpaper")
        || normalized == QLatin1String("pattern")
        || normalized == QLatin1String("solid")
        || normalized == QLatin1String("gradient")) {
        return normalized;
    }
    return {};
}

// File ids are int32 values. An id outside that range means "no usable file"
// rather than a truncated id that would download the wrong document.
int documentFileId(const QJsonObject& background)
{
    const qint64 id = jsonInt64(
        background.value(QStringLiteral("document"))
            .toObject()
            .value(QStringLiteral("file"))
            .toObject()
            .value(QStringLiteral("id")));
    if (id <= 0 || id > std::numeric_limits<int>::max())
        return 0;
    return static_cast<int>(id);
}

} // namespace

QJsonObject getBackgroundsRequest(bool forDarkTheme)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getBackgrounds")},
        {QStringLiteral("for_dark_theme"), forDarkTheme},
    };
}

QJsonObject getBackgroundUrlRequest(const QString& name, const QString& type)
{
    const QString trimmedName = name.trimmed();
    const QString canonicalType = canonicalBackgroundType(type);
    // A URL cannot be built without a name, and a type the renderer does not
    // know would produce an asset it cannot paint.
    if (trimmedName.isEmpty() || canonicalType.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getBackgroundUrl")},
        {QStringLiteral("name"), trimmedName},
        {QStringLiteral("type"), canonicalType},
    };
}

QJsonObject setBackgroundRequest(qint64 backgroundId, qint64 chatId)
{
    // A background without a positive id cannot be addressed. chatId 0 is the
    // global scope, and negative ids are real chats, so both are sent as-is.
    if (backgroundId <= 0)
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setBackground")},
        {QStringLiteral("background_id"), backgroundId},
        {QStringLiteral("chat_id"), chatId},
    };
}

QJsonObject setBackgroundTypeRequest(bool forDarkTheme, const QString& type)
{
    const QString canonicalType = canonicalBackgroundType(type);
    if (canonicalType.isEmpty())
        return {};

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setBackgroundType")},
        {QStringLiteral("for_dark_theme"), forDarkTheme},
        {QStringLiteral("type"), canonicalType},
    };
}

BackgroundInfo backgroundFromJson(const QJsonObject& background)
{
    BackgroundInfo info;

    // Only the id is required: without a positive one the entry cannot be
    // selected or looked up, so it is reported as malformed (id 0) instead of
    // being stored with a half-filled payload.
    const qint64 id = jsonInt64(background.value(QStringLiteral("id")));
    if (id <= 0)
        return info;

    info.id = id;
    info.name = background.value(QStringLiteral("name")).toString().trimmed();
    info.isDefault = background.value(QStringLiteral("is_default")).toBool();
    info.isDark = background.value(QStringLiteral("is_dark")).toBool();
    info.fileId = documentFileId(background);
    return info;
}

} // namespace beygram::telegram
