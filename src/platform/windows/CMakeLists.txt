add_library(beygram_platform_windows STATIC
    DpapiSecretStore.cpp
    DpapiSecretStore.h
)

target_link_libraries(beygram_platform_windows
    PUBLIC
        beygram_core
        Qt6::Core
)

if(WIN32)
    target_link_libraries(beygram_platform_windows PRIVATE crypt32)
endif()

target_include_directories(beygram_platform_windows PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
