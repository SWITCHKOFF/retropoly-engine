// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/version.h
//
// SemVer и совместимость UI ↔ SDK (Спринт 35–36, неделя 70).
#pragma once

#include <string>

namespace rpe {
namespace ui {

/** Версия UI client, зафиксированная при сборке. */
const char* uiVersion();

/** Разобранная semver-версия X.Y.Z (суффиксы не допускаются). */
struct SemVer
{
    int major = 0;
    int minor = 0;
    int patch = 0;
};

/** Разобрать строгую версию "X.Y.Z". */
bool parseSemVer(const std::string& text, SemVer* out);

/** Сравнение: <0, 0, >0. */
int compareSemVer(const SemVer& left, const SemVer& right);

/** Строковое представление. */
std::string semVerToString(const SemVer& version);

/** Результат проверки совместимости UI и SDK. */
enum class VersionCompatibility
{
    Compatible, /**< major/minor совпадают (patch может отличаться). */
    PatchMismatch, /**< major/minor совпадают, patch отличается — warning. */
    MinorMismatch, /**< отличается minor — UI отказывается работать. */
    MajorMismatch, /**< отличается major — UI отказывается работать. */
    Invalid /**< версия не разбирается. */
};

/**
 * Проверить совместимость версии UI и SDK.
 *
 * Политика v0.2.1: UI работает только с SDK той же major/minor серии;
 * отличие patch — предупреждение (SDK/UI собираются из одного тега, но
 * допускается hotfix-пересборка одной стороны).
 *
 * @param ui_version Версия UI (uiVersion()).
 * @param sdk_version Версия из sdk_manifest.json.
 * @param detail Человекочитаемое пояснение (может быть nullptr).
 */
VersionCompatibility checkVersionCompatibility(const std::string& ui_version,
                                               const std::string& sdk_version, std::string* detail);

}  // namespace ui
}  // namespace rpe
