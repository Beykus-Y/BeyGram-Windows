#include "Redaction.h"

#include <QRegularExpression>

namespace beygram::logging {
namespace {

struct Rule {
    QRegularExpression pattern;
    QString replacement;
};

const QRegularExpression::PatternOptions kCaseInsensitive =
    QRegularExpression::CaseInsensitiveOption;

const QList<Rule>& rules()
{
    static const QList<Rule> value = {
        // Telegram QR login links carry a one-time login token.
        {QRegularExpression(QStringLiteral(R"(tg://login\?token=[A-Za-z0-9._~-]+)"),
                            kCaseInsensitive),
         QStringLiteral("tg://login?token=[redacted]")},

        // JSON form: "api_hash": "..."
        {QRegularExpression(
             QStringLiteral(
                 R"RX("(api[_-]?id|api[_-]?hash|token|password|passwd|secret|authorization|credential|cookie|session|nonce|login|phone|proxy|subscribe(?:_url)?|url|link)"\s*:\s*"[^"]*")RX"),
             kCaseInsensitive),
         QStringLiteral("\"\\1\": \"[redacted]\"")},

        // key=value / key: value form, quoted or bare.
        {QRegularExpression(
             QStringLiteral(
                 R"RX(\b(api[_-]?id|api[_-]?hash|token|password|passwd|secret|authorization|credential|cookie|session|nonce|login|phone|proxy|subscribe(?:_url)?|url|link)(\s*[=:]\s*)("[^"\n]*"|'[^'\n]*'|[^\s,;]+))RX"),
             kCaseInsensitive),
         QStringLiteral("\\1\\2[redacted]")},

        // HTTP authorization headers.
        {QRegularExpression(QStringLiteral(R"(\bBearer\s+[A-Za-z0-9._~+/=-]+)"),
                            kCaseInsensitive),
         QStringLiteral("Bearer [redacted]")},

        // Bot tokens: <bot id>:<secret>.
        {QRegularExpression(QStringLiteral(R"(\b\d{6,12}:[A-Za-z0-9_-]{25,}\b)")),
         QStringLiteral("[redacted-bot-token]")},

        // Phone numbers, with or without a leading +. A formatted number must
        // contain a separator; plain long integers (chat/message ids) are not
        // phone numbers and must survive diagnostics.
        {QRegularExpression(QStringLiteral(R"((?<![\w])\+\d[\d\s().-]{7,}\d(?![\w]))")),
         QStringLiteral("[redacted-phone]")},
        {QRegularExpression(
             QStringLiteral(R"((?<![\w])\d[\d\s().-]*[\s().-][\d\s().-]{7,}\d(?![\w]))")),
         QStringLiteral("[redacted-phone]")},

        // Telegram api_hash is 32 lowercase hex characters.
        {QRegularExpression(QStringLiteral(R"(\b[0-9a-f]{32}\b)")),
         QStringLiteral("[redacted-hex-secret]")},
    };
    return value;
}

} // namespace

QString redact(const QString& text)
{
    if (text.isEmpty())
        return text;

    QString result = text;
    for (const Rule& rule : rules())
        result.replace(rule.pattern, rule.replacement);
    return result;
}

} // namespace beygram::logging
