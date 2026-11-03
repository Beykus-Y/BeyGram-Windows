add_library(beygram_telegram STATIC
    AuthState.cpp
    AuthState.h
    TelegramConfig.cpp
    TelegramConfig.h
    TelegramSession.cpp
    TelegramSession.h
    TdJsonTransport.cpp
    TdJsonTransport.h
)

target_include_directories(beygram_telegram PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(beygram_telegram
    PUBLIC
        beygram_core
        beygram_tdlib
        Qt6::Core
        beygram_warnings
)
