add_library(beygram_platform_windows STATIC
    DatabaseKeyStore.cpp
    DatabaseKeyStore.h
    DpapiSecretStore.cpp
    DpapiSecretStore.h
    WindowChrome.cpp
    WindowChrome.h
)

target_link_libraries(beygram_platform_windows
    PUBLIC
        beygram_core
        Qt6::Core
        Qt6::Gui
)

if(WIN32)
    target_link_libraries(beygram_platform_windows PRIVATE crypt32 dwmapi)
endif()

target_include_directories(beygram_platform_windows PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
