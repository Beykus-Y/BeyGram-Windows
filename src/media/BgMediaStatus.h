#pragma once

#include <QString>

namespace beygram::media {

/// Honest capability report for the future BG Media integration. It never
/// claims playback support that is not compiled into this binary.
struct BgMediaStatus {
    bool linked = false;
    QString title;
    QString detail;
};

[[nodiscard]] BgMediaStatus bgMediaStatus();

} // namespace beygram::media
