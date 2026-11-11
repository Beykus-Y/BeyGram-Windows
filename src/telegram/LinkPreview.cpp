#include "LinkPreview.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

#include <limits>

namespace beygram::telegram {
namespace {

/// Preview cards are downloaded together with the bubble, so a small image is
/// enough; this is the long edge a thumbnail should reach when one exists.
constexpr int kMinThumbnailEdge = 240;

/// File ids are TDLib int32 values and may arrive as JSON numbers or strings;
/// anything outside that range means "no file" instead of a truncated id.
int fileIdFromJson(const QJsonValue& value)
{
    const qint64 id = jsonInt64(value);
    if (id <= 0 || id > std::numeric_limits<int>::max())
        return 0;
    return static_cast<int>(id);
}

int fileIdFromFile(const QJsonObject& file)
{
    return fileIdFromJson(file.value(QStringLiteral("id")));
}

/// The smallest size at least kMinThumbnailEdge on its long edge; when every
/// size is smaller, the smallest one wins. The largest size is never picked.
int smallestSuitableSizeFileId(const QJsonObject& photo)
{
    const QJsonArray sizes = photo.value(QStringLiteral("sizes")).toArray();
    int suitableId = 0;
    qint64 suitableArea = 0;
    int fallbackId = 0;
    qint64 fallbackArea = 0;

    for (const QJsonValue& value : sizes) {
        const QJsonObject size = value.toObject();
        const int fileId = fileIdFromFile(size.value(QStringLiteral("photo")).toObject());
        if (fileId == 0)
            continue;

        const qint64 width = size.value(QStringLiteral("width")).toInt();
        const qint64 height = size.value(QStringLiteral("height")).toInt();
        const qint64 area = width * height;
        if (qMax(width, height) >= kMinThumbnailEdge) {
            if (suitableId == 0 || area < suitableArea) {
                suitableId = fileId;
                suitableArea = area;
            }
        } else if (fallbackId == 0 || area < fallbackArea) {
            fallbackId = fileId;
            fallbackArea = area;
        }
    }

    return suitableId != 0 ? suitableId : fallbackId;
}

/// Thumbnails of video, animation, document and video note media.
int thumbnailFileId(const QJsonObject& media)
{
    return fileIdFromFile(media.value(QStringLiteral("thumbnail"))
                              .toObject()
                              .value(QStringLiteral("file"))
                              .toObject());
}

int albumCoverFileId(const QJsonObject& audio)
{
    return fileIdFromFile(audio.value(QStringLiteral("album_cover_thumbnail"))
                              .toObject()
                              .value(QStringLiteral("file"))
                              .toObject());
}

/// Photo-bearing preview types expose a TDLib photo with several sizes; media
/// types expose a single thumbnail file. Both shapes are covered here.
int previewThumbnailFileId(const QJsonObject& preview)
{
    const QJsonObject type = preview.value(QStringLiteral("type")).toObject();
    const QString typeName = type.value(QStringLiteral("@type")).toString();

    int fileId = smallestSuitableSizeFileId(type.value(QStringLiteral("photo")).toObject());

    if (fileId == 0 && typeName == QLatin1String("linkPreviewTypeVideo")) {
        fileId = smallestSuitableSizeFileId(type.value(QStringLiteral("cover")).toObject());
        if (fileId == 0)
            fileId = thumbnailFileId(type.value(QStringLiteral("video")).toObject());
    }
    if (fileId == 0 && typeName == QLatin1String("linkPreviewTypeVideoNote"))
        fileId = thumbnailFileId(type.value(QStringLiteral("video_note")).toObject());
    if (fileId == 0 && typeName == QLatin1String("linkPreviewTypeAnimation"))
        fileId = thumbnailFileId(type.value(QStringLiteral("animation")).toObject());
    if (fileId == 0 && typeName == QLatin1String("linkPreviewTypeAudio"))
        fileId = albumCoverFileId(type.value(QStringLiteral("audio")).toObject());
    if (fileId == 0 && typeName == QLatin1String("linkPreviewTypeDocument"))
        fileId = thumbnailFileId(type.value(QStringLiteral("document")).toObject());

    // Embedded players carry a photo thumbnail next to the embedded media.
    if (fileId == 0)
        fileId = smallestSuitableSizeFileId(type.value(QStringLiteral("thumbnail")).toObject());
    if (fileId == 0)
        fileId = thumbnailFileId(type.value(QStringLiteral("video")).toObject());
    if (fileId == 0)
        fileId = thumbnailFileId(type.value(QStringLiteral("animation")).toObject());
    if (fileId == 0)
        fileId = thumbnailFileId(type.value(QStringLiteral("video_note")).toObject());
    if (fileId == 0)
        fileId = thumbnailFileId(type.value(QStringLiteral("document")).toObject());
    if (fileId == 0)
        fileId = smallestSuitableSizeFileId(type.value(QStringLiteral("cover")).toObject());
    if (fileId == 0)
        fileId = smallestSuitableSizeFileId(type.value(QStringLiteral("photo_icon")).toObject());

    // Older TDLib releases kept the preview photo on linkPreview itself.
    if (fileId == 0)
        fileId = smallestSuitableSizeFileId(preview.value(QStringLiteral("photo")).toObject());

    return fileId;
}

/// description is a formattedText object in current TDLib and a plain string
/// in older releases.
QString textFromFormatted(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    if (value.isObject())
        return value.toObject().value(QStringLiteral("text")).toString();
    return {};
}

} // namespace

LinkPreview linkPreviewFromMessage(const QJsonObject& message)
{
    LinkPreview preview;

    const QJsonObject content = message.value(QStringLiteral("content")).toObject();
    const QJsonObject linkPreview = content.value(QStringLiteral("link_preview")).toObject();
    const QJsonObject options = content.value(QStringLiteral("link_preview_options")).toObject();

    // The canonical URL wins; link_preview_options only carries a URL when the
    // sender picked one explicitly, and display_url may be a shortened form.
    preview.url = linkPreview.value(QStringLiteral("url")).toString();
    if (preview.url.isEmpty())
        preview.url = options.value(QStringLiteral("url")).toString();
    if (preview.url.isEmpty())
        preview.url = linkPreview.value(QStringLiteral("display_url")).toString();

    preview.title = linkPreview.value(QStringLiteral("title")).toString();
    preview.description = textFromFormatted(linkPreview.value(QStringLiteral("description")));
    preview.siteName = linkPreview.value(QStringLiteral("site_name")).toString();

    const QString type = linkPreview.value(QStringLiteral("type"))
                             .toObject()
                             .value(QStringLiteral("@type"))
                             .toString();
    preview.isVideo = type == QLatin1String("linkPreviewTypeVideo")
        || type == QLatin1String("linkPreviewTypeVideoNote");
    preview.thumbnailFileId = previewThumbnailFileId(linkPreview);

    // Without a URL there is nothing to open, and without a linkPreview object
    // there is nothing to show, so no card is offered in either case.
    preview.hasPreview = !preview.url.isEmpty() && !linkPreview.isEmpty();
    return preview;
}

} // namespace beygram::telegram
