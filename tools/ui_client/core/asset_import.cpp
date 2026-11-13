// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/asset_import.cpp
#include "asset_import.h"

#include <fstream>
#include <sstream>
#include <vector>

#include "fs_util.h"
#include "process.h"

namespace rpe {
namespace ui {

namespace {

uint16_t readLe16(const unsigned char* data)
{
    return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
}

uint32_t readLe32(const unsigned char* data)
{
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

std::string joinLines(const std::vector<std::string>& lines)
{
    std::string result;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) result.push_back('\n');
        result.append(lines[i]);
    }
    return result;
}

} // namespace

bool assetTypeFromExtension(const std::string& extension, AssetType* out)
{
    if (out == nullptr) return false;
    const std::string ext = toLowerAscii(extension);
    if (ext == ".gltf" || ext == ".glb") {
        *out = AssetType::Mesh;
        return true;
    }
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
        *out = AssetType::Texture;
        return true;
    }
    return false;
}

bool probeAssetFile(const std::string& path, const std::string& project_root, AssetEntry* entry,
                    std::string* error, Lang lang)
{
    if (entry == nullptr) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (error != nullptr) *error = trf(lang, "err.path_missing", path.c_str());
        return false;
    }
    unsigned char header[24] = {0};
    input.read(reinterpret_cast<char*>(header), sizeof(header));
    const std::streamsize got = input.gcount();
    const uint64_t size = fileSize(path);
    if (entry->type == AssetType::Mesh) {
        if (got < 24) {
            if (error != nullptr) *error = "mesh: короткий заголовок";
            return false;
        }
        const uint32_t magic = readLe32(header);
        const uint32_t version = readLe32(header + 4);
        const uint32_t vertices = readLe32(header + 8);
        const uint32_t indices = readLe32(header + 12);
        const uint32_t texture_id = readLe32(header + 16);
        if (magic != 0x4853454Du || version != 1u) {
            if (error != nullptr) *error = "mesh: неверный magic/version";
            return false;
        }
        const uint64_t expected =
            24u + static_cast<uint64_t>(vertices) * 21u + static_cast<uint64_t>(indices) * 6u;
        if (size != expected) {
            if (error != nullptr) *error = "mesh: размер не соответствует заголовку";
            return false;
        }
        entry->vertex_count = vertices;
        entry->triangle_count = indices;
        entry->texture_id = texture_id;
        entry->size = size;
        (void)project_root;
        return true;
    }

    if (got < 20) {
        if (error != nullptr) *error = "tex: короткий заголовок";
        return false;
    }
    const uint32_t magic = readLe32(header);
    const uint32_t version = readLe32(header + 4);
    const uint16_t width = readLe16(header + 8);
    const uint16_t height = readLe16(header + 10);
    const uint32_t palette_offset = readLe32(header + 12);
    const uint32_t pixel_offset = readLe32(header + 16);
    if (magic != 0x00584554u || version != 1u) {
        if (error != nullptr) *error = "tex: неверный magic/version";
        return false;
    }
    if (width != 64u && width != 128u && width != 256u) {
        if (error != nullptr) *error = "tex: недопустимая ширина";
        return false;
    }
    if (height != 64u && height != 128u && height != 256u) {
        if (error != nullptr) *error = "tex: недопустимая высота";
        return false;
    }
    const uint64_t expected =
        static_cast<uint64_t>(pixel_offset) + static_cast<uint64_t>(width) * height;
    if (size < expected || size < static_cast<uint64_t>(palette_offset) + 512u) {
        if (error != nullptr) *error = "tex: размер меньше заявленного";
        return false;
    }
    entry->width = width;
    entry->height = height;
    entry->size = size;
    return true;
}

AssetImportResult importAsset(const std::string& project_root, Project* project, const SdkInfo& sdk,
                              const std::string& source_path, Lang lang)
{
    AssetImportResult result;
    if (project == nullptr) {
        result.error = tr(lang, "err.no_project");
        return result;
    }
    if (!sdk.valid || sdk.converter_path.empty()) {
        result.error = trf(lang, "err.converter_missing", sdk.path.c_str());
        return result;
    }
    if (!isRegularFile(source_path)) {
        result.error = trf(lang, "err.source_missing", source_path.c_str());
        return result;
    }
    AssetType type = AssetType::Mesh;
    if (!assetTypeFromExtension(fileExtension(source_path), &type)) {
        result.error = trf(lang, "err.asset_ext", fileExtension(source_path).c_str());
        return result;
    }

    const std::string source_dir = joinPath(project_root, "assets/source");
    const std::string build_dir = joinPath(project_root, "assets/build");
    std::string ensure_error;
    if (!createDirectories(source_dir, &ensure_error) ||
        !createDirectories(build_dir, &ensure_error)) {
        result.error = ensure_error;
        return result;
    }

    // Источник внутри assets/source не копируем повторно.
    std::string relative_source;
    const std::string absolute_source = absolutePath(source_path);
    if (pathIsInside(source_dir, absolute_source)) {
        relative_source =
            normalizePath(absolute_source.substr(absolutePath(source_dir).size() + 1));
        relative_source = "assets/source/" + relative_source;
    } else {
        std::string target_source = joinPath(source_dir, sanitizeFileName(fileName(source_path)));
        if (pathExists(target_source)) {
            std::string original;
            std::string existing;
            if (!readTextFile(absolute_source, &original, nullptr) ||
                !readTextFile(target_source, &existing, nullptr) || original != existing) {
                target_source = uniquePath(target_source);
            }
        }
        if (!pathExists(target_source)) {
            if (!copyFile(absolute_source, target_source, &ensure_error)) {
                result.error = ensure_error;
                return result;
            }
        }
        relative_source = "assets/source/" + fileName(target_source);
    }

    // Повторный импорт того же источника переиспользует output.
    std::string relative_output;
    for (size_t i = 0; i < project->assets.size(); ++i) {
        if (project->assets[i].source == relative_source) {
            relative_output = project->assets[i].output;
            break;
        }
    }
    if (relative_output.empty()) {
        const std::string extension = type == AssetType::Mesh ? ".mesh" : ".tex";
        const std::string candidate =
            joinPath(build_dir, sanitizeFileName(fileStem(source_path)) + extension);
        relative_output = "assets/build/" + fileName(uniquePath(candidate));
    }
    const std::string output_path = joinPath(project_root, relative_output);

    ProcessSpec spec;
    spec.argv.push_back(sdk.converter_path);
    spec.argv.push_back(absolute_source);
    spec.argv.push_back(output_path);
    spec.working_directory = project_root;
    ProcessRunner runner;
    std::string start_error;
    if (!runner.start(spec, &start_error)) {
        result.error = trf(lang, "err.process_start", sdk.converter_path.c_str());
        return result;
    }
    runner.wait(-1);
    result.log = joinLines(runner.takeLines());
    if (runner.exitCode() != 0) {
        result.error = trf(lang, "err.import_failed", runner.exitCode());
        return result;
    }
    if (!isRegularFile(output_path)) {
        result.error = trf(lang, "err.import_output", "файл не создан");
        return result;
    }

    AssetEntry entry;
    entry.source = relative_source;
    entry.output = relative_output;
    entry.type = type;
    entry.source_size = fileSize(absolute_source);
    std::string probe_error;
    if (!probeAssetFile(output_path, project_root, &entry, &probe_error, lang)) {
        result.error = trf(lang, "err.import_output", probe_error.c_str());
        return result;
    }
    entry.size = fileSize(output_path);

    bool replaced = false;
    for (size_t i = 0; i < project->assets.size(); ++i) {
        if (project->assets[i].source == entry.source ||
            project->assets[i].output == entry.output) {
            project->assets[i] = entry;
            replaced = true;
            break;
        }
    }
    if (!replaced) project->assets.push_back(entry);

    result.ok = true;
    result.entry = entry;
    result.output = output_path;
    return result;
}

bool removeAsset(const std::string& project_root, Project* project, size_t index,
                 bool delete_output, std::string* error, Lang lang)
{
    if (project == nullptr || index >= project->assets.size()) {
        if (error != nullptr) *error = tr(lang, "err.no_project");
        return false;
    }
    const AssetEntry entry = project->assets[index];
    if (delete_output) {
        const std::string output = resolveAssetOutput(project_root, entry);
        if (!pathIsInside(project_root, output)) {
            if (error != nullptr) *error = trf(lang, "err.path_unsafe", output.c_str());
            return false;
        }
        if (!removeFile(output, error)) return false;
    }
    project->assets.erase(project->assets.begin() + static_cast<long>(index));
    return true;
}

}  // namespace ui
}  // namespace rpe
