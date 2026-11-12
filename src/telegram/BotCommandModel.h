#pragma once

#include "BotCommands.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace beygram::telegram {

/// Holds one bot's advertised commands for the command menu.
///
/// The model is a plain value holder: it stores already-parsed BotCommand
/// entries and answers prefix lookups without touching TDLib or the UI. Names
/// are de-duplicated case-insensitively on assignment because Telegram treats
/// bot command names case-insensitively, so "Help" and "help" are the same
/// command; the first entry wins and the incoming order is otherwise kept.
class BotCommandModel final : public QObject {
    Q_OBJECT

public:
    explicit BotCommandModel(QObject* parent = nullptr);

    /// Replaces the stored commands, dropping later entries whose name matches
    /// an earlier one case-insensitively. Emits changed().
    void setCommands(QVector<BotCommand> commands);

    /// Returns a copy of the stored commands, in menu order.
    [[nodiscard]] QVector<BotCommand> commands() const;

    /// Returns the commands whose name starts with `prefix`, compared
    /// case-insensitively and keeping the stored order. An empty prefix
    /// matches every command.
    [[nodiscard]] QVector<BotCommand> filtered(const QString& prefix) const;

    [[nodiscard]] int count() const;

    /// Drops every command and emits changed() when there was anything to drop.
    void clear();

signals:
    void changed();

private:
    QVector<BotCommand> m_commands;
};

} // namespace beygram::telegram
