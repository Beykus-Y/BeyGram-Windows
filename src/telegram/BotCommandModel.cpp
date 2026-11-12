#include "BotCommandModel.h"

#include <algorithm>
#include <utility>

namespace beygram::telegram {

BotCommandModel::BotCommandModel(QObject* parent)
    : QObject(parent)
{
}

void BotCommandModel::setCommands(QVector<BotCommand> commands)
{
    m_commands.clear();
    m_commands.reserve(commands.size());

    for (BotCommand& command : commands) {
        const bool duplicate = std::any_of(
            m_commands.cbegin(), m_commands.cend(), [&command](const BotCommand& existing) {
                return existing.command.compare(command.command, Qt::CaseInsensitive) == 0;
            });
        if (!duplicate)
            m_commands.append(std::move(command));
    }

    emit changed();
}

QVector<BotCommand> BotCommandModel::commands() const
{
    return m_commands;
}

QVector<BotCommand> BotCommandModel::filtered(const QString& prefix) const
{
    QVector<BotCommand> matches;
    matches.reserve(m_commands.size());

    for (const BotCommand& command : m_commands) {
        if (command.command.startsWith(prefix, Qt::CaseInsensitive))
            matches.append(command);
    }

    return matches;
}

int BotCommandModel::count() const
{
    return m_commands.size();
}

void BotCommandModel::clear()
{
    if (m_commands.isEmpty())
        return;

    m_commands.clear();
    emit changed();
}

} // namespace beygram::telegram
