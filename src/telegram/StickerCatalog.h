#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One entry of a TDLib StickerSets response, reduced to what a set strip and
/// an install button need.
///
/// thumbnailFileId is the set preview: stickerSetInfo.thumbnail's file, or,
/// when the set has no thumbnail, the first cover sticker's thumbnail and then
/// the cover sticker's own file. Zero means no usable preview exists, so a
/// caller must not ask the file store for it.
struct StickerSetInfo {
    qint64 id = 0;
    QString title;
    QString name;
    int stickerCount = 0;
    int thumbnailFileId = 0;
    bool installed = false;
    bool archived = false;
};

/// The three sticker-catalog lists the picker needs. A single response fills at
/// most one of them; the others stay empty.
struct StickerCatalogState {
    QVector<StickerSetInfo> installedSets;
    QVector<StickerSetInfo> featuredSets;
    QVector<qint64> recentStickerIds;
};

/// Parses one TDLib response into the catalog state. The response @type decides
/// which list is filled:
///
///   stickerSets         -> installedSets (installed and archived sets alike;
///                          each entry keeps its own is_archived flag)
///   trendingStickerSets -> featuredSets
///   stickers            -> recentStickerIds (getRecentStickers)
///
/// Any other object, including an error, yields an empty state. Malformed
/// entries are skipped: a set needs a positive id, a sticker needs a positive
/// id, and repeated recent sticker ids are kept only once. The parser does no
/// logging and never touches the network.
[[nodiscard]] StickerCatalogState stickerCatalogFromResponse(const QJsonObject& response);

/// Builds the request for the account's installed regular sticker sets.
///
/// The pinned schema (TDLib 1.8.69, commit 4c6377aa) has no is_archived field
/// on getInstalledStickerSets; archived sets are a separate function. With
/// archived=true this builder therefore sends getArchivedStickerSets from the
/// beginning of the list (offset_sticker_set_id 0) with the schema maximum of
/// 100 sets. Both variants return StickerSets, so stickerCatalogFromResponse
/// parses either answer the same way.
[[nodiscard]] QJsonObject getInstalledStickerSetsRequest(bool archived = false);

/// Builds the request for the featured (trending) regular sticker sets.
///
/// getFeaturedStickerSets was removed from TDLib when featured sets became
/// trending sets; this schema only has getTrendingStickerSets, which is what
/// the builder sends. The name is kept because "featured" is the picker's
/// wording. The answer is trendingStickerSets, which fills featuredSets.
[[nodiscard]] QJsonObject getFeaturedStickerSetsRequest();

/// Builds changeStickerSet for one set. TDLib requires is_archived on every
/// call, so the field is always sent; installed=false with archived=false is
/// the plain "remove from my sets" case.
[[nodiscard]] QJsonObject changeStickerSetRequest(qint64 setId, bool installed,
                                                  bool archived = false);

} // namespace beygram::telegram
