# SPDX-License-Identifier: MIT
# cmake/sha256_file.cmake — детерминированный SHA-256 побочного артефакта.
# Использование: cmake -DINPUT=<file> -DOUTPUT=<file.sha256> -P sha256_file.cmake
if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "sha256_file.cmake: нужны -DINPUT=<file> -DOUTPUT=<file>")
endif()
if(NOT EXISTS "${INPUT}")
    message(FATAL_ERROR "sha256_file.cmake: нет файла '${INPUT}'")
endif()
file(SHA256 "${INPUT}" _sha256)
get_filename_component(_name "${INPUT}" NAME)
file(WRITE "${OUTPUT}" "${_sha256}  ${_name}\n")
