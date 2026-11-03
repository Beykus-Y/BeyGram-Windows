add_library(beygram_ui STATIC
    AppController.cpp
    AppController.h
    QrCodeImageProvider.cpp
    QrCodeImageProvider.h
    ThemeController.cpp
    ThemeController.h
    ../thirdparty/qrcodegen/qrcodegen.cpp
)

target_include_directories(beygram_ui
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/qrcodegen"
)

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
