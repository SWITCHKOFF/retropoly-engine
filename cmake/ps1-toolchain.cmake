# SPDX-License-Identifier: MIT
# ============================================================================
# cmake/ps1-toolchain.cmake — PS1 SDK toolchain wrapper (S33–34).
#
# Требует установленный PSn00bSDK v0.24 (pinned) и PSN00BSDK_LIBS.
# ============================================================================
if(NOT DEFINED ENV{PSN00BSDK_LIBS})
    message(FATAL_ERROR
        "PSN00BSDK_LIBS не задан. Установите PSn00bSDK v0.24 и выполните:\n"
        "  export PSN00BSDK_LIBS=$HOME/PSn00bSDK/lib/libpsn00b")
endif()
include("$ENV{PSN00BSDK_LIBS}/cmake/sdk.cmake")
set(PLATFORM_PS1 ON CACHE BOOL "RetroPolyEngine PS1 target" FORCE)
