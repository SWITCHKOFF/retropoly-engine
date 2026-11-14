// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/ui_client/core/i18n.h
//
// Локализация UI client (RU/EN, только строки UI; без внешних сервисов).
#pragma once

#include <string>

namespace rpe {
namespace ui {

/** Язык интерфейса. */
enum class Lang
{
    Ru,
    En
};

/** Разобрать код языка ("ru"/"en"); неизвестный → Ru. */
Lang parseLang(const std::string& code);
/** Код языка для settings.json. */
const char* langCode(Lang lang);

/** Перевод по ключу; неизвестный ключ возвращается как есть. */
const char* tr(Lang lang, const char* key);

/** Перевод с printf-форматированием. */
std::string trf(Lang lang, const char* key, ...);

}  // namespace ui
}  // namespace rpe
