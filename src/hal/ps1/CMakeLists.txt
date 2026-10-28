# ============================================================================
# HAL PS1 — PSn00bSDK v0.24 (pinned, §3.2), GTE, Ordering Table, CD-ROM.
# Реализация: Спринты 13–14 (docs/plans/sprint_13_14.md).
# Сборка только через пресет ps1-release + toolchainFile sdk.cmake.
# ============================================================================

add_library(retro_engine_hal STATIC
    platform_ps1.cpp
)
# Флаги задаёт toolchain через sdk.cmake; канонические -Werror применяются лишь там,
# где позволяет SDK (§3.5: «GCC/Clang: -Wall -Wextra -Wpedantic -Werror (где позволяет SDK)»).
retro_engine_apply_flags(retro_engine_hal)
