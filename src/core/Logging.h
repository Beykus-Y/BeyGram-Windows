#pragma once

#include <QString>
#include <QStringList>

namespace beygram::logging {

/// Installs a thread-safe logging handler that redacts secrets, writes a
/// rotating file under `logDirectory` and keeps a bounded in-memory ring for
/// the diagnostics panel. Safe to call more than once.
void initialize(const QString& logDirectory, bool verbose = false);

/// Restores the default Qt handler and flushes the log file. Intended for
/// orderly application shutdown and for tests.
void shutdown();

/// Last redacted log lines, oldest first. At most 200 entries.
[[nodiscard]] QStringList recentMessages();

/// Absolute path of the active log file, or an empty string when logging to
/// stderr only.
[[nodiscard]] QString logFilePath();

[[nodiscard]] bool isVerbose();

} // namespace beygram::logging
