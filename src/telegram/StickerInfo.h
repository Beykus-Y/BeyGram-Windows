#pragma once

#include <QJsonObject>
#include <QString>

namespace beygram::telegram {

/// Sticker metadata extracted from a TDLib message, independent of the UI.
/// A sticker without a preview thumbnail keeps thumbnailFileId == fullFileId
/// so callers always have something to download.
struct StickerInfo {
    qint64 stickerId = 0;
    qint64 setId = 0;
    QString emoji;
    int width = 0;
    int height = 0;
    int thumbnailFileId = 0;
    int fullFileId = 0;
    bool isAnimated = false;
    bool isVideo = false;
    bool isMask = false;
    bool isPremium = false;
};

/// Parses message["content"] when it is a messageSticker. Any other content
/// (or a malformed sticker) yields a default-constructed StickerInfo.
[[nodiscard]] StickerInfo stickerFromMessage(const QJsonObject& message);

/// Builds the TDLib getStickerSet request for a set id.
[[nodiscard]] QJsonObject getStickerSetRequest(qint64 setId);

} // namespace beygram::telegram
