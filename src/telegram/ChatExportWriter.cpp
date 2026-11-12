#include "ChatExportWriter.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>

namespace beygram::telegram {
namespace {

QString normalizedTitle(const QString& title)
{
    return title.trimmed().isEmpty() ? QStringLiteral("Chat") : title;
}

QVector<ExportedMessage> sortedMessages(const QVector<ExportedMessage>& messages)
{
    QVector<ExportedMessage> sorted = messages;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const ExportedMessage& left, const ExportedMessage& right) {
                         if (left.date != right.date)
                             return left.date < right.date;
                         return left.id < right.id;
                     });
    return sorted;
}

/// Android's exportIsoTimestamp: a non-positive date has no ISO spelling.
QString isoDate(qint64 seconds)
{
    if (seconds <= 0)
        return {};
    return QDateTime::fromSecsSinceEpoch(seconds).toUTC().toString(Qt::ISODate);
}

QString htmlEscape(const QString& value)
{
    QString escaped;
    escaped.reserve(value.size());
    for (const QChar character : value) {
        switch (character.unicode()) {
        case u'&':
            escaped += QLatin1String("&amp;");
            break;
        case u'<':
            escaped += QLatin1String("&lt;");
            break;
        case u'>':
            escaped += QLatin1String("&gt;");
            break;
        case u'"':
            escaped += QLatin1String("&quot;");
            break;
        case u'\'':
            escaped += QLatin1String("&#39;");
            break;
        default:
            escaped += character;
            break;
        }
    }
    return escaped;
}

QString senderLabel(const QString& sender)
{
    return sender.trimmed().isEmpty() ? QStringLiteral("Unknown sender") : sender;
}

constexpr const char* kStyle = R"CSS(
:root { color-scheme: light dark; }
* { box-sizing: border-box; }
body { margin: 0; font-family: system-ui, -apple-system, "Segoe UI", sans-serif; background: #f4f5f7; color: #17181a; }
main { max-width: 860px; margin: 0 auto; padding: 24px 16px 48px; }
.chat-header, .chat-footer { background: #fff; border-radius: 16px; padding: 18px 20px; margin-bottom: 16px; }
h1 { font-size: 22px; margin: 0 0 8px; overflow-wrap: anywhere; }
.meta { color: #5f6368; font-size: 13px; margin: 3px 0; }
.messages { display: flex; flex-direction: column; gap: 10px; }
.message { background: #fff; border-radius: 14px; padding: 12px 14px; max-width: 78%; }
.message header { display: flex; gap: 8px; align-items: baseline; margin-bottom: 5px; font-size: 12px; color: #5f6368; }
.sender { font-weight: 600; color: #1f6f4d; overflow-wrap: anywhere; }
.text { white-space: pre-wrap; overflow-wrap: anywhere; font-size: 15px; line-height: 1.42; }
.media { display: inline-block; margin: 8px 0 0; padding: 2px 8px; border-radius: 999px; background: #e7e9ed; color: #4b4f55; font-size: 12px; }
.chat-footer { color: #5f6368; font-size: 12px; }
.chat-footer p { margin: 0; }
@media (prefers-color-scheme: dark) {
    body { background: #101214; color: #e8eaed; }
    .chat-header, .chat-footer, .message { background: #1c1f23; }
    .sender { color: #7fd4ab; }
    .meta, .message header, .chat-footer { color: #aeb4bc; }
    .media { background: #262a2f; color: #c2c7ce; }
}
)CSS";

} // namespace

QString ChatExportWriter::toHtml(const QString& chatTitle,
                                 const QVector<ExportedMessage>& messages)
{
    const QString title = normalizedTitle(chatTitle);
    const QString escapedTitle = htmlEscape(title);
    const QVector<ExportedMessage> sorted = sortedMessages(messages);

    QString html;
    html.reserve(4096 + sorted.size() * 256);
    html += QLatin1String("<!doctype html>\n<html lang=\"und\">\n<head>\n<meta charset=\"utf-8\">\n");
    html += QLatin1String("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n");
    html += QLatin1String("<meta name=\"generator\" content=\"BeyGram\">\n<title>");
    html += escapedTitle;
    html += QLatin1String("</title>\n<style>");
    html += QLatin1String(kStyle);
    html += QLatin1String("</style>\n</head>\n<body>\n<main>\n<header class=\"chat-header\">\n<h1>");
    html += escapedTitle;
    html += QLatin1String("</h1>\n<p class=\"meta\">");
    html += QString::number(sorted.size());
    html += (sorted.size() == 1) ? QLatin1String(" message") : QLatin1String(" messages");
    html += QLatin1String("</p>\n</header>\n<section class=\"messages\">\n");

    for (const ExportedMessage& message : sorted) {
        const QString iso = isoDate(message.date);
        html += QLatin1String("<article class=\"message\" id=\"message-");
        html += QString::number(message.id);
        html += QLatin1String("\">\n<header><span class=\"sender\">");
        html += htmlEscape(senderLabel(message.sender));
        html += QLatin1String("</span> ");
        if (iso.isEmpty()) {
            html += QLatin1String("<time>Unknown date</time>");
        } else {
            const QString escapedIso = htmlEscape(iso);
            html += QLatin1String("<time datetime=\"");
            html += escapedIso;
            html += QLatin1String("\">");
            html += escapedIso;
            html += QLatin1String("</time>");
        }
        html += QLatin1String("</header>\n");

        if (!message.text.trimmed().isEmpty()) {
            html += QLatin1String("<div class=\"text\">");
            html += htmlEscape(message.text);
            html += QLatin1String("</div>\n");
        }
        if (!message.mediaType.trimmed().isEmpty()) {
            html += QLatin1String("<p class=\"media\">");
            html += htmlEscape(message.mediaType);
            html += QLatin1String("</p>\n");
        }
        html += QLatin1String("</article>\n");
    }

    html += QLatin1String("</section>\n<footer class=\"chat-footer\">\n<p>");
    html += QLatin1String("This file contains private chat data and is not protected by BeyGram. Store it securely.");
    html += QLatin1String("</p>\n</footer>\n</main>\n</body>\n</html>\n");
    return html;
}

QJsonObject ChatExportWriter::toJson(const QString& chatTitle,
                                     const QVector<ExportedMessage>& messages)
{
    const QString title = normalizedTitle(chatTitle);
    const QVector<ExportedMessage> sorted = sortedMessages(messages);

    QJsonArray array;
    for (const ExportedMessage& message : sorted) {
        array.append(QJsonObject{
            { QStringLiteral("id"), message.id },
            { QStringLiteral("date"), message.date },
            { QStringLiteral("dateIso"), isoDate(message.date) },
            { QStringLiteral("sender"), message.sender },
            { QStringLiteral("text"), message.text },
            { QStringLiteral("mediaType"), message.mediaType },
        });
    }

    return QJsonObject{
        { QStringLiteral("format"), QStringLiteral("beygram-chat-export") },
        { QStringLiteral("version"), 1 },
        { QStringLiteral("chat"), QJsonObject{ { QStringLiteral("title"), title } } },
        { QStringLiteral("messages"), array },
        { QStringLiteral("summary"),
          QJsonObject{ { QStringLiteral("messages"), static_cast<int>(array.size()) } } },
    };
}

} // namespace beygram::telegram
