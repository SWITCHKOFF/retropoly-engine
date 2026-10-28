# ============================================================================
# Канонические флаги компиляции — Спринт 0 (§3.5, docs/plans/sprint_00.md):
#   MSVC      : /W4 /WX /MP /Zi
#   GCC/Clang : -Wall -Wextra -Wpedantic -Werror (где позволяет SDK)
#
# third-party изолируется: флаги применяются ТОЛЬКО к целям движка через
# retro_engine_apply_flags(); вендоренные библиотеки (SDL3, GoogleTest, glad,
# PSn00bSDK) собираются собственными CMake-скриптами и не получают -Werror.
# ============================================================================

function(retro_engine_apply_flags target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /MP /Zi)
    else()
        set(_retro_cflags -Wall -Wextra -Wpedantic -Werror)
        set(_retro_ldflags "")

        if(ENABLE_COVERAGE)
            list(APPEND _retro_cflags --coverage)
            list(APPEND _retro_ldflags --coverage)
        endif()

        if(ENABLE_ASAN)
            list(APPEND _retro_cflags -fsanitize=address -fno-omit-frame-pointer)
            list(APPEND _retro_ldflags -fsanitize=address)
        endif()

        target_compile_options(${target} PRIVATE ${_retro_cflags})
        if(_retro_ldflags)
            target_link_options(${target} PRIVATE ${_retro_ldflags})
        endif()
    endif()
endfunction()
