#include "BgMediaStatus.h"

namespace beygram::media {

BgMediaStatus bgMediaStatus()
{
#ifdef BEYGRAM_HAS_BGMEDIA
    return {true, QStringLiteral("BG Media core is linked"),
            QStringLiteral("BG Beat and BG Mix can be wired to the desktop playback layer.")};
#else
    return {false, QStringLiteral("BG Media is not linked in this build"),
            QStringLiteral(
                "The Windows port keeps a C++ boundary for BG Media, but this build does "
                "not bundle the native core yet. The Android build uses BG Media through "
                "JNI; the desktop build will link the native core directly. No music "
                "playback is claimed here.")};
#endif
}

} // namespace beygram::media
