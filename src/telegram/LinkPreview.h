#pragma once

#include <QJsonObject>
#include <QString>

namespace beygram::telegram {

/// Link preview metadata extracted from a TDLib message, independent of the UI.
///
/// A message without a link preview, or without any URL to open, keeps
/// hasPreview == false. Missing fields stay empty on purpose: nothing here is
/// ever filled with placeholder text, so callers can decide what to show.
struct LinkPreview {
    QString url;
    QString title;
    QString description;
    QString siteName;
    int thumbnailFileId = 0;
    bool hasPreview = false;
    bool isVideo = false;
};

/// Parses message["content"]["link_preview"] and link_preview_options from a
/// TDLib message. Only the preview of messageText content is meaningful, but a
/// missing or malformed preview yields a default-constructed LinkPreview.
[[nodiscard]] LinkPreview linkPreviewFromMessage(const QJsonObject& message);

} // namespace beygram::telegram
