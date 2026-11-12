#include "TriggerWords.h"

#include <algorithm>
#include <QSet>
#include <QStringList>
#include <QStringView>

namespace beygram::telegram {
namespace {

bool isAsciiDigit(QChar character)
{
    const char16_t code = character.unicode();
    return code >= u'0' && code <= u'9';
}

bool isAsciiLetter(QChar character)
{
    const char16_t code = character.unicode();
    return (code >= u'a' && code <= u'z') || (code >= u'A' && code <= u'Z');
}

bool isAsciiAlnum(QChar character)
{
    return isAsciiLetter(character) || isAsciiDigit(character);
}

/// The line format has no escaping, so a field containing a separator or a line
/// break cannot be decoded again.
bool isRepresentable(const QString& value)
{
    return !value.contains(QLatin1Char('|')) && !value.contains(QLatin1Char('\n'))
        && !value.contains(QLatin1Char('\r'));
}

/// Trims texts, drops empty ones and keeps the first word per exact text. The
/// same normalisation feeds encoding and matching, so both see one rule set.
QVector<TriggerWord> normalizedWords(const QVector<TriggerWord>& words)
{
    QVector<TriggerWord> kept;
    kept.reserve(words.size());

    QSet<QString> seen;
    for (const TriggerWord& word : words) {
        const QString text = word.text.trimmed();
        if (text.isEmpty() || seen.contains(text))
            continue;

        seen.insert(text);
        kept.append(TriggerWord{text, word.emoji, word.caseSensitive});
    }
    return kept;
}

/// Longest text first, so a short word can never shadow a longer one that
/// begins at the same offset. Stable, so equal lengths keep the caller's order.
bool longerFirst(const TriggerWord& left, const TriggerWord& right)
{
    return left.text.size() > right.text.size();
}

/// A word only constrains the side where it starts or ends with an ASCII letter
/// or digit. Other scripts and punctuation can touch the word freely.
bool matchesAt(QStringView text, qsizetype start, const TriggerWord& word)
{
    const qsizetype length = word.text.size();
    if (length == 0 || length > text.size() - start)
        return false;

    const QStringView candidate = text.mid(start, length);
    const Qt::CaseSensitivity sensitivity =
        word.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    if (candidate.compare(QStringView(word.text), sensitivity) != 0)
        return false;

    if (isAsciiAlnum(word.text.at(0)) && start > 0
        && isAsciiAlnum(text.at(start - 1))) {
        return false;
    }

    const qsizetype end = start + length;
    if (isAsciiAlnum(word.text.at(length - 1)) && end < text.size()
        && isAsciiAlnum(text.at(end))) {
        return false;
    }

    return true;
}

} // namespace

QString encodeTriggerWords(const QVector<TriggerWord>& words)
{
    QStringList lines;

    for (const TriggerWord& word : normalizedWords(words)) {
        if (!isRepresentable(word.text) || !isRepresentable(word.emoji))
            continue;

        lines.append(word.text + QLatin1Char('|') + word.emoji + QLatin1Char('|')
                     + (word.caseSensitive ? QLatin1Char('1') : QLatin1Char('0')));
    }

    return lines.join(QLatin1Char('\n'));
}

QVector<TriggerWord> decodeTriggerWords(const QString& encoded)
{
    QVector<TriggerWord> words;
    if (encoded.isEmpty())
        return words;

    const QStringList lines = encoded.split(QLatin1Char('\n'));
    words.reserve(lines.size());

    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);

        const QStringList fields = line.split(QLatin1Char('|'));
        if (fields.size() != 3)
            continue;

        const QString text = fields.at(0).trimmed();
        if (text.isEmpty())
            continue;

        const QString flag = fields.at(2);
        if (flag != QLatin1String("0") && flag != QLatin1String("1"))
            continue;

        words.append(TriggerWord{text, fields.at(1), flag == QLatin1String("1")});
    }

    return words;
}

QString applyTriggerWords(const QString& text, const QVector<TriggerWord>& words)
{
    if (text.isEmpty() || words.isEmpty())
        return text;

    QVector<TriggerWord> ordered = normalizedWords(words);
    if (ordered.isEmpty())
        return text;

    std::stable_sort(ordered.begin(), ordered.end(), longerFirst);

    const QStringView input(text);
    QString result;
    result.reserve(text.size());

    qsizetype index = 0;
    while (index < input.size()) {
        const TriggerWord* match = nullptr;
        for (const TriggerWord& word : ordered) {
            if (matchesAt(input, index, word)) {
                match = &word;
                break;
            }
        }

        if (match == nullptr) {
            result.append(input.at(index));
            ++index;
            continue;
        }

        // Consuming the whole word is what makes the pass single: the emoji
        // just written is never scanned for further words.
        result.append(match->emoji);
        index += match->text.size();
    }

    return result;
}

} // namespace beygram::telegram
