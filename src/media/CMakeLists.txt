add_library(beygram_media STATIC
    BgMediaStatus.cpp
    BgMediaStatus.h
)

target_link_libraries(beygram_media
    PUBLIC
        beygram_core
        beygram_bgmedia
)
