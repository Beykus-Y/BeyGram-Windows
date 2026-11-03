#include "TextFormatting.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QList>
#include <QVector>

#include <algorithm>

namespace beygram::telegram {
namespace {

struct EntityRange {
    qsizetype start = 0;
    qsizetype end = 0;
    int order = 0;
    QString openTag;
    QString closeTag;
};

struct EntityTags {
    QString openTag;
    QString closeTag;
};

QString escapeText(const QString& text)
{
    QString escaped;
    escaped.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
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
        case u'\r':
            if (i + 1 < text.size() && text.at(i + 1).unicode() == u'\n')
                break;
            escaped += QStringLiteral("<br/>");
            break;
        case u'\n':
            escaped += QStringLiteral("<br/>");
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

QString escapeAttribute(const QString& value)
{
    QString escaped;
    escaped.reserve(value.size());
    for (const QChar ch : value) {
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

QString urlScheme(const QString& url)
{
    for (qsizetype i = 0; i < url.size(); ++i) {
        const QChar ch = url.at(i);
        if (ch.unicode() == u':')
            return url.left(i).toLower();
        if (ch.unicode() == u'/'
            || ch.unicode() == u'?'
            || ch.unicode() == u'#'
            || ch.isSpace())
            return {};
    }
    return {};
}

// Returns an escaped href when the target uses an allowed scheme, or an empty
// string when the link must be rendered as plain text.
QString linkHref(const QString& url)
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

    return escapeAttribute(trimmed);
}

EntityTags spanTags(const QString& className)
{
    return {QStringLiteral("<span class=\"") + className + QStringLiteral("\">"),
            QStringLiteral("</span>")};
}

EntityTags anchorTags(const QString& url)
{
    const QString href = linkHref(url);
    if (href.isEmpty())
        return {};
    return {QStringLiteral("<a href=\"") + href + QStringLiteral("\">"),
            QStringLiteral("</a>")};
}

EntityTags tagsForEntity(const QJsonObject& type, const QString& entityText)
{
    const QString kind = type.value(QStringLiteral("@type")).toString();

    if (kind == QStringLiteral("textEntityTypeBold"))
        return {QStringLiteral("<b>"), QStringLiteral("</b>")};
    if (kind == QStringLiteral("textEntityTypeItalic"))
        return {QStringLiteral("<i>"), QStringLiteral("</i>")};
    if (kind == QStringLiteral("textEntityTypeUnderline"))
        return {QStringLiteral("<u>"), QStringLiteral("</u>")};
    if (kind == QStringLiteral("textEntityTypeStrikethrough"))
        return {QStringLiteral("<s>"), QStringLiteral("</s>")};
    if (kind == QStringLiteral("textEntityTypeSpoiler"))
        return spanTags(QStringLiteral("spoiler"));
    if (kind == QStringLiteral("textEntityTypeCode"))
        return {QStringLiteral("<code>"), QStringLiteral("</code>")};
    if (kind == QStringLiteral("textEntityTypePre")
        || kind == QStringLiteral("textEntityTypePreCode")) {
        return {QStringLiteral("<pre>"), QStringLiteral("</pre>")};
    }
    if (kind == QStringLiteral("textEntityTypeBlockquote"))
        return {QStringLiteral("<blockquote>"), QStringLiteral("</blockquote>")};
    if (kind == QStringLiteral("textEntityTypeTextUrl"))
        return anchorTags(type.value(QStringLiteral("url")).toString());
    if (kind == QStringLiteral("textEntityTypeUrl"))
        return anchorTags(entityText);
    if (kind == QStringLiteral("textEntityTypeMention")
        || kind == QStringLiteral("textEntityTypeTextMention")) {
        return spanTags(QStringLiteral("mention"));
    }
    if (kind == QStringLiteral("textEntityTypeHashtag"))
        return spanTags(QStringLiteral("hashtag"));
    if (kind == QStringLiteral("textEntityTypeCashtag"))
        return spanTags(QStringLiteral("cashtag"));
    if (kind == QStringLiteral("textEntityTypeBotCommand"))
        return spanTags(QStringLiteral("bot-command"));
    if (kind == QStringLiteral("textEntityTypeEmailAddress"))
        return spanTags(QStringLiteral("email"));
    if (kind == QStringLiteral("textEntityTypePhoneNumber"))
        return spanTags(QStringLiteral("phone"));
    if (kind == QStringLiteral("textEntityTypeBankCardNumber"))
        return spanTags(QStringLiteral("bank-card"));

    // customEmoji keeps its literal text: this module does not resolve emoji
    // image files. Unknown entity types are ignored as well.
    return {};
}

bool entityFromJson(const QJsonObject& entity, const QString& text, int order,
                    EntityRange& range)
{
    const QJsonValue offsetValue = entity.value(QStringLiteral("offset"));
    const QJsonValue lengthValue = entity.value(QStringLiteral("length"));
    if (!offsetValue.isDouble() || !lengthValue.isDouble())
        return false;

    const int offset = offsetValue.toInt(-1);
    const int length = lengthValue.toInt(0);
    if (offset < 0 || length <= 0)
        return false;

    const qsizetype textLength = text.size();
    if (qsizetype(offset) >= textLength)
        return false;

    qsizetype start = offset;
    qsizetype end = qsizetype(offset) + qsizetype(length);
    if (end > textLength)
        end = textLength;
    if (end <= start)
        return false;

    // Never cut a surrogate pair in half: expand the range to the pair edges.
    if (start > 0 && text.at(start - 1).isHighSurrogate()
        && text.at(start).isLowSurrogate()) {
        --start;
    }
    if (end < textLength && text.at(end - 1).isHighSurrogate()
        && text.at(end).isLowSurrogate()) {
        ++end;
    }

    const QJsonObject type = entity.value(QStringLiteral("type")).toObject();
    const EntityTags tags = tagsForEntity(type, text.mid(start, end - start));
    if (tags.openTag.isEmpty() || tags.closeTag.isEmpty())
        return false;

    range.start = start;
    range.end = end;
    range.order = order;
    range.openTag = tags.openTag;
    range.closeTag = tags.closeTag;
    return true;
}

QString buildHtml(const QString& text, const QVector<EntityRange>& ranges)
{
    QList<qsizetype> boundaries;
    boundaries.reserve(ranges.size() * 2 + 2);
    boundaries.append(0);
    boundaries.append(text.size());
    for (const EntityRange& range : ranges) {
        boundaries.append(range.start);
        boundaries.append(range.end);
    }
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());

    QList<int> stack;
    QString html;
    html.reserve(text.size() + ranges.size() * 16);

    qsizetype cursor = 0;
    for (const qsizetype boundary : boundaries) {
        if (boundary > cursor) {
            html += escapeText(text.mid(cursor, boundary - cursor));
            cursor = boundary;
        }

        // Close the ranges that end at this boundary. When an outer range ends
        // before an inner one (overlapping, not nested entities), the inner
        // ranges are closed first and reopened afterwards, which keeps the
        // emitted markup well formed.
        while (true) {
            int target = -1;
            for (int i = int(stack.size()) - 1; i >= 0; --i) {
                if (ranges.at(stack.at(i)).end <= boundary) {
                    target = i;
                    break;
                }
            }
            if (target < 0)
                break;

            QList<int> reopen;
            for (int i = int(stack.size()) - 1; i > target; --i) {
                html += ranges.at(stack.at(i)).closeTag;
                reopen.prepend(stack.at(i));
            }
            html += ranges.at(stack.at(target)).closeTag;
            stack.resize(target);

            for (const int index : reopen) {
                html += ranges.at(index).openTag;
                stack.append(index);
            }
        }

        // Open the ranges that start here, outermost (largest end) first so
        // nested entities keep their natural order.
        QList<int> starting;
        for (int i = 0; i < int(ranges.size()); ++i) {
            if (ranges.at(i).start == boundary)
                starting.append(i);
        }
        std::sort(starting.begin(), starting.end(), [&ranges](int left, int right) {
            if (ranges.at(left).end != ranges.at(right).end)
                return ranges.at(left).end > ranges.at(right).end;
            return ranges.at(left).order < ranges.at(right).order;
        });
        for (const int index : starting) {
            html += ranges.at(index).openTag;
            stack.append(index);
        }
    }

    // Every range ends at or before text.size(), so this is defensive only.
    for (int i = int(stack.size()) - 1; i >= 0; --i)
        html += ranges.at(stack.at(i)).closeTag;

    return html;
}

} // namespace

FormattedText formatTdlibText(const QJsonObject& formattedText)
{
    FormattedText result;
    result.plainText = formattedText.value(QStringLiteral("text")).toString();
    if (result.plainText.isEmpty())
        return result;

    QVector<EntityRange> ranges;
    const QJsonArray entities = formattedText.value(QStringLiteral("entities")).toArray();
    ranges.reserve(entities.size());

    for (int i = 0; i < int(entities.size()); ++i) {
        EntityRange range;
        if (entityFromJson(entities.at(i).toObject(), result.plainText, i, range))
            ranges.append(range);
    }

    result.html = buildHtml(result.plainText, ranges);
    return result;
}

} // namespace beygram::telegram
