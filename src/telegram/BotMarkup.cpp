#include "BotMarkup.h"

#include "TelegramTypes.h"

#include <QJsonArray>
#include <QJsonValue>

namespace beygram::telegram {
namespace {

/// Reads one keyboardButton / inlineKeyboardButton object. The button object
/// itself is not type-checked: only its nested type decides what it does, so a
/// button from either keyboard shape parses the same way.
BotButton buttonFrom(const QJsonObject& button)
{
    BotButton parsed;
    parsed.text = button.value(QStringLiteral("text")).toString();

    const QJsonObject type = button.value(QStringLiteral("type")).toObject();
    const QString tdlibType = type.value(QStringLiteral("@type")).toString();

    if (tdlibType == QLatin1String("keyboardButtonTypeText")) {
        parsed.type = QStringLiteral("text");
    } else if (tdlibType == QLatin1String("keyboardButtonTypeRequestPhoneNumber")) {
        parsed.type = QStringLiteral("requestPhoneNumber");
    } else if (tdlibType == QLatin1String("keyboardButtonTypeRequestLocation")) {
        parsed.type = QStringLiteral("requestLocation");
    } else if (tdlibType == QLatin1String("keyboardButtonTypeRequestPoll")) {
        parsed.type = QStringLiteral("requestPoll");
    } else if (tdlibType == QLatin1String("keyboardButtonTypeRequestUsers")) {
        parsed.type = QStringLiteral("requestUsers");
    } else if (tdlibType == QLatin1String("keyboardButtonTypeRequestChat")) {
        parsed.type = QStringLiteral("requestChat");
    } else if (tdlibType == QLatin1String("keyboardButtonTypeWebApp")) {
        parsed.type = QStringLiteral("webApp");
        parsed.url = type.value(QStringLiteral("url")).toString();
    } else if (tdlibType == QLatin1String("inlineKeyboardButtonTypeCallback")) {
        parsed.type = QStringLiteral("callback");
        parsed.data = type.value(QStringLiteral("data")).toString();
    } else if (tdlibType == QLatin1String("inlineKeyboardButtonTypeCallbackWithPassword")) {
        parsed.type = QStringLiteral("callbackWithPassword");
        parsed.data = type.value(QStringLiteral("data")).toString();
    } else if (tdlibType == QLatin1String("inlineKeyboardButtonTypeCallbackGame")) {
        parsed.type = QStringLiteral("callbackGame");
    } else if (tdlibType == QLatin1String("inlineKeyboardButtonTypeUrl")) {
        parsed.type = QStringLiteral("url");
        parsed.url = type.value(QStringLiteral("url")).toString();
    } else if (tdlibType == QLatin1String("inlineKeyboardButtonTypeLoginUrl")) {
        parsed.type = QStringLiteral("loginUrl");
        parsed.url = type.value(QStringLiteral("url")).toString();
    } else if (tdlibType == QLatin1String("inlineKeyboardButtonTypeCopyText")) {
        parsed.type = QStringLiteral("copyText");
        parsed.data = type.value(QStringLiteral("text")).toString();
    } else if (tdlibType == QLatin1String("inlineKeyboardButtonTypeUser")) {
        parsed.type = QStringLiteral("user");
        parsed.userId = jsonInt64(type.value(QStringLiteral("user_id")));
    } else if (tdlibType == QLatin1String("inlineKeyboardButtonTypeSwitchInline")) {
        parsed.type = QStringLiteral("switchInline");
        parsed.data = type.value(QStringLiteral("query")).toString();
    } else if (tdlibType == QLatin1String("inlineKeyboardButtonTypeBuy")) {
        parsed.type = QStringLiteral("buy");
    } else if (tdlibType == QLatin1String("inlineKeyboardButtonTypeWebApp")) {
        parsed.type = QStringLiteral("webApp");
        parsed.url = type.value(QStringLiteral("url")).toString();
    } else {
        // An unknown variant stays visible with its text but is not actionable,
        // so a newer TDLib cannot make this layer press something it cannot
        // describe.
        parsed.type = QStringLiteral("unsupported");
    }

    return parsed;
}

/// Parses the rows array shared by both keyboard markups. A row without any
/// usable button is dropped: it carries nothing a view could lay out.
QVector<QVector<BotButton>> rowsFrom(const QJsonArray& rows)
{
    QVector<QVector<BotButton>> parsedRows;
    parsedRows.reserve(rows.size());

    for (const QJsonValue& rowValue : rows) {
        if (!rowValue.isArray())
            continue;

        const QJsonArray rowArray = rowValue.toArray();
        QVector<BotButton> row;
        row.reserve(rowArray.size());

        for (const QJsonValue& buttonValue : rowArray) {
            if (!buttonValue.isObject())
                continue;
            row.append(buttonFrom(buttonValue.toObject()));
        }

        if (!row.isEmpty())
            parsedRows.append(row);
    }

    return parsedRows;
}

} // namespace

BotKeyboard botMarkupFromMessage(const QJsonObject& message)
{
    BotKeyboard keyboard;

    const QJsonValue markupValue = message.value(QStringLiteral("reply_markup"));
    if (!markupValue.isObject())
        return keyboard;

    const QJsonObject markup = markupValue.toObject();
    const QString type = markup.value(QStringLiteral("@type")).toString();

    if (type == QLatin1String("replyMarkupShowKeyboard")) {
        keyboard.replyKeyboard = true;
        keyboard.oneTime = markup.value(QStringLiteral("one_time")).toBool(false);
        // TDLib defaults resize_keyboard to true, so an omitted flag still
        // means the bot wants the keyboard resized.
        keyboard.resize = markup.value(QStringLiteral("resize_keyboard")).toBool(true);
        keyboard.placeholder =
            markup.value(QStringLiteral("input_field_placeholder")).toString();
        keyboard.rows = rowsFrom(markup.value(QStringLiteral("rows")).toArray());
    } else if (type == QLatin1String("replyMarkupInlineKeyboard")) {
        keyboard.inlineKeyboard = true;
        keyboard.rows = rowsFrom(markup.value(QStringLiteral("rows")).toArray());
    }

    return keyboard;
}

QJsonObject callbackQueryRequest(qint64 chatId, qint64 messageId, const QString& data)
{
    const QJsonObject payload{
        {QStringLiteral("@type"), QStringLiteral("callbackQueryPayloadData")},
        {QStringLiteral("data"), data},
    };

    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("getCallbackQueryAnswer")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("message_id"), messageId},
        {QStringLiteral("payload"), payload},
    };
}

} // namespace beygram::telegram
