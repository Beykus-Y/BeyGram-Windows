# Shared compiler warnings for first-party Windows targets.

add_library(beygram_warnings INTERFACE)

if(MSVC)
    target_compile_options(beygram_warnings INTERFACE
        /W4
        /permissive-
        /Zc:__cplusplus
        /utf-8
        /EHsc
    )
    # Qt headers are not warning-clean at /W4 on every MSVC release.
    target_compile_options(beygram_warnings INTERFACE /external:W0)
else()
    target_compile_options(beygram_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()
