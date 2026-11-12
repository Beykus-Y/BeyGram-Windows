cmake_minimum_required(VERSION 3.24)

project(BeyGramWindows VERSION 0.1.0 LANGUAGES CXX)

# BeyGram Windows is a native Qt 6 / C++20 port of the Android client. The Android
# sources remain in app/ as the functional reference until their migration phases
# are complete. Nothing in this file builds them.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BEYGRAM_BUILD_TESTS "Build unit tests" ON)
option(BEYGRAM_WITH_BGMEDIA "Link the BG Media C++ core when third_party/bg-media is present" OFF)
option(BEYGRAM_EMBED_API_CREDENTIALS "Embed Telegram api_id/api_hash from secrets.properties (local test builds only)" OFF)
set(BEYGRAM_SECRETS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/secrets.properties" CACHE FILEPATH "Android-style secrets.properties used for the local Windows test build")

if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Multimedia Test Widgets)
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

include(cmake/BeyGramWarnings.cmake)
include(cmake/BeyGramTdlib.cmake)
include(cmake/BeyGramBgMedia.cmake)

add_subdirectory(src)

if(BEYGRAM_EMBED_API_CREDENTIALS)
    if(NOT EXISTS "${BEYGRAM_SECRETS_FILE}")
        message(FATAL_ERROR
            "BEYGRAM_EMBED_API_CREDENTIALS=ON but ${BEYGRAM_SECRETS_FILE} does not exist")
    endif()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${BEYGRAM_SECRETS_FILE}")
    file(STRINGS "${BEYGRAM_SECRETS_FILE}" _beygram_secret_lines
         REGEX "^[ \t]*telegramApi(Id|Hash)[ \t]*=")

    foreach(_line IN LISTS _beygram_secret_lines)
        if(_line MATCHES "^[ \t]*telegramApiId[ \t]*=[ \t]*(.*)$")
            string(STRIP "${CMAKE_MATCH_1}" BEYGRAM_BUILD_API_ID)
        elseif(_line MATCHES "^[ \t]*telegramApiHash[ \t]*=[ \t]*(.*)$")
            string(STRIP "${CMAKE_MATCH_1}" BEYGRAM_BUILD_API_HASH)
        endif()
    endforeach()

    if(NOT BEYGRAM_BUILD_API_ID MATCHES "^[0-9]+$")
        message(FATAL_ERROR "telegramApiId in ${BEYGRAM_SECRETS_FILE} is missing or invalid")
    endif()
    string(LENGTH "${BEYGRAM_BUILD_API_HASH}" _beygram_hash_length)
    if(NOT _beygram_hash_length EQUAL 32 OR NOT BEYGRAM_BUILD_API_HASH MATCHES "^[0-9a-fA-F]+$")
        message(FATAL_ERROR "telegramApiHash in ${BEYGRAM_SECRETS_FILE} is missing or invalid")
    endif()

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BeyGramBuildSecrets.h.in"
        "${CMAKE_BINARY_DIR}/generated/BeyGramBuildSecrets.h"
        @ONLY
    )
    target_compile_definitions(beygram_ui PRIVATE BEYGRAM_EMBEDDED_CREDENTIALS=1)
    target_include_directories(beygram_ui PRIVATE "${CMAKE_BINARY_DIR}/generated")
    message(STATUS "Embedding Telegram API credentials from secrets.properties (local test build)")
endif()

qt_add_executable(beygram WIN32
    src/app/main.cpp
)

qt_add_qml_module(beygram
    URI BeyGram
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Main.qml
        qml/Shell.qml
        qml/AuthPage.qml
        qml/components/AuthField.qml
        qml/components/Avatar.qml
        qml/components/Card.qml
        qml/components/ChoiceChip.qml
        qml/components/AlbumGrid.qml
        qml/components/ChatNotificationSettings.qml
        qml/components/NotificationSettings.qml
        qml/components/LinkPreviewCard.qml
        qml/components/PollBubble.qml
        qml/components/PollComposerDialog.qml
        qml/components/VoiceRecorder.qml
        qml/components/TranslationSheet.qml
        qml/components/StoryViewer.qml
        qml/components/StoragePanel.qml
        qml/components/StickerPanel.qml
        qml/components/StatisticsPanel.qml
        qml/components/StarsPanel.qml
        qml/components/ShortcutsPanel.qml
        qml/components/SessionsPanel.qml
        qml/components/SecretChatBanner.qml
        qml/components/SearchSuggestions.qml
        qml/components/ProfileEditor.qml
        qml/components/PrivacyPanel.qml
        qml/components/PremiumCard.qml
        qml/components/PlaybackBar.qml
        qml/components/PaymentSheet.qml
        qml/components/NetworkPanel.qml
        qml/components/MediaGallery.qml
        qml/components/GiftPicker.qml
        qml/components/FolderEditor.qml
        qml/components/EmojiPicker.qml
        qml/components/DeepLinkPrompt.qml
        qml/components/ContactsPanel.qml
        qml/components/ChatListControls.qml
        qml/components/CallOverlay.qml
        qml/components/BotKeyboard.qml
        qml/components/AppearancePanel.qml
        qml/components/ProfileEditDialog.qml
        qml/components/SelectionToolbar.qml
        qml/components/StickerPicker.qml
        qml/components/WallpaperPicker.qml
        qml/components/ForwardDialog.qml
        qml/components/ChatRow.qml
        qml/components/ChatFolderTabs.qml
        qml/components/MessageBubble.qml
        qml/components/PinnedMessageBanner.qml
        qml/components/ReactionBar.qml
        qml/components/ReactionPicker.qml
        qml/components/StickerBubble.qml
        qml/components/TypingIndicator.qml
        qml/components/MediaViewerOverlay.qml
        qml/components/MediaPlaybackOverlay.qml
        qml/pages/ChatsPage.qml
        qml/components/NavButton.qml
        qml/components/Pill.qml
        qml/components/PrimaryButton.qml
        qml/components/SecondaryButton.qml
        qml/components/StatusRow.qml
        qml/pages/OverviewPage.qml
        qml/pages/SettingsPage.qml
        qml/pages/ArchivePage.qml
        qml/pages/GlobalSearchPage.qml
        qml/pages/ToolsPage.qml
        qml/pages/DesignGalleryPage.qml
        qml/ds/DsTooltip.qml
        qml/ds/DsToast.qml
        qml/ds/DsTheme.qml
        qml/ds/DsTextField.qml
        qml/ds/DsTabs.qml
        qml/ds/DsSwitch.qml
        qml/ds/DsSpinner.qml
        qml/ds/DsSlider.qml
        qml/ds/DsSheet.qml
        qml/ds/DsSegmented.qml
        qml/ds/DsSection.qml
        qml/ds/DsSearchField.qml
        qml/ds/DsMenuItem.qml
        qml/ds/DsMenu.qml
        qml/ds/DsListRow.qml
        qml/ds/DsIconButton.qml
        qml/ds/DsIcon.qml
        qml/ds/DsHeader.qml
        qml/ds/DsEmptyState.qml
        qml/ds/DsDivider.qml
        qml/ds/DsDialog.qml
        qml/ds/DsComposer.qml
        qml/ds/DsChip.qml
        qml/ds/DsChatRow.qml
        qml/ds/DsCard.qml
        qml/ds/DsButton.qml
        qml/ds/DsBubble.qml
        qml/ds/DsBadge.qml
        qml/ds/DsAvatar.qml
        qml/ds/design.js
        qml/pages/EnvironmentPage.qml
        qml/pages/LogsPage.qml
)

target_link_libraries(beygram
    PRIVATE
        beygram_ui
        beygram_platform_windows
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::QuickControls2
        beygram_warnings
)

if(BEYGRAM_TDLIB_FOUND)
    beygram_copy_tdlib_runtime(beygram)
endif()

# The QML module is compiled into the executable. Qt's deployment tool creates
# the redistributable folder (see docs/WINDOWS_FOUNDATION.md).

if(BEYGRAM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "BeyGram Windows ${PROJECT_VERSION} | Qt ${Qt6Core_VERSION} | tests=${BEYGRAM_BUILD_TESTS}")
message(STATUS "BeyGram TDLib: ${BEYGRAM_TDLIB_STATUS}")
message(STATUS "BeyGram BG Media: ${BEYGRAM_BGMEDIA_STATUS}")
