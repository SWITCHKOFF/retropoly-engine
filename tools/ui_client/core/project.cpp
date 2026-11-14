// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/project.cpp
#include "project.h"

#include <algorithm>
#include <cctype>
#include <set>

#include "fs_util.h"
#include "json.h"
#include "version.h"

namespace rpe {
namespace ui {

namespace {

const char* const kProjectFileName = ".rpeproject";

bool isIdentifier(const std::string& value)
{
    if (value.empty() || value.size() > 128) return false;
    for (size_t i = 0; i < value.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (std::isalnum(c) == 0 && c != '_' && c != '-' && c != '.') return false;
    }
    return value != "." && value != "..";
}

std::string extForAsset(const AssetEntry& asset)
{
    return asset.type == AssetType::Mesh ? ".mesh" : ".tex";
}

bool sourceExtMatches(AssetType type, const std::string& extension)
{
    if (type == AssetType::Mesh) {
        return extension == ".gltf" || extension == ".glb";
    }
    return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
}

void addIssue(std::vector<ValidationIssue>* issues, IssueSeverity severity,
              const std::string& field, const std::string& message)
{
    ValidationIssue issue;
    issue.severity = severity;
    issue.field = field;
    issue.message = message;
    issues->push_back(issue);
}

bool getString(const JsonValue& object, const std::string& key, bool required, std::string* out,
               std::vector<ValidationIssue>* issues, Lang lang)
{
    const JsonValue* value = object.find(key);
    if (value == nullptr) {
        if (required) {
            addIssue(issues, IssueSeverity::Error, key,
                     trf(lang, "err.field_required", key.c_str()));
            return false;
        }
        return true;
    }
    if (!value->isString()) {
        addIssue(issues, IssueSeverity::Error, key, trf(lang, "err.field_type", key.c_str()));
        return false;
    }
    *out = value->asString();
    return true;
}

bool getUint(const JsonValue& object, const std::string& key, uint64_t* out,
             std::vector<ValidationIssue>* issues, Lang lang)
{
    const JsonValue* value = object.find(key);
    if (value == nullptr) return true;
    int64_t number = 0;
    if (!value->asInt64(&number) || number < 0) {
        addIssue(issues, IssueSeverity::Error, key, trf(lang, "err.field_type", key.c_str()));
        return false;
    }
    *out = static_cast<uint64_t>(number);
    return true;
}

void rejectUnknownFields(const JsonValue& object, const std::set<std::string>& allowed,
                         const std::string& prefix, std::vector<ValidationIssue>* issues, Lang lang)
{
    for (size_t i = 0; i < object.object().size(); ++i) {
        const std::string& key = object.object()[i].first;
        if (allowed.find(key) == allowed.end()) {
            addIssue(issues, IssueSeverity::Error, prefix + key,
                     trf(lang, "err.field_unknown", key.c_str()));
        }
    }
}

std::string resolvedPath(const std::string& root, const std::string& path)
{
    if (path.empty()) return std::string();
    if (isAbsolutePath(path)) return normalizePath(path);
    return joinPath(root, path);
}

} // namespace

const char* platformCode(Platform platform)
{
    switch (platform) {
        case Platform::Pc:
            return "pc";
        case Platform::Ps1:
            return "ps1";
        case Platform::Saturn:
            return "saturn";
    }
    return "pc";
}

bool parsePlatform(const std::string& code, Platform* out)
{
    if (out == nullptr) return false;
    if (code == "pc") {
        *out = Platform::Pc;
        return true;
    }
    if (code == "ps1") {
        *out = Platform::Ps1;
        return true;
    }
    if (code == "saturn") {
        *out = Platform::Saturn;
        return true;
    }
    return false;
}

const char* platformNameKey(Platform platform)
{
    switch (platform) {
        case Platform::Pc:
            return "target.pc";
        case Platform::Ps1:
            return "target.ps1";
        case Platform::Saturn:
            return "target.saturn";
    }
    return "target.pc";
}

const char* assetTypeCode(AssetType type)
{
    return type == AssetType::Mesh ? "mesh" : "texture";
}

bool parseAssetType(const std::string& code, AssetType* out)
{
    if (out == nullptr) return false;
    if (code == "mesh") {
        *out = AssetType::Mesh;
        return true;
    }
    if (code == "texture") {
        *out = AssetType::Texture;
        return true;
    }
    return false;
}

const char* assetTypeNameKey(AssetType type)
{
    return type == AssetType::Mesh ? "assets.mesh" : "assets.texture";
}

std::string projectRoot(const std::string& project_file)
{
    if (isDirectory(project_file)) return normalizePath(project_file);
    const std::string parent = parentPath(project_file);
    return parent.empty() ? "." : parent;
}

std::string projectFilePath(const std::string& root)
{
    return joinPath(root, kProjectFileName);
}

std::vector<ValidationIssue> validateProject(const Project& project, const std::string& root,
                                             bool check_paths, Lang lang)
{
    std::vector<ValidationIssue> issues;
    if (project.schema != 1) {
        addIssue(&issues, IssueSeverity::Error, "schema", trf(lang, "err.schema", project.schema));
    }
    const bool name_ok = !project.name.empty() && project.name.size() <= 64 &&
                         isIdentifier(project.name) && project.name.front() != '.';
    if (!name_ok) {
        addIssue(&issues, IssueSeverity::Error, "name",
                 trf(lang, "err.name", project.name.c_str()));
    }
    SemVer version;
    if (!parseSemVer(project.engine_version, &version)) {
        addIssue(&issues, IssueSeverity::Error, "engine_version",
                 trf(lang, "err.version", project.engine_version.c_str()));
    }
    if (!isIdentifier(project.target)) {
        addIssue(&issues, IssueSeverity::Error, "target",
                 trf(lang, "err.name", project.target.c_str()));
    }
    if (!project.sdk_path.empty() && !isAbsolutePath(project.sdk_path) &&
        !isSafeRelativePath(project.sdk_path)) {
        addIssue(&issues, IssueSeverity::Error, "sdk_path",
                 trf(lang, "err.path_unsafe", project.sdk_path.c_str()));
    }
    if (project.sdk_path.empty()) {
        addIssue(&issues, IssueSeverity::Warning, "sdk_path",
                 trf(lang, "err.sdk_manifest_missing", "<empty>"));
    }
    if (!isSafeRelativePath(project.build_dir)) {
        addIssue(&issues, IssueSeverity::Error, "build_dir",
                 trf(lang, "err.path_unsafe", project.build_dir.c_str()));
    }
    if (!project.run_target.empty() && !isIdentifier(project.run_target) &&
        !isSafeRelativePath(project.run_target)) {
        addIssue(&issues, IssueSeverity::Error, "run_target",
                 trf(lang, "err.path_unsafe", project.run_target.c_str()));
    }
    if (!project.emulator_path.empty() && !isAbsolutePath(project.emulator_path) &&
        !isSafeRelativePath(project.emulator_path)) {
        addIssue(&issues, IssueSeverity::Error, "emulator_path",
                 trf(lang, "err.path_unsafe", project.emulator_path.c_str()));
    }

    std::set<std::string> outputs;
    for (size_t i = 0; i < project.assets.size(); ++i) {
        const AssetEntry& asset = project.assets[i];
        const std::string prefix = "assets[" + std::to_string(i) + "].";
        if (!isSafeRelativePath(asset.source)) {
            addIssue(&issues, IssueSeverity::Error, prefix + "source",
                     trf(lang, "err.path_unsafe", asset.source.c_str()));
        }
        if (!isSafeRelativePath(asset.output)) {
            addIssue(&issues, IssueSeverity::Error, prefix + "output",
                     trf(lang, "err.path_unsafe", asset.output.c_str()));
        }
        if (fileExtension(asset.output) != extForAsset(asset)) {
            addIssue(&issues, IssueSeverity::Error, prefix + "output",
                     trf(lang, "err.asset_type", asset.output.c_str()));
        }
        if (!sourceExtMatches(asset.type, fileExtension(asset.source))) {
            addIssue(&issues, IssueSeverity::Error, prefix + "source",
                     trf(lang, "err.asset_ext", fileExtension(asset.source).c_str()));
        }
        if (!outputs.insert(asset.output).second) {
            addIssue(&issues, IssueSeverity::Error, prefix + "output",
                     trf(lang, "err.asset_duplicate", asset.output.c_str()));
        }
        if (check_paths) {
            const std::string source_path = resolvedPath(root, asset.source);
            const std::string output_path = resolvedPath(root, asset.output);
            if (!isRegularFile(source_path)) {
                addIssue(&issues, IssueSeverity::Warning, prefix + "source",
                         trf(lang, "err.path_missing", source_path.c_str()));
            }
            if (!isRegularFile(output_path)) {
                addIssue(&issues, IssueSeverity::Warning, prefix + "output",
                         trf(lang, "err.path_missing", output_path.c_str()));
            }
        }
    }

    if (check_paths) {
        if (!project.sdk_path.empty()) {
            const std::string sdk_path = resolvedPath(root, project.sdk_path);
            if (!isDirectory(sdk_path)) {
                addIssue(&issues, IssueSeverity::Warning, "sdk_path",
                         trf(lang, "err.path_missing", sdk_path.c_str()));
            }
        }
        if (!project.emulator_path.empty()) {
            const std::string emulator = resolvedPath(root, project.emulator_path);
            if (!isRegularFile(emulator)) {
                addIssue(&issues, IssueSeverity::Warning, "emulator_path",
                         trf(lang, "err.path_missing", emulator.c_str()));
            }
        }
    }
    return issues;
}

ProjectLoadResult loadProject(const std::string& path, Lang lang)
{
    ProjectLoadResult result;
    std::string file = path;
    if (isDirectory(file)) {
        file = projectFilePath(file);
    } else if (fileExtension(file) != ".rpeproject") {
        // Путь без расширения трактуется как каталог проекта.
        file = projectFilePath(file);
    }
    if (!isRegularFile(file)) {
        result.error = trf(lang, "err.project_not_found", file.c_str());
        return result;
    }
    std::string text;
    std::string read_error;
    if (!readTextFile(file, &text, &read_error)) {
        result.error = trf(lang, "err.project_read", read_error.c_str());
        return result;
    }
    JsonValue root;
    JsonParseError parse_error;
    if (!jsonParse(text, &root, &parse_error)) {
        result.error = trf(lang, "err.project_json", fileName(file).c_str(), parse_error.line,
                           parse_error.column, parse_error.message.c_str());
        return result;
    }
    if (!root.isObject()) {
        result.error = tr(lang, "err.root_type");
        return result;
    }

    std::vector<ValidationIssue> issues;
    const std::set<std::string> allowed = {
        "schema",   "name",   "engine_version", "sdk_path",   "target",
        "platform", "assets", "build_dir",      "run_target", "emulator_path"};
    rejectUnknownFields(root, allowed, "", &issues, lang);

    Project project;
    const JsonValue* schema = root.find("schema");
    if (schema == nullptr) {
        addIssue(&issues, IssueSeverity::Error, "schema",
                 trf(lang, "err.field_required", "schema"));
    } else {
        int64_t value = 0;
        if (!schema->asInt64(&value)) {
            addIssue(&issues, IssueSeverity::Error, "schema",
                     trf(lang, "err.field_type", "schema"));
        } else {
            project.schema = static_cast<int>(value);
        }
    }
    getString(root, "name", true, &project.name, &issues, lang);
    getString(root, "engine_version", true, &project.engine_version, &issues, lang);
    getString(root, "sdk_path", true, &project.sdk_path, &issues, lang);
    getString(root, "target", true, &project.target, &issues, lang);
    getString(root, "build_dir", false, &project.build_dir, &issues, lang);
    getString(root, "run_target", false, &project.run_target, &issues, lang);
    getString(root, "emulator_path", false, &project.emulator_path, &issues, lang);

    std::string platform;
    if (getString(root, "platform", true, &platform, &issues, lang)) {
        if (!parsePlatform(platform, &project.platform)) {
            addIssue(&issues, IssueSeverity::Error, "platform",
                     trf(lang, "err.platform", platform.c_str()));
        }
    }

    const JsonValue* assets = root.find("assets");
    if (assets == nullptr) {
        addIssue(&issues, IssueSeverity::Error, "assets",
                 trf(lang, "err.field_required", "assets"));
    } else if (!assets->isArray()) {
        addIssue(&issues, IssueSeverity::Error, "assets", trf(lang, "err.field_type", "assets"));
    } else {
        const std::set<std::string> asset_allowed = {
            "source", "output", "type",         "size",           "source_size",
            "width",  "height", "vertex_count", "triangle_count", "texture_id"};
        for (size_t i = 0; i < assets->array().size(); ++i) {
            const JsonValue& item = assets->array()[i];
            const std::string prefix = "assets[" + std::to_string(i) + "].";
            if (!item.isObject()) {
                addIssue(&issues, IssueSeverity::Error, prefix,
                         trf(lang, "err.field_type", "assets[]"));
                continue;
            }
            rejectUnknownFields(item, asset_allowed, prefix, &issues, lang);
            AssetEntry asset;
            getString(item, "source", true, &asset.source, &issues, lang);
            getString(item, "output", true, &asset.output, &issues, lang);
            std::string type;
            if (getString(item, "type", true, &type, &issues, lang)) {
                if (!parseAssetType(type, &asset.type)) {
                    addIssue(&issues, IssueSeverity::Error, prefix + "type",
                             trf(lang, "err.asset_type", type.c_str()));
                }
            }
            getUint(item, "size", &asset.size, &issues, lang);
            getUint(item, "source_size", &asset.source_size, &issues, lang);
            uint64_t value = 0;
            if (getUint(item, "width", &value, &issues, lang)) {
                asset.width = static_cast<uint32_t>(value);
            }
            value = 0;
            if (getUint(item, "height", &value, &issues, lang)) {
                asset.height = static_cast<uint32_t>(value);
            }
            value = 0;
            if (getUint(item, "vertex_count", &value, &issues, lang)) {
                asset.vertex_count = static_cast<uint32_t>(value);
            }
            value = 0;
            if (getUint(item, "triangle_count", &value, &issues, lang)) {
                asset.triangle_count = static_cast<uint32_t>(value);
            }
            value = 0;
            if (getUint(item, "texture_id", &value, &issues, lang)) {
                asset.texture_id = static_cast<uint32_t>(value);
            }
            project.assets.push_back(asset);
        }
    }

    const std::string root_dir = projectRoot(file);
    std::vector<ValidationIssue> validation = validateProject(project, root_dir, false, lang);
    issues.insert(issues.end(), validation.begin(), validation.end());

    bool has_error = false;
    for (size_t i = 0; i < issues.size(); ++i) {
        if (issues[i].severity == IssueSeverity::Error) {
            has_error = true;
            break;
        }
    }
    result.issues = issues;
    if (has_error) {
        result.error =
            issues.empty() ? trf(lang, "err.project_read", file.c_str()) : issues.front().message;
        return result;
    }
    result.project = project;
    result.ok = true;
    return result;
}

bool saveProject(const std::string& path, const Project& project, std::string* error, Lang lang)
{
    std::string file = path;
    if (fileExtension(file) == ".rpeproject") {
        // Путь уже указывает на файл проекта.
    } else {
        file = projectFilePath(path);
    }
    const std::string root = projectRoot(file);
    std::vector<ValidationIssue> issues = validateProject(project, root, false, lang);
    for (size_t i = 0; i < issues.size(); ++i) {
        if (issues[i].severity == IssueSeverity::Error) {
            if (error != nullptr) *error = issues[i].message;
            return false;
        }
    }

    JsonValue root_json = JsonValue::makeObject();
    root_json.append("schema", JsonValue::makeInt(project.schema));
    root_json.append("name", JsonValue::makeString(project.name));
    root_json.append("engine_version", JsonValue::makeString(project.engine_version));
    root_json.append("sdk_path", JsonValue::makeString(project.sdk_path));
    root_json.append("target", JsonValue::makeString(project.target));
    root_json.append("platform", JsonValue::makeString(platformCode(project.platform)));
    JsonValue assets = JsonValue::makeArray();
    for (size_t i = 0; i < project.assets.size(); ++i) {
        const AssetEntry& asset = project.assets[i];
        JsonValue item = JsonValue::makeObject();
        item.append("source", JsonValue::makeString(asset.source));
        item.append("output", JsonValue::makeString(asset.output));
        item.append("type", JsonValue::makeString(assetTypeCode(asset.type)));
        item.append("size", JsonValue::makeInt(static_cast<int64_t>(asset.size)));
        item.append("source_size", JsonValue::makeInt(static_cast<int64_t>(asset.source_size)));
        item.append("width", JsonValue::makeInt(asset.width));
        item.append("height", JsonValue::makeInt(asset.height));
        item.append("vertex_count", JsonValue::makeInt(asset.vertex_count));
        item.append("triangle_count", JsonValue::makeInt(asset.triangle_count));
        item.append("texture_id", JsonValue::makeInt(asset.texture_id));
        assets.push(item);
    }
    root_json.append("assets", assets);
    root_json.append("build_dir", JsonValue::makeString(project.build_dir));
    root_json.append("run_target", JsonValue::makeString(project.run_target));
    root_json.append("emulator_path", JsonValue::makeString(project.emulator_path));

    return writeTextFileAtomic(file, jsonSerialize(root_json), error);
}

std::string resolveSdkPath(const std::string& root, const Project& project)
{
    return resolvedPath(root, project.sdk_path);
}

std::string resolveBuildDir(const std::string& root, const Project& project)
{
    return resolvedPath(root, project.build_dir);
}

std::string resolveAssetSource(const std::string& root, const AssetEntry& asset)
{
    return resolvedPath(root, asset.source);
}

std::string resolveAssetOutput(const std::string& root, const AssetEntry& asset)
{
    return resolvedPath(root, asset.output);
}

std::string runTargetName(const Project& project)
{
    return project.run_target.empty() ? project.target : project.run_target;
}

Project makeProjectTemplate(const std::string& name, const std::string& engine_version,
                            const std::string& sdk_path, Platform platform,
                            const std::string& target, const std::string& run_target)
{
    Project project;
    project.schema = 1;
    project.name = name;
    project.engine_version = engine_version;
    project.sdk_path = sdk_path;
    project.platform = platform;
    project.target = target;
    project.run_target = run_target;
    project.build_dir = "build";
    return project;
}

}  // namespace ui
}  // namespace rpe
