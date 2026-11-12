#include "ContactImportParser.h"

#include <QStringList>

namespace beygram::telegram {
namespace {

constexpr qsizetype kMinPhoneDigits = 5;
constexpr qsizetype kMaxPhoneDigits = 15;

/// Splits the input into logical lines, joining RFC 6350 folded lines: a line
/// that starts with a space or tab continues the previous one and the leading
/// whitespace character itself is dropped.
QStringList unfoldedLines(const QString& text)
{
    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    const QChar byteOrderMark(QChar::ByteOrderMark);
    if (normalized.startsWith(byteOrderMark))
        normalized.remove(0, 1);

    const QStringList rawLines = normalized.split(QLatin1Char('\n'));
    QStringList lines;
    lines.reserve(rawLines.size());

    for (const QString& rawLine : rawLines) {
        const bool isContinuation =
            !rawLine.isEmpty()
            && (rawLine.at(0) == QLatin1Char(' ') || rawLine.at(0) == QLatin1Char('\t'))
            && !lines.isEmpty();
        if (isContinuation)
            lines.last().append(rawLine.mid(1));
        else
            lines.append(rawLine);
    }

    return lines;
}

/// Returns the index of the ':' that separates a property header from its
/// value. A colon inside a quoted parameter value does not count.
qsizetype valueSeparator(const QString& line)
{
    bool quoted = false;
    for (qsizetype index = 0; index < line.size(); ++index) {
        const QChar character = line.at(index);
        if (character == QLatin1Char('\\') && quoted && index + 1 < line.size()) {
            ++index;
            continue;
        }
        if (character == QLatin1Char('"')) {
            quoted = !quoted;
        } else if (character == QLatin1Char(':') && !quoted) {
            return index;
        }
    }
    return -1;
}

/// Extracts the upper-case property name from a header such as
/// `item1.TEL;TYPE=CELL`, dropping the group prefix and any parameters.
QString propertyName(const QString& header)
{
    QString name = header;
    const qsizetype semicolon = name.indexOf(QLatin1Char(';'));
    if (semicolon >= 0)
        name.truncate(semicolon);

    name = name.trimmed();
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot >= 0)
        name = name.mid(dot + 1);

    return name.trimmed().toUpper();
}

/// Decodes the RFC 6350 text escapes used in FN, N and EMAIL values.
QString unescapedText(const QString& value)
{
    QString text;
    text.reserve(value.size());

    for (qsizetype index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);
        if (character != QLatin1Char('\\') || index + 1 >= value.size()) {
            text.append(character);
            continue;
        }

        const QChar escaped = value.at(++index);
        switch (escaped.unicode()) {
        case u'n':
        case u'N':
            text.append(QLatin1Char('\n'));
            break;
        case u',':
            text.append(QLatin1Char(','));
            break;
        case u';':
            text.append(QLatin1Char(';'));
            break;
        case u'\\':
            text.append(QLatin1Char('\\'));
            break;
        default:
            text.append(escaped);
            break;
        }
    }

    return text;
}

/// Splits a structured value on semicolons that are not escaped, so
/// `Smith\;Jones;Ada` keeps the semicolon inside the family name.
QStringList splitStructuredValue(const QString& value)
{
    QStringList parts;
    QString current;

    for (qsizetype index = 0; index < value.size(); ++index) {
        const QChar character = value.at(index);
        if (character == QLatin1Char('\\') && index + 1 < value.size()) {
            current.append(character);
            current.append(value.at(++index));
        } else if (character == QLatin1Char(';')) {
            parts.append(current);
            current.clear();
        } else {
            current.append(character);
        }
    }

    parts.append(current);
    return parts;
}

QString valueWithoutScheme(QString value, const QString& scheme)
{
    value = value.trimmed();
    if (value.startsWith(scheme, Qt::CaseInsensitive))
        value = value.mid(scheme.size()).trimmed();
    return value;
}

struct Card {
    QString formattedName;
    QString structuredName;
    QString telephone;
    QString email;
};

void appendCard(const Card& card, ContactImportResult* result)
{
    const QString phone = normalizePhoneForImport(card.telephone);
    if (phone.isEmpty()) {
        ++result->skipped;
        return;
    }

    ImportedContact contact;
    contact.phone = phone;
    contact.email = unescapedText(card.email).trimmed();

    // FN is the display name as the card's author wrote it. Splitting it would
    // guess where the given name ends, so it stays whole in firstName.
    const QString formattedName = unescapedText(card.formattedName).trimmed();
    if (!formattedName.isEmpty()) {
        contact.firstName = formattedName;
    } else if (!card.structuredName.trimmed().isEmpty()) {
        // N is family;given;additional;prefix;suffix.
        const QStringList parts = splitStructuredValue(card.structuredName);
        if (!parts.isEmpty())
            contact.lastName = unescapedText(parts.at(0)).trimmed();
        if (parts.size() > 1)
            contact.firstName = unescapedText(parts.at(1)).trimmed();
    }

    result->contacts.append(contact);
}

} // namespace

QString normalizePhoneForImport(const QString& phone)
{
    QString value = phone.trimmed();
    if (value.startsWith(QStringLiteral("tel:"), Qt::CaseInsensitive))
        value = value.mid(4).trimmed();

    const bool hasInternationalZeroPrefix = value.startsWith(QStringLiteral("00"));

    QString digits;
    digits.reserve(value.size());
    for (const QChar character : value) {
        if (character >= QLatin1Char('0') && character <= QLatin1Char('9'))
            digits.append(character);
    }

    if (hasInternationalZeroPrefix && digits.startsWith(QStringLiteral("00")))
        digits.remove(0, 2);

    if (digits.size() < kMinPhoneDigits || digits.size() > kMaxPhoneDigits)
        return {};

    return QStringLiteral("+") + digits;
}

ContactImportResult parseVCards(const QString& text)
{
    ContactImportResult result;
    Card card;
    bool insideCard = false;

    const QStringList lines = unfoldedLines(text);
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();

        if (line.compare(QStringLiteral("BEGIN:VCARD"), Qt::CaseInsensitive) == 0) {
            // A new BEGIN before END leaves the previous block unterminated.
            if (insideCard)
                ++result.skipped;
            card = Card{};
            insideCard = true;
            continue;
        }

        if (line.compare(QStringLiteral("END:VCARD"), Qt::CaseInsensitive) == 0) {
            if (!insideCard)
                continue;
            appendCard(card, &result);
            card = Card{};
            insideCard = false;
            continue;
        }

        if (!insideCard || line.isEmpty())
            continue;

        const qsizetype separator = valueSeparator(line);
        if (separator < 0)
            continue;

        const QString name = propertyName(line.left(separator));
        const QString value = line.mid(separator + 1).trimmed();

        if (name == QLatin1String("FN")) {
            if (card.formattedName.isEmpty())
                card.formattedName = value;
        } else if (name == QLatin1String("N")) {
            if (card.structuredName.isEmpty())
                card.structuredName = value;
        } else if (name == QLatin1String("TEL")) {
            if (card.telephone.isEmpty())
                card.telephone = valueWithoutScheme(value, QStringLiteral("tel:"));
        } else if (name == QLatin1String("EMAIL")) {
            if (card.email.isEmpty())
                card.email = valueWithoutScheme(value, QStringLiteral("mailto:"));
        }
    }

    if (insideCard)
        ++result.skipped;

    return result;
}

} // namespace beygram::telegram
