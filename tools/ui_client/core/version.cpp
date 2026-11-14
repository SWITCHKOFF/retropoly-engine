// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/version.cpp
#include "version.h"

#include <cctype>
#include <cstdlib>

#ifndef RPE_UI_VERSION
#define RPE_UI_VERSION "0.2.1"
#endif

namespace rpe {
namespace ui {

namespace {

bool parseComponent(const std::string& text, size_t* index, int* out)
{
    if (index == nullptr || out == nullptr) return false;
    const size_t begin = *index;
    size_t end = begin;
    while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])) != 0) {
        ++end;
    }
    if (end == begin) return false;
    if (end - begin > 9) return false;
    const long value = std::strtol(text.substr(begin, end - begin).c_str(), nullptr, 10);
    if (value < 0 || value > 999999) return false;
    *out = static_cast<int>(value);
    *index = end;
    return true;
}

} // namespace

const char* uiVersion()
{
    return RPE_UI_VERSION;
}

bool parseSemVer(const std::string& text, SemVer* out)
{
    if (out == nullptr) return false;
    size_t index = 0;
    SemVer parsed;
    if (!parseComponent(text, &index, &parsed.major)) return false;
    if (index >= text.size() || text[index] != '.') return false;
    ++index;
    if (!parseComponent(text, &index, &parsed.minor)) return false;
    if (index >= text.size() || text[index] != '.') return false;
    ++index;
    if (!parseComponent(text, &index, &parsed.patch)) return false;
    if (index != text.size()) return false;
    *out = parsed;
    return true;
}

int compareSemVer(const SemVer& left, const SemVer& right)
{
    if (left.major != right.major) return left.major < right.major ? -1 : 1;
    if (left.minor != right.minor) return left.minor < right.minor ? -1 : 1;
    if (left.patch != right.patch) return left.patch < right.patch ? -1 : 1;
    return 0;
}

std::string semVerToString(const SemVer& version)
{
    return std::to_string(version.major) + "." + std::to_string(version.minor) + "." +
           std::to_string(version.patch);
}

VersionCompatibility checkVersionCompatibility(const std::string& ui_version,
                                               const std::string& sdk_version, std::string* detail)
{
    SemVer ui;
    SemVer sdk;
    if (!parseSemVer(ui_version, &ui) || !parseSemVer(sdk_version, &sdk)) {
        if (detail != nullptr) {
            *detail = "некорректная версия (UI '" + ui_version + "', SDK '" + sdk_version + "')";
        }
        return VersionCompatibility::Invalid;
    }
    if (ui.major != sdk.major) {
        if (detail != nullptr) {
            *detail = "major-версии не совпадают: UI " + ui_version + ", SDK " + sdk_version;
        }
        return VersionCompatibility::MajorMismatch;
    }
    if (ui.minor != sdk.minor) {
        if (detail != nullptr) {
            *detail = "minor-версии не совпадают: UI " + ui_version + ", SDK " + sdk_version;
        }
        return VersionCompatibility::MinorMismatch;
    }
    if (ui.patch != sdk.patch) {
        if (detail != nullptr) {
            *detail = "patch-версии отличаются: UI " + ui_version + ", SDK " + sdk_version;
        }
        return VersionCompatibility::PatchMismatch;
    }
    if (detail != nullptr) *detail = "версии совпадают";
    return VersionCompatibility::Compatible;
}

} // namespace ui
} // namespace rpe
