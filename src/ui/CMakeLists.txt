add_library(beygram_ui STATIC
    AppController.cpp
    AppController.h
    ThemeController.cpp
    ThemeController.h
)

target_include_directories(beygram_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(beygram_ui
    PUBLIC
        beygram_core
        beygram_telegram
        beygram_media
        beygram_platform_windows
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        beygram_warnings
)
