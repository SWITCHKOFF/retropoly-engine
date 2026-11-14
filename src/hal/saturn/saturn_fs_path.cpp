// SPDX-License-Identifier: MIT
// RetroPolyEngine — src/hal/saturn/saturn_fs_path.cpp
#include "hal/saturn/saturn_fs_path.h"

#include <string.h>

namespace saturn {

PathDiag path_normalize(const char* path, char* out)
{
    if (out != nullptr) {
        out[0] = '\0';
    }
    if (path == nullptr || out == nullptr) {
        return PATH_ERR_NULL_ARGUMENT;
    }

    const size_t length = strlen(path);
    if (length == 0u) {
        return PATH_ERR_EMPTY;
    }
    if (length > SATURN_FS_NAME_MAX) {
        return PATH_ERR_TOO_LONG;
    }

    size_t dot_index = 0u;
    size_t dot_count = 0u;
    for (size_t i = 0u; i < length; ++i) {
        const char c = path[i];
        if (c == '/' || c == '\\' || c == ':' || c == ';') {
            return PATH_ERR_HOST_PATH;
        }
        if (c == '.') {
            ++dot_count;
            if (dot_count > 1u) {
                return PATH_ERR_INVALID_DOT;
            }
            dot_index = i;
            continue;
        }
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-')) {
            return PATH_ERR_INVALID_CHARACTER;
        }
    }

    if (path[0] == '.' || path[length - 1u] == '.') {
        return PATH_ERR_INVALID_DOT;
    }

    const size_t name_length = (dot_count == 1u) ? dot_index : length;
    const size_t ext_length = (dot_count == 1u) ? (length - dot_index - 1u) : 0u;
    if (name_length == 0u || name_length > 8u) {
        return PATH_ERR_INVALID_83;
    }
    if (dot_count == 1u && (ext_length == 0u || ext_length > 3u)) {
        return PATH_ERR_INVALID_83;
    }

    for (size_t i = 0u; i < length; ++i) {
        char c = path[i];
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - ('a' - 'A'));
        }
        out[i] = c;
    }
    out[length] = '\0';
    return PATH_OK;
}

const char* path_diag_string(PathDiag diag)
{
    switch (diag) {
        case PATH_OK:
            return "ok";
        case PATH_ERR_NULL_ARGUMENT:
            return "null argument";
        case PATH_ERR_EMPTY:
            return "empty path";
        case PATH_ERR_TOO_LONG:
            return "path too long";
        case PATH_ERR_HOST_PATH:
            return "host path or separator";
        case PATH_ERR_INVALID_CHARACTER:
            return "invalid character";
        case PATH_ERR_INVALID_DOT:
            return "invalid dot";
        case PATH_ERR_INVALID_83:
            return "not an 8.3 name";
        case PATH_ERR_COUNT:
        default:
            return "unknown";
    }
}

}  // namespace saturn
