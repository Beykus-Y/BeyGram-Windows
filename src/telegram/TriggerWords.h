#pragma once

#include <QString>
#include <QVector>

namespace beygram::telegram {

/// One trigger word: `text` typed in a message is replaced by `emoji` before
/// the message is sent. `caseSensitive` selects whether the case of `text` has
/// to match exactly; the default ignores case.
struct TriggerWord {
    QString text;
    QString emoji;
    bool caseSensitive = false;

    bool operator==(const TriggerWord&) const = default;
};

/// Serialises trigger words as one compact line per word:
///
///     text|emoji|0
///     text|emoji|1
///
/// joined by '\n', where the last field is 1 for a case-sensitive word. Text is
/// trimmed; a word with an empty text is dropped, and a later word that repeats
/// an earlier text exactly (after trimming) is discarded, so the first word for
/// a text survives. A word whose text or emoji contains '|', '\n' or '\r'
/// cannot be represented by this format and is omitted. An empty list encodes
/// to an empty string.
[[nodiscard]] QString encodeTriggerWords(const QVector<TriggerWord>& words);

/// Parses the output of encodeTriggerWords. Lines that do not have exactly
/// three fields, have an empty text or carry a flag other than "0"/"1" are
/// ignored; CRLF line ends are accepted. Decoding does not deduplicate.
[[nodiscard]] QVector<TriggerWord> decodeTriggerWords(const QString& encoded);

/// Replaces trigger words in `text` in one left-to-right pass. At every
/// position the longest matching word wins. The substituted emoji is never
/// scanned again, so replacements cannot cascade into one another.
///
/// A match must not touch an ASCII letter or digit on the side where the word
/// itself starts or ends with one: "cat" matches "cat!", "cat_1" and "caté", but
/// not "concatenate", "cat1" or "1cat". Case is ignored unless the word is
/// case-sensitive. Text is trimmed and deduplicated like encodeTriggerWords;
/// empty text and an empty word list return `text` unchanged.
[[nodiscard]] QString applyTriggerWords(const QString& text,
                                        const QVector<TriggerWord>& words);

} // namespace beygram::telegram
