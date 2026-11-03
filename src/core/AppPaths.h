#pragma once

#include <QString>

namespace beygram {

/// Well-known per-user locations. Keeping them in one place prevents ad-hoc
/// path building and keeps account data separable for future TDLib sessions.
class AppPaths {
public:
    [[nodiscard]] static QString dataDirectory();
    [[nodiscard]] static QString logDirectory();
    [[nodiscard]] static QString cacheDirectory();
    [[nodiscard]] static QString configDirectory();

    /// Per-account TDLib database/files root. The identifier is sanitised so a
    /// malformed registry value can never escape the data directory.
    [[nodiscard]] static QString tdlibAccountDirectory(const QString& accountId);

    [[nodiscard]] static bool ensureDirectory(const QString& path, QString* error = nullptr);

    /// Sanitises one path component. Returns an empty string for unusable input.
    [[nodiscard]] static QString safeComponent(const QString& value);

private:
    AppPaths() = delete;
};

} // namespace beygram
