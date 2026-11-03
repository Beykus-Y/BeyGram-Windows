#pragma once

#include <QString>

namespace beygram::logging {

/// Removes secrets from any text that may reach a log file, the diagnostics
/// panel or a user-facing error. Ported from the Android DiagnosticsRedactor
/// and extended for TDLib authorization payloads.
///
/// This is defence in depth, not a guarantee: callers must still avoid logging
/// request bodies wholesale.
[[nodiscard]] QString redact(const QString& text);

} // namespace beygram::logging
