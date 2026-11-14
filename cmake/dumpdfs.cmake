# SPDX-License-Identifier: MIT
# RetroPolyEngine — cmake/dumpdfs.cmake
#
# Запуск pinned dumpdfs с записью listing'а в файл без shell-редиректа
# (Windows-совместимость N64-сборки). Параметры:
#   -DDUMPDFS=<path>  — dumpdfs executable;
#   -DDFS=<path>      — DFS image;
#   -DOUT=<path>      — listing output.
# ============================================================================
if(NOT DEFINED DUMPDFS OR NOT DEFINED DFS OR NOT DEFINED OUT)
    message(FATAL_ERROR "dumpdfs.cmake: требуются DUMPDFS, DFS, OUT")
endif()

execute_process(
    COMMAND "${DUMPDFS}" -l "${DFS}"
    OUTPUT_FILE "${OUT}"
    RESULT_VARIABLE _dumpdfs_result
    ERROR_VARIABLE _dumpdfs_error)

if(NOT _dumpdfs_result EQUAL 0)
    message(FATAL_ERROR "dumpdfs failed (${_dumpdfs_result}): ${_dumpdfs_error}")
endif()
