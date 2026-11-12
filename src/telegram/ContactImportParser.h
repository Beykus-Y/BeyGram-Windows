#pragma once

#include <QString>
#include <QVector>

namespace beygram::telegram {

/// A contact read from a vCard, ready for `importContacts`.
struct ImportedContact {
    QString firstName;
    QString lastName;
    QString phone; // normalized: a leading '+' followed by 5..15 digits
    QString email;
};

/// The outcome of parsing a vCard stream.
struct ContactImportResult {
    QVector<ImportedContact> contacts;
    /// Complete blocks that carried no usable phone number, plus blocks left
    /// unterminated at the end of the input.
    int skipped = 0;
};

/// Normalizes a phone number for import: keeps ASCII digits, converts an
/// international "00" prefix to '+', and returns '+' followed by the digits.
///
/// Returns an empty string unless the result carries 5..15 digits, so callers
/// can use the empty result as "not importable". A `tel:` URI is accepted as
/// well, because vCard 4 stores TEL values that way.
[[nodiscard]] QString normalizePhoneForImport(const QString& phone);

/// Parses every BEGIN:VCARD..END:VCARD block in `text`.
///
/// The parser is pure: no I/O, no session state and no logging.
///
/// Supported fields:
///   - FN for a formatted name; it is kept whole in `firstName` because a
///     display name does not say where the given name ends.
///   - N (family;given;additional;prefix;suffix) when FN is absent or blank;
///     family becomes `lastName` and given becomes `firstName`.
///   - the first non-empty TEL and EMAIL values; a `tel:` or `mailto:` URI
///     scheme is removed.
///
/// Folded lines (a continuation starting with a space or tab) are joined
/// before parsing, text escapes such as `\\;` and `\\n` are decoded, and the
/// property, BEGIN and END markers are matched case-insensitively.
///
/// A block is skipped when its phone number does not normalize to 5..15
/// digits. Text outside vCard blocks is ignored, and a BEGIN:VCARD without a
/// matching END:VCARD counts as one skipped block.
[[nodiscard]] ContactImportResult parseVCards(const QString& text);

} // namespace beygram::telegram
