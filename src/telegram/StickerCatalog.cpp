#include "StickerCatalog.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

#include <limits>

namespace beygram::telegram {
namespace {

// The schema caps getArchivedStickerSets and getTrendingStickerSets at 100
// entries; TDLib may return fewer trending sets than asked for.
constexpr int kStickerSetsLimit = 100;

QJsonObject stickerTypeRegular()
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("stickerTypeRegular")},
    };
}

// File ids are TDLib int32 values; an id outside that range means "no file"
// rather than a truncated id that would download the wrong file.
int fileIdFromFile(const QJsonObject& file)
{
    const qint64 id = jsonInt64(file.value(QStringLiteral("id")));
    if (id <= 0 || id > std::numeric_limits<int>::max())
        return 0;
    return static_cast<int>(id);
}

int thumbnailFileId(const QJsonObject& object)
{
    return fileIdFromFile(
        object.value(QStringLiteral("thumbnail")).toObject().value(QStringLiteral("file")).toObject());
}

// stickerSetInfo.thumbnail is the intended preview. Older/partial responses may
// leave it empty while covers still carry a usable image, so the covers are
// used as a fallback instead of showing a blank cell.
int stickerSetThumbnailFileId(const QJsonObject& set)
{
    const int thumbnail = thumbnailFileId(set);
    if (thumbnail != 0)
        return thumbnail;

    const QJsonArray covers = set.value(QStringLiteral("covers")).toArray();
    for (const QJsonValue& coverValue : covers) {
        if (!coverValue.isObject())
            continue;
        const QJsonObject cover = coverValue.toObject();
        const int coverThumbnail = thumbnailFileId(cover);
        if (coverThumbnail != 0)
            return coverThumbnail;
        const int coverFile = fileIdFromFile(
            cover.value(QStringLiteral("sticker")).toObject());
        if (coverFile != 0)
            return coverFile;
    }

    return 0;
}

int stickerCountFromJson(const QJsonObject& set)
{
    // The pinned schema names the count size; sticker_count is accepted for
    // releases that spell the same field out.
    const QJsonValue count = set.contains(QStringLiteral("sticker_count"))
        ? set.value(QStringLiteral("sticker_count"))
        : set.value(QStringLiteral("size"));
    const qint64 value = jsonInt64(count);
    if (value <= 0)
        return 0;
    if (value > std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    return static_cast<int>(value);
}

// A set without a positive id is not addressable, so it is dropped by the
// caller. Everything else is optional metadata.
StickerSetInfo stickerSetFromJson(const QJsonObject& set)
{
    StickerSetInfo info;
    info.id = jsonInt64(set.value(QStringLiteral("id")));
    info.title = set.value(QStringLiteral("title")).toString();
    info.name = set.value(QStringLiteral("name")).toString();
    info.stickerCount = stickerCountFromJson(set);
    info.thumbnailFileId = stickerSetThumbnailFileId(set);
    info.installed = set.value(QStringLiteral("is_installed")).toBool();
    info.archived = set.value(QStringLiteral("is_archived")).toBool();
    return info;
}

QVector<StickerSetInfo> stickerSetsFromJson(const QJsonValue& value)
{
    QVector<StickerSetInfo> sets;
    if (!value.isArray())
        return sets;

    const QJsonArray entries = value.toArray();
    sets.reserve(entries.size());
    for (const QJsonValue& entryValue : entries) {
        if (!entryValue.isObject())
            continue;
        const StickerSetInfo info = stickerSetFromJson(entryValue.toObject());
        if (info.id <= 0)
            continue;
        sets.append(info);
    }
    return sets;
}

// getRecentStickers answers with a Stickers object; only the ids are kept
// because the file metadata is read per sticker when it is shown.
QVector<qint64> recentStickerIdsFromJson(const QJsonObject& response)
{
    QVector<qint64> ids;
    const QJsonArray entries = response.value(QStringLiteral("stickers")).toArray();
    ids.reserve(entries.size());
    for (const QJsonValue& entryValue : entries) {
        if (!entryValue.isObject())
            continue;
        const qint64 id = jsonInt64(entryValue.toObject().value(QStringLiteral("id")));
        if (id <= 0 || ids.contains(id))
            continue;
        ids.append(id);
    }
    return ids;
}

} // namespace

StickerCatalogState stickerCatalogFromResponse(const QJsonObject& response)
{
    StickerCatalogState state;

    // Strict dispatch on @type: a stickerSets answer is never mistaken for
    // recent stickers, and an error or unrelated object leaves the state empty.
    const QString type = response.value(QStringLiteral("@type")).toString();
    if (type == QLatin1String("stickerSets")) {
        state.installedSets = stickerSetsFromJson(response.value(QStringLiteral("sets")));
    } else if (type == QLatin1String("trendingStickerSets")) {
        state.featuredSets = stickerSetsFromJson(response.value(QStringLiteral("sets")));
    } else if (type == QLatin1String("stickers")) {
        state.recentStickerIds = recentStickerIdsFromJson(response);
    }

    return state;
}

QJsonObject getInstalledStickerSetsRequest(bool archived)
{
    if (archived) {
        // This schema has no is_archived on getInstalledStickerSets. Archived
        // sets are listed by getArchivedStickerSets, which pages from an id and
        // takes a limit.
        return QJsonObject{
            {QStringLiteral("@type"), QStringLiteral("getArchivedStickerSets")},
            {QStringLiteral("sticker_type"), stickerTypeRegular()},
            {QStringLiteral("offset_sticker_set_id"), qint64(0)},
            {QStringLiteral("limit"), kStickerSetsLimit},
        };
    }

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getInstalledStickerSets")},
        {QStringLiteral("sticker_type"), stickerTypeRegular()},
    };
}

QJsonObject getFeaturedStickerSetsRequest()
{
    // getFeaturedStickerSets no longer exists in the pinned schema; featured
    // sets are trending sets now.
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getTrendingStickerSets")},
        {QStringLiteral("sticker_type"), stickerTypeRegular()},
        {QStringLiteral("offset"), 0},
        {QStringLiteral("limit"), kStickerSetsLimit},
    };
}

QJsonObject changeStickerSetRequest(qint64 setId, bool installed, bool archived)
{
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("changeStickerSet")},
        {QStringLiteral("set_id"), setId},
        {QStringLiteral("is_installed"), installed},
        {QStringLiteral("is_archived"), archived},
    };
}

} // namespace beygram::telegram
