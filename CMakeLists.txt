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

if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Test)
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

include(cmake/BeyGramWarnings.cmake)
include(cmake/BeyGramTdlib.cmake)
include(cmake/BeyGramBgMedia.cmake)

add_subdirectory(src)

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
        qml/components/Card.qml
        qml/components/NavButton.qml
        qml/components/Pill.qml
        qml/components/PrimaryButton.qml
        qml/components/SecondaryButton.qml
        qml/components/StatusRow.qml
        qml/pages/OverviewPage.qml
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
