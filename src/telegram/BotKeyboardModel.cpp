#include "BotKeyboardModel.h"

#include "BotMarkup.h"

#include <optional>
#include <utility>

namespace beygram::telegram {
namespace {

/// Flattens one already-parsed TDLib button into the shape this model exposes.
/// Returns nullopt for a button the UI cannot act on: no text to show, or a
/// variant outside the five kinds this model knows.
std::optional<BotKeyboardButton> keyboardButtonFrom(const BotButton& button)
{
    if (button.text.isEmpty())
        return std::nullopt;

    BotKeyboardButton parsed;
    parsed.text = button.text;

    if (button.type == QLatin1String("callback")
        || button.type == QLatin1String("callbackWithPassword")) {
        parsed.type = QStringLiteral("callback");
        parsed.value = button.data;
    } else if (button.type == QLatin1String("url")
               || button.type == QLatin1String("loginUrl")) {
        parsed.type = QStringLiteral("url");
        parsed.value = button.url;
    } else if (button.type == QLatin1String("webApp")) {
        parsed.type = QStringLiteral("webapp");
        parsed.value = button.url;
    } else if (button.type == QLatin1String("switchInline")) {
        parsed.type = QStringLiteral("switchInline");
        parsed.value = button.data;
    } else if (button.type == QLatin1String("text")) {
        parsed.type = QStringLiteral("text");
    } else {
        // Contact and location requests, games, user buttons and unknown
        // variants have no representation here, so they are not offered.
        return std::nullopt;
    }

    return parsed;
}

/// Keeps row and column order while dropping buttons that do not survive the
/// flattening. A row left without buttons is dropped as well.
QVector<QVector<BotKeyboardButton>> keyboardRowsFrom(const QVector<QVector<BotButton>>& source)
{
    QVector<QVector<BotKeyboardButton>> rows;
    rows.reserve(source.size());

    for (const QVector<BotButton>& sourceRow : source) {
        QVector<BotKeyboardButton> row;
        row.reserve(sourceRow.size());

        for (const BotButton& button : sourceRow) {
            if (std::optional<BotKeyboardButton> parsed = keyboardButtonFrom(button))
                row.append(std::move(*parsed));
        }

        if (!row.isEmpty())
            rows.append(std::move(row));
    }

    return rows;
}

} // namespace

BotKeyboardModel::BotKeyboardModel(QObject* parent)
    : QObject(parent)
{
}

void BotKeyboardModel::setReplyMarkup(const QJsonObject& markup)
{
    const QString markupType = markup.value(QStringLiteral("@type")).toString();

    QVector<QVector<BotKeyboardButton>> rows;
    bool forceReply = false;

    if (markupType == QLatin1String("replyMarkupForceReply")) {
        forceReply = true;
    } else {
        // botMarkupFromMessage() already knows both keyboard markup shapes and
        // their TDLib button variants, so feed it the markup as a message.
        const QJsonObject message{{QStringLiteral("reply_markup"), markup}};
        rows = keyboardRowsFrom(botMarkupFromMessage(message).rows);
    }

    if (rows == m_rows && forceReply == m_forceReply)
        return;

    m_rows = std::move(rows);
    m_forceReply = forceReply;
    emit changed();
}

QVector<QVector<BotKeyboardButton>> BotKeyboardModel::rows() const
{
    return m_rows;
}

int BotKeyboardModel::rowCount() const
{
    return m_rows.size();
}

bool BotKeyboardModel::visible() const
{
    return !m_rows.isEmpty();
}

bool BotKeyboardModel::forceReply() const
{
    return m_forceReply;
}

void BotKeyboardModel::clear()
{
    if (m_rows.isEmpty() && !m_forceReply)
        return;

    m_rows.clear();
    m_forceReply = false;
    emit changed();
}

} // namespace beygram::telegram
