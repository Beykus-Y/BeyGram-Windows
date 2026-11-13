#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

// Converts TDLib message text and its formatting entities into an HTML
// fragment suitable for Qt Quick Text with textFormat: Text.RichText.
//
// TDLib reports entity offsets and lengths in UTF-16 code units, so they match
// QString::size()/mid() directly. Text is always escaped before markup is
// applied, entity ranges are sorted, and malformed or overlapping ranges are
// ignored. Link targets are restricted to the http, https, tg and mailto
// schemes, and every emitted anchor carries rel="noopener".

namespace beygram::telegram {

// Escapes &, <, >, " and ' so the result can be embedded in HTML text or an
// attribute value. Newlines are left untouched.
[[nodiscard]] QString escapeHtml(const QString& text);

// Renders plain text plus a TDLib "entities" array (textEntity objects) into
// escaped HTML. Unknown entity types keep their inner text.
[[nodiscard]] QString renderEntities(const QString& text, const QJsonArray& entities);

// Renders a TDLib "formattedText" object: {"text": "...", "entities": [...]}.
[[nodiscard]] QString renderMessageText(const QJsonObject& textObject);

// Returns the unformatted "text" field of a TDLib "formattedText" object.
[[nodiscard]] QString plainText(const QJsonObject& textObject);

} // namespace beygram::telegram
