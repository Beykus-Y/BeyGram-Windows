#include "StickerInfo.h"

#include "TelegramTypes.h"

#include <QJsonValue>

#include <limits>

namespace beygram::telegram {
namespace {

/// File ids are TDLib int32 values; anything outside that range is treated as
/// "no file" instead of being silently truncated.
int fileIdFromJson(const QJsonObject& file)
{
    const qint64 id = jsonInt64(file.value(QStringLiteral("id")));
    if (id <= 0 || id > std::numeric_limits<int>::max())
        return 0;
    return static_cast<int>(id);
}

bool hasPremiumAnimation(const QJsonObject& sticker)
{
    return sticker.value(QStringLiteral("premium_animation")).isObject();
}

} // namespace

StickerInfo stickerFromMessage(const QJsonObject& message)
{
    StickerInfo info;

    const QJsonValue contentValue = message.value(QStringLiteral("content"));
    if (!contentValue.isObject())
        return info;

    const QJsonObject content = contentValue.toObject();
    if (content.value(QStringLiteral("@type")).toString()
        != QLatin1String("messageSticker")) {
        return info;
    }

    const QJsonObject sticker = content.value(QStringLiteral("sticker")).toObject();
    if (sticker.isEmpty())
        return info;

    info.stickerId = jsonInt64(sticker.value(QStringLiteral("id")));
    info.setId = jsonInt64(sticker.value(QStringLiteral("set_id")));
    info.emoji = sticker.value(QStringLiteral("emoji")).toString();
    info.width = sticker.value(QStringLiteral("width")).toInt();
    info.height = sticker.value(QStringLiteral("height")).toInt();

    info.fullFileId = fileIdFromJson(sticker.value(QStringLiteral("sticker")).toObject());
    const QJsonObject thumbnail =
        sticker.value(QStringLiteral("thumbnail")).toObject();
    info.thumbnailFileId =
        fileIdFromJson(thumbnail.value(QStringLiteral("file")).toObject());
    if (info.thumbnailFileId == 0)
        info.thumbnailFileId = info.fullFileId;

    // Older TDLib releases use stickerFormatAnimated/Video/Static; newer ones
    // use stickerFormatTgs/Webm/Webp. Accept both spellings.
    const QString format = sticker.value(QStringLiteral("format"))
                               .toObject()
                               .value(QStringLiteral("@type"))
                               .toString();
    info.isAnimated = format == QLatin1String("stickerFormatAnimated")
        || format == QLatin1String("stickerFormatTgs");
    info.isVideo = format == QLatin1String("stickerFormatVideo")
        || format == QLatin1String("stickerFormatWebm");
    if (format.isEmpty() && sticker.value(QStringLiteral("is_animated")).toBool())
        info.isAnimated = true;

    // sticker.type exists in older releases, sticker.full_type in newer ones.
    QString stickerType = sticker.value(QStringLiteral("type"))
                              .toObject()
                              .value(QStringLiteral("@type"))
                              .toString();
    if (stickerType.isEmpty()) {
        stickerType = sticker.value(QStringLiteral("full_type"))
                          .toObject()
                          .value(QStringLiteral("@type"))
                          .toString();
    }
    info.isMask = stickerType == QLatin1String("stickerTypeMask")
        || stickerType == QLatin1String("stickerFullTypeMask");
    if (stickerType.isEmpty() && sticker.value(QStringLiteral("is_mask")).toBool())
        info.isMask = true;

    info.isPremium = hasPremiumAnimation(sticker)
        || content.value(QStringLiteral("is_premium")).toBool();

    return info;
}

QJsonObject getStickerSetRequest(qint64 setId)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getStickerSet")},
        {QStringLiteral("set_id"), setId},
    };
}

} // namespace beygram::telegram
