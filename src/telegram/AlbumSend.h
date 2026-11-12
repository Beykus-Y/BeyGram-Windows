#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace beygram::telegram {

/// One member of an outgoing media album. A photo is sent as inputMessagePhoto,
/// a video as inputMessageVideo; both point at a local file. width/height are
/// hints for the recipient's preview and may be left at 0, in which case TDLib
/// fills them in from the uploaded file.
struct AlbumItem {
    QString path;
    QString caption;
    int width = 0;
    int height = 0;
    bool video = false;
};

/// Builds a TDLib sendMessageAlbum request. One input_message_contents entry is
/// produced per item, in the same order, and only the first item's caption is
/// used; every other content carries an empty formattedText so the album still
/// has the schema-required caption object. The builder refuses an empty album,
/// more than ten items (the schema limit) and items with a blank path by
/// returning an empty QJsonObject. A reply target is added only for a positive
/// message id. The builder performs no file access, no logging and adds no
/// @extra; the transport adds correlation.
[[nodiscard]] QJsonObject sendAlbumRequest(qint64 chatId, const QVector<AlbumItem>& items,
                                          qint64 replyToMessageId = 0);

} // namespace beygram::telegram
