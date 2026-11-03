# TDLib discovery for the Windows client.
#
# The client talks to TDLib through the official JSON interface (tdjson). The
# library is loaded at runtime with QLibrary, so a missing DLL leaves the app
# buildable and makes the UI report the real state instead of inventing a
# connection. When a tdjson.dll is found here it is copied next to the executable.
#
# Point BEYGRAM_TDLIB_DIR at a TDLib build tree (the folder that contains
# tdjson.dll), or add that folder to PATH.

set(BEYGRAM_TDLIB_DIR "" CACHE PATH "Directory containing tdjson.dll (TDLib JSON interface build)")

set(BEYGRAM_TDLIB_DLL "" CACHE FILEPATH "Explicit path to tdjson.dll")
set(BEYGRAM_TDLIB_STATUS "not configured")

find_file(BEYGRAM_TDLIB_DLL_FOUND
    NAMES tdjson.dll
    HINTS
        "${BEYGRAM_TDLIB_DIR}"
        "${BEYGRAM_TDLIB_DIR}/bin"
        "$ENV{BEYGRAM_TDLIB_DIR}"
    PATH_SUFFIXES bin
    NO_DEFAULT_PATH
)

if(NOT BEYGRAM_TDLIB_DLL AND BEYGRAM_TDLIB_DLL_FOUND)
    set(BEYGRAM_TDLIB_DLL "${BEYGRAM_TDLIB_DLL_FOUND}" CACHE FILEPATH "Explicit path to tdjson.dll" FORCE)
endif()

if(BEYGRAM_TDLIB_DLL AND EXISTS "${BEYGRAM_TDLIB_DLL}")
    get_filename_component(BEYGRAM_TDLIB_RUNTIME_DIR "${BEYGRAM_TDLIB_DLL}" DIRECTORY)
    file(GLOB BEYGRAM_TDLIB_RUNTIME_DLLS "${BEYGRAM_TDLIB_RUNTIME_DIR}/*.dll")
    set(BEYGRAM_TDLIB_FOUND TRUE)
    set(BEYGRAM_TDLIB_STATUS "found: ${BEYGRAM_TDLIB_DLL}")
    message(STATUS "TDLib JSON interface found at ${BEYGRAM_TDLIB_DLL}")
else()
    set(BEYGRAM_TDLIB_FOUND FALSE)
    set(BEYGRAM_TDLIB_RUNTIME_DLLS "")
    set(BEYGRAM_TDLIB_STATUS "not found (runtime will report it; see docs/WINDOWS_FOUNDATION.md)")
    message(STATUS "tdjson.dll not found. The app will build and report the missing library at runtime.")
endif()

# Copies tdjson.dll and its runtime dependencies (OpenSSL, zlib) next to a
# target. Dependencies must live beside tdjson.dll for the Windows loader.
function(beygram_copy_tdlib_runtime target)
    if(NOT BEYGRAM_TDLIB_FOUND)
        return()
    endif()
    foreach(dll IN LISTS BEYGRAM_TDLIB_RUNTIME_DLLS)
        get_filename_component(dll_name "${dll}" NAME)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${dll}"
                    "$<TARGET_FILE_DIR:${target}>/${dll_name}"
            COMMENT "Copying ${dll_name} next to ${target}"
        )
    endforeach()
endfunction()

add_library(beygram_tdlib INTERFACE)
target_compile_definitions(beygram_tdlib INTERFACE BEYGRAM_TDLIB_RUNTIME_LOAD=1)

if(BEYGRAM_TDLIB_FOUND)
    target_compile_definitions(beygram_tdlib INTERFACE BEYGRAM_TDLIB_BUILD_PRESENT=1)
endif()
