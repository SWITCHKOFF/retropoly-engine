// SPDX-License-Identifier: MIT
// RetroPolyEngine — tools/asset_converter/converter_common.h
//
// Общие коды результата конвертера (host-only инструмент; Спринт 9–10).
// Контракт ошибок — статус-код без исключений (Failures — DET code; §1).
// Классы кода загрузчика (AssetDiag, src/assets/assets.h) сюда не входят:
// они о чтении рантаймом, здесь — о конвертации.
#pragma once

/** Детерминированные диагностические коды конвертера. Порядок — контракт. */
typedef enum
{
    CONVERT_OK = 0,              /**< Конвертация и запись прошла. */
    CONVERT_ERR_OPEN_INPUT,      /**< Файл не найден/не декодируется (cgltf/stb). */
    CONVERT_ERR_PARSE,           /**< Структура разломана: validate/атрибуты/индексы. */
    CONVERT_ERR_UNSUPPORTED,     /**< MVP-ограничения: dims, count, mode, типы. */
    CONVERT_ERR_WRITE_OUTPUT,    /**< fopen/fwrite выходного файла отказа. */
    CONVERT_ERR_ALLOCATION,      /**< Хост-нехватка памяти инструмента. */
    CONVERT_ERR_COUNT
} ConverterDiag;
