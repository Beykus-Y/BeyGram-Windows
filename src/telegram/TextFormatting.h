#pragma once

#include <QJsonObject>
#include <QString>

// Renders a TDLib "formattedText" object into plain text plus a compact HTML
// fragment suitable for Qt Quick Text with textFormat: Text.RichText.
//
// TDLib reports entity offsets and lengths in UTF-16 code units, so they line
// up 1:1 with QString::size()/mid() and need no byte-to-character conversion.
// The message text is always HTML-escaped before entities are applied, entity
// boundaries never split a surrogate pair, and link targets are restricted to
// the http, https, tg and mailto schemes, so hostile message content can never
// inject markup, attributes or script.

namespace beygram::telegram {

struct FormattedText {
    QString plainText;
    QString html;
};

[[nodiscard]] FormattedText formatTdlibText(const QJsonObject& formattedText);

} // namespace beygram::telegram
