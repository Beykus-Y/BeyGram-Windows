configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/BeyGramVersion.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/BeyGramVersion.h"
    @ONLY
)

add_library(beygram_core STATIC
    AppPaths.cpp
    AppPaths.h
    ColorContrast.cpp
    ColorContrast.h
    Logging.cpp
    Logging.h
    Redaction.cpp
    Redaction.h
    Result.h
)

target_include_directories(beygram_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.."
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(beygram_core
    PUBLIC
        Qt6::Core
        Qt6::Gui
        beygram_warnings
)

target_compile_definitions(beygram_core PUBLIC BEYGRAM_VERSION_STRING="${PROJECT_VERSION}")
