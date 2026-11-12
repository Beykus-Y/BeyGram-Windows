#include "ChatThemeState.h"

#include "TelegramTypes.h"

#include <QJsonValue>
#include <QStringList>

namespace beygram::telegram {
namespace {

// Payloads use TDLib's snake_case members; the camelCase spellings are accepted
// for callers that hand over an object built elsewhere in the app.
QJsonValue member(const QJsonObject& object, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QJsonValue value = object.value(key);
        if (!value.isUndefined() && !value.isNull())
            return value;
    }
    return {};
}

QString stringMember(const QJsonObject& object, const QStringList& keys)
{
    const QJsonValue value = member(object, keys);
    return value.isString() ? value.toString() : QString();
}

// TDLib sends int32 colors; the JSON interface sends them as numbers, and as
// strings once they would lose precision. Anything else keeps 0.
int colorMember(const QJsonObject& object, const QStringList& keys)
{
    const QJsonValue value = member(object, keys);
    if (!value.isDouble() && !value.isString())
        return 0;
    return static_cast<int>(jsonInt64(value));
}

qint64 idMember(const QJsonObject& object, const QStringList& keys)
{
    const QJsonValue value = member(object, keys);
    if (!value.isDouble() && !value.isString())
        return 0;
    return jsonInt64(value);
}

bool looksLikeTheme(const QJsonObject& object)
{
    return object.contains(QStringLiteral("theme_name"))
        || object.contains(QStringLiteral("light_theme"))
        || object.contains(QStringLiteral("dark_theme"))
        || object.contains(QStringLiteral("accent_color"))
        || object.contains(QStringLiteral("accentColor"));
}

// The chatTheme object arrives under "chatTheme" (the spelling the app's update
// bridge uses), "chat_theme" or "theme" (the member newer updates use). A bare
// chatTheme object is used as-is.
QJsonObject themeObjectOf(const QJsonObject& source)
{
    const QJsonValue nested = member(
        source,
        {QStringLiteral("chatTheme"), QStringLiteral("chat_theme"), QStringLiteral("theme")});
    if (nested.isObject())
        return nested.toObject();
    return looksLikeTheme(source) ? source : QJsonObject{};
}

ChatThemeState themeStateOf(const QJsonObject& source)
{
    const QJsonObject theme = themeObjectOf(source);

    ChatThemeState state;
    state.name = stringMember(theme, {QStringLiteral("theme_name"), QStringLiteral("name")});
    state.lightThemeId = idMember(
        theme, {QStringLiteral("light_theme"), QStringLiteral("lightTheme")});
    state.darkThemeId = idMember(
        theme, {QStringLiteral("dark_theme"), QStringLiteral("darkTheme")});
    state.accentColor = colorMember(
        theme, {QStringLiteral("accent_color"), QStringLiteral("accentColor")});
    state.outgoingBubbleColor = colorMember(
        theme, {QStringLiteral("outgoing_bubble_color"),
                QStringLiteral("outgoingBubbleColor")});
    state.messageTextColor = colorMember(
        theme, {QStringLiteral("message_text_color"), QStringLiteral("messageTextColor")});

    // The per-chat choice lives on the chat or update itself; the theme object
    // only describes what that choice resolves to. An explicitly empty name is
    // TDLib's "app-wide theme", so it wins over the resolved name too.
    const QJsonValue chosenName = member(source, {QStringLiteral("theme_name")});
    if (chosenName.isString())
        state.name = chosenName.toString();

    return state;
}

} // namespace

ChatThemeState chatThemeFromChat(const QJsonObject& chat)
{
    return themeStateOf(chat);
}

ChatThemeState chatThemeFromUpdate(const QJsonObject& update)
{
    return themeStateOf(update);
}

QJsonObject setChatThemeRequest(qint64 chatId, const QString& themeName)
{
    // An empty name is TDLib's "use the app-wide theme".
    return QJsonObject{
        {QStringLiteral("@type"), QStringLiteral("setChatTheme")},
        {QStringLiteral("chat_id"), chatId},
        {QStringLiteral("theme_name"), themeName.trimmed()},
    };
}

} // namespace beygram::telegram
