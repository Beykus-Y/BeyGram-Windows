#pragma once

#include <QJsonObject>
#include <QtGlobal>

namespace beygram::telegram {

/// Builds a TDLib sendMessage request whose input_message_content is an
/// inputMessageSticker. The sticker file is referenced through inputFileId when
/// fileId is positive; without a registered file id the sticker id is passed as
/// an inputFileRemote identifier. The thumbnail stays null and the sticker
/// dimensions and emoji stay empty/zero so TDLib keeps the server-side sticker
/// metadata. A reply target is added only for a positive message id. The builder
/// performs no logging and adds no @extra; the transport adds correlation.
[[nodiscard]] QJsonObject sendStickerRequest(qint64 chatId, qint64 stickerId,
                                             int fileId, qint64 replyToMessageId = 0);

} // namespace beygram::telegram
