# Optional BG Media C++ core.
#
# BG Media is a separate repository (third_party/bg-media is declared as a git
# submodule upstream). The Windows port must be able to host BG Beat and BG Mix
# without an Android/JNI layer, but linking is deliberately opt-in for Phase 0
# so a missing submodule never breaks the foundation build.

set(BEYGRAM_BGMEDIA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/bg-media" CACHE PATH "BG Media source tree")

if(BEYGRAM_WITH_BGMEDIA)
    if(EXISTS "${BEYGRAM_BGMEDIA_DIR}/CMakeLists.txt")
        add_subdirectory("${BEYGRAM_BGMEDIA_DIR}" "${CMAKE_BINARY_DIR}/bg-media" EXCLUDE_FROM_ALL)
        set(BEYGRAM_BGMEDIA_STATUS "enabled (${BEYGRAM_BGMEDIA_DIR})")
        set(BEYGRAM_BGMEDIA_AVAILABLE TRUE)
    else()
        set(BEYGRAM_BGMEDIA_AVAILABLE FALSE)
        set(BEYGRAM_BGMEDIA_STATUS "requested but third_party/bg-media is empty; see docs/WINDOWS_FOUNDATION.md")
        message(WARNING "BEYGRAM_WITH_BGMEDIA=ON but ${BEYGRAM_BGMEDIA_DIR}/CMakeLists.txt does not exist.")
    endif()
else()
    set(BEYGRAM_BGMEDIA_AVAILABLE FALSE)
    set(BEYGRAM_BGMEDIA_STATUS "disabled (Phase 0)")
endif()

add_library(beygram_bgmedia INTERFACE)
if(BEYGRAM_BGMEDIA_AVAILABLE AND TARGET bgmedia_core)
    target_link_libraries(beygram_bgmedia INTERFACE bgmedia_core)
    target_compile_definitions(beygram_bgmedia INTERFACE BEYGRAM_HAS_BGMEDIA=1)
endif()
