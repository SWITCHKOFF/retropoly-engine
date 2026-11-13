// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/sdk.cpp
#include "sdk.h"

#include <cstdlib>
#include <set>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "fs_util.h"
#include "json.h"
#include "version.h"

namespace rpe {
namespace ui {

namespace {

std::string getStringOr(const JsonValue& object, const char* key, const std::string& fallback)
{
    const JsonValue* value = object.find(key);
    if (value == nullptr || !value->isString()) return fallback;
    return value->asString();
}

bool getBoolOr(const JsonValue& object, const char* key, bool fallback)
{
    const JsonValue* value = object.find(key);
    bool result = fallback;
    if (value == nullptr || !value->asBool(&result)) return fallback;
    return result;
}

int getIntOr(const JsonValue& object, const char* key, int fallback)
{
    const JsonValue* value = object.find(key);
    int64_t result = fallback;
    if (value == nullptr || !value->asInt64(&result)) return fallback;
    return static_cast<int>(result);
}

bool isExecutableFile(const std::string& path)
{
    if (!isRegularFile(path)) return false;
#ifndef _WIN32
    return access(path.c_str(), X_OK) == 0;
#else
    return true;
#endif
}

std::string platformSdkEnv(Platform platform)
{
    switch (platform) {
        case Platform::Pc:
            return "RPE_SDK_PC";
        case Platform::Ps1:
            return "RPE_SDK_PS1";
        case Platform::Saturn:
            return "RPE_SDK_SATURN";
    }
    return "RPE_SDK_PC";
}

const char* const kRequiredComponents[] = {"public_headers", "libraries", "cmake_package",
                                           "examples",       "templates", "legal_notices",
                                           "version",        "checksums", "manifest"};

} // namespace

std::string environmentValue(const char* name)
{
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : std::string();
}

std::string exampleNameForPlatform(Platform platform)
{
    switch (platform) {
        case Platform::Pc:
            return "hello_triangle";
        case Platform::Ps1:
            return "hello_triangle_ps1";
        case Platform::Saturn:
            return "hello_triangle_saturn";
    }
    return "hello_triangle";
}

SdkLoadResult loadSdk(const std::string& path, Lang lang)
{
    SdkLoadResult result;
    const std::string root = absolutePath(path);
    SdkInfo sdk;
    sdk.path = root;

    if (!isDirectory(root)) {
        result.error = trf(lang, "err.sdk_manifest_missing", root.c_str());
        return result;
    }
    const std::string manifest_path = joinPath(root, "sdk_manifest.json");
    if (!isRegularFile(manifest_path)) {
        result.error = trf(lang, "err.sdk_manifest_missing", root.c_str());
        return result;
    }
    std::string text;
    std::string read_error;
    if (!readTextFile(manifest_path, &text, &read_error)) {
        result.error = trf(lang, "err.project_read", read_error.c_str());
        return result;
    }
    JsonValue manifest;
    JsonParseError parse_error;
    if (!jsonParse(text, &manifest, &parse_error)) {
        result.error = trf(lang, "err.sdk_manifest_json", parse_error.message.c_str());
        return result;
    }
    if (!manifest.isObject()) {
        result.error = trf(lang, "err.sdk_manifest_invalid", "root is not an object");
        return result;
    }

    sdk.schema = getIntOr(manifest, "schema", 0);
    sdk.package = getStringOr(manifest, "package", "");
    sdk.version = getStringOr(manifest, "version", "");
    sdk.git_commit = getStringOr(manifest, "git_commit", "");
    sdk.git_describe = getStringOr(manifest, "git_describe", "");
    sdk.commit_date = getStringOr(manifest, "commit_date", "");
    sdk.git_dirty = getBoolOr(manifest, "git_dirty", false);
    sdk.arch = getStringOr(manifest, "arch", "");
    sdk.os = getStringOr(manifest, "os", "");
    sdk.build_type = getStringOr(manifest, "build_type", "");
    sdk.compiler_banner = getStringOr(manifest, "compiler_banner", "");

    const std::string platform = getStringOr(manifest, "platform", "");
    if (!parsePlatform(platform, &sdk.platform)) {
        result.error =
            trf(lang, "err.sdk_manifest_invalid", ("unknown platform: " + platform).c_str());
        return result;
    }

    const JsonValue* compiler = manifest.find("compiler");
    if (compiler != nullptr && compiler->isObject()) {
        sdk.compiler_id = getStringOr(*compiler, "id", "");
        sdk.compiler_version = getStringOr(*compiler, "version", "");
    }
    const JsonValue* abi = manifest.find("abi");
    if (abi != nullptr && abi->isObject()) {
        sdk.abi_cxx = getStringOr(*abi, "cxx_standard", "");
        sdk.abi_endianness = getStringOr(*abi, "endianness", "");
        sdk.pointer_bits = getIntOr(*abi, "pointer_bits", 0);
    }
    const JsonValue* components = manifest.find("components");
    if (components != nullptr && components->isObject()) {
        for (size_t i = 0; i < components->object().size(); ++i) {
            const std::string& key = components->object()[i].first;
            bool enabled = false;
            components->object()[i].second.asBool(&enabled);
            sdk.components[key] = enabled;
        }
    }
    const JsonValue* examples = manifest.find("examples");
    if (examples != nullptr && examples->isArray()) {
        for (size_t i = 0; i < examples->array().size(); ++i) {
            if (examples->array()[i].isString()) {
                sdk.examples.push_back(examples->array()[i].asString());
            }
        }
    }
    const JsonValue* targets = manifest.find("targets");
    if (targets != nullptr && targets->isArray()) {
        for (size_t i = 0; i < targets->array().size(); ++i) {
            if (targets->array()[i].isString()) {
                sdk.targets.push_back(targets->array()[i].asString());
            }
        }
    }

    if (sdk.schema != 1) {
        result.error = trf(lang, "err.sdk_schema", sdk.schema);
        return result;
    }
    if (sdk.package != "retro-engine-sdk") {
        result.error = trf(lang, "err.sdk_manifest_invalid", ("package: " + sdk.package).c_str());
        return result;
    }
    SemVer version;
    if (!parseSemVer(sdk.version, &version)) {
        result.error = trf(lang, "err.sdk_manifest_invalid", ("version: " + sdk.version).c_str());
        return result;
    }
    for (size_t i = 0; i < sizeof(kRequiredComponents) / sizeof(kRequiredComponents[0]); ++i) {
        std::map<std::string, bool>::const_iterator it =
            sdk.components.find(kRequiredComponents[i]);
        if (it == sdk.components.end() || !it->second) {
            result.error = trf(lang, "err.sdk_component", kRequiredComponents[i]);
            return result;
        }
    }
    if (sdk.platform == Platform::Pc) {
        std::map<std::string, bool>::const_iterator it = sdk.components.find("asset_converter");
        if (it != sdk.components.end() && !it->second) {
            result.error = trf(lang, "err.sdk_component", "asset_converter");
            return result;
        }
    }

    const std::string required_files[] = {
        "include/retro_engine/retro_engine.h",
        "include/retro_engine/version.h",
        "lib/cmake/RetroPolyEngine/RetroPolyEngineConfig.cmake",
        "sdk_manifest.json",
        "checksums.sha256",
        "VERSION.txt",
    };
    for (size_t i = 0; i < sizeof(required_files) / sizeof(required_files[0]); ++i) {
        if (!isRegularFile(joinPath(root, required_files[i]))) {
            result.error = trf(lang, "err.sdk_component", required_files[i]);
            return result;
        }
    }
    if (!isDirectory(joinPath(root, "templates/project"))) {
        result.error = trf(lang, "err.sdk_component", "templates/project");
        return result;
    }
    const std::string example = exampleNameForPlatform(sdk.platform);
    if (!isDirectory(joinPath(root, "examples/" + example))) {
        result.error = trf(lang, "err.sdk_component", ("examples/" + example).c_str());
        return result;
    }
    if (sdk.platform == Platform::Pc) {
        const std::string converter = joinPath(root, "tools/asset_converter");
        if (isExecutableFile(converter)) {
            sdk.converter_path = converter;
        } else if (isExecutableFile(converter + ".exe")) {
            sdk.converter_path = converter + ".exe";
        } else {
            result.error = trf(lang, "err.sdk_component", "tools/asset_converter");
            return result;
        }
    }

    std::string detail;
    const VersionCompatibility compatibility =
        checkVersionCompatibility(uiVersion(), sdk.version, &detail);
    if (compatibility == VersionCompatibility::MajorMismatch ||
        compatibility == VersionCompatibility::MinorMismatch ||
        compatibility == VersionCompatibility::Invalid) {
        result.error = trf(lang, "err.sdk_version", detail.c_str());
        return result;
    }
    if (compatibility == VersionCompatibility::PatchMismatch) {
        sdk.warning = trf(lang, "version.patch_warning", detail.c_str());
    }

    sdk.valid = true;
    result.ok = true;
    result.sdk = sdk;
    return result;
}

std::vector<std::string> candidateSdkPaths(const AppSettings& settings, Platform platform)
{
    std::vector<std::string> candidates;
    std::set<std::string> seen;
    const std::string explicit_path = sdkPathForPlatform(settings, static_cast<int>(platform));
    const std::string env_path = environmentValue(platformSdkEnv(platform).c_str());

    std::vector<std::string> raw;
    raw.push_back(explicit_path);
    raw.push_back(env_path);
    if (!settings.sdk_root.empty()) {
        raw.push_back(settings.sdk_root);
        raw.push_back(joinPath(settings.sdk_root, platformCode(platform)));
        raw.push_back(joinPath(settings.sdk_root, "sdk-" + std::string(platformCode(platform))));
    }
    if (platform == Platform::Pc) {
        const std::string legacy = environmentValue("RPE_SDK_DIR");
        if (!legacy.empty()) raw.push_back(legacy);
    }
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i].empty()) continue;
        const std::string absolute = absolutePath(raw[i]);
        if (seen.insert(absolute).second) candidates.push_back(absolute);
    }
    return candidates;
}

SdkInfo findSdkForPlatform(const AppSettings& settings, Platform platform, Lang lang)
{
    const std::vector<std::string> candidates = candidateSdkPaths(settings, platform);
    SdkInfo fallback;
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (!isDirectory(candidates[i])) continue;
        const SdkLoadResult loaded = loadSdk(candidates[i], lang);
        if (loaded.ok && loaded.sdk.platform == platform) {
            return loaded.sdk;
        }
        if (fallback.error.empty() && !loaded.error.empty()) {
            fallback = loaded.sdk;
            fallback.error = loaded.error;
        }
    }
    return fallback;
}

std::vector<TargetAvailability> discoverTargets(const AppSettings& settings, Lang lang)
{
    std::vector<TargetAvailability> targets;
    const Platform platforms[] = {Platform::Pc, Platform::Ps1, Platform::Saturn};
    for (size_t i = 0; i < sizeof(platforms) / sizeof(platforms[0]); ++i) {
        TargetAvailability target;
        target.platform = platforms[i];
        const SdkInfo sdk = findSdkForPlatform(settings, platforms[i], lang);
        if (!sdk.valid) {
            target.available = false;
            target.reason_key = "target.reason.no_sdk";
            target.reason_detail = sdk.error.empty() ? trf(lang, "target.reason.no_sdk",
                                                           tr(lang, platformNameKey(platforms[i])))
                                                     : sdk.error;
            targets.push_back(target);
            continue;
        }
        target.sdk_path = sdk.path;
        if (platforms[i] == Platform::Ps1 && environmentValue("PSN00BSDK_LIBS").empty()) {
            target.available = false;
            target.reason_key = "target.reason.psn00bsdk";
            target.reason_detail = tr(lang, "target.reason.psn00bsdk");
            targets.push_back(target);
            continue;
        }
        if (platforms[i] == Platform::Saturn && environmentValue("JO_ENGINE_ROOT").empty()) {
            target.available = false;
            target.reason_key = "target.reason.jo_engine";
            target.reason_detail = tr(lang, "target.reason.jo_engine");
            targets.push_back(target);
            continue;
        }
        target.available = true;
        targets.push_back(target);
    }
    return targets;
}

std::string findExecutable(const std::string& configured, const std::vector<std::string>& names)
{
    if (!configured.empty()) {
        const std::string path = absolutePath(configured);
        if (isExecutableFile(path)) return path;
        // Явно заданное имя без пути — искать в PATH.
        if (isAbsolutePath(configured) || configured.find('/') != std::string::npos ||
            configured.find('\\') != std::string::npos) {
            return std::string();
        }
        const std::string found = findExecutable(std::string(), {configured});
        if (!found.empty()) return found;
    }
    const std::string path_env = environmentValue("PATH");
    if (path_env.empty()) return std::string();
    size_t index = 0;
    while (index <= path_env.size()) {
        const size_t separator = path_env.find_first_of(
#ifdef _WIN32
            ";"
#else
            ":"
#endif
            ,
            index);
        const size_t end = separator == std::string::npos ? path_env.size() : separator;
        const std::string directory = path_env.substr(index, end - index);
        if (!directory.empty()) {
            for (size_t n = 0; n < names.size(); ++n) {
                const std::string candidate = joinPath(directory, names[n]);
                if (isExecutableFile(candidate)) return absolutePath(candidate);
            }
        }
        if (separator == std::string::npos) break;
        index = separator + 1;
    }
    return std::string();
}

} // namespace ui
} // namespace rpe
