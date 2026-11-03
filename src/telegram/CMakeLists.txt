add_library(beygram_telegram STATIC
    AccountRegistry.cpp
    AccountRegistry.h
    AuthRequests.cpp
    AuthRequests.h
    AuthState.cpp
    AuthState.h
    ChatFilterModel.cpp
    ChatFilterModel.h
    ChatListModel.cpp
    ChatListModel.h
    CredentialStore.cpp
    CredentialStore.h
    MessageListModel.cpp
    MessageListModel.h
    SessionLock.cpp
    SessionLock.h
    TdJsonTransport.cpp
    TdJsonTransport.h
    TdLibLocator.cpp
    TdLibLocator.h
    TelegramConfig.cpp
    TelegramConfig.h
    TelegramRepository.cpp
    TelegramRepository.h
    TelegramRequests.cpp
    TelegramRequests.h
    TelegramSession.cpp
    TelegramSession.h
    TelegramTypes.cpp
    TelegramTypes.h
)

target_include_directories(beygram_telegram PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

target_link_libraries(beygram_telegram
    PUBLIC
        beygram_core
        beygram_platform_windows
        beygram_tdlib
        Qt6::Core
        beygram_warnings
)
