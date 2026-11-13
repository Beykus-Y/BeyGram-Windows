#include "MessageFormatter.h"

#include <QJsonValue>
#include <QVector>

#include <algorithm>
#include <cmath>

// TextFormatting::formatTdlibText renders TDLib entities, but it only accepts
// a whole formattedText object, converts newlines to <br/>, does not escape
// apostrophes, and emits anchors without rel="noopener". Message rendering
// needs the stricter contract below, so this module keeps its own small
// renderer instead of post-processing that output.

namespace beygram::telegram {
namespace {

struct EntityTags {
    QString openTag;
    QString closeTag;
};

struct EntityRange {
    qsizetype start = 0;
    qsizetype end = 0;
    EntityTags tags;
};

QString urlScheme(const QString& url)
{
    for (qsizetype i = 0; i < url.size(); ++i) {
        const QChar ch = url.at(i);
        if (ch.unicode() == u':')
            return url.left(i).toLower();
        if (ch.unicode() == u'/'
            || ch.unicode() == u'?'
            || ch.unicode() == u'#'
            || ch.isSpace()) {
            return {};
        }
    }
    return {};
}

// Returns an escaped href for an allowed scheme, or an empty string when the
// target must be rendered as plain text.
QString safeHref(const QString& url)
{
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty())
        return {};

    for (const QChar ch : trimmed) {
        if (ch.unicode() < 0x20 || ch.unicode() == 0x7f)
            return {};
    }

    const QString scheme = urlScheme(trimmed);
    if (scheme != QStringLiteral("http")
        && scheme != QStringLiteral("https")
        && scheme != QStringLiteral("tg")
        && scheme != QStringLiteral("mailto")) {
        return {};
    }

    return escapeHtml(trimmed);
}

EntityTags spanTags(const QString& className)
{
    return {QStringLiteral("<span class=\"") + className + QStringLiteral("\">"),
            QStringLiteral("</span>")};
}

EntityTags anchorTags(const QString& url)
{
    const QString href = safeHref(url);
    if (href.isEmpty())
        return {};
    return {QStringLiteral("<a href=\"") + href + QStringLiteral("\" rel=\"noopener\">"),
            QStringLiteral("</a>")};
}

// Accepts the canonical "textEntityType..." spelling as well as the bare
// suffix, so callers may pass either a TDLib type object or a plain string.
QString normalizedEntityKind(const QJsonValue& typeValue)
{
    QString kind;
    if (typeValue.isString()) {
        kind = typeValue.toString();
    } else {
        const QJsonObject type = typeValue.toObject();
        kind = type.value(QStringLiteral("@type")).toString();
        if (kind.isEmpty())
            kind = type.value(QStringLiteral("type")).toString();
    }

    const QString prefix = QStringLiteral("textEntityType");
    if (kind.startsWith(prefix, Qt::CaseInsensitive))
        kind = kind.mid(prefix.size());
    return kind;
}

bool isEntityKind(const QString& kind, const char* name)
{
    return kind.compare(QLatin1String(name), Qt::CaseInsensitive) == 0;
}

EntityTags tagsForEntity(const QJsonValue& typeValue, const QJsonObject& entity,
                         const QString& entityText)
{
    const QString kind = normalizedEntityKind(typeValue);
    const QJsonObject type = typeValue.toObject();

    if (isEntityKind(kind, "bold"))
        return {QStringLiteral("<b>"), QStringLiteral("</b>")};
    if (isEntityKind(kind, "italic"))
        return {QStringLiteral("<i>"), QStringLiteral("</i>")};
    if (isEntityKind(kind, "underline"))
        return {QStringLiteral("<u>"), QStringLiteral("</u>")};
    if (isEntityKind(kind, "strikethrough"))
        return {QStringLiteral("<s>"), QStringLiteral("</s>")};
    if (isEntityKind(kind, "spoiler"))
        return spanTags(QStringLiteral("spoiler"));
    if (isEntityKind(kind, "code"))
        return {QStringLiteral("<code>"), QStringLiteral("</code>")};
    if (isEntityKind(kind, "pre") || isEntityKind(kind, "preCode"))
        return {QStringLiteral("<pre>"), QStringLiteral("</pre>")};
    if (isEntityKind(kind, "blockquote")
        || isEntityKind(kind, "expandableBlockquote")) {
        return {QStringLiteral("<blockquote>"), QStringLiteral("</blockquote>")};
    }
    if (isEntityKind(kind, "textUrl")) {
        QString url = type.value(QStringLiteral("url")).toString();
        if (url.isEmpty())
            url = entity.value(QStringLiteral("url")).toString();
        if (url.isEmpty())
            url = entityText;
        return anchorTags(url);
    }
    if (isEntityKind(kind, "url"))
        return anchorTags(entityText);
    if (isEntityKind(kind, "link")) {
        QString url = type.value(QStringLiteral("url")).toString();
        if (url.isEmpty())
            url = entity.value(QStringLiteral("url")).toString();
        return anchorTags(url.isEmpty() ? entityText : url);
    }
    if (isEntityKind(kind, "mention")
        || isEntityKind(kind, "textMention")
        || isEntityKind(kind, "mentionName")) {
        return spanTags(QStringLiteral("mention"));
    }
    if (isEntityKind(kind, "hashtag"))
        return spanTags(QStringLiteral("hashtag"));
    if (isEntityKind(kind, "cashtag"))
        return spanTags(QStringLiteral("cashtag"));
    if (isEntityKind(kind, "botCommand"))
        return spanTags(QStringLiteral("bot-command"));
    if (isEntityKind(kind, "emailAddress"))
        return spanTags(QStringLiteral("email"));
    if (isEntityKind(kind, "phoneNumber"))
        return spanTags(QStringLiteral("phone"));

    // Unknown types and custom emoji keep their literal text.
    return {};
}

bool parseEntity(const QJsonObject& entity, const QString& text, EntityRange& range)
{
    const QJsonValue offsetValue = entity.value(QStringLiteral("offset"));
    const QJsonValue lengthValue = entity.value(QStringLiteral("length"));
    if (!offsetValue.isDouble() || !lengthValue.isDouble())
        return false;

    const double offsetNumber = offsetValue.toDouble();
    const double lengthNumber = lengthValue.toDouble();
    if (!std::isfinite(offsetNumber) || !std::isfinite(lengthNumber))
        return false;
    if (std::floor(offsetNumber) != offsetNumber
        || std::floor(lengthNumber) != lengthNumber) {
        return false;
    }
    if (offsetNumber < 0 || lengthNumber <= 0)
        return false;

    const double textLength = double(text.size());
    if (offsetNumber >= textLength)
        return false;
    if (lengthNumber > textLength - offsetNumber)
        return false;

    const qsizetype start = qsizetype(offsetNumber);
    const qsizetype end = start + qsizetype(lengthNumber);
    if (end <= start)
        return false;

    // A range that starts or ends inside a surrogate pair is malformed.
    if (start > 0 && text.at(start - 1).isHighSurrogate()
        && text.at(start).isLowSurrogate()) {
        return false;
    }
    if (end < text.size() && text.at(end - 1).isHighSurrogate()
        && text.at(end).isLowSurrogate()) {
        return false;
    }

    const QJsonValue typeValue = entity.value(QStringLiteral("type"));
    const EntityTags tags = tagsForEntity(typeValue, entity, text.mid(start, end - start));
    if (tags.openTag.isEmpty() || tags.closeTag.isEmpty())
        return false;

    range.start = start;
    range.end = end;
    range.tags = tags;
    return true;
}

} // namespace

QString escapeHtml(const QString& text)
{
    QString escaped;
    escaped.reserve(text.size() + text.size() / 8);

    for (const QChar ch : text) {
        switch (ch.unicode()) {
        case u'&':
            escaped += QStringLiteral("&amp;");
            break;
        case u'<':
            escaped += QStringLiteral("&lt;");
            break;
        case u'>':
            escaped += QStringLiteral("&gt;");
            break;
        case u'"':
            escaped += QStringLiteral("&quot;");
            break;
        case u'\'':
            escaped += QStringLiteral("&#39;");
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

QString renderEntities(const QString& text, const QJsonArray& entities)
{
    if (text.isEmpty())
        return {};

    QVector<EntityRange> ranges;
    ranges.reserve(entities.size());
    for (qsizetype i = 0; i < entities.size(); ++i) {
        const QJsonValue value = entities.at(i);
        if (!value.isObject())
            continue;

        EntityRange range;
        if (parseEntity(value.toObject(), text, range))
            ranges.append(range);
    }

    std::stable_sort(ranges.begin(), ranges.end(),
                     [](const EntityRange& left, const EntityRange& right) {
                         return left.start < right.start;
                     });

    QString html;
    html.reserve(text.size() + ranges.size() * 24);

    qsizetype cursor = 0;
    for (const EntityRange& range : ranges) {
        if (range.start < cursor)
            continue; // Overlaps an already rendered entity.

        html += escapeHtml(text.mid(cursor, range.start - cursor));
        html += range.tags.openTag;
        html += escapeHtml(text.mid(range.start, range.end - range.start));
        html += range.tags.closeTag;
        cursor = range.end;
    }
    html += escapeHtml(text.mid(cursor));
    return html;
}

QString renderMessageText(const QJsonObject& textObject)
{
    return renderEntities(textObject.value(QStringLiteral("text")).toString(),
                          textObject.value(QStringLiteral("entities")).toArray());
}

QString plainText(const QJsonObject& textObject)
{
    return textObject.value(QStringLiteral("text")).toString();
}

} // namespace beygram::telegram
