#include "BotsFacade.h"

#include <optional>
#include <utility>

namespace beygram::ui {

namespace {

/// A TDLib id held as text, or nothing when the text is not a plain positive
/// decimal integer.
///
/// QML carries ids as strings, so the facade is the boundary where that text
/// becomes a qint64. Only ASCII digits are accepted: a sign, inner whitespace,
/// a decimal point or a non-numeric character is a caller bug, not a value to
/// guess at, and an overflow is rejected by toLongLong().
std::optional<qint64> parseId(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return std::nullopt;

    for (const QChar character : trimmed) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return std::nullopt;
    }

    bool ok = false;
    const qint64 value = trimmed.toLongLong(&ok);
    if (!ok || value <= 0)
        return std::nullopt;

    return value;
}

/// A command name or prefix in the form the menu stores it: trimmed, with a
/// single leading '/' removed. BotCommands applies the same rule to the TDLib
/// payload, so "/start", " /start " and "start" all name the same command.
QString normalizedCommandName(const QString& name)
{
    QString normalized = name.trimmed();
    if (normalized.startsWith(QLatin1Char('/')))
        normalized = normalized.mid(1).trimmed();
    return normalized;
}

} // namespace

BotsFacade::BotsFacade(QObject* parent) : QObject(parent)
{
    // Each model suppresses no-op changes on its own, so forwarding all three
    // is enough: a redundant write emits nothing, a real one repaints every
    // binding on this object exactly once.
    connect(&m_commands, &telegram::BotCommandModel::changed, this, &BotsFacade::changed);
    connect(&m_inline, &telegram::InlineQueryController::changed, this, &BotsFacade::changed);
    connect(&m_webApp, &telegram::WebAppSessionController::changed, this,
            &BotsFacade::changed);
}

QStringList BotsFacade::commands() const
{
    const QVector<telegram::BotCommand> stored = m_commands.commands();

    QStringList names;
    names.reserve(stored.size());
    for (const telegram::BotCommand& command : stored)
        names.append(command.command);

    return names;
}

void BotsFacade::setCommands(const QStringList& names, const QStringList& descriptions)
{
    QVector<telegram::BotCommand> commands;
    commands.reserve(names.size());

    for (qsizetype i = 0; i < names.size(); ++i) {
        const QString name = normalizedCommandName(names.at(i));
        // A command without a name cannot be inserted after the slash, so it
        // is dropped here rather than shown as an unusable button.
        if (name.isEmpty())
            continue;

        const QString description =
            i < descriptions.size() ? descriptions.at(i).trimmed() : QString();
        commands.append(telegram::BotCommand{name, description});
    }

    // The model owns de-duplication and the order, so the facade does not
    // repeat those rules.
    m_commands.setCommands(std::move(commands));
}

QStringList BotsFacade::filterCommands(const QString& prefix) const
{
    const QVector<telegram::BotCommand> matches =
        m_commands.filtered(normalizedCommandName(prefix));

    QStringList names;
    names.reserve(matches.size());
    for (const telegram::BotCommand& command : matches)
        names.append(command.command);

    return names;
}

bool BotsFacade::inlineActive() const
{
    return m_inline.active();
}

QString BotsFacade::inlineQuery() const
{
    return m_inline.query();
}

QStringList BotsFacade::inlineTitles() const
{
    const QVector<telegram::InlineResultRow> rows = m_inline.results();

    QStringList titles;
    titles.reserve(rows.size());
    for (const telegram::InlineResultRow& row : rows)
        titles.append(row.title);

    return titles;
}

void BotsFacade::openInline(const QString& botUserId)
{
    const std::optional<qint64> id = parseId(botUserId);
    if (!id)
        return;

    // A malformed id must not disturb the session already open, and the
    // controller already keeps a repeated open quiet.
    m_inline.open(*id);
}

void BotsFacade::setInlineQuery(const QString& query)
{
    // A query belongs to a bot: without a session there is nothing to ask, so
    // the write is ignored instead of leaving a stray query behind.
    if (!m_inline.active())
        return;

    const QString trimmed = query.trimmed();
    if (trimmed == m_inline.query())
        return;

    // The controller drops the previous answer and the selection, then emits
    // changed(); the request follows it so a binding sees the cleared list
    // before the caller starts the next fetch.
    m_inline.setQuery(trimmed);
    emit inlineQueryRequested(m_inline.query());
}

void BotsFacade::setInlineResults(const QStringList& ids, const QStringList& titles)
{
    QVector<telegram::InlineResultRow> rows;
    rows.reserve(ids.size());

    for (qsizetype i = 0; i < ids.size(); ++i) {
        telegram::InlineResultRow row;
        row.id = ids.at(i);
        row.title = i < titles.size() ? titles.at(i) : QString();
        rows.append(row);
    }

    // The controller ignores an answer without an open bot or a query, drops
    // rows without an id, keeps the first of repeated ids and stays quiet for
    // an identical answer.
    m_inline.setResults(std::move(rows));
}

void BotsFacade::selectInlineResult(int index)
{
    // Checked against the answer before the call: the controller ignores an
    // out-of-range index, so reading the selection afterwards would report the
    // previous row and send the wrong result.
    const QVector<telegram::InlineResultRow> rows = m_inline.results();
    if (index < 0 || index >= rows.size())
        return;

    m_inline.setSelectedIndex(index);
    emit inlineResultChosen(rows.at(index).id);
}

void BotsFacade::closeInline()
{
    m_inline.close();
}

bool BotsFacade::webAppActive() const
{
    return m_webApp.active();
}

QString BotsFacade::webAppTitle() const
{
    return m_webApp.active() ? m_webApp.info().title : QString();
}

void BotsFacade::openWebApp(const QString& chatId, const QString& botUserId,
                            const QString& title, const QString& url)
{
    const std::optional<qint64> chat = parseId(chatId);
    const std::optional<qint64> bot = parseId(botUserId);
    if (!chat || !bot)
        return;

    const QString target = url.trimmed();
    // The URL is the only thing the embedded browser will load, so an unsafe
    // one is refused outright instead of being stored and shown as openable.
    if (!telegram::isSafeWebAppUrl(target))
        return;

    telegram::WebAppInfo info;
    info.botUserId = *bot;
    info.url = target;
    info.title = title.trimmed();
    info.isSecure = true;

    // The controller rejects non-positive ids on its own; both are already
    // positive here, so this always opens (or replaces) the session.
    m_webApp.open(*chat, *bot, info);

    // A request signal is not a state change: reopening the same app is
    // quiet on changed() but must still launch it again.
    emit webAppOpenRequested(target);
}

void BotsFacade::closeWebApp()
{
    m_webApp.close();
}

} // namespace beygram::ui
