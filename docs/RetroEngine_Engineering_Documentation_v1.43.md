---
title: "RetroEngine — инженерная документация"
version: "1.43"
date: "2026-09-07"
status: "Инженерная спецификация MVP"
language: "ru"
---

# RetroEngine — Engineering Documentation

> Ретро-движок для консолей 5-го поколения и PC.
>
> Инженерная документация: требования, архитектура, toolchain, реализация, QA, релиз и сопровождение.

| Параметр | Значение |
| --- | --- |
| Версия | 1.43 |
| Дата | 2026-09-07 |
| Основной MVP | PC (SDL3 + OpenGL) + Sony PlayStation 1 |
| Будущие платформы | Sega Saturn, Nintendo 64, Atari Jaguar, Panasonic 3DO |
| Статус | Инженерная спецификация MVP |

## Нормативная база

**Статус:** нормативная инженерная база MVP. Каноническая версия: **1.43**.

> **Каноническое правило.** `CMakePresets.json` — единственная точка входа для сборки контрибьютора. Для каждой сборки выбирается ровно одна конфигурация: `pc-debug`, `pc-release`, `pc-debug-render` или `ps1-release`.

> **Каноническое правило.** Форматы `.mesh/.tex v1` определены на уровне байтов, используют little-endian и не зависят от ABI. Padding структур хоста не сериализуется.

> **Каноническое правило.** Порядок рендерера PS1: `TRANSFORM → CLIP → CULL → SORT → PACK → SUBMIT`.

> **Каноническое правило.** `W0` — детерминированная conformance-нагрузка; `W1` — release-performance-нагрузка. Все заявления о FPS, памяти и визуальном результате должны указывать workload и целевое окружение.

> **Каноническое правило.** Переполнение renderer, command buffer, OT и arena является явной ошибкой с диагностикой. Примитивы не должны отбрасываться молча.

> **Справочные данные будущих платформ.** Параметры Saturn, N64, Jaguar и 3DO являются reference material и подлежат повторной проверке перед реализацией соответствующих backend.

# Содержание

- [ЭТАП 1: ИНИЦИАЦИЯ И БИЗНЕС-АНАЛИЗ](#этап-1-инициация-и-бизнес-анализ)
 - [Зачем нам это?](#зачем-нам-это)
 - [Целевая аудитория](#целевая-аудитория)
 - [Метрики успеха](#метрики-успеха)
 - [Анализ юридических рисков](#анализ-юридических-рисков)
 - [Оценка технической осуществимости](#оценка-технической-осуществимости)
- [ЭТАП 2: УСТАВ ПРОЕКТА И РЕСУРСНОЕ ПЛАНИРОВАНИЕ](#этап-2-устав-проекта-и-ресурсное-планирование)
 - [Команда проекта](#команда-проекта)
 - [Что НЕ нужно для MVP](#что-не-нужно-для-mvp)
 - [Оборудование для MVP](#оборудование-для-mvp)
 - [Скоуп MVP (v0.1.0)](#скоуп-mvp-v010)
- [ЭТАП 3: СБОР И АНАЛИЗ ТРЕБОВАНИЙ (REQUIREMENTS ENGINEERING)](#этап-3-сбор-и-анализ-требований-requirements-engineering)
 - [1. ВВЕДЕНИЕ](#1-введение)
 - [2. АППАРАТНЫЕ ОГРАНИЧЕНИЯ ИГРОВЫХ ПЛАТФОРМ](#2-аппаратные-ограничения-игровых-платформ)
 - [3. ТРЕБОВАНИЯ К PC](#3-требования-к-pc)
 - [4. НЕФУНКЦИОНАЛЬНЫЕ ТРЕБОВАНИЯ](#4-нефункциональные-требования)
 - [5. ФУНКЦИОНАЛЬНЫЕ ТРЕБОВАНИЯ MVP](#5-функциональные-требования-mvp)
 - [6. ОГРАНИЧЕНИЯ И КОМПРОМИССЫ](#6-ограничения-и-компромиссы)
 - [7. ТАБЛИЦЫ СРАВНЕНИЯ КОНСОЛЕЙ](#7-таблицы-сравнения-консолей)
 - [8. РЕКОМЕНДАЦИИ ПО РЕАЛИЗАЦИИ](#8-рекомендации-по-реализации)
- [ЭТАП 4: АРХИТЕКТУРА И ПРОЕКТИРОВАНИЕ (SYSTEM DESIGN)](#этап-4-архитектура-и-проектирование-system-design)
 - [1. ВВЕДЕНИЕ И ЦЕЛИ ПРОЕКТИРОВАНИЯ](#1-введение-и-цели-проектирования)
- [ЭТАП 5: ПОДГОТОВКА ИНФРАСТРУКТУРЫ И TOOLCHAIN (DEVOPS & CI/CD)](#этап-5-подготовка-инфраструктуры-и-toolchain-devops-cicd)
 - [1. ВВЕДЕНИЕ И ЦЕЛИ DEVOPS](#1-введение-и-цели-devops)
 - [2. АДАПТАЦИЯ КОРПОРАТИВНОГО ПОДХОДА ПОД СОЛО-РАЗРАБОТЧИКА](#2-адаптация-корпоративного-подхода-под-соло-разработчика)
 - [3. ЛОКАЛЬНАЯ СРЕДА РАЗРАБОТКИ (WINDOWS 11)](#3-локальная-среда-разработки-windows-11)
 - [4. СИСТЕМА СБОРКИ (CMAKE + NINJA)](#4-система-сборки-cmake-ninja)
- [ЭТАП 6: НЕПОСРЕДСТВЕННАЯ РАЗРАБОТКА (IMPLEMENTATION)](#этап-6-непосредственная-разработка-implementation)
 - [1. ВВЕДЕНИЕ И АДАПТАЦИЯ ПОД СОЛО-РАЗРАБОТЧИКА](#1-введение-и-адаптация-под-соло-разработчика)
 - [2. Нужно время на изучение PSn00bSDK, GTE, GPU PS1.](#2-нужно-время-на-изучение-psn00bsdk-gte-gpu-ps1)
 - [2. ОБЗОР ИТЕРАЦИЙ: MVP VS БУДУЩИЕ ВЕРСИИ](#2-обзор-итераций-mvp-vs-будущие-версии)
 - [3. ДЕТАЛЬНЫЙ ПЛАН СПРИНТОВ MVP (PC + PS1)](#3-детальный-план-спринтов-mvp-pc-ps1)
 - [4. БУДУЩИЕ ИТЕРАЦИИ (ПОСЛЕ MVP)](#4-будущие-итерации-после-mvp)
 - [5. DEFINITION OF DONE ДЛЯ КАЖДОГО СПРИНТА](#5-definition-of-done-для-каждого-спринта)
 - [6. РИСКИ И МИТИГАЦИЯ](#6-риски-и-митигация)
 - [7. КРИТЕРИИ ЗАВЕРШЕНИЯ ЭТАПА](#7-критерии-завершения-этапа)
 - [ИТОГИ ЭТАПА 6](#итоги-этапа-6)
 - [1. РАБОЧИЙ ДВИЖОК v0.1.0](#1-рабочий-движок-v010)
 - [2. ПУБЛИЧНЫЙ РЕПОЗИТОРИЙ](#2-публичный-репозиторий)
 - [3. СООБЩЕСТВО](#3-сообщество)
 - [4. ОПЫТ](#4-опыт)
- [ЭТАП 7: ТЕСТИРОВАНИЕ И QA (TESTING & QUALITY ASSURANCE)](#этап-7-тестирование-и-qa-testing-quality-assurance)
 - [1. ВВЕДЕНИЕ И АДАПТАЦИЯ ПОД СОЛО-РАЗРАБОТЧИКА](#1-введение-и-адаптация-под-соло-разработчика)
 - [DuckStation is the primary PS1 emulator for MVP smoke testing; no universal percentage accuracy claim is made.](#duckstation-is-the-primary-ps1-emulator-for-mvp-smoke-testing-no-universal-percentage-accuracy-claim-is-made)
 - [CI/CD reduces repeated manual verification; no fixed productivity percentage is assumed.](#cicd-reduces-repeated-manual-verification-no-fixed-productivity-percentage-is-assumed)
 - [2. ПИРАМИДА ТЕСТИРОВАНИЯ ДЛЯ РЕТРО-ДВИЖКА](#2-пирамида-тестирования-для-ретро-движка)
 - [3. UNIT-ТЕСТЫ](#3-unit-тесты)
 - [4. INTEGRATION-ТЕСТЫ](#4-integration-тесты)
 - [5. HARDWARE QA (АДАПТИРОВАНО ПОД ЭМУЛЯТОРЫ)](#5-hardware-qa-адаптировано-под-эмуляторы)
 - [1. Тайминги DMA](#1-тайминги-dma)
 - [3. GPU-баги](#3-gpu-баги)
 - [5. CD-ROM](#5-cd-rom)
 - [2. Программатор EPROM/Flash ($50-100)](#2-программатор-epromflash-50-100)
 - [6. STRESS-TESTING](#6-stress-testing)
 - [7. REGRESSION TESTING](#7-regression-testing)
- [ЭТАП 8: РЕЛИЗ, ДОКУМЕНТАЦИЯ И ПОДДЕРЖКА (RELEASE & MAINTENANCE)](#этап-8-релиз-документация-и-поддержка-release-maintenance)
 - [1. ВВЕДЕНИЕ И АДАПТАЦИЯ ПОД СОЛО-РАЗРАБОТЧИКА](#1-введение-и-адаптация-под-соло-разработчика)
 - [2. ДОКУМЕНТАЦИЯ](#2-документация)
 - [3. УПАКОВКА И ДИСТРИБУЦИЯ](#3-упаковка-и-дистрибуция)
 - [4. POST-RELEASE SUPPORT](#4-post-release-support)
 - [4. POST-RELEASE SUPPORT (ПРОДОЛЖЕНИЕ)](#4-post-release-support-продолжение)
 - [5. МАРКЕТИНГ И ПРОДВИЖЕНИЕ](#5-маркетинг-и-продвижение)
 - [YouTube Video Link](#youtube-video-link)
 - [6. МЕТРИКИ УСПЕХА ПОСЛЕ РЕЛИЗА](#6-метрики-успеха-после-релиза)
 - [7. ПЛАН ПОДДЕРЖКИ](#7-план-поддержки)
 - [8. РИСКИ И МИТИГАЦИЯ](#8-риски-и-митигация)
 - [РИСК 1: Нет контрибьюторов после релиза](#риск-1-нет-контрибьюторов-после-релиза)
 - [РИСК 2: Критические баги после релиза](#риск-2-критические-баги-после-релиза)
 - [РИСК 3: Выгорание от поддержки](#риск-3-выгорание-от-поддержки)
 - [РИСК 4: Токсичное сообщество](#риск-4-токсичное-сообщество)
 - [РИСК 5: Форк проекта](#риск-5-форк-проекта)
 - [РИСК 6: Конкуренты (другие движки)](#риск-6-конкуренты-другие-движки)
 - [9. КРИТЕРИИ ЗАВЕРШЕНИЯ ЭТАПА](#9-критерии-завершения-этапа)
 - [ДОКУМЕНТАЦИЯ](#документация)
 - [ИТОГИ ЭТАПА 8](#итоги-этапа-8)
- [DOCUMENT STATUS AND CANONICALITY](#document-status-and-canonicality)
- [ci/normative_contracts.yaml](#cinormative_contractsyaml)
- [CI contract](#ci-contract)
- [scripts/verify_release_contract.py (host-side validator)](#scriptsverify_release_contractpy-host-side-validator)
- [root CMakeLists.txt (canonical architecture)](#root-cmakeliststxt-canonical-architecture)
- [Asset converter is a host tool; it is never compiled with the PS1 cross compiler.](#asset-converter-is-a-host-tool-it-is-never-compiled-with-the-ps1-cross-compiler)
- [MVP PS1 builds consume versioned/generated example assets from assets/build/.](#mvp-ps1-builds-consume-versionedgenerated-example-assets-from-assetsbuild)
- [Canonical PC runtime targets are the documented demo executables.](#canonical-pc-runtime-targets-are-the-documented-demo-executables)
- [The release workflow packages rotating_cube as the reference PC runtime artifact.](#the-release-workflow-packages-rotating_cube-as-the-reference-pc-runtime-artifact)
- [MSVC (Windows PC) add_compile_options(/W4 /WX)](#msvc-windows-pc-add_compile_optionsw4-wx)
- [GCC / Clang](#gcc-clang)
- [root CMakeLists.txt (canonical architecture)](#root-cmakeliststxt-canonical-architecture)
- [Asset converter is a host tool; it is never compiled with the PS1 cross compiler.](#asset-converter-is-a-host-tool-it-is-never-compiled-with-the-ps1-cross-compiler)
- [MVP PS1 builds consume versioned/generated example assets from assets/build/.](#mvp-ps1-builds-consume-versionedgenerated-example-assets-from-assetsbuild)
- [Canonical PC runtime targets are the documented demo executables.](#canonical-pc-runtime-targets-are-the-documented-demo-executables)
- [The release workflow packages rotating_cube as the reference PC runtime artifact.](#the-release-workflow-packages-rotating_cube-as-the-reference-pc-runtime-artifact)
- [Root PC configuration](#root-pc-configuration)
- [.github/workflows/regression.yml](#githubworkflowsregressionyml)
- [Редакционная справка по оформлению](#редакционная-справка-по-оформлению)
- [REVISION SOURCES --- ТЕХНИЧЕСКАЯ ПРОВЕРКА](#revision-sources-----техническая-проверка)
- [REFERENCE SOURCES FOR HARDWARE AND BUILD CONTRACTS](#reference-sources-for-hardware-and-build-contracts)
- [REVISION 1.43 --- EXECUTABLE CONTRACT CONSOLIDATION](#revision-143-----executable-contract-consolidation)

# ЭТАП 1: ИНИЦИАЦИЯ И БИЗНЕС-АНАЛИЗ

## Зачем нам это?

> Образовательная ценность: Движок позволит разработчикам изучать архитектуру ретро-консолей через современный, документированный код.
>
> Сообщество homebrew: В 2026 году homebrew-сцена переживает ренессанс. Движок снизит порог входа для новых разработчиков.
>
> Физические релизы: Инди-студии выпускают игры на картриджах/CD. Движок ускорит разработку.
>
> Личный бренд: Open-source проект с поддержкой ретро-консолей --- это уникальный портфолио для соло-разработчика.
>
> Контрибьюторы: Если проект успешен, другие разработчики присоединятся, что ускорит развитие.

## Целевая аудитория

> Primary: Инди-разработчики, которые хотят сделать ретро-игру на PS1 без глубокого знания ассемблера.
>
> Secondary: Энтузиасты homebrew, изучающие архитектуру консолей.
>
> Tertiary: Студии, выпускающие физические релизы на PS1 (cartridges/CDs).

## Метрики успеха

> Количество звезд на GitHub (цель: 100+ за первый год).
>
> Количество форков и контрибьюторов (цель: 3+ контрибьюторов за первый год).
>
> Количество игр, сделанных на движке (цель: 3+ демоигры за первый год).
>
> Количество скачиваний релизов (цель: 500+ за первый год).

## Анализ юридических рисков

1. PS1 (Tier 1 --- MVP):

> Тулчейн: PSn00bSDK (open-source; core libraries/CMake are MPL-2.0; release package includes separately licensed components). Риск: низкий

2. PC (SDL3):

> Тулчейн: SDL3 (zlib license); OpenGL --- API specification, runtime implementation is supplied by the host GPU driver/OS. Риск: низкий.

3. Остальные консоли (Tier 2-3 --- будущие версии):

> Saturn: Jo Engine (open-source, MIT). Риск низкий.
>
> N64: libdragon (open-source, MIT). Риск низкий.
>
> Jaguar: jaguar-sdk (open-source, public domain). Риск низкий.
>
> 3DO: historical SDK/tooling requires separate legal review before redistribution or vendoring; it is not an MVP dependency. Current trademark/rights status must be verified from an authoritative source before any release claim. Position of the rights holder cited in the research follows: "Права на товарный знак 3DO и связанные с ним дизайны, а также домен 3DO.com принадлежат компании Throwback Entertainment. Эти активы не были проданы, переданы или лицензированы компании Empire Interactive, и планов по их продаже нет" [1] (<https://www.timeextension.com/news/2026/07/empire-interactive-withdraws-from-3do-project-due-to-likelihood-of-prolonged-legal-disputes?spm=a2ty_o01.29997173.0.0.20fd55fbt2Yenk>) [2] (<https://www.engadget.com/2206578/3do-revival-empire-interactive/?spm=a2ty_o01.29997173.0.0.20fd55fbt2Yenk>).

4. Single-header libraries: cgltf (MIT license). stb_image (MIT license, Public Domain). Риск: низкий.

## Оценка технической осуществимости

1. Архитектуры консолей:

> PS1: MIPS R3000A (33 МГц), 2 МБ RAM, 1 МБ VRAM, GTE (Geometry Transformation Engine). Сложность: Средняя. Есть хорошая документация, PSn00bSDK.
>
> Saturn: Dual SH-2 (28 МГц), 2 МБ RAM, VDP1 + VDP2. Сложность: Очень высокая. Два CPU, сложная графическая подсистема.
>
> N64: MIPS R4300i (93 МГц), 4 МБ RAM, RCP (Reality Co-Processor). Сложность: Высокая. Микрокод для RCP, сложная архитектура.
>
> Jaguar: 68000 CPU plus multiple programmable graphics/audio processor units (Tom/Jerry). Сложность: очень высокая. Точная processor topology и roles are backend-specific; see S7 before implementation.
>
> 3DO: ARM60 @ 12.5 МГц, 2 MiB DRAM + 1 MiB VRAM in the minimum configuration; MADAM CEL engine + matrix engine provide hardware assistance. Backend complexity: High. Proprietary historical SDK/licensing requires separate legal review; it is not an MVP dependency.

2. Реалистичная оценка для соло-разработчика:

> MVP (PC + PS1): плановый срок 40 недель (\~10 месяцев) при 10-15 часах/неделю. PS1 has strong community documentation and PSn00bSDK support.
>
> Tier 2 (Saturn или N64): Реалистично за 6-12 месяцев после MVP. Saturn --- ад с двумя CPU, N64 --- сложен из-за микрокода.
>
> Tier 3 (Jaguar, 3DO): Реалистично за 12-18 месяцев после Tier 2. Очень сложные архитектуры, мало документации, юридические проблемы.

3. Риски:

> Риск выгорания: Высокий.
>
> Риск нехватки времени: Высокий.
>
> Риск технических проблем: Средний. Некоторые баги на эмуляторах могут быть неотловимы без реального железа.
>
> Риск отсутствия контрибьюторов: Средний.

4. Митигация рисков:

> Выгорание: Работай \~10-15 часов в неделю. Делай перерывы.
>
> Нехватка времени: Фокусируйся на MVP (PC + PS1). Не распыляйся на все консоли сразу.
>
> Технические проблемы: Используй эмуляторы с хорошей отладкой (DuckStation, PCSX-Redux). Если нужен реальный hardware --- купи PS1 на eBay (Оценочная стоимость: \~$107).
>
> Отсутствие контрибьюторов: Активно продвигай проект на Reddit, Tiktok, Discord, YouTube. Пиши туториалы, делай демо.

# ЭТАП 2: УСТАВ ПРОЕКТА И РЕСУРСНОЕ ПЛАНИРОВАНИЕ

## Команда проекта

1. Ты (Solo Developer):

> Роли: Lead Engine Programmer, Toolchain Engineer, Technical Artist, QA Specialist, Documentation Writer, Community Manager.
>
> Обязанности: Архитектура движка, написание кода, настройка кросс-компиляторов, конвертация ассетов, тестирование, документация, общение с сообществом.
>
> Время: 10-15 часов в неделю (если есть основная работа) или 30-40 часов (если это фулл-тайм).

2. AI-ассистенты (Cursor AI, Claude, ChatGPT):

> Роли: Junior Programmer, Code Reviewer, Documentation Assistant.
>
> Обязанности: Автодополнение кода, объяснение сложных концепций, написание документации, помощь с отладкой, рефакторинг.
>
> Экономия времени: 30-50% на рутинных задачах.

3. Контрибьюторы сообщества (после релиза MVP):

> Роли: Toolchain Engineer (помощь с кросс-компиляторами), Technical Artist (помощь с ассетами), QA Tester (тестирование на реальном железе).
>
> Как привлечь:

- Активное продвижение на Reddit (r/ps1, r/homebrew, r/gamedev).

- Youtube\\Tiktok с демонстрациями работы движка.

- Discord-сервер проекта для общения.

- Хорошая документация и туториалы.

> Ожидания: 5-10 активных контрибьюторов в первый год (если проект успешен).

4. Менторы и эксперты (неформальные):

> Кто: Разработчики из homebrew-сообщества (PSn00bSDK, libdragon, Jo Engine).
>
> Как взаимодействовать:

- Задавать вопросы на GitHub Issues их проектов.

- Участвовать в Discord-серверах homebrew-сообщества.

- Читать их код и документацию.

> Ожидания: Помощь с сложными техническими вопросами, но не постоянная поддержка.

## Что НЕ нужно для MVP

> Отдельный Toolchain Engineer (ты сам настроишь кросс-компиляторы через PSn00bSDK).
>
> Отдельный Technical Artist (ты сам сконвертируешь ассеты через свой конвертер).
>
> Отдельный QA Hardware Specialist (у тебя нет реального железа, используешь эмуляторы).

## Оборудование для MVP

1. Уже есть (бесплатно):

> PC на Windows 11: Основная рабочая станция.
>
> Эмуляторы PS1: DuckStation (бесплатно), PCSX-Redux (бесплатно).
>
> Git + GitHub: Бесплатно для open-source проектов.
>
> CMake + Ninja: Бесплатно.
>
> PS1 toolchain: prebuilt pinned PSn00bSDK release (includes mipsel-none-elf GCC toolchain and SDK). MSYS2/WSL2 is the host shell option, not the source of the PS1 compiler package.
>
> PSn00bSDK: open-source; core libraries/CMake are MPL-2.0 and the release bundle contains separately licensed components. Redistribution must follow each component license.
>
> Интернет: Для загрузки тулчейнов, библиотек, документации.

2. Минимально необходимо (бюджет $0-100):

> USB-геймпад (DualShock 1/2 клон): $10-20.
>
> Зачем: Тестирование ввода на PC и PS1 (через адаптер).
>
> Альтернатива: Использовать клавиатуру для MVP.
>
> Cursor IDE: AI-powered IDE (рекомендуется хотя бы подписка $20/мес).

3. Желательно (бюджет $50-200):

> Реальная PS1 (опционально): \~$107 на eBay (б/у консоль в хорошем состоянии).
>
> Зачем: Проверка таймингов DMA, работы с CD-ROM, реального FPS.
>
> Когда покупать: После релиза MVP, когда будут первые баги, которые не ловятся в эмуляторе.
>
> Альтернатива: Не покупать, если бюджет ограничен. Эмулятор используется как основной MVP validation target; timing- и hardware-specific свойства, критичные для релиза, должны быть отдельно проверены.

4. НЕ нужно для MVP:

> Оригинальные девкиты: $500-2000, недоступны для покупки.
>
> Программаторы EPROM/Flash: $100-300, не нужны без реального железа.
>
> Осциллографы и логические анализаторы: $200-1000, не нужны для MVP.
>
> Saturn/N64/Jaguar/3DO консоли: $100-500 каждая, не нужны до Tier 2-3.

## Скоуп MVP (v0.1.0)

> 1. ВХОДИТ В MVP (Must Have):
>
> Ядро движка:
>
> ECS (Entity Component System): Управление сущностями и компонентами.
>
> Fixed-point математика (16.16): Векторы, матрицы, трансформации.
>
> Система памяти: Linear allocator (для уровней), Pool allocator (для динамических объектов).
>
> Логирование: Logger API; direct printf is forbidden in release runtime builds.
>
> Рендерер:
>
> Affine текстурирование: Без perspective correction.
>
> Flat/Gouraud shading: Вершинное освещение.
>
> Painter's algorithm: Сортировка полигонов по глубине (без Z-буфера).
>
> 8-bit текстуры с палитрами (RGB555+STP): Квантизация до 256 цветов.
>
> Разрешение 320x240: С nearest filtering.
>
> HAL (Hardware Abstraction Layer):
>
> PC-реализация: SDL3 (окно, ввод, аудио) + OpenGL 3.3 (рендеринг с эмуляцией PS1). PS1-реализация: PSn00bSDK (GPU driver, CD-ROM driver, pad driver).
>
> Система ввода:
>
> Клавиатура (PC): WASD + мышь.
>
> Геймпад (PC + PS1): DualShock 1/2 через SDL3 (PC) или PS1 pad driver.
>
> Файловая система:
>
> PC: Стандартная файловая система (чтение .mesh, .tex файлов).
>
> PS1: read-only disc filesystem через CD-ROM; для MVP используем ISO9660-compatible asset layout. Memory Card save storage --- отдельный сервис и не является файловой системой диска.
>
> Пайплайн ассетов:
>
> Конвертер мешей: glTF -> бинарный .mesh (C++14 утилита).
>
> Конвертер текстур: PNG/JPG -> бинарный .tex (8-bit indexed + палитра RGB555+STP).
>
> Интеграция с CMake: на PC-конфигурации конвертер может запускаться автоматически через \`add_custom_command\`; PS1 cross-build не компилирует host tool и потребляет заранее сгенерированные/versioned assets из \`assets/build/\`.
>
> Аудио (заглушка):
>
> PC: PCM через SDL3 (простой бипер или тишина).
>
> PS1: Тишина или простой бипер через BIOS call.
>
> Документация:
>
> README.md: Инструкция "Как начать".
>
> ARCHITECTURE.md: Описание архитектуры движка.
>
> BUILDING.md: Инструкция по сборке для PC и PS1.
>
> API.md: Описание API (ECS, рендерер, ввод).
>
> ASSETS.md: Инструкция по конвертации ассетов.
>
> LEGAL.md: Перечень библиотек и их лицензий.
>
> Примеры: hello_triangle: Минимальный пример (PC + PS1).
>
> rotating_cube: Демка MVP (вращающийся текстурированный куб с управлением камерой).
>
> 2. НЕ ВХОДИТ В MVP (откладываем до v0.2.0+):
>
> Поддержка Saturn/N64/Jaguar/3DO: Только PC + PS1.
>
> Динамическое освещение: Только вершинное/ambient.
>
> Аппаратный звук: Только заглушка.
>
> Полноценная физика: Только AABB коллизии.
>
> Сетевой код / Мультиплеер: Не нужно.
>
> Скриптовый язык (Lua/Python): Только C++.
>
> Редактор уровней: Не нужно для MVP.
>
> Z-буфер: Только Painter's algorithm.
>
> Perspective correct текстурирование: Только affine.
>
> Скелетная анимация: Только статические меши.
>
> Частицы и эффекты: Не нужно для MVP.

3. Критерии готовности MVP (Definition of Done):

> Код компилируется под PC (Windows 11 + MSVC/MinGW) и PS1 (mipsel-none-elf-gcc + PSn00bSDK).
>
> Демка rotating_cube работает на PC (60 FPS) и PS1 (30 FPS в эмуляторе DuckStation).
>
> Куб загружается из бинарного .mesh файла, текстура --- из .tex файла.
>
> Камера управляется с клавиатуры (WASD) и геймпада.
>
> Код ядра един для PC и PS1 (через HAL, без изменений в логике).
>
> Документация полная (README, ARCHITECTURE, BUILDING, API, ASSETS, LEGAL).
>
> Примеры работают (hello_triangle, rotating_cube).
>
> Исходный код проекта публикуется под MIT; сторонние зависимости сохраняют свои исходные лицензии.

4. Метрики успеха MVP:

> Технические:

- W1: не менее 60 FPS на зафиксированной reference PC environment; не менее 30 FPS на зафиксированной PS1 validation environment (в эмуляторе).

- Размер PS1 executable (PS-EXE) < 500 КБ; размер CD image задаётся отдельно и не является лимитом бинарника.

- PS1 W1 peak main RAM <= 1.5 MiB for the reference workload; the absolute physical limit is 2 MiB including SDK/runtime usage.

> Сообщество:

- Community goals are tracked separately from engineering acceptance and do not block the technical MVP.

- Community goals are tracked separately from engineering acceptance and do not block the technical MVP.

- Community goals are tracked separately from engineering acceptance and do not block the technical MVP.

> Личные:

- Завершить MVP за 12 месяцев.

- Не выгореть (работать 10-15 часов в неделю).

# ЭТАП 3: СБОР И АНАЛИЗ ТРЕБОВАНИЙ (REQUIREMENTS ENGINEERING)

## 1. ВВЕДЕНИЕ

> Цель данного документа --- детально описать аппаратные ограничения консолей 5-го поколения и сформулировать технические требования к ретро-движку. Этот этап является критическим, так как ошибки в понимании ограничений железа приведут к необходимости переписывания ядра движка.
>
> Фокус документа:

- MVP (v0.1.0): PC (SDL3 + OpenGL) + PS1

- Будущие версии: Saturn, N64, Jaguar, 3DO

> Все требования основаны на реальных спецификациях железа и опыте homebrew сообщества (PSn00bSDK, libdragon, Jo Engine).

## 2. АППАРАТНЫЕ ОГРАНИЧЕНИЯ ИГРОВЫХ ПЛАТФОРМ

### 2.1. SONY PLAYSTATION 1

PS1 HARDWARE NOTE: GTE register formats are operation-specific. The engine fixed_t 16.16 is an engine representation only. The PS1 backend MUST convert Mat4/fixed_t data into GTE-native rotation, translation, projection and depth representations. See [S1].

Основные характеристики:

- CPU: MIPS R3000A @ 33.8688 МГц

- RAM: 2 МБ (основная) + 1 МБ VRAM + 512 КБ Sound RAM

- GPU: Custom 2D/3D accelerator

- CD-ROM: 2x speed (300 КБ/с)

- Разрешение: 256x224, 320x240, 512x240, 640x480 (interlaced)

Графические ограничения:

> 1. НЕТ аппаратного Z-буфера

- Используется Painter's algorithm (сортировка полигонов по глубине)

-

- Аппаратного Z-buffer нет. Глубина для OT хранится/квантуется через GTE OTZ и выбирается при mapping в Ordering Table; конкретная глубина зависит от длины OT.

> 2. Аффинное текстурирование (без perspective correction)

- Текстуры "плывут" (wobbling) при движении камеры

- Это характерный "ретро-лук" PS1

- Perspective-correct mapping отсутствует в нативном PS1 GPU; affine mapping является hardware behavior. Не связывать это с фиксированным числом операций CPU на пиксель.

> 3. Квантование вершин

- PC reference renderer may emulate PS1-style vertex wobble with a 1/16-pixel heuristic; native PS1 GTE screen coordinates are represented separately in the backend.

- Это вызывает "дрожание" (jitter) вершин при движении

- Еще одна характерная черта PS1-графики

> 4. Текстуры

- Форматы: 4-bit (16 цветов), 8-bit (256 цветов), 16-bit (RGB555)

- Максимальный размер: 256x256 (4-bit), 256x256 (8-bit), 256x256 (16-bit)

-

- CLUT (Color Look-Up Table): палитры хранятся в VRAM

> 5. Полигоны

- GPU PS1 поддерживает и треугольники, и четырёхугольники (например, POLY_FT3/POLY_FT4). MVP может нормализовать входной mesh к треугольникам, но это решение движка, а не ограничение hardware.

- Flat shading (один цвет на полигон) или Gouraud shading (интерполяция)

- Текстурированные или нетекстурированные

- Полупрозрачность (translucency) --- ограниченная, через blending

> 6. Производительность

-

-

- Ограничение: скорость заполнения (fillrate) --- Удалено как нормативная hardware-цифра: fill-rate не должен задаваться одной универсальной цифрой без указания primitive, shading, texture mode, overdraw и resolution. Использовать benchmark workload.

МАТЕМАТИКА

1. НЕТ аппаратного FPU (Floating Point Unit)

- На PS1 нет аппаратного FPU; при включённом soft-float операции с плавающей точкой выполняются программно, а стоимость зависит от операции и компилятора.

- Не использовать фиксированный коэффициент замедления без benchmark; фактическая стоимость зависит от операции и ABI.

2. GTE (Geometry Transformation Engine)

- Сопрцессор для 3D-математики

- Аппаратные операции трансформации, lighting-related operations и perspective division через fixed-point pipeline GTE; frustum clipping является явной backend/engine стадией до потребления projected coordinates.

- NCLIP используется для проверки ориентации полигона; DPCS/DPC относится к обработке цветов. Это не два основных режима трансформации.

- GTE: фиксированная арифметика; форматы зависят от регистра/операции. Engine fixed_t 16.16 не является универсальным внутренним форматом GTE.

- Не использовать одну фиксированную цифру transform/sec как hardware specification; производительность зависит от команды, batching и CPU/GTE synchronization. Benchmark является нормативным источником.

3. ЦЕЛОЧИСЛЕННОЕ УМНОЖЕНИЕ CPU

- MIPS integer multiply in CPU (mult/multu): 64-bit product is returned through HI/LO. The R3000A does not provide a GTE MAC instruction; do not describe CPU mult/multu as a GTE MAC.

- Используется для fixed-point умножения

- Instruction: mult (signed 32x32 -> 64-bit HI/LO result); fixed_mul must still perform the required arithmetic shift for the 16.16 representation.

ПАМЯТЬ

1. Основная RAM: 2 МБ

- Организация: линейная, без MMU (Memory Management Unit)

- Доступ: 32-bit, 16-bit, 8-bit

- Скорость доступа: зависит от контроллера, доступа и contention; не использовать одну универсальную bandwidth-цифру как hardware contract.

2. VRAM: 1 МБ

- Организация: 1024 halfwords × 512 строк (16-bit pixel units) (16-bit на пиксель)

- Используется для: текстур, CLUT, framebuffer, drawing area

- Доступ: через DMA или прямую запись

3. Sound RAM: 512 КБ

- Используется для: сэмплов, аудио-данных

- Доступ: через SPU (Sound Processing Unit)

ВВОД

- Контроллер: Digital Pad или DualShock (аналоговые стики)

- Кнопки: D-pad, 4 face buttons, L1/R1, L2/R2, Start, Select

- Аналоговые стики (DualShock): 2 стика, 8-bit точность

- Подключение: последовательный SIO-протокол; обмен с контроллером выполняется байтами с подтверждением ACK.

ЗВУК

- SPU (Sound Processing Unit): 24 канала

- Формат SPU: 4-bit ADPCM samples; the SPU datapath produces 16-bit audio samples internally. CD-XA ADPCM is a separate CD/decoder path.

- Частота дискретизации: 44.1 кГц

- Эффекты: реверб, модуляция

- Ограничение: 512 КБ SPU RAM total; part of the space is used for capture/reverb/other SPU state, so applications must not budget the full 512 КБ as sample storage.

ФАЙЛОВАЯ СИСТЕМА

- CD-ROM: ISO9660 (Mode 2/Form 1)

- Скорость чтения: 300 КБ/сек (2x)

- Latency CD-ROM зависит от положения головки и access pattern; не использовать фиксированное значение seek time в performance model.

- Ёмкость носителя и файловой системы не является runtime budget движка; в MVP размер ISO-образа отслеживается отдельной метрикой. Точное ограничение зависит от media/image layout.

- Формат: .bin/.cue или .iso

ОСОБЕННОСТИ ДЛЯ ДВИЖКА

- Использовать GTE для всех трансформаций

- Engine simulation uses 16.16 fixed-point; PS1/GTE conversion uses native hardware formats in the backend adapter.

- Ordering Table для сортировки полигонов

- DMA для загрузки данных в VRAM

- Минимальная read-only ISO9660-backed загрузка файлов входит в MVP; background streaming/prefetching не является release gate v0.1.0.

- Оптимизация под fillrate (меньше overdraw)

### 2.2. SEGA SATURN

ОСНОВНЫЕ ХАРАКТЕРИСТИКИ

- CPU: 2x Hitachi SH-2 @ 28.6 МГц (Master + Slave)

- RAM: 2 МБ Work RAM total: 1 МБ WRAM-L + 1 МБ WRAM-H; the two blocks have different bus/access characteristics.

-

- GPU: VDP1 (полигоны) + VDP2 (плоскости/фон)

- CD-ROM: 2x speed (300 КБ/с)

- Разрешение: 320x224, 352x224, 640x448 (interlaced)

ГРАФИЧЕСКИЕ ОГРАНИЧЕНИЯ

1. Двойная видео-подсистема

- VDP1: рисует полигоны и спрайты

- VDP2: рисует фоновые слои (scrolling planes) - Синхронизация VDP1 и VDP2 --- сложнейшая задача

2. Quad-oriented VDP1 primitives

- VDP1 работает с 4-угольниками

- VDP1 primitives are quadrilateral-oriented; if the future backend accepts triangles, it must define its own triangle-to-quad conversion policy. This is an engine policy, not a blanket hardware requirement.

- Это усложняет рендерер

3. Текстуры

-

- Максимальный размер: 256x256

-

4. Производительность

-

- Ограничение: VDP1 fillrate

МАТЕМАТИКА

1. НЕТ аппаратного FPU

- SH-2 имеет встроенный умножитель (MAC)

- Engine-level fixed-point is recommended for the shared simulation contract; Saturn backend code may use native SH-2 integer/MAC representations internally and must convert explicitly at the HAL/backend boundary.

2. Два CPU

- Master SH-2: general game/backend work; workloads may be partitioned between the two SH-2 CPUs. Do not assign VDP1 ownership as a hard hardware rule.

- Slave SH-2: additional parallel workload; actual task partitioning is software-defined. Sound hardware has its own SCSP processor/DSP path.

- Синхронизация: через SCU (System Control Unit)

- Проблема: балансировка нагрузки между CPU

ПАМЯТЬ

- Work RAM Low: 1 МБ; access is primarily through the CPU-side bus and differs from WRAM-H.

- Work RAM High: 1 МБ shared through the broader system bus; access characteristics differ from WRAM-L.

-

- Sound RAM: 512 КБ

ОСОБЕННОСТИ ДЛЯ ДВИЖКА

- Параллелизм: Master + Slave SH-2

- VDP1 для полигонов, VDP2 для фона

- Квады вместо треугольников

- Сложная синхронизация видео-подсистем

### 2.3. NINTENDO 64 (N64)

ОСНОВНЫЕ ХАРАКТЕРИСТИКИ

- CPU: NEC VR4300 (MIPS R4300i) @ 93.75 МГц

- RAM: 4 МБ base; Expansion Pak добавляет ещё 4 МБ usable RDRAM (итого 8 МБ).

- GPU: RCP (Reality Co-Processor) @ 62.5 МГц

- RSP (Reality Signal Processor): векторный процессор

- RDP (Reality Display Processor): растеризатор

- ROM: картриджи (4-64 МБ)

- Разрешение: 256x240, 320x240, 640x480 (interlaced)

ГРАФИЧЕСКИЕ ОГРАНИЧЕНИЯ

1. RCP (Reality Co-Processor)

- RSP: программируемый векторный процессор (микрокод)

- RDP: аппаратный растеризатор

- Микрокод определяет графический конвейер

2. Z-буфер

- Есть аппаратный Z-buffer (18-bit internal depth representation). 24-bit как hardware Z-buffer не заявляется.

- Hardware depth buffering is a major architectural difference from PS1; Saturn depth/ordering behavior must be described separately and is not reduced to a single equivalent Z-buffer claim.

- Opaque visibility can use the hardware depth path; ordering remains relevant for transparency and engine-specific compositing.

3. Perspective correct текстурирование

- Аппаратная поддержка

-

4. Текстуры

-

- Тайловая организация (64x64 или 32x64 тайлы)

- Фильтрация: bilinear filtering (аппаратная)

5. Производительность

-

- Ограничение: fillrate, пропускная память

МАТЕМАТИКА

1. FPU в CPU

- MIPS R4300i имеет встроенный FPU

- FPU is available; do not assume a universal 1-cycle latency. Use instruction-specific benchmark data for future N64 backend decisions.

2. RSP (Reality Signal Processor)

- Векторный процессор для математики

- Программируется через микрокод

- Используется для: трансформация вершин, освещение, скиннинг

ПАМЯТЬ

- RAM: 4 МБ base; 8 МБ с Expansion Pak.

- RDRAM: специфичная память с высокой пропускной способностью

- Ограничение: узкое горлышко памяти (memory bandwidth)

ОСОБЕННОСТИ ДЛЯ ДВИЖКА

- Микрокод для RSP (сложнейшая часть)

- Z-буфер (не нужна сортировка)

- Perspective correct текстурирование

- Тайловая организация текстур

- The CPU has an FPU; use it only in future backend-specific code where the cost/benefit is benchmarked.

### 2.4. ATARI JAGUAR

ОСНОВНЫЕ ХАРАКТЕРИСТИКИ

- CPU: Motorola 68000 @ 13.3 МГц (основной)

- GPU: Tom (26.6 МГц) + Jerry (26.6 МГц)

- RAM: 2 МБ

- ROM: картриджи (до 6 МБ)

- Разрешение: 160x200, 320x240, 640x480

ГРАФИЧЕСКИЕ ОГРАНИЧЕНИЯ

1. Tom (GPU/Blitter)

- Программируемый растеризатор

- Нет готового 3D-API

- Всё пишется вручную (blitter operations)

2. Jerry (DSP)

- Цифровой сигнальный процессор

- Используется для: звук, помощь в математике

3. Текстуры

- Форматы: 1-32 bit

- Texture mapping is programmable through GPU/Blitter routines rather than a fixed-function triangle texture sampler.

- Texture-mapping workloads can involve the GPU and/or Blitter; the exact implementation is backend-specific.

МАТЕМАТИКА

- НЕТ FPU

-

- Tom и Jerry имеют DSP-инструкции для математики

ОСОБЕННОСТИ ДЛЯ ДВИЖКА

- Прямое программирование blitter

- Five processors are present across the Jaguar architecture; the 68000 is the host/boot processor while Tom and Jerry contain additional programmable processors.

- Нет готового API --- всё вручную

- Очень сложно для разработки

### 2.5. PANASONIC 3DO

ОСНОВНЫЕ ХАРАКТЕРИСТИКИ

- CPU: ARM60 @ 12.5 МГц

- RAM: 2 MiB DRAM + 1 MiB VRAM minimum configuration (3 MiB total RAM).

- Video: MADAM CEL engine + CLIO display/video system; not a conventional polygon GPU.

- CD-ROM: nominal 2x class (\~300 КБ/с); model-specific behavior must be verified before backend work

- Разрешение: 320x240

ГРАФИЧЕСКИЕ ОГРАНИЧЕНИЯ

1. Cel-based video/compositing

- Видео состоит из "ячеек" (cells)

- The CEL engine composites pixel-based cells; do not model it as a conventional triangle rasterizer. Geometry is software-defined around the CEL/CCoB pipeline.

- The system includes a CEL engine and a MADAM matrix engine; 3D transforms and cell command generation are software/API driven with hardware assistance for selected operations.

2. Текстуры

- Pixel/cell formats are hardware-specific and must be selected from the 3DO hardware documentation; do not treat "6/12/16-bit" as a single generic texture format contract.

- CEL/cell compositing is hardware-assisted; do not model this as generic texture sampling.

МАТЕМАТИКА

- НЕТ FPU

- ARM60 имеет умножитель

- Fixed-point is the practical math model; MADAM includes a matrix engine with fixed-point operations.

ОСОБЕННОСТИ ДЛЯ ДВИЖКА

- Cel-based compositing; geometry/3D transforms are primarily software-defined

- Официальный SDK (Portfolio OS и сопудствующее ей ПО) под "юридической защитой"

- Очень специфичная архитектура

## 3. ТРЕБОВАНИЯ К PC

SDL3 РОЛЬ

- Создание окна (Window)

- Обработка ввода (Keyboard, Mouse, Gamepad)

- Аудио (PCM через SDL3 audio subsystem)

- Тайминг (Delta time, FPS counter)

РЕНДЕРИНГ

1. OpenGL 3.3 (не SDL3 GPU API)

- Причина: стабильность, отладка через RenderDoc

- Эмуляция PS1-ограничений через шейдеры

2. Эмуляция PS1 через OpenGL

- Отключить Z-буфер (glDisable(GL_DEPTH_TEST))

- Аффинное текстурирование (не делить на W в шейдере)

- Квантование вершин (floor(pos * 16.0) / 16.0)

- Разрешение 320x240 с nearest filtering

- 8-bit текстуры с палитрами

3. Фреймбуфер 320x240

- Рендеринг в текстуру 320x240

- Вывод на экран с масштабированием (nearest)

PC КАК "GOLDEN MASTER"

- Логика игры пишется на PC

- Если код работает на PC, он должен работать на PS1 (через HAL)

- Различия только в HAL (рендерер, ввод, файловая система)

## 4. НЕФУНКЦИОНАЛЬНЫЕ ТРЕБОВАНИЯ

REFERENCE WORKLOAD W0 (NORMATIVE): 320x240, 12 textured triangles, CI8/256-entry palette, Gouraud on, transparency off, fixed camera, single visible room. W0 is the deterministic conformance workload. Correctness is established by fixed input/state and canonical framebuffer/reference-output comparison; performance is reported separately from correctness.

REFERENCE WORKLOAD W1 (NORMATIVE FOR PERFORMANCE): 4 visible cubes = 48 textured triangles, CI8/256-entry palette, Gouraud on, transparency off, fixed camera, one 256x256 texture per cube at most. W1 is the release performance workload. FPS and memory claims MUST name the measured target environment and build configuration.

REFERENCE ENVIRONMENT CONTRACT (NORMATIVE):

PC performance claims MUST record OS build, CPU model, GPU model/driver, compiler/toolchain version, build type, internal resolution, presentation resolution, VSync state, power/performance mode and measurement interval in \`benchmarks/reference_environment.yaml\`. PS1 emulator performance claims MUST additionally record DuckStation build/commit, renderer/backend, emulation speed, VSync state, region/video mode, host OS/CPU, measurement interval, engine-side completed-frame count, Git revision and asset-set SHA-256.

PS1 PERFORMANCE GATE SEMANTICS:
The MVP PS1 gate is an emulator validation gate, not a hardware-performance claim. W1 MUST sustain at least 30 completed engine frames per second for a 60-second run in the pinned DuckStation validation configuration, with emulator speed held at 100% and no sustained speed reduction below 98%. This demonstrates that the reference workload fits the documented emulator timing envelope. It MUST NOT be described as evidence that retail PS1 hardware sustains 30 FPS; real-hardware timing remains a post-MVP validation item.

CANONICAL \`benchmarks/reference_environment.yaml\` EXAMPLE:
pc:
os: "recorded"
cpu: "recorded"
gpu: "recorded"
gpu_driver: "recorded"
compiler: "recorded"
build_type: "Release"
internal_resolution: "320x240"
presentation_resolution: "recorded"
vsync: false
power_mode: "recorded"
measurement_seconds: 60
git_revision: "required"
asset_set_sha256: "required"
ps1_emulator:
emulator: "DuckStation"
build: "pinned validation build"
renderer: "recorded"
video_region: "recorded"
emulation_speed_percent: 100
vsync: false
host_os: "recorded"
host_cpu: "recorded"
measurement_seconds: 60
minimum_engine_fps: 30
minimum_emulator_speed_percent: 98
git_revision: "required"
asset_set_sha256: "required"

- PC W1: >= 60 FPS on the recorded reference PC environment (стабильно)

- PS1 W1: >= 30 FPS on the recorded DuckStation validation environment (stable emulator validation; not a real-hardware performance guarantee) на W1

- Время кадра: < 33 мс (PS1), < 16 мс (PC)

ПАМЯТЬ

-

-

- PC: использование RAM < 500 МБ

РАЗМЕР БИНАРНИКА

- PS1: CD image size is tracked as a release artifact metric; PS-EXE target ≤ 500 KiB. No universal CD-image-size ceiling is an MVP runtime contract.

- PC: отдельного жёсткого лимита нет; размер отслеживается как build metric

СОВМЕСТИМОСТЬ

- PC: Windows 10/11, Linux, macOS

- PS1: target compatibility is limited to the revisions explicitly tested; emulator-only MVP does not constitute proof for every retail SCPH revision

- Эмуляторы: DuckStation, PCSX-Redux

НАДЕЖНОСТЬ

- Нет утечек памяти (тест 24 часа)

- Нет крашей (тест 1000 запусков)

- Стабильный FPS (нет просадок)

## 5. ФУНКЦИОНАЛЬНЫЕ ТРЕБОВАНИЯ MVP

ЯДРО

- ECS (Entity Component System)

- Fixed-point математика (16.16)

- Система памяти (Linear + Pool аллокаторы)

- Логирование через Logger API; direct printf is forbidden in release runtime builds.

РЕНДЕРЕР

- Affine текстурирование

- Flat/Gouraud shading

- Painter's algorithm (без Z-буфера)

- 8-bit текстуры с палитрами (RGB555+STP)

- Разрешение 320x240

HAL

- PC: SDL3 + OpenGL 3.3

- PS1: PSn00bSDK

ВВОД

- Клавиатура (PC)

- Геймпад (PC + PS1)

ФАЙЛОВАЯ СИСТЕМА

- PC: стандартная ФС

- PS1: ISO9660 (CD-ROM)

ПАЙПЛАЙН АССЕТОВ

- glTF -> .mesh (бинарный формат)

- PNG/JPG -> .tex (8-bit indexed + палитра)

- C++14 утилита

АУДИО

- PC: optional beep/stub only; full PCM audio is post-MVP.

- PS1: тишина или бипер

## 6. ОГРАНИЧЕНИЯ И КОМПРОМИССЫ

ОГРАНИЧЕНИЯ

- Нет Z-буфера на PS1 -> Painter's algorithm (медленно)

- На PS1 нет FPU в CPU; fixed-point выбран как engine-level representation для deterministic simulation. Точность определяется форматом и диапазоном, а не фактом отсутствия FPU.

- Аффинное текстурирование -> "Wobbling" (характерный вид)

- 2 МБ RAM на PS1 -> ограничение на размер уровней

- Нет реального железа -> только эмуляторы (могут врать в таймингах)

КОМПРОМИССЫ

- Производительность vs Точность: fixed-point даёт предсказуемый диапазон и deterministic integer arithmetic; преимущество по скорости подтверждается только benchmark-ом.

- Простота vs Функциональность: MVP минималистичен, без сложных фич

- Кроссплатформенность vs Оптимизация: Единый код, но не максимальная производительность на каждой платформе

## 7. ТАБЛИЦЫ СРАВНЕНИЯ КОНСОЛЕЙ

Статус данных: PS1 --- нормативные hardware facts для MVP. Saturn/N64/Jaguar/3DO --- справочные архитектурные ориентиры, не backend acceptance criteria. В future sections, labels "hardware fact" describe silicon behavior; "engine policy" describes the proposed backend mapping. Before implementation, re-verify values against platform-specific primary/reference documentation.

СРАВНЕНИЕ CPU

 Консоль CPU Частота FPU Производительность / примечание
 --------- ---------------- ------------- ----- -----------------------------------------------------------------------------------------
 PS1 MIPS R3000A 33.8688 МГц НЕТ Benchmark only
 Saturn 2x SH-2 28.6 МГц НЕТ Benchmark only
 N64 MIPS R4300i 93.75 МГц ДА Benchmark only
 Jaguar Motorola 68000 13.295 МГц НЕТ Host CPU; graphics/audio work is largely offloaded to programmable Tom/Jerry processors
 3DO ARM60 12.5 МГц НЕТ Benchmark only

СРАВНЕНИЕ ПАМЯТИ

 Консоль RAM Video / subsystem memory Sound RAM Media
 --------- ------------------------------- ------------------------------------------ ----------------------------------- -------------------------------------------
 PS1 2 МБ main RAM 1 МБ VRAM 512 КБ SPU RAM CD-ROM, nominal 2x (\~300 KiB/s)
 Saturn 2 МБ Work RAM (Low + High) VDP1/VDP2-specific VRAM 512 КБ sound RAM CD-ROM, nominal 2x
 N64 4 МБ; 8 МБ with Expansion Pak RCP/video memory model Audio is subsystem-specific Cartridge ROM
 Jaguar 2 МБ DRAM Shared DRAM / Tom-Jerry subsystem Shared DRAM / Jerry DSP subsystem Cartridge ROM (CD add-on on some systems)
 3DO 2 МБ main RAM Video/system memory is platform-specific Subsystem-specific CD-ROM; verify profile per target

*- -- консоли использовали унифицированную память (UMA, в качестве неё часто выступала оперативная память), которая не имела строгого распределения на видео- и звуковую память

СРАВНЕНИЕ ГРАФИКИ
For Jaguar, "ДА (Blitter)" describes hardware Z compare/write support in 16-bit pixel mode; texture mapping itself remains programmable. Future-platform rows are reference facts only and are not MVP acceptance contracts.

 Консоль Z-буфер Текстурирование Depth / texture model
 --------- --------------------------------- ------------------------------ ----------------------------------------------------------------------------------------------------------
 PS1 НЕТ Affine No hardware Z-buffer; affine mapping
 Saturn НЕТ Affine No universal z-buffer contract; backend-specific VDP1/VDP2 ordering
 N64 ДА Perspective correct Hardware depth buffer; perspective-correct texture pipeline
 Jaguar ДА (Blitter, 16-bit pixel mode) Programmable texture mapping Blitter provides hardware-assisted Z compare/write in 16-bit pixel mode; texture mapping is programmable
 3DO НЕТ Аппаратное (cell) Cell-based compositing / hardware-assisted video; backend-specific

## 8. РЕКОМЕНДАЦИИ ПО РЕАЛИЗАЦИИ

ДЛЯ MVP (PC + PS1)

- Использовать Fixed-point математику (16.16) везде

- Не использовать Z-буфер (Painter's algorithm)

- Использовать GTE на PS1 для 3D-трансформаций

- Эмулировать PS1-ограничения на PC через OpenGL шейдеры

- Оптимизировать под fillrate (меньше overdraw)

- Использовать DMA для загрузки данных в VRAM на PS1

ДЛЯ БУДУЩИХ ВЕРСИЙ

- Saturn: параллелизм Master + Slave SH-2

- N64: микрокод для RSP, использовать Z-буфер

- Jaguar: прямое программирование blitter

- 3DO: cell-based рендеринг

# ЭТАП 4: АРХИТЕКТУРА И ПРОЕКТИРОВАНИЕ (SYSTEM DESIGN)

NORMATIVE PRECEDENCE: The canonical MVP contract is defined by the latest section explicitly marked "NORMATIVE". Older duplicated examples, historical revisions, and non-normative reference text MUST NOT override it.

LAYER OWNERSHIP: Core (math, memory, containers, IDs and ECS storage) is a foundation used by Systems and Services; it is not a Service and does not depend on HAL. Game code uses public Systems/Services APIs and MUST NOT call HAL functions directly.

BUILD PRECEDENCE: CMakePresets.json is the only canonical build entry point. All BUILDING.md, scripts, IDE tasks and CI examples MUST use the preset-generated paths build/pc-debug, build/pc-release and build/ps1-release.

## 1. ВВЕДЕНИЕ И ЦЕЛИ ПРОЕКТИРОВАНИЯ

> Цель данного этапа --- зафиксировать архитектурные решения, которые станут фундаментом движка. Ошибки на этом этапе требуют переписывания 60-80% кода, поэтому каждое решение должно быть обосновано.

ПРИНЦИПЫ ПРОЕКТИРОВАНИЯ

1. Data-Oriented Design (DOD)

- Данные хранятся последовательно в памяти (cache-friendly).

- Избегаем виртуальных вызовов в горячих путях.

- Компоненты --- POD-структуры, системы --- функции над массивами.

2. Минимальные абстракции

- Абстракция должна стоить 0 тактов CPU в runtime.

- HAL выбирается compile-time через CMake source selection; локальная #ifdef/template specialization допускается только там, где действительно требуется compile-time specialization. Runtime virtual dispatch в hot paths не используется.

- Никаких std::function, std::any, динамического полиморфизма в ядре.

3. Явное управление памятью

- Нет new/delete в runtime.

- Нет malloc/free в runtime.

- Все аллокации предсказуемы и ограничены.

4. Детерминированность

- При одинаковой последовательности входных событий и фиксированном 60 Hz simulation step состояние игры воспроизводимо побитно на поддерживаемых платформах.

- Fixed-point математика гарантирует это на всех платформах.

- Нет плавающей запятой в игровой логике.

5. Кроссплатформенность через HAL, а не через абстракции

- Ядро не знает о существовании PS1 или PC.

- Core не вызывает HAL напрямую; Systems/Services используют публичные API и выбранный backend через HAL boundary.

- Реализация Platform_* выбирается на этапе компиляции.

6. Тестируемость на PC

- Основная разработка и deterministic conformance выполняются на PC; PS1 acceptance проводится отдельно в целевом backend.

- PC-сборка должна быть "Golden Master" по логике.

- Консольные сборки используют общий engine representation; platform-specific conversion, submission и hardware state принадлежат backend.

MVP v0.1.0 targets exactly PC (SDL3 + OpenGL 3.3) and PlayStation 1 (PSn00bSDK v0.24). Saturn, N64, Jaguar and 3DO are post-MVP. PC = Personal Computer (host platform); PS1 = Sony PlayStation 1; backend = platform-specific renderer/HAL implementation; engine representation = platform-independent data before backend conversion.

Workload contract: W0 is the deterministic conformance scene; W1 is the demo/performance scene. Performance claims must identify which workload produced the measurement.

Visibility contract: TRANSFORM is followed by CLIP before CULL. MVP supports finite near/far clipping; geometry intersecting the near plane must be clipped or explicitly rejected before projected coordinates are consumed.

Renderer safety contract: command-buffer, OT and arena exhaustion are explicit failures (status result plus diagnostic). Rendering code must never silently drop a primitive because an internal fixed-capacity buffer is full.

Failure policy: allocation/command/OT overflow => status=false and deterministic diagnostic code; malformed asset => reject before dereference; invalid handle => reject/assert in debug; unsupported geometry or clip failure => explicit renderer diagnostic. No undefined memory access, modulo-based depth wrapping, or silent primitive loss is permitted.

Renderer conformance cases must include at least: fully visible triangle, backface culled triangle, triangle crossing the near plane, triangle crossing the far plane, and invalid depth/clip rejection. Results must be deterministic for W0.

Lifecycle contract: Platform_Init/Shutdown own platform services; Render_Init/Shutdown own renderer state. Render_Init is called exactly once per process after Platform_Init and before the first Render_BeginFrame; Platform_Init does not call Render_Init.

Lifecycle invariant test: Platform_Init followed by Render_Init must be required in every sample/test; calling Render_Init twice or calling Render_EndFrame before Render_Init is a contract violation.

Canonical implementation rule: CMakePresets.json is the single build entry point. CMake selects exactly one platform backend translation-unit set; common engine code must not contain platform-specific #ifdef branches unless a shared source file genuinely requires compile-time specialization.

Engine scalar: signed int32_t fixed_t, 16.16. Native hardware representations are backend-specific and require explicit conversion.

PS1 main RAM acceptance: peak <= 1.5 MiB, inclusive of runtime/SDK, stack, static data and engine arenas. ECS/dynamic pools have a hard 256 KiB reservation with explicit MVP component capacities; MAX_ENTITIES=10,000 is the handle-domain limit, not a promise that every component pool can hold 10,000 rows. Explicit engine arena capacities total 1376 KiB; 160 KiB additional measured headroom is allowed within the 1536 KiB acceptance ceiling. A separate 512 KiB physical reserve is never allocated by engine systems. PS1 VRAM acceptance: peak <= 800 KiB.

CD staging budget: 64 KiB. CD throughput and latency are measurements, not universal engine constants.

Entity contract: 0 = invalid; lower 24 bits = index; upper 8 bits = generation; MAX_ENTITIES = 10,000.

Reference render target: 320x240. PC presentation scales the reference image to the host window; the reference PC renderer disables the OpenGL depth test.

Depth contract: PS1 uses Ordering Table depth buckets; PC uses deterministic back-to-front ordering. Arbitrary cyclic/intersecting opaque geometry is outside the visibility guarantee of the MVP renderer.

Binary assets: .mesh/.tex version 1, little-endian, bounds-checked before use. Malformed input must fail without out-of-bounds access.

Binary layout contract: serialized integers are little-endian with explicitly specified field widths; host struct padding/alignment is never serialized. .mesh/.tex v1 headers define magic, version, counts/sizes and payload offsets; loaders validate all arithmetic for overflow before pointer formation/copy.

Build contract: CMake Presets are canonical. Exactly one platform backend is compiled into an engine build. \`src/CMakeLists.txt\` selects exactly one \`retro_engine_hal\` before creating \`retro_engine\`. PC canonical runtime targets are \`hello_triangle\` and \`rotating_cube\`; \`rotating_cube\` is the release reference target under \`build/<preset>/game/\`. Published W0/W1 performance results MUST include \`benchmarks/reference_environment.yaml\` metadata.

MVP audio: lifecycle API plus optional beep stub only. Full XA/PCM/ADPCM streaming is post-MVP.

EDITORIAL RULE: Any code block or command path that conflicts with the canonical contracts is historical/non-normative and MUST be removed from future revisions rather than copied into the repository. This includes legacy vcpkg-only SDL3 setup, unpinned dependency fetches, megabyte automatic stack buffers, and release paths for undeclared targets.

### 1.1. НОРМАТИВНЫЕ КОНТРАКТЫ MVP

### 1.2. ИСПОЛНЯЕМЫЙ НОРМАТИВНЫЙ КОНТРАКТ

ЦЕЛЬ: Каждый обязательный MVP-инвариант должен быть представлен не только текстом, но и машинно проверяемым test/validator/CI gate. This section is normative.

Правило: нормативный пункт MUST map to at least one executable check. Если утверждение нельзя проверить автоматически, оно должно быть явно помечено как manual acceptance или informational, а не как silent normative contract.

#### 1.2.1. CANONICAL CONTRACT MANIFEST

# ci/normative_contracts.yaml
version: 1
build_entrypoint: CMakePresets.json
exactly_one_backend: true
canonical_presets:

- pc-debug
- pc-release
- pc-debug-render
- ps1-release
 engine_scalar:
 type: int32_t
 format: Q16.16
 overflow: saturating
 multiply_rounding: trunc_toward_zero
 divide_zero: debug_assert_release_saturate
 memory:
 ps1_peak_main_ram_bytes: 1572864
 ps1_peak_vram_bytes: 819200
 ecs_reservation_bytes: 262144
 cd_staging_bytes: 65536
 renderer:
 reference_resolution: [320, 240]
 order: [TRANSFORM, CLIP, CULL, SORT, PACK, SUBMIT]
 depth_test: disabled_on_pc_reference
 silent_primitive_drop: forbidden
 assets:
 mesh_version: 1
 tex_version: 1
 endian: little
 abi_padding_serialized: false
 release:
 ps1_exe_max_bytes: 512000
 w0: deterministic_framebuffer_conformance
 w1:
 pc_min_fps: 60
 ps1_emulator_min_fps: 30
 measurement_seconds: 60

CI MUST validate this manifest against the project configuration. A changed normative value requires updating this manifest and the corresponding ADR/test in the same change.

#### 1.2.2. MEMORY-SAFETY CONFORMANCE TESTS

// tests/memory_safety_contract_test.cpp
#include <gtest/gtest.h>
#include <cstdint>
#include <cstddef>

TEST(LinearAllocatorContract, ReturnedAddressHonorsAlignment) {
alignas(16) uint8_t storage[128];
LinearAllocator a(storage + 1, sizeof(storage) - 1);
void* p = a.allocate(16, 16);
ASSERT_NE(p, nullptr);
EXPECT_EQ(reinterpret_cast<uintptr_t>(p) & 15u, 0u);
}

TEST(StackAllocatorContract, ReturnedAddressHonorsAlignment) {
alignas(32) uint8_t storage[128];
StackAllocator a(storage + 3, sizeof(storage) - 3);
void* p = a.push(16, 32);
ASSERT_NE(p, nullptr);
EXPECT_EQ(reinterpret_cast<uintptr_t>(p) & 31u, 0u);
}

TEST(PoolAllocatorContract, RejectsForeignAndInteriorPointers) {
Pool<int, 4> pool;
int foreign = 0;
EXPECT_FALSE(pool.deallocate(&foreign));
int* p = pool.allocate();
ASSERT_NE(p, nullptr);
EXPECT_FALSE(pool.deallocate(p + 1));
EXPECT_TRUE(pool.deallocate(p));
EXPECT_FALSE(pool.deallocate(p));
}

These tests are release-blocking for the memory contract. The test fixture MUST deliberately exercise unaligned arena bases and foreign/interior/double-free pointers.

#### 1.2.3. FIXED-POINT EXECUTABLE CONTRACT

// tests/fixed_contract_test.cpp
TEST(FixedContract, SaturatingArithmetic) {
EXPECT_EQ(fixed_add(INT32_MAX, 1), INT32_MAX);
EXPECT_EQ(fixed_sub(INT32_MIN, 1), INT32_MIN);
}

TEST(FixedContract, MultiplyUsesQ16_16Scale) {
EXPECT_EQ(fixed_mul(fixed_from_int(5), fixed_from_int(7)), fixed_from_int(35));
}

TEST(FixedContract, ReductionUsesSingleFinalClamp) {
const Vec3 a = vec3_make(INT32_MAX, INT32_MAX, INT32_MAX);
const Vec3 b = vec3_make(FIXED_ONE, FIXED_ONE, FIXED_ONE);
EXPECT_EQ(vec3_dot(a, b), INT32_MAX);
}

The reduction tests MUST compare raw fixed_t results, not only floating-point conversions. Floating-point is permitted in the host-side oracle only; it is not the normative runtime representation.

#### 1.2.4. ENTITY HANDLE CONTRACT

// Canonical encoding
using Entity = uint32_t;
constexpr uint32_t ENTITY_INDEX_BITS = 24;
constexpr uint32_t ENTITY_GENERATION_BITS = 8;
constexpr uint32_t ENTITY_INDEX_MASK = 0x00FFFFFFu;
constexpr uint32_t ENTITY_GENERATION_SHIFT = 24;
constexpr Entity INVALID_ENTITY = 0;

inline Entity entity_make(uint32_t index, uint32_t generation) {
return (index & ENTITY_INDEX_MASK) \| (generation << ENTITY_GENERATION_SHIFT);
}
inline uint32_t entity_index(Entity e) { return e & ENTITY_INDEX_MASK; }
inline uint32_t entity_generation(Entity e) { return e >> ENTITY_GENERATION_SHIFT; }

Conformance MUST verify stale-handle rejection, generation increment on destroy/reuse, and retirement when the 8-bit generation would wrap from 255. A registry implementation that keeps only m_next_entity without alive/generation state does NOT satisfy this contract.

#### 1.2.5. PS1 RGB555+STP CONTRACT

inline uint16_t pack_rgb555_stp(uint8_t r5, uint8_t g5, uint8_t b5, bool stp) {
return static_cast<uint16_t>(
(r5 & 0x1F) \|
((g5 & 0x1F) << 5) \|
((b5 & 0x1F) << 10) \|
(stp ? 0x8000u : 0u));
}

TEST(TextureContract, RGB555BitLayout) {
EXPECT_EQ(pack_rgb555_stp(31, 0, 0, true), 0x801Fu);
EXPECT_EQ(pack_rgb555_stp(0, 31, 0, true), 0x83E0u);
EXPECT_EQ(pack_rgb555_stp(0, 0, 31, true), 0xFC00u);
}

The texture converter MUST implement this bit layout. Test data MUST NOT use expressions such as \`(i << 8) \| i\` as a stand-in for RGB555+STP.

#### 1.2.6. RENDERER DEPTH / OT CONTRACT

// Canonical engine-side depth key before backend mapping.
// depth_min <= depth_max and both are configured per renderer instance.
uint32_t depth_key_from_camera_z(int32_t camera_z, uint32_t depth_min, uint32_t depth_max) {
if (camera_z < 0) return DEPTH_REJECT;
const uint32_t z = static_cast<uint32_t>(camera_z);
if (z < depth_min \|\| z > depth_max) return DEPTH_REJECT;
return z - depth_min;
}

TEST(RenderContract, RejectsOutOfRangeDepthWithoutModuloAlias) {
EXPECT_EQ(depth_key_from_camera_z(-1, 0, 4095), DEPTH_REJECT);
EXPECT_EQ(depth_key_from_camera_z(4096, 0, 4095), DEPTH_REJECT);
EXPECT_NE(depth_key_from_camera_z(1024, 0, 4095), DEPTH_REJECT);
}

PS1 OT insertion MUST reject or clamp out-of-range depth before indexing. Modulo wrapping is forbidden. The exact depth-range mapping used by the backend MUST be represented by a deterministic unit test with golden input/output vectors.

#### 1.2.7. ASSET FORMAT VALIDATION CONTRACT

// Validator requirements for .mesh/.tex v1
struct ValidationResult {
bool ok;
uint32_t error_code;
};

// REQUIRED checks, in this order:
// 1) file_size >= header_size
// 2) magic/version
// 3) count/offset arithmetic in uint64_t
// 4) payload end <= file_size
// 5) every index < vertex_count
// 6) only then persistent allocation
// 7) rollback allocation on any post-allocation failure

The loader test suite MUST include truncated headers, integer-overflow-sized counts, out-of-range indices, overlapping .tex payloads, invalid width/height, and malformed offsets. Every failure case MUST prove that the persistent arena usage is unchanged.

#### 1.2.8. CMAKE / CI EXECUTABLE GATES

# CI contract
cmake --preset pc-debug
cmake --build --preset pc-debug
ctest --preset pc-debug --output-on-failure

cmake --preset pc-release
cmake --build --preset pc-release
ctest --preset pc-release --output-on-failure

cmake --preset ps1-release
cmake --build --preset ps1-release

CI MUST fail when: more than one PLATFORM_* cache option is true; neither is true; a non-canonical build directory is used; the PS1 toolchain file is absent; generated .mesh/.tex files fail format validation; or PS1 EXE/image outputs are missing.

#### 1.2.9. RELEASE GATE AS AN EXECUTABLE SPECIFICATION

# scripts/verify_release_contract.py (host-side validator)
assert preset_names == {"pc-debug", "pc-release", "pc-debug-render", "ps1-release"}
assert ps1_executable_size <= 512000
assert ps1_peak_main_ram <= 1572864
assert ps1_peak_vram <= 819200
assert w0_framebuffer_hash == canonical_w0_hash
assert w1_pc_fps >= 60
assert w1_ps1_emulator_fps >= 30
assert all_asset_validation_cases_pass
assert no_renderer_overflow_is_silent

A release is not valid merely because all binaries compile. The release job MUST execute the contract validator and publish its result as a CI artifact. Manual hardware validation remains explicitly separate from the emulator gate.

#### 1.2.10. CODE EXAMPLE STATUS RULE

Every code block in this document MUST be labeled \`[NORMATIVE]\`, \`[REFERENCE]\`, \`[ILLUSTRATIVE]\`, or \`[HISTORICAL]\`. \`[NORMATIVE]\` blocks MUST compile or be directly translatable into a test/validator. \`[HISTORICAL]\` blocks MUST NOT appear in the canonical implementation sections.

## 2. ОБЩАЯ АРХИТЕКТУРА (СЛОИ И МОДУЛИ)

> Движок организован в 4 слоя. Зависимости идут строго сверху вниз.

+-------------------------------------------------------------------+

\| СЛОЙ 4: ИГРА (Game Layer) \|

\| - Game state, уровни, скрипты (только C++) \|

\| - Использует API Слоёв 1-3 \|

+-------------------------------------------------------------------+

\|

v

+--------------------------------------------------------------------+

\| СЛОЙ 3: СИСТЕМЫ (Systems Layer) \|

\| - RenderSystem, MovementSystem, CollisionSystem\|

\| - Работают с ECS (Registry) \|

+---------------------------------------------------------------------+

\|

v

+----------------------------------------------------------------------+

\| СЛОЙ 2: СЕРВИСЫ (Services Layer) \|

\| - Renderer, Audio, Input, FileSystem, AssetLoader \|

\| - Математика, память, логирование \|

+----------------------------------------------------------------------+

\|

v

+----------------------------------------------------------------------+

\| СЛОЙ 1: HAL (Hardware Abstraction Layer) \|

\| - Platform_*, Render_*, Audio_*, Input_* \|

\| - Реализации: PC (SDL3+OpenGL), PS1 (PSn00bSDK) \|

+-----------------------------------------------------------------------+

\|

v

[ ЖЕЛЕЗО / ОС ]

ПРАВИЛО ЗАВИСИМОСТЕЙ

- Слой N может использовать только Слои < N.

- Слой 1 (HAL) не знает о существовании Слоёв 2-4.

- Слой 2 (Сервисы) не знает о конкретных платформах.- Слой 3 (Системы) не знает о железе.

- Слой 4 (Игра) не знает о ECS-реализации (только API).

МОДУЛИ ДВИЖКА

> core/ - ECS, математика, память, логирование, утилиты hal/ - платформенно-зависимый код (PC, PS1) render/ - рендерер (общий код + платформенные бэкенды) audio/ - аудио (заглушка для MVP) input/ - система ввода io/ - файловая система, стриминг assets/ - загрузка и управление ассетами game/ - игровая логика (примеры) tools/ - конвертер ассетов (C++14 утилита)

## 3. HARDWARE ABSTRACTION LAYER (HAL)

3.1. ПРИНЦИПЫ HAL

> HAL решает главную проблему кроссплатформенности: как писать единый код ядра, который работает на PC и PS1 без изменений.

ТРИ ВОЗМОЖНЫХ ПОДХОДА

1)

> \+ Гибкость, можно менять бэкенды в runtime

- Накладные расходы на каждый вызов (vtable lookup)

- Нельзя inline

- НЕ ПОДХОДИТ для ретро-железа

2) Compile-time полиморфизм (шаблоны C++):

> \+ 0 накладных расходов

- Сложный код, долгое время компиляции

- Частичная специализация ограничена

- НЕ ПОДХОДИТ как основной механизм

3) Препроцессор (#ifdef PLATFORM_PC / PLATFORM_PS1):

> \+ 0 накладных расходов
>
> \+ Простота
>
> \+ Явно видно, какой код для какой платформы

- Риск "лапши" из #ifdef

- ПОДХОДИТ для MVP (используем этот подход)

ИТОГОВОЕ РЕШЕНИЕ

> Используем compile-time platform selection через CMake: src/hal/pc/*.cpp или src/hal/ps1/*.cpp выбираются на этапе конфигурации. Общий код обращается только к public service/HAL headers; распространённые #ifdef PLATFORM_PC / PLATFORM_PS1 в common .cpp/.h не используются.

Преимущество: нулевые runtime dispatch costs без размазывания preprocessor-веток по ядру. Инвариант: в одном build tree компилируется ровно один backend.

```cpp
// platform.h — единый заголовочный файл для ядра

#pragma once

#include <stdint.h>

#include <stddef.h>

// ==== Инициализация ====

bool Platform_Init(void);

void Platform_Shutdown(void);

// ==== Главный цикл ====

bool Platform_PumpEvents(void); // Обработка событий окна/ввода

void Platform_SwapBuffers(void); // Presentation кадра

void Platform_SleepMs(uint32_t ms); // Ожидание

uint32_t Platform_GetTimeMs(void); // Текущее время (для delta time)

// ==== Рендеринг (общий API) ====

bool Render_Init(uint32_t width, uint32_t height);

void Render_Shutdown(void);

void Render_BeginFrame(void);

void Render_EndFrame(void);

void Render_Clear(uint8_t r, uint8_t g, uint8_t b);

// Работа с текстурами

typedef uint32_t TextureHandle;

TextureHandle Render_CreateTexture(uint32_t w, uint32_t h,

const uint8_t* pixels, // 8-bit indexed

const uint16_t* palette); // RGB555+STP, 256 цветов

void Render_DestroyTexture(TextureHandle handle);

void Render_BindTexture(TextureHandle handle);

// Отрисовка

void Render_SetCamera(const struct Mat4* view, const struct Mat4* proj);

typedef struct {

fixed_t x, y, z; // Engine-space signed 16.16

int8_t nx, ny, nz; // Нормаль (сжатая)

uint8_t u, v; // UV (0..255 -> 0.0..1.0)

uint8_t r, g, b, a; // Цвет вершины

} Vertex;

typedef struct {

uint16_t v0, v1, v2;

} Triangle;

void Render_DrawMesh(const Vertex* vertices, uint32_t vertex_count,

const Triangle* indices, uint32_t index_count,

const struct Mat4* model);

// ==== Ввод ====

typedef enum {

INPUT_KEY_W, INPUT_KEY_A, INPUT_KEY_S, INPUT_KEY_D,

INPUT_KEY_SPACE, INPUT_KEY_ESCAPE,

// ... другие клавиши

INPUT_KEY_COUNT

} KeyCode;

typedef enum {

INPUT_PAD_UP, INPUT_PAD_DOWN, INPUT_PAD_LEFT, INPUT_PAD_RIGHT,

INPUT_PAD_CROSS, INPUT_PAD_CIRCLE, INPUT_PAD_SQUARE, INPUT_PAD_TRIANGLE,

INPUT_PAD_L1, INPUT_PAD_R1, INPUT_PAD_L2, INPUT_PAD_R2,

INPUT_PAD_START, INPUT_PAD_SELECT,

INPUT_PAD_COUNT

} PadButton;

bool Input_IsKeyDown(KeyCode key);

bool Input_IsPadButtonDown(uint32_t pad_index, PadButton button);

int16_t Input_GetPadAxis(uint32_t pad_index, uint32_t axis); // -128..127

// ==== Аудио (заглушка для MVP) ====

bool Audio_Init(void);

void Audio_Shutdown(void);

void Audio_PlayBeep(uint32_t frequency, uint32_t duration_ms);

// ==== Файловая система ====

typedef struct {

uint8_t* data;

uint32_t size;

} FileData;

FileData FileSystem_LoadFile(const char* path);

void FileSystem_FreeFile(FileData* file);
```
#### Runtime polymorphism отвергнут для hot-path интерфейсов MVP; C++ templates остаются допустимым локальным механизмом compile-time specialization, но не являются HAL framework.

```cpp
// hal/pc/platform_pc.cpp

#include <SDL3/SDL.h>

#include <SDL3/SDL_main.h>

#include <glad/gl.h>

static SDL_Window* g_window = nullptr;

static SDL_GLContext g_gl_context = nullptr;

bool Platform_Init(void) {

if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {

return false;

}

// OpenGL attributes MUST be set before window creation.

SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);

SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

g_window = SDL_CreateWindow(

"RetroEngine", 1280, 960,

SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

if (!g_window) return false;

g_gl_context = SDL_GL_CreateContext(g_window);

if (!g_gl_context) return false;

if (!SDL_GL_MakeCurrent(g_window, g_gl_context)) return false;

// Load OpenGL 3.3 entry points before using them.

if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) return false;

return true;

}

void Platform_Shutdown(void) {

if (g_gl_context) SDL_GL_DestroyContext(g_gl_context);

if (g_window) SDL_DestroyWindow(g_window);

SDL_Quit();

}

void Platform_SwapBuffers(void) {

SDL_GL_SwapWindow(g_window);

}
```
### 3.4. РЕАЛИЗАЦИЯ ДЛЯ PS1 (PSn00bSDK)

```cpp
// hal/ps1/platform_ps1.cpp

#include <psxetc.h>

#include <psxgpu.h>

#include <psxgte.h>

#include <psxcd.h>

#include <psxpad.h>

static const int SCREEN_XRES = 320;

static const int SCREEN_YRES = 240;

bool Platform_Init(void) {

ResetGraph(0);

InitGeom();

SetGeomOffset(SCREEN_XRES / 2, SCREEN_YRES / 2);

SetGeomScreen(SCREEN_XRES / 2);

// Display/draw environments and the Ordering Table are initialized by Render_Init.

// CD and pad services are initialized by their owning services.

return true;

}

void Platform_Shutdown(void) { /* release engine-owned platform resources */ }
```
### 3.5. КОМПИЛЯЦИЯ И УСЛОВНАЯ КОМПИЛЯЦИЯ

```cmake
# root CMakeLists.txt (canonical architecture)

cmake_minimum_required(VERSION 3.25)

project(retro_engine LANGUAGES C CXX ASM)

option(PLATFORM_PC "Build PC backend" OFF)

option(PLATFORM_PS1 "Build PS1 backend" OFF)

if(PLATFORM_PC AND PLATFORM_PS1)

message(FATAL_ERROR "Select exactly one target platform")

elseif(NOT PLATFORM_PC AND NOT PLATFORM_PS1)

message(FATAL_ERROR "Select exactly one target platform")

endif()

if(PLATFORM_PC)

add_subdirectory(src/third_party/sdl3 EXCLUDE_FROM_ALL)

endif()

add_subdirectory(src)

# Asset converter is a host tool; it is never compiled with the PS1 cross compiler.

# MVP PS1 builds consume versioned/generated example assets from assets/build/.

if(PLATFORM_PC)

add_subdirectory(tools/asset_converter)

endif()

add_subdirectory(examples/hello_triangle)

add_subdirectory(examples/rotating_cube)

if(PLATFORM_PC)

# Canonical PC runtime targets are the documented demo executables.

# The release workflow packages rotating_cube as the reference PC runtime artifact.

set_target_properties(hello_triangle rotating_cube PROPERTIES

RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/game")

add_subdirectory(tests)

endif()
```
## 4. МАТЕМАТИЧЕСКОЕ ЯДРО (MATH CORE)

### 4.1. FIXED-POINT 16.16 --- БАЗОВЫЙ ТИП

### OVERFLOW / ROUNDING CONTRACT

fixed_add, fixed_sub, fixed_mul and fixed_div use saturating results at the int32_t fixed_t limits. fixed_mul uses a signed 64-bit intermediate and scales by 2\^16 with truncation toward zero. Matrix/vector reductions MUST use a wider accumulator for the complete reduction and clamp exactly once when converting the final result to fixed_t. fixed_div treats a zero divisor as a contract violation: debug builds assert; release builds return INT32_MAX for non-negative a and INT32_MIN for negative a.

> Почему Fixed-Point 16.16:

- PS1/Saturn/Jaguar не имеют IEEE-754 FPU на основном CPU; soft-float cost должен подтверждаться benchmark-ом, без универсального коэффициента 10-50x.

- 16.16: signed int32_t with 16 fractional bits; representable range is [-32768.0, 32767.9999847412...] with quantum 1/65536 (\~0.0000152588).

- Для ретро-графики 320x240 это избыточная точность.

- Упрощает cross-platform determinism; для bitwise determinism дополнительно фиксируются overflow/rounding, iteration order, RNG и tie-break rules.

- GTE использует несколько фиксированных форматов и разрядностей; engine-level fixed_t 16.16 преобразуется в GTE-native representation через отдельный backend adapter.

```cpp
// core/math/fixed.h

#pragma once

#include <stdint.h>

#include <limits.h>

#include <assert.h>

#include <math.h>

typedef int32_t fixed_t; // signed 16.16

#define FIXED_BITS 16

#define FIXED_ONE (INT32_C(1) << FIXED_BITS)

static inline fixed_t fixed_clamp_i64(int64_t v) {

if (v > INT32_MAX) return INT32_MAX;

if (v < INT32_MIN) return INT32_MIN;

return (fixed_t)v;

}

// Deterministic contract: add/sub/mul/div saturate to int32_t range.

static inline fixed_t fixed_from_int(int32_t v) { return fixed_clamp_i64((int64_t)v * FIXED_ONE); }

static inline fixed_t fixed_from_float(float v) {

if (!isfinite(v)) return 0;

double scaled = (double)v * (double)FIXED_ONE;

if (scaled >= (double)INT32_MAX) return INT32_MAX;

if (scaled <= (double)INT32_MIN) return INT32_MIN;

return (fixed_t)scaled; // truncation toward zero

}

static inline int32_t fixed_to_int(fixed_t v) { return v / FIXED_ONE; }

static inline float fixed_to_float(fixed_t v) { return (float)v / (float)FIXED_ONE; }

static inline fixed_t fixed_add(fixed_t a, fixed_t b) { return fixed_clamp_i64((int64_t)a + (int64_t)b); }

static inline fixed_t fixed_sub(fixed_t a, fixed_t b) { return fixed_clamp_i64((int64_t)a - (int64_t)b); }

static inline fixed_t fixed_mul(fixed_t a, fixed_t b) { return fixed_clamp_i64(((int64_t)a * (int64_t)b) / FIXED_ONE); }

static inline fixed_t fixed_div(fixed_t a, fixed_t b) {

if (b == 0) { assert(!"fixed_div: division by zero"); return a >= 0 ? INT32_MAX : INT32_MIN; }

return fixed_clamp_i64(((int64_t)a * FIXED_ONE) / b);

}

fixed_t fixed_sqrt(fixed_t v);
```
### 4.2. ВЕКТОРЫ (Vec2, Vec3, Vec4)

```cpp
// core/math/vec3.h

#pragma once

#include "fixed.h"

typedef struct { fixed_t x, y, z; } Vec3;

inline Vec3 vec3_make(fixed_t x, fixed_t y, fixed_t z) { return Vec3{x, y, z}; }

inline Vec3 vec3_add(Vec3 a, Vec3 b) { return Vec3{fixed_add(a.x,b.x), fixed_add(a.y,b.y), fixed_add(a.z,b.z)}; }

inline Vec3 vec3_sub(Vec3 a, Vec3 b) { return Vec3{fixed_sub(a.x,b.x), fixed_sub(a.y,b.y), fixed_sub(a.z,b.z)}; }

inline Vec3 vec3_scale(Vec3 v, fixed_t s) { return Vec3{fixed_mul(v.x,s), fixed_mul(v.y,s), fixed_mul(v.z,s)}; }

inline fixed_t fixed_reduce3_products(fixed_t a0, fixed_t a1, fixed_t a2,

fixed_t b0, fixed_t b1, fixed_t b2) {

// Keep the full Q16.16 products until the complete reduction is formed.

const int64_t acc = (int64_t)a0 * b0

+ (int64_t)a1 * b1

+ (int64_t)a2 * b2;

return fixed_clamp_i64(acc / FIXED_ONE);

}

inline fixed_t vec3_dot(Vec3 a, Vec3 b) {

return fixed_reduce3_products(a.x, a.y, a.z, b.x, b.y, b.z);

}

inline Vec3 vec3_cross(Vec3 a, Vec3 b) {

const int64_t ax = (int64_t)a.y * b.z - (int64_t)a.z * b.y;

const int64_t ay = (int64_t)a.z * b.x - (int64_t)a.x * b.z;

const int64_t az = (int64_t)a.x * b.y - (int64_t)a.y * b.x;

return Vec3{

fixed_clamp_i64(ax / FIXED_ONE),

fixed_clamp_i64(ay / FIXED_ONE),

fixed_clamp_i64(az / FIXED_ONE)

};

}

inline fixed_t vec3_length_sq(Vec3 v) { return vec3_dot(v,v); }

fixed_t vec3_length(Vec3 v);

Vec3 vec3_normalize(Vec3 v);
```
### 4.3. МАТРИЦЫ (Mat4)

```cpp
// Матрица 4x4 в column-major порядке (как в OpenGL)

typedef struct {

fixed_t m[16];

} Mat4;

void mat4_identity(Mat4* out);

void mat4_multiply(Mat4* out, const Mat4* a, const Mat4* b);

void mat4_translate(Mat4* out, fixed_t x, fixed_t y, fixed_t z);

void mat4_rotate_x(Mat4* out, fixed_t angle);

void mat4_rotate_y(Mat4* out, fixed_t angle);

void mat4_rotate_z(Mat4* out, fixed_t angle);

void mat4_perspective(Mat4* out, fixed_t fov, fixed_t aspect,

fixed_t near, fixed_t far);

void mat4_look_at(Mat4* out, Vec3 eye, Vec3 target, Vec3 up);

Vec4 mat4_transform(const Mat4* m, Vec4 v);
```
### 4.4. АППАРАТНОЕ УСКОРЕНИЕ (GTE НА PS1)

> GTE (Geometry Transformation Engine) --- специализированный PS1 coprocessor. Его registers используют operation-specific fixed-point formats; engine Mat4/fixed_t не передаются в GTE как raw memory layout.
>
> ОПЕРАЦИИ GTE:

- RTPS/RTPT: perspective transformation pipeline для одной/трёх вершин с использованием GTE rotation/translation/projection state.

- NCLIP: вычисление signed screen-space area для определения ориентации полигона; используется backend для backface rejection.

- NCS/NCCT: normal color calculation / lighting operations.

- INTPL: interpolation operation in the GTE color pipeline.

- SQR: component-wise square operation.

- CC: color calculation / lighting operation.

> ИСПОЛЬЗОВАНИЕ В ДВИЖКЕ:

```cpp
#ifdef PLATFORM_PS1

struct GteTransform {

GteMatrix3x3 rotation;

GteVector3 translation;

int32_t ofx;

int32_t ofy;

int32_t h;

};

// Convert engine-space transform to GTE-native state.

// Do NOT cast Mat4* directly to a GTE matrix.

inline void gte_transform_triangle(

const Vec3* v0, const Vec3* v1, const Vec3* v2,

const GteTransform& gte,

ScreenVertex* out_v0, ScreenVertex* out_v1, ScreenVertex* out_v2) {

gte_SetRotMatrix(&gte.rotation);

gte_SetTransMatrix(&gte.translation);

gte_SetDQA_DQB(/* backend-specific projection parameters */);

SVECTOR gv0, gv1, gv2;

backend_convert_vertex_to_gte(v0, &gv0);

backend_convert_vertex_to_gte(v1, &gv1);

backend_convert_vertex_to_gte(v2, &gv2);

gte_ldv3(&gv0, &gv1, &gv2);

gte_rtpt();

gte_stsxy3(&out_v0->x, &out_v1->x, &out_v2->x);

gte_stsz3(&out_v0->z, &out_v1->z, &out_v2->z);

}

#endif
```
### 4.5. ТЕСТИРОВАНИЕ ТОЧНОСТИ

> КРИТИЧНО: Fixed-point код должен давать те же результаты на PC и PS1.

```cpp
// tests/math_test.cpp

#include <gtest/gtest.h>

#include "core/math/fixed.h"

#include <cmath>

TEST(FixedPoint, MulAccuracy) {

const int N = 10000;

double max_error = 0.0;

for (int i = 0; i < N; ++i) {

float a = (rand() / (float)RAND_MAX) * 100.0f - 50.0f;

float b = (rand() / (float)RAND_MAX) * 100.0f - 50.0f;

fixed_t fa = fixed_from_float(a);

fixed_t fb = fixed_from_float(b);

fixed_t fc = fixed_mul(fa, fb);

float result = fixed_to_float(fc);

float expected = a * b;

double error = std::abs(result - expected);

if (error > max_error) max_error = error;

}

// Погрешность должна быть < 0.001 для 16.16

EXPECT_LT(max_error, 0.001);

}

TEST(FixedPoint, Determinism) {

// Fixed raw vectors are compared against platform-independent expected raw results; this test is a single vector example.

fixed_t a = fixed_from_int(123);

fixed_t b = fixed_from_int(456);

fixed_t c = fixed_mul(a, b);

EXPECT_EQ(c, fixed_from_int(123 * 456));

}
```
## 5. MEMORY MANAGEMENT

ALLOCATOR SAFETY CONTRACT: arena checks MUST be overflow-safe (compare against remaining capacity rather than using m_offset + size); StackAllocator::pop MUST validate that the marker belongs to the same arena; PoolAllocator::deallocate MUST reject pointers outside the pool in debug builds. No allocator may form an out-of-range pointer before validation.

### 5.1. МОДЕЛЬ ПАМЯТИ КОНСОЛЕЙ

> Для PS1 target нет полноценной desktop-style VM/runtime memory isolation; конкретные firmware/SDK capabilities зависят от platform. Не использовать это утверждение как универсальное правило всех 5-го поколения.

- Операционной системы

- Виртуальной памяти

- Стандартного malloc (или он очень плохой)

- Защиты памяти (segfault может не произойти, просто мусор)

Следствия:

- Движок сам управляет всей памятью.

- Утечки и fragmentation являются проектными рисками; время до отказа зависит от workload и не фиксируется одним числом.

- Фрагментация может исчерпать доступную память; время до отказа зависит от workload.

- Нужны детерминированные аллокаторы.

### 5.2. LINEAR ALLOCATOR (BUMP)

> ИСПОЛЬЗОВАНИЕ: Загрузка уровней, ассетов.

```cpp
class LinearAllocator {

public:

struct Marker {

const LinearAllocator* owner;

size_t offset;

};

LinearAllocator(void* buffer, size_t capacity)

: m_buffer(static_cast<uint8_t*>(buffer)), m_capacity(capacity), m_offset(0) {}

void* allocate(size_t size, size_t alignment = 8) {

if (!m_buffer || alignment == 0 || (alignment & (alignment - 1)) != 0) return nullptr;

const uintptr_t base = reinterpret_cast<uintptr_t>(m_buffer);

if (m_offset > m_capacity) return nullptr;

const uintptr_t current = base + m_offset;

const size_t padding = static_cast<size_t>((alignment - (current & (alignment - 1))) & (alignment - 1));

const size_t remaining = m_capacity - m_offset;

if (padding > remaining) return nullptr;

if (size > remaining - padding) return nullptr;

void* ptr = m_buffer + m_offset + padding;

m_offset += padding + size;

return ptr;

}

Marker mark() const { return Marker{this, m_offset}; }

bool rollback(Marker marker) {

if (marker.owner != this) return false;

if (marker.offset > m_offset || marker.offset > m_capacity) return false;

m_offset = marker.offset;

return true;

}

void reset() { m_offset = 0; }

size_t used() const { return m_offset; }

size_t remaining() const { return m_offset <= m_capacity ? m_capacity - m_offset : 0; }

private:

uint8_t* m_buffer;

size_t m_capacity;

size_t m_offset;

};

// Arena contract: the returned address, not merely the offset, satisfies alignment.
```
> ПРИМЕР ИСПОЛЬЗОВАНИЯ:

```text
// При загрузке уровня

// Пример показывает arena в static storage; мегабайтные буферы не размещаются на CPU stack.

static uint8_t level_buffer[448 * 1024]; // 1 MiB, выделяется политикой memory subsystem

LinearAllocator level_alloc(level_buffer, sizeof(level_buffer));

Mesh* mesh = (Mesh*)level_alloc.allocate(sizeof(Mesh));

Texture* tex = (Texture*)level_alloc.allocate(sizeof(Texture));

// ... загрузка данных ...

// При смене уровня

level_alloc.reset(); // Освободили всю память уровня за O(1)
```
### 5.3. POOL ALLOCATOR

> ИСПОЛЬЗОВАНИЕ: Динамические объекты (пули, враги, частицы).

```cpp
template<typename T, size_t N>

class Pool {

public:

static_assert(N > 0 && N <= 65535, "Pool capacity must fit uint16_t free-list index");

Pool() : m_free_head(0), m_used_count(0) {

for (size_t i = 0; i + 1 < N; ++i) m_indices[i] = static_cast<uint16_t>(i + 1);

m_indices[N - 1] = 0xFFFF;

for (size_t i = 0; i < N; ++i) m_allocated[i] = false;

}

T* allocate() {

if (m_free_head == 0xFFFF) return nullptr;

const uint16_t idx = m_free_head;

m_free_head = m_indices[idx];

m_allocated[idx] = true;

++m_used_count;

return &m_objects[idx];

}

bool deallocate(T* ptr) {

if (!ptr) return false;

const uintptr_t p = reinterpret_cast<uintptr_t>(ptr);

const uintptr_t begin = reinterpret_cast<uintptr_t>(&m_objects[0]);

const uintptr_t end = begin + sizeof(m_objects);

if (p < begin || p >= end) return false;

const uintptr_t byte_offset = p - begin;

if ((byte_offset % sizeof(T)) != 0) return false;

const size_t idx = static_cast<size_t>(byte_offset / sizeof(T));

if (idx >= N || !m_allocated[idx]) return false;

m_allocated[idx] = false;

m_indices[idx] = m_free_head;

m_free_head = static_cast<uint16_t>(idx);

--m_used_count;

return true;

}

template<typename Func>

void for_each(Func&& func) {

for (size_t i = 0; i < N; ++i) if (m_allocated[i]) func(m_objects[i]);

}

private:

T m_objects[N];

uint16_t m_indices[N];

bool m_allocated[N];

uint16_t m_free_head;

size_t m_used_count;

};

// Safety contract: foreign pointers, interior pointers and double frees are rejected without UB.
```
> ПРИМЕР:

```text
Pool<Bullet, 256> bullets; // Максимум 256 пуль одновременно

void spawn_bullet(Vec3 pos, Vec3 dir) {

Bullet* b = bullets.allocate();

if (!b) return; // Пул полон — не стреляем

b->position = pos;

b->direction = dir;

b->lifetime = 5000; // 5 секунд

}
```
### 5.4. STACK ALLOCATOR (ДЛЯ КАДРОВ)

ИСПОЛЬЗОВАНИЕ: Временные данные в рамках одного кадра (сортировка полигонов, трансформации)

```cpp
class StackAllocator {

public:

struct Marker {

const StackAllocator* owner;

size_t offset;

};

StackAllocator(void* buffer, size_t capacity)

: m_buffer(static_cast<uint8_t*>(buffer)), m_capacity(capacity), m_offset(0) {}

void* push(size_t size, size_t alignment = 8) {

if (!m_buffer || alignment == 0 || (alignment & (alignment - 1)) != 0) return nullptr;

if (m_offset > m_capacity) return nullptr;

const uintptr_t base = reinterpret_cast<uintptr_t>(m_buffer);

const uintptr_t current = base + m_offset;

const size_t padding = static_cast<size_t>((alignment - (current & (alignment - 1))) & (alignment - 1));

const size_t remaining = m_capacity - m_offset;

if (padding > remaining || size > remaining - padding) return nullptr;

void* ptr = m_buffer + m_offset + padding;

m_offset += padding + size;

return ptr;

}

Marker mark() const { return Marker{this, m_offset}; }

bool pop(Marker marker) {

if (marker.owner != this) return false;

if (marker.offset > m_offset || marker.offset > m_capacity) return false;

m_offset = marker.offset;

return true;

}

void reset_frame() { m_offset = 0; }

private:

uint8_t* m_buffer;

size_t m_capacity;

size_t m_offset;

};
```
### 5.5. СТРАТЕГИЯ РАЗМЕЩЕНИЯ ДАННЫХ

 ТИП ДАННЫХ АЛЛОКАТОР ВРЕМЯ ЖИЗНИ
 -------------------------- ------------------ --------------
 Менеджеры (синглтоны) Static (global) Весь runtime
 ECS Registry Static (global) Весь runtime
 Уровень (меши, текстуры) Linear Allocator Один уровень
 Пули, враги Pool Allocator Динамически
 Временные буферы кадра Stack Allocator Один кадр
 Загруженные файлы Linear Allocator До выгрузки

### 5.6. MEMORY MAP PS1

// scripts/build_ps1.bat

\@echo off

setlocal

set PSN00BSDK_ROOT=C:\\PSn00bSDK

set PSN00BSDK_LIBS=%PSN00BSDK_ROOT%\\lib\\libpsn00b

if not exist "%PSN00BSDK_LIBS%\\cmake\\sdk.cmake" (

echo ERROR: PSn00bSDK v0.24 CMake toolchain not found.

exit /b 1

)

cmake --preset ps1-release

if errorlevel 1 exit /b 1

cmake --build --preset ps1-release

if errorlevel 1 exit /b 1

echo Build successful.

endlocal

// scripts/run_pc.bat

\@echo off

setlocal

set "EXE_PATH=build\\pc-release\\game\\rotating_cube.exe"
if not exist "%EXE_PATH%" (echo ERROR: PC example not found: %EXE_PATH% & exit /b 1)
start "RetroEngine PC" "%EXE_PATH%"
endlocal

// scripts/run_ps1.bat

\@echo off

setlocal

set EMU_PATH=C:\\emulators\\DuckStation\\duckstation-qt-x64.exe

set "DISC_PATH=build\\ps1-release\\retro-engine.cue"

if not exist "%EMU_PATH%" (echo ERROR: DuckStation not found at %EMU_PATH% & exit /b 1)

if not exist "%DISC_PATH%" (echo ERROR: PS1 disc image not found: %DISC_PATH% & exit /b 1)

"%EMU_PATH%" "%DISC_PATH%"

set RESULT=%ERRORLEVEL%

endlocal & exit /b %RESULT%

------------------------------------------------------------------------

АДРЕСНОЕ ПРОСТРАНСТВО PS1 (упрощенно; VRAM отделена от CPU address space):
0x8000_0000 - 0x801F_FFFF : Main RAM (2 MiB physical; first 64 KiB contains BIOS-reserved/fixed-address data; user/engine RAM begins at physical 0x0001_0000)
0x1F80_0000 - 0x1F80_03FF : Scratchpad (1 KiB)
0x1F80_1000 - 0x1F80_1FFF : I/O Ports (4 KiB)
0x1F80_2000 - 0x1F80_3FFF : Expansion Region 2 / I/O (8 KiB)
BIOS ROM is mapped separately at the BIOS ROM address range and is not part of Main RAM. The engine MUST NOT treat the first 64 KiB of main RAM as engine-owned memory; actual startup/linker ownership is determined by the pinned PSn00bSDK v0.24 configuration.
VRAM: 1 MiB separate GPU memory; do not count it as Main RAM.

ПЛАНИРУЕМЫЙ БЮДЖЕТ Main RAM (максимальные резервы):

Runtime/SDK reserve: 128 KiB
CPU stack: 64 KiB
Static code/data: 256 KiB
ECS/dynamic pools: 256 KiB
Level linear arena: 448 KiB
Per-frame arena: 160 KiB
CD staging/cache: 64 KiB
Measurement/acceptance headroom: 160 KiB
Physical unallocated safety reserve: 512 KiB
ECS ledger: MAX_ENTITIES is the handle-domain limit, not the per-component population. Each ComponentPool keeps a 10,000-entry uint16_t sparse index (20 KiB) plus only its fixed-capacity dense data/entity arrays. MVP component capacities are: Position 1024, Rotation 1024, Velocity 1024, MeshRenderer 256, Camera 8, PlayerControl 8, Collider 256. The normative ECS budget includes these sparse indices, dense storage, pool metadata and registry slot state. With the capacities above, the seven component pools consume approximately 198 KiB for sparse+dense pool storage; allowing 10,000 entity lifecycle bytes and allocator/type metadata keeps the complete ECS reservation below the 256 KiB budget with a small fixed margin. Any capacity increase requires updating this ledger before implementation.

Acceptance target: peak Main RAM <= 1.5 MiB, inclusive of runtime/SDK, stack, static data and all engine arenas. Explicit engine arena capacities sum to 1376 KiB; up to 160 KiB of additional measured usage may be consumed within the 1536 KiB acceptance ceiling. The 256 KiB ECS/dynamic-pool reservation is a hard budget for the capacities listed above; it is not permission to instantiate all 10,000 entities in every component pool. A separate 512 KiB physical reserve remains unallocated and is not part of the engine budget. VRAM has a separate budget and is measured independently.

No component may silently consume the safety reserve. Arena exhaustion must produce an explicit out-of-memory diagnostic rather than expanding the arena.

## 6. ENTITY COMPONENT SYSTEM (ECS)

### 6.1. АРХИТЕКТУРА ECS

> ТРИ КЛЮЧЕВЫХ ЭЛЕМЕНТА:

1. ENTITY --- просто ID (uint32_t). Не содержит данных.

- 0 = invalid entity

- Верхние 8 бит = generation (для защиты от use-after-free)

- Нижние 24 бита = index

2. COMPONENT --- POD-структура с данными.

- Не содержит логики.

- Хранится в массиве (в Registry).

- Примеры: Position, Velocity, Mesh, Camera, PlayerControl.

3. SYSTEM --- функция, работающая с массивами компонентов.

- Не содержит состояния.

- Выполняется каждый кадр.

- Примеры: MovementSystem, RenderSystem, InputSystem.

### 6.2. REGISTRY

```cpp
// core/ecs/registry.h

#pragma once

#include <stdint.h>

#include <stddef.h>

template<typename T> struct ComponentTypeId;

template<typename... Components> class View;

using Entity = uint32_t;

constexpr Entity INVALID_ENTITY = 0;

constexpr size_t MAX_ENTITIES = 10000;

// DOD contract: no RTTI, virtual component arrays, std::unordered_map, or heap allocation in simulation hot paths.

// MVP tradeoff: the sparse entity-to-component index is sized to MAX_ENTITIES; total ComponentPool capacities are independently budgeted within ECS/dynamic-pool memory.

template<typename T, size_t Capacity>

class ComponentPool {

public:

static_assert(Capacity > 0, "Component capacity must be non-zero");

void add(Entity e, const T& value);

void remove(Entity e); // swap-back removal

T* get(Entity e);

bool has(Entity e) const;

private:

T m_data[Capacity];

Entity m_index_to_entity[Capacity];

uint16_t m_entity_to_index[MAX_ENTITIES]; // 20 KiB sparse index per pool; pool capacity remains fixed and budgeted separately

};

class Registry {

public:

Entity create_entity();

void destroy_entity(Entity entity);

template<typename T> void add_component(Entity entity, const T& component);

template<typename T> void remove_component(Entity entity);

template<typename T> T* get_component(Entity entity);

template<typename T> bool has_component(Entity entity) const;

template<typename... Components> View<Components...> view();

private:

Entity m_next_entity = 1;

};
```
#### Registry uses fixed-capacity component pools selected by ComponentTypeId. The MVP implementation has no RTTI, virtual dispatch, unordered_map or heap allocation in the simulation hot path.

Entity lifecycle contract: create_entity allocates a slot and returns its current generation; destroy_entity marks the slot dead and increments its generation before reuse. Every entity/component operation validates alive state and generation. If an 8-bit generation would wrap from 255, that slot is retired permanently and is never silently reused; this prevents stale-handle aliasing. A retired slot reduces available capacity but does not weaken handle-safety guarantees.

```cpp
// game/components.h

#pragma once

#include "core/math/vec3.h"

#include "core/math/fixed.h"

struct Position {

Vec3 value;

};

struct Rotation {

fixed_t yaw, pitch, roll; // Углы Эйлера в fixed-point

};

struct Velocity {

Vec3 value;

};

struct MeshRenderer {

uint32_t mesh_id;

uint32_t texture_id;

};

struct Camera {

fixed_t fov;

fixed_t near_plane;

fixed_t far_plane;

};

struct PlayerControl {

fixed_t move_speed;

fixed_t rotate_speed;

};

struct Collider {

Vec3 min;

Vec3 max;

};
```
Each ComponentPool<T> stores dense component data plus entity/index mappings. Swap-back removal keeps iteration contiguous.

#### Entity IDs contain generation + index; registry stores alive/generation state per slot and rejects stale handles before component access. Generation wrap is handled by retiring the slot rather than allowing uint8_t aliasing.

View contract: a multi-component View chooses a dense iteration order and exposes direct references/pointers to all requested components. It MUST NOT perform Registry::get_component() or any entity-to-component sparse lookup inside the iteration loop. The normative hot-path requirement is O(1) direct access per component after the iterator selects its dense row.

```cpp
class MovementSystem {

public:

static void update(Registry& registry, fixed_t delta_time) {

auto view = registry.view<Position, Velocity>();

for (auto it = view.begin(); it != view.end(); ++it) {

Position& pos = it.position();

Velocity& vel = it.velocity();

pos.value.x = fixed_add(pos.value.x, fixed_mul(vel.value.x, delta_time));

pos.value.y = fixed_add(pos.value.y, fixed_mul(vel.value.y, delta_time));

pos.value.z = fixed_add(pos.value.z, fixed_mul(vel.value.z, delta_time));

}

}

};
```
### 6.5. ИТЕРАЦИЯ И CACHE-FRIENDLY ДОСТУП

> КЛЮЧЕВАЯ ОПТИМИЗАЦИЯ:
>
> Когда мы итерируем по Position + Velocity, мы хотим, чтобы эти компоненты лежали в памяти последовательно.

```text
ComponentArray<Position>::m_data:

[ pos0 | pos1 | pos2 | pos3 | ... ] <- последовательно в памяти

ComponentArray<Velocity>::m_data:

[ vel0 | vel1 | vel2 | vel3 | ... ] <- последовательно в памяти
```
## 7. RENDER PIPELINE

### 7.1. ГРАФ КОНВЕЙЕРА

> СЦЕНА -> [TRANSFORM] -> [CLIP] -> [CULL] -> [SORT] -> [PACK] -> [SUBMIT] -> FRAMEBUFFER (GTE) (NCLIP) (OT) (DMA) (GPU)

1. TRANSFORM: engine-space model/view transform; projection-specific state формируется backend-ом.

- На PS1: backend adapter преобразует engine Mat4/fixed_t в GTE-native rotation/translation/projection state и использует RTPS/RTPT. Engine Mat4 нельзя передавать в GTE raw cast-ом.

- На PC: reference backend использует engine-level coordinate convention и затем эмулирует выбранные PS1 visual constraints.

2. CLIP: primitives, пересекающие near/far clip planes, клипуются или явно отбрасываются до CULL/SORT. MVP не допускает использования недействительных projected coordinates.

- На PS1: clipping выполняется engine/backend code; NCLIP используется для signed screen-space area/backface rejection и не заменяет frustum clipping.

- 3. CULL: Backface culling выполняется после clipping, когда это применимо.

4. SORT: Сортировка opaque polygons по bounded depth key для Painter's Algorithm. Intersecting/cyclic surfaces outside MVP visibility guarantee.

- На PS1: backend заполняет OT по depth buckets; GPU обходит OT после DMA.

- На PC: через std::sort с отключенным Z-буфером.

5. PACK: Упаковка backend-neutral render commands в фиксированный command buffer; overflow возвращает status=false и диагностируется.

- На PS1: GPU packet (структуры фиксированного формата).

- На PC: OpenGL draw calls.

6. SUBMIT: Backend преобразует команды в native GPU packets/OT entries и выполняет submission.

- На PS1: через DMA.

- На PC: через glDrawElements.

### 7.2. СОРТИРОВКА (PAINTER'S ALGORITHM)

> ПРОБЛЕМА:
>
> Без Z-буфера непрозрачные полигоны должны быть ordered back-to-front; при пересечении треугольников Painter's Algorithm не гарантирует корректный результат.
>
> РЕШЕНИЕ НА PC: сортировка по camera-space depth без modulo wrapping; глубина clamped в допустимый диапазон reference renderer.

```text
std::sort(triangles.begin(), triangles.end(),

[](const Triangle& a, const Triangle& b) {

if (a.depth != b.depth) return a.depth > b.depth; // Дальние сначала

return a.submission_index < b.submission_index; // Deterministic tie-breaker

});
```
> РЕШЕНИЕ НА PS1 (Ordering Table): mapping camera depth -> OTZ -> bounded OT bucket. Дублирующие depth values допускаются; overflow глубины не должен приводить к modulo aliasing.

```cpp
// OT — массив указателей на списки полигонов

// Engine policy: depth_key is the clamped camera-space depth mapped to OT buckets.

// Equal depth values are resolved by submission_index before insertion; OT insertion must preserve that order.

// Index 0 is near; index (ot_size - 1) is far.

// Depth outside the configured range is rejected or clamped before indexing; modulo wrapping is forbidden.

bool render_submit_ot(GPU_OT* ot, uint32_t ot_size) {

if (!ot || ot_size == 0) {

renderer_diagnostic(RENDER_DIAG_INVALID_OT);

return false;

}

for (const Triangle& tri : triangles) {

const int32_t raw_depth = tri.depth;

if (raw_depth < 0) {

renderer_diagnostic(RENDER_DIAG_INVALID_DEPTH);

return false;

}

const uint32_t depth =

static_cast<uint32_t>(raw_depth) < ot_size

? static_cast<uint32_t>(raw_depth)

: (ot_size - 1u);

if (!gpu_add_to_ot(ot, depth, &tri.packet)) return false;

}

// DMA передаёт OT в GPU после полной deterministic insertion.

gpu_draw_ot(ot);

return true;

}
```
### 7.3. ПАКЕТИРОВАНИЕ КОМАНД (COMMAND BUFFER)

> ПРОБЛЕМА:
>
> Вызовы submission должны быть батчированы там, где backend это допускает; command buffer является engine-side staging structure, а PS1 GPU packet/OT layout остаётся backend-specific.
>
> РЕШЕНИЕ:
>
> Собираем renderer commands за кадр. На PS1 команды преобразуются в backend-native GPU packets/OT entries; на PC command buffer не означает один literal GPU call.

```cpp
// render/command_buffer.h

class CommandBuffer {

public:

bool draw_mesh(const Mesh* mesh, const Mat4* mvp) {

DrawMeshCmd* cmd = allocate<DrawMeshCmd>();

if (!cmd) { m_overflowed = true; return false; }

cmd->type = CMD_DRAW_MESH;

cmd->mesh = mesh;

cmd->mvp = *mvp;

return true;

}

bool overflowed() const { return m_overflowed; }

void submit() {

// PC: consume commands and issue backend draw submissions.

// PS1: convert commands to GPU packets + OT entries and submit through the SDK/DMA path.

}

private:

uint8_t m_buffer[64 * 1024];

size_t m_offset = 0;

bool m_overflowed = false;

};
```
### 7.4. ОТРИСОВКА НА PC (OPENGL С ЭМУЛЯЦИЕЙ PS1)

> КЛЮЧЕВЫЕ ШЕЙДЕРЫ:

```glsl
// shaders/ps1_emu.vert

#version 330 core

layout(location = 0) in vec3 a_position;

layout(location = 1) in vec2 a_texcoord;

layout(location = 2) in vec4 a_color;

uniform mat4 u_mvp;

noperspective out vec2 v_texcoord;

out vec4 v_color;

void main() {

vec4 clip = u_mvp * vec4(a_position, 1.0);

vec2 ndc = clip.xy / clip.w;

// Optional PS1-style wobble emulation heuristic: 1/16-pixel screen-space quantization; this is not the native GTE SXY storage format.

vec2 screen = (ndc * 0.5 + 0.5) * vec2(320.0, 240.0);

screen = floor(screen * 16.0 + 0.5) / 16.0;

ndc = screen / vec2(320.0, 240.0) * 2.0 - 1.0;

clip.xy = ndc * clip.w;

gl_Position = clip;

v_texcoord = a_texcoord; // noperspective => affine interpolation

v_color = a_color;

}

// shaders/ps1_emu.frag

#version 330 core

in vec2 v_texcoord;

in vec4 v_color;

uniform sampler2D u_texture_index;

uniform sampler2D u_palette;

out vec4 frag_color;

void main() {

uint index = uint(round(texture(u_texture_index, v_texcoord).r * 255.0));

vec3 color = texelFetch(u_palette, ivec2(int(index), 0), 0).rgb;

frag_color = vec4(color * v_color.rgb, 1.0);

}
```
### 7.5. ОТРИСОВКА НА PS1 (GPU + ORDERING TABLE)

```cpp
// hal/ps1/render_gpu.cpp

// Reference implementation skeleton (not a standalone compilation unit). It shows the required backend sequencing; wrapper names are engine-local; SDK symbol names

// are pinned by PSn00bSDK v0.24 inside this adapter.

// W0/W1 use Gouraud shading; the PS1 path therefore emits the GT3 packet and copies per-vertex RGB.

// Transparency is disabled by the MVP workload, so STP is not interpreted in this path.

struct ClippedPolygon {

Vertex v[6];

uint32_t count;

};

// Contract: clip_triangle_frustum_viewspace performs finite near/far clipping before projection.

// The convex clipped polygon contains 0..6 vertices and is triangulated into a fan before GTE submission. CLIP MUST interpolate position, UV, color and normal attributes for generated vertices.

bool clip_triangle_frustum_viewspace(const Vertex* v0, const Vertex* v1, const Vertex* v2, ClippedPolygon* out);

bool render_draw_triangle_ps1(const Vertex* v0, const Vertex* v1, const Vertex* v2,

TextureHandle tex) {

SVECTOR gv0, gv1, gv2;

ClippedPolygon clipped;

if (!clip_triangle_frustum_viewspace(v0, v1, v2, &clipped)) return true; // rejected geometry

if (clipped.count < 3) return true;

// Triangulate the clipped convex polygon as (0, i, i+1). Each generated triangle

// repeats the normal GTE projection/backface/depth/packet path below.

for (uint32_t i = 1; i + 1 < clipped.count; ++i) {

backend_convert_vertex_to_gte(&clipped.v[0], &gv0);

backend_convert_vertex_to_gte(&clipped.v[i], &gv1);

backend_convert_vertex_to_gte(&clipped.v[i + 1], &gv2);

gte_ldv3(&gv0, &gv1, &gv2);

gte_rtpt();

gte_stsxy3(&screen_x0, &screen_x1, &screen_x2);

gte_stsz3(&z0, &z1, &z2);

// Frustum clipping is completed in view space before this GTE projection step. NCLIP only performs signed screen-space area/backface rejection.

gte_nclip();

gte_stopz(&signed_area);

if (signed_area <= 0) continue; // culled, not an error

POLY_GT3 packet;

setPolyGT3(&packet);

setTexPage(&packet, backend_texture_page(tex));

setClut(&packet, backend_texture_clut(tex));

packet.x0 = screen_x0; packet.y0 = screen_y0;

packet.x1 = screen_x1; packet.y1 = screen_y1;

packet.x2 = screen_x2; packet.y2 = screen_y2;

packet.u0 = clipped.v[0].u; packet.v0 = clipped.v[0].v;

packet.u1 = clipped.v[i].u; packet.v1 = clipped.v[i].v;

packet.u2 = clipped.v[i + 1].u; packet.v2 = clipped.v[i + 1].v;

packet.r0 = clipped.v[0].r; packet.g0 = clipped.v[0].g; packet.b0 = clipped.v[0].b;

packet.r1 = clipped.v[i].r; packet.g1 = clipped.v[i].g; packet.b1 = clipped.v[i].b;

packet.r2 = clipped.v[i + 1].r; packet.g2 = clipped.v[i + 1].g; packet.b2 = clipped.v[i + 1].b;

const uint32_t depth = backend_map_depth_to_ot((uint32_t)z0, (uint32_t)z1, (uint32_t)z2);

if (!backend_ot_add(ot, depth, &packet)) return false;

}

return true;

}

// Contract: command-buffer/OT overflow returns false with a deterministic diagnostic;

// primitives must never be silently dropped.
```
## 8. ASSET PIPELINE

### 8.1. ФОРМАТ .MESH

```text
// .mesh v1 — byte-defined serialized format (little-endian, ABI-independent)

// File layout: [Header:24 bytes][MeshVertex * vertex_count][MeshIndex * index_count]

// Serialized MeshVertex = 21 bytes. Compiler padding/alignment is NOT serialized.

Header offsets:

0x00 u32 magic = 0x4853454D ("MESH")

0x04 u32 version = 1

0x08 u32 vertex_count

0x0C u32 index_count (multiple of 3)

0x10 u32 texture_id

0x14 i32 bounding_sphere_radius (signed 16.16 engine-space)

Vertex bytes (21 bytes):

0..11 i32 x,y,z (signed 16.16)

12..14 i8 nx,ny,nz (-127..127)

15..16 u8 u,v (0..255)

17..20 u8 r,g,b,a

Index bytes (6 bytes):

0..1 u16 v0; 2..3 u16 v1; 4..5 u16 v2

Loader contract:

- decode fields explicitly from bytes; never cast file.data to MeshHeader/MeshVertex;

- validate count multiplication/addition for uint64 overflow before pointer formation;

- validate magic, version, counts, payload size and every index before allocating persistent storage;

- a failed load MUST NOT consume persistent linear-arena capacity.
```
### 8.2. ФОРМАТ .TEX

```text
// .tex v1 — byte-defined CI8 texture format (little-endian, ABI-independent)

// Header size = 20 bytes. Compiler padding/alignment is NOT serialized.

Header offsets:

0x00 u32 magic = 0x00584554 ("TEX\0")

0x04 u32 version = 1

0x08 u16 width (64, 128 or 256 for MVP)

0x0A u16 height (64, 128 or 256 for MVP)

0x0C u32 palette_offset

0x10 u32 pixel_offset

Payload:

- palette_offset points to exactly 512 bytes: 256 x LE u16 RGB555+STP entries

- pixel_offset points to exactly width*height bytes of CI8 indices

- palette and pixel payloads must be fully inside file_size and must not overlap the header

- width*height and every offset/end calculation must be checked for uint64 overflow before use

Loader contract:

- decode all header fields explicitly as little-endian;

- validate magic/version/width/height/offsets/payload bounds before allocating persistent storage;

- malformed .tex MUST be rejected without out-of-bounds access and without consuming persistent arena capacity.
```
### 8.3. RUNTIME ЗАГРУЗКА

```cpp
bool mesh_validate_and_load(const uint8_t* data, size_t file_size, LinearAllocator* alloc, Mesh** out) {

MeshHeaderDisk h;

if (!decode_mesh_header_le(data, file_size, &h)) return false;

if (h.magic != 0x4853454D || h.version != 1) return false;

if (h.vertex_count == 0 || h.index_count == 0 || (h.index_count % 3) != 0) return false;

const uint64_t vertex_bytes = (uint64_t)h.vertex_count * 21u;

const uint64_t index_bytes = (uint64_t)h.index_count * 6u;

const uint64_t payload_end = 24u + vertex_bytes + index_bytes;

if (vertex_bytes / 21u != h.vertex_count || index_bytes / 6u != h.index_count) return false;

if (payload_end < 24u || payload_end < vertex_bytes || payload_end < index_bytes || payload_end > file_size) return false;

const uint64_t index_base = 24u + vertex_bytes;

for (uint32_t i = 0; i < h.index_count; ++i) {

const uint64_t index_offset_u64 = index_base + (uint64_t)i * 6u;

if (index_offset_u64 > file_size || 6u > (uint64_t)file_size - index_offset_u64) return false;

const size_t index_offset = static_cast<size_t>(index_offset_u64);

uint16_t v0 = read_le_u16(data + index_offset + 0u);

uint16_t v1 = read_le_u16(data + index_offset + 2u);

uint16_t v2 = read_le_u16(data + index_offset + 4u);

if (v0 >= h.vertex_count || v1 >= h.vertex_count || v2 >= h.vertex_count) return false;

}

const LinearAllocator::Marker mark = alloc->mark();

Mesh* mesh = alloc->allocate_object<Mesh>();

if (!mesh) return false;

mesh->vertex_count = h.vertex_count;

mesh->index_count = h.index_count;

mesh->vertices = alloc->allocate_array<MeshVertex>(h.vertex_count);

mesh->indices = alloc->allocate_array<MeshIndex>(h.index_count);

if (!mesh->vertices || !mesh->indices) {

alloc->rollback(mark);

return false;

}

decode_vertices_le(data + 24u, h.vertex_count, mesh->vertices);

decode_indices_le(data + 24u + vertex_bytes, h.index_count, mesh->indices);

*out = mesh;

return true;

}

// Loader invariant: every offset is validated in uint64_t before conversion to size_t or pointer arithmetic.

// Every failure after the first persistent allocation rolls back to the pre-load marker; all offset arithmetic is validated in uint64_t before conversion to size_t/pointer arithmetic, so malformed or oversized assets do not consume persistent arena capacity.
```
### 8.4. СТРИМИНГ С CD-ROM (PS1)

PS1 DISC IMAGE CONTRACT (MVP): release images are ISO9660-backed .cue/.bin artifacts generated through the pinned PSn00bSDK/mkpsxiso integration. The image MUST contain the exact system.cnf boot configuration, the generated PS-EXE filename, and versioned .mesh/.tex assets. Runtime FileSystem_LoadFile() addresses the ISO9660 path only; host filesystem paths are not part of the PS1 runtime contract. Filenames, path case, sector/image format, and system.cnf content are release-contract data and MUST be tested by the release job.
ПРОБЛЕМА: PS1 CD-ROM is a nominal 2x-class drive (\~300 КБ/с). Access latency varies with seek position and access pattern; do not use a universal seek-time constant. Sequential transfer of 2 MiB at 300 КБ/с is already several seconds before overhead.
РЕШЕНИЕ:

1. Предзагрузка: optional for MVP; overlap with gameplay is allowed only when CD access and RAM budgets permit.
2. Буферизация: initial CD staging buffer target = 64 KiB. This is an engine budget, not a hardware requirement.
3. Асинхронность: use the PSn00bSDK CD transfer/IRQ APIs; CD-ROM is not modeled as a generic DMA producer by the engine.

> ПРОБЛЕМА: PS1 CD-ROM is a nominal 2x-class drive (\~300 КБ/с). Access latency varies with seek position and access pattern; do not use a universal seek-time constant. Sequential transfer of 2 MiB at 300 КБ/с is already several seconds before overhead.
>
> РЕШЕНИЕ:

1. Предзагрузка: optional for MVP; overlap with gameplay is allowed only when CD access and RAM budgets permit.

2. Буферизация: initial CD staging buffer target = 64 KiB. This is an engine budget, not a hardware requirement.

3. Асинхронность: use the PSn00bSDK CD transfer/IRQ APIs; CD-ROM is not modeled as a generic DMA producer by the engine.

## 9. ПОТОК ИНИЦИАЛИЗАЦИИ И ГЛАВНЫЙ ЦИКЛ

> ИНИЦИАЛИЗАЦИЯ:

main()

> \|
>
> +-> Platform_Init() // SDL3/GPU/CD-ROM
>
> \| \|
>
> \| +-> Render_Init()
>
> \| +-> Input_Init()
>
> \| +-> Audio_Init()
>
> \|
>
> +-> Memory_Init() // Создание аллокаторов
>
> \| \|
>
> \| +-> LinearAllocator для уровней
>
> \| +-> Pool для сущностей
>
> \| +-> Stack для кадров
>
> \|
>
> +-> ECS_Init() // Создание Registry
>
> \|
>
> +-> AssetLoader_Init() // Инициализация загрузчика
>
> \|
>
> +-> Game_Init() // Игровая логика
>
> \|
>
> +-> Загрузка начального уровня

ГЛАВНЫЙ ЦИКЛ:

```cpp
const fixed_t SIM_DT = (fixed_t)(FIXED_ONE / 60); // deterministic 16.16 representation of one 60 Hz step

uint32_t last_time = Platform_GetTimeMs();

uint64_t accumulator_60hz = 0; // elapsed milliseconds expressed in 1/60-second ticks

// Wall-clock time only schedules fixed simulation ticks; it is never passed directly to gameplay systems.

while (Platform_PumpEvents()) {

uint32_t now = Platform_GetTimeMs();

const uint32_t elapsed_ms = now - last_time;

last_time = now;

accumulator_60hz += (uint64_t)elapsed_ms * 60u;

while (accumulator_60hz >= 1000u) {

accumulator_60hz -= 1000u;

const fixed_t delta_time = SIM_DT;

// 1. Обработка ввода

InputSystem::update(registry, delta_time);

// 2. Игровая логика

GameUpdate(registry, delta_time);

// 3. Физика / коллизии

CollisionSystem::update(registry, delta_time);

// 4. Движение

MovementSystem::update(registry, delta_time);

}

// 5. Рендеринг

Render_BeginFrame();

Render_Clear(0, 0, 32);

RenderSystem::update(registry); // Собирает команды

Render_EndFrame(); // Submit + SwapBuffers

// 6. Очистка кадрового стека

frame_stack.reset_frame();

}

Game_Shutdown();

Memory_Shutdown();

Platform_Shutdown();
```
## 10. СТРУКТУРА ДИРЕКТОРИЙ ПРОЕКТА

> retro-engine/
>
> \|
>
> +-- CMakeLists.txt # Главный CMake
>
> +-- README.md
>
> +-- LICENSE # MIT
>
> +-- LEGAL.md # Юридическая информация
>
> \|
>
> +-- src/
>
> \| +-- CMakeLists.txt
>
> \| \|
>
> \| +-- core/ # FOUNDATION: CORE
>
> \| \| +-- ecs/
>
> \| \| \| +-- registry.h
>
> \| \| \| +-- registry.cpp
>
> \| \| \| +-- component_array.h
>
> \| \| +-- math/
>
> \| \| \| +-- fixed.h
>
> \| \| \| +-- vec2.h
>
> \| \| \| +-- vec3.h
>
> \| \| \| +-- vec4.h
>
> \| \| \| +-- mat3.h
>
> \| \| \| +-- mat4.h
>
> \| \| \| +-- quat.h
>
> \| \| \| +-- math.cpp
>
> \| \| +-- memory/
>
> \| \| \| +-- linear_allocator.h
>
> \| \| \| +-- pool_allocator.h
>
> \| \| \| +-- stack_allocator.h
>
> \| \| +-- log/
>
> \| \| \| +-- logger.h
>
> \| \| \| +-- logger.cpp
>
> \| \| +-- types.h # Базовые типы
>
> \| \|
>
> \| +-- hal/ # СЛОЙ 1: HAL
>
> \| \| +-- platform.h # Единый API
>
> \| \| +-- pc/
>
> \| \| \| +-- platform_pc.cpp
>
> \| \| \| +-- render_opengl.cpp
>
> \| \| \| +-- input_sdl.cpp
>
> \| \| \| +-- audio_sdl.cpp
>
> \| \| \| +-- filesystem_pc.cpp
>
> \| \| +-- ps1/
>
> \| \| +-- platform_ps1.cpp
>
> \| \| +-- render_gpu.cpp
>
> \| \| +-- input_pad.cpp
>
> \| \| +-- audio_spu.cpp
>
> \| \| +-- filesystem_cdrom.cpp
>
> \| \|
>
> \| +-- render/ # СЛОЙ 2: SERVICES / RENDER
>
> \| \| +-- renderer.h
>
> \| \| +-- renderer.cpp
>
> \| \| +-- mesh.h
>
> \| \| +-- mesh.cpp
>
> \| \| +-- texture.h
>
> \| \| +-- texture.cpp
>
> \| \| +-- command_buffer.h
>
> \| \| +-- shaders/
>
> \| \| +-- ps1_emu.vert
>
> \| \| +-- ps1_emu.frag
>
> \| \|
>
> \| +-- io/ # СЛОЙ 2: SERVICES / IO
>
> \| \| +-- input.h
>
> \| \| +-- input.cpp
>
> \| \| +-- filesystem.h
>
> \| \| +-- cdrom_stream.h
>
> \| \| +-- cdrom_stream.cpp
>
> \| \|
>
> \| +-- audio/ # СЛОЙ 2: SERVICES / AUDIO
>
> \| \| +-- audio.h
>
> \| \| +-- audio.cpp
>
> \| \|
>
> \| +-- assets/ # СЛОЙ 2: SERVICES / ASSETS
>
> \| +-- asset_loader.h
>
> \| +-- asset_loader.cpp
>
> \| +-- mesh_loader.cpp
>
> \| +-- texture_loader.cpp
>
> \|
>
> +-- game/ # СЛОЙ 4: Игра
>
> \| +-- components.h
>
> \| +-- systems/
>
> \| \| +-- movement_system.h
>
> \| \| +-- render_system.h
>
> \| \| +-- input_system.h
>
> \| \| +-- collision_system.h
>
> \| +-- main.cpp
>
> \|
>
> +-- tools/ # Инструменты
>
> \| +-- asset_converter/
>
> \| +-- CMakeLists.txt
>
> \| +-- main.cpp
>
> \| +-- mesh_converter.cpp
>
> \| +-- texture_converter.cpp
>
> \| +-- color_quantizer.cpp
>
> \| +-- third_party/
>
> \| +-- cgltf.h
>
> \| +-- stb_image.h
>
> \| +-- stb_image_write.h
>
> \|
>
> +-- examples/
>
> \| +-- hello_triangle/
>
> \| \| +-- CMakeLists.txt
>
> \| \| +-- main.cpp
>
> \| +-- rotating_cube/ \| +-- CMakeLists.txt
>
> \| +-- main.cpp
>
> \|
>
> +-- assets/
>
> \| +-- source/ # Исходные ассеты
>
> \| \| +-- cube.gltf
>
> \| \| +-- texture.png
>
> \| +-- build/ # Сконвертированные
>
> \| +-- cube.mesh \| +-- texture.tex
>
> \|
>
> +-- tests/
>
> \| +-- CMakeLists.txt
>
> \| +-- math_test.cpp
>
> \| +-- ecs_test.cpp
>
> \| +-- memory_test.cpp
>
> \|
>
> +-- docs/
>
> \| +-- ARCHITECTURE.md
>
> \| +-- BUILDING.md
>
> \| +-- API.md
>
> \| +-- ASSETS.md
>
> \|
>
> +-- toolchain/
>
> \| +-- ps1-setup.sh
>
> \| +-- README.md
>
> \|
>
> +-- scripts/
>
> \| +-- build_pc.bat
>
> \| +-- build_ps1.bat
>
> \| +-- run_pc.bat
>
> \| +-- run_ps1.bat
>
> \|
>
> +-- .github/
>
> +-- workflows/
>
> +-- build-pc.yml / build-ps1.yml / tests.yml / release.yml

## 11. ДИАГРАММЫ ЗАВИСИМОСТЕЙ

> ДИАГРАММА ВЫЗОВОВ ПРИ ОТРИСОВКЕ КАДРА:
>
> Game Layer Systems Layer Services Layer HAL
>
> \| \| \| \|
>
> \| GameUpdate() \| \| \|
>
> \|------------------------->\| \| \|
>
> \| \| \| \|
>
> \| \| RenderSystem::update() \| \|
>
> \| \|----------------------->\| \|
>
> \| \| \| \|
>
> \| \| \| Render_DrawMesh()\|
>
> \| \| \|----------------->\|
>
> \| \| \| \|
>
> \| \| \| \| [PS1: GTE + GPU]
>
> \| \| \| \| [PC: OpenGL]
>
> \| \| \| \|
>
> \| \| \| Render_EndFrame()\|
>
> \| \| \|----------------->\|
>
> \| \| \| \| [SwapBuffers]
>
> ДИАГРАММА ЗАГРУЗКИ УРОВНЯ:
>
> Game Layer Asset Layer IO Layer Memory Layer
>
> \| \| \| \|
>
> \| LoadLevel("lvl1") \| \| \|
>
> \|--------------------->\| \| \|
>
> \| \| \| \|
>
> \| \| level_alloc.reset()\| \|
>
> \| \|------------------------------------------>\|
>
> \| \| \| \|
>
> \| \| FileSystem_Load() \| \|
>
> \| \|------------------->\| \|
>
> \| \| \| [CD-ROM / FS] \|
>
> \| \| \| \|
>
> \| \|<-------------------\| \|
>
> \| \| \| \|
>
> \| \| mesh_load() \| \|
>
> \| \|------------------------------------------>\|
>
> \| \| \| \|
>
> \| \| texture_load() \| \|
>
> \| \|------------------------------------------>\|
>
> \| \| \| \|
>
> \|<---------------------\| \| \|
>
> \| Level ready \| \| \|

## 12. КРИТИЧЕСКИЕ АРХИТЕКТУРНЫЕ РЕШЕНИЯ (ADR)

> ADR-001: Compile-time выбор backend через CMake и локальную условную компиляцию
>
> РЕШЕНИЕ: CMakePresets.json выбирает ровно одну платформу и соответствующий набор translation units. #ifdef PLATFORM_PC / PLATFORM_PS1 допускается только внутри shared source, если действительно требуется compile-time specialization; общий код не должен содержать platform-specific #ifdef.
>
> ОБОСНОВАНИЕ: Нет runtime virtual dispatch в hot paths; platform backend не существует в бинарнике другой платформы. CMake source selection обеспечивает более чистую границу, чем распространение #ifdef по общему коду.
>
> КОМПРОМИСС: Сборка выбирается на этапе конфигурации, поэтому один бинарник не переключает backend во время выполнения. Для MVP это приемлемо и соответствует target-specific runtime constraints.
>
> АЛЬТЕРНАТИВЫ: Runtime polymorphism (отвергнут для hot paths из-за дополнительного indirection/ABI complexity; может быть рассмотрен позже для non-hot tooling).
>
> ADR-002: Fixed-Point 16.16 вместо Float
>
> РЕШЕНИЕ: fixed_t (signed int32_t Q16.16) является основным engine scalar для simulation/math. Handles, indices, sizes, timestamps, bitmasks и memory offsets используют целочисленные типы соответствующего назначения и не трактуются как fixed-point.
>
> КОМПРОМИСС: Меньшая точность, но для ретро-графики это приемлемо.
>
> АЛЬТЕРНАТИВЫ: Float везде (отвергнут из-за производительности на PS1).
>
> ADR-003: Painter's Algorithm вместо Z-буфера
>
> РЕШЕНИЕ: Сортировка полигонов по глубине перед отрисовкой.
>
> КОМПРОМИСС: O(n log n) сортировка каждый кадр. Oграничение на количество полигонов.
>
> АЛЬТЕРНАТИВЫ: Z-буфер на N64 (отложен до версии 1.5+).
>
> ADR-004: ECS вместо классического ООП
>
> РЕШЕНИЕ: Entity (ID) + Components (POD) + Systems (функции).
>
> ОБОСНОВАНИЕ: ECS выбран прежде всего ради плотного и предсказуемого layout данных, cache-friendly итерации по компонентам, отсутствия per-object heap allocation и удобной композиции сущностей. Отсутствие virtual dispatch в simulation hot paths является дополнительным преимуществом, а не единственным основанием выбора.
>
> КОМПРОМИСС: Более сложный код Registry.
>
> АЛЬТЕРНАТИВЫ: Классическое ООП с inheritance/virtual dispatch отвергнуто для simulation hot paths из-за худшего соответствия плотному DOD layout, дополнительного indirection и более сложной lifetime/memory модели. ООП допустимо на tooling/editor/non-hot уровнях.
>
> ADR-005: C++14 утилита для конвертации ассетов вместо Python
>
> РЕШЕНИЕ: C++14 утилита с cgltf + stb_image, вызываемая через CMake.
>
> ОБОСНОВАНИЕ: Единый язык проекта и отсутствие runtime-зависимости от Python. Конвертер является HOST-ONLY инструментом и НЕ кросс-компилируется в PS1 runtime; PS1 builds consume generated/versioned assets.
>
> КОМПРОМИСС: Более сложная разработка утилиты.
>
> АЛЬТЕРНАТИВЫ: Python-скрипт (отвергнут из-за зависимостей).
>
> ADR-006: OpenGL 3.3 для PC-рендера вместо SDL3 GPU API и Vulkan
> РЕШЕНИЕ: Использовать OpenGL 3.3 с шейдерами, эмулирующими PS1.
> ОБОСНОВАНИЕ: OpenGL 3.3 выбран из-за зрелого tooling/debug workflow и RenderDoc в целевой PC-среде. SDL3 GPU API и Vulkan не входят в MVP, чтобы не добавлять второй graphics abstraction path. Vulkan также не даёт преимущества для задачи воспроизведения фиксированного 320x240 PS1-style raster contract и увеличивает объём explicit resource/synchronization management.
> КОМПРОМИСС: Нужно вручную эмулировать PS1-ограничения.
> АЛЬТЕРНАТИВЫ: SDL3 GPU API и Vulkan могут быть рассмотрены только после MVP через отдельный benchmark/diagnostic comparison.
>
> РЕШЕНИЕ: Использовать OpenGL 3.3 с шейдерами, эмулирующими PS1.
>
> ОБОСНОВАНИЕ: OpenGL 3.3 выбран из-за зрелого tooling/debug workflow и прямой поддержки RenderDoc в целевой PC-среде. SDL3 GPU API не входит в MVP, чтобы не добавлять второй graphics abstraction path.
>
> КОМПРОМИСС: Нужно вручную эмулировать PS1-ограничения.
>
> АЛЬТЕРНАТИВЫ: SDL3 GPU API (отложен; при необходимости пересмотреть после стабилизации MVP и отдельного benchmark/debugging comparison).
>
> ADR-007: Гибридная система памяти
>
> РЕШЕНИЕ: Linear Allocator + Pool Allocator + Stack Allocator.
>
> ОБОСНОВАНИЕ: Разные паттерны использования требуют разных аллокаторов. Нет фрагментации. Детерминированное использование памяти.
>
> КОМПРОМИСС: Сложнее, чем просто malloc/free.
>
> АЛЬТЕРНАТИВЫ: Кастомный heap (отвергнут из-за фрагментации).

# ЭТАП 5: ПОДГОТОВКА ИНФРАСТРУКТУРЫ И TOOLCHAIN (DEVOPS & CI/CD)

## 1. ВВЕДЕНИЕ И ЦЕЛИ DEVOPS

> ЦЕЛИ ЭТАПА:

- Настроить локальную среду разработки на Windows 11 за 1 час.

- Обеспечить сборку PC-версии одной командой.

- Обеспечить сборку PS1-версии одной командой.

- Автоматизировать сборку и тесты на GitHub (CI/CD).

- Сделать процесс воспроизводимым для контрибьюторов.

ПРИНЦИПЫ DEVOPS ДЛЯ СОЛО-РАЗРАБОТЧИКА:

- "One-click setup" --- новый контрибьютор должен настроить среду за 1 час.

- "Reproducible builds" --- один и тот же коммит = один и тот же бинарник.

- "Fail fast" --- ошибки должны ловиться на этапе компиляции, а не в эмуляторе.

- "Minimal dependencies" --- минимум внешних зависимостей, максимум vendor.

- "Documentation as code" --- документация в репозитории, не в голове.

## 2. АДАПТАЦИЯ КОРПОРАТИВНОГО ПОДХОДА ПОД СОЛО-РАЗРАБОТЧИКА

> КОРПОРАТИВНЫЙ ПОДХОД (из ТЗ):

- Docker-контейнеры с кросс-компиляторами для 5 архитектур.

- Headless-эмуляторы для автоматических тестов.

- Отдельный DevOps-инженер.

> АДАПТИРОВАННЫЙ ПОДХОД (для соло):

- MSYS2 на Windows вместо Docker (проще, быстрее, не требует Hyper-V).

- Готовые кросс-компиляторы (не собираем сами --- это недели работы).

- GitHub Actions вместо локального CI (бесплатно для open-source).

- Unit-тесты на PC вместо headless-эмуляторов (проще и быстрее).

- Headless-эмуляторы --- опционально, на поздних этапах.

> ПОЧЕМУ НЕ DOCKER:

- Docker Desktop на Windows требует WSL2 + Hyper-V --- тяжело для ноутбука.

- Проблемы с пробросом USB (для программаторов, если появятся).

- Лишняя абстракция для соло-разработчика.

- Кросс-компиляция через MSYS2 работает нативно и быстрее.

> КОГДА DOCKER МОЖЕТ ПОНАДОБИТЬСЯ (версия 2.0+):

- Когда появятся контрибьюторы с Linux/macOS.

- Когда нужно собирать бинарники для всех 5 консолей в одном месте.

- Когда будет отдельный CI-сервер.

## 3. ЛОКАЛЬНАЯ СРЕДА РАЗРАБОТКИ (WINDOWS 11)

### 3.1. БАЗОВЫЕ ИНСТРУМЕНТЫ

> СПИСОК НЕОБХОДИМОГО СОФТА:

1. Git for Windows (<https://git-scm.com/download/win>)

- Версия: последняя стабильная (2.40+).

- Опции при установке:

- "Use Git from Git Bash only" --- чтобы не ломать другие инструменты

- "Checkout as-is, commit Unix-style line endings" --- важно для CMake

- "Use MinTTY" --- нормальный терминал

2\. CMake (<https://cmake.org/download/>)

- Версия: 3.25+ (для поддержки SDL3 и современных фич).

- Опции при установке:

- "Add CMake to system PATH" --- обязательно

- "Install for all users" --- опционально

3. Ninja (<https://github.com/ninja-build/ninja/releases>)

- Версия: 1.11+

- Установка: скачать ninja.exe, положить в C:\\Tools\\ninja\\ добавить в PATH

- Почему Ninja: быстрые инкрементальные сборки; абсолютное ускорение зависит от host platform и проекта.

4. Visual Studio 2022 Build Tools (<https://visualstudio.microsoft.com/downloads/>)

- Установить "Разработка классических приложений на C++"

- Компоненты:

- MSVC v143

- Windows 11 SDK (10.0.22621.0+)

- Зачем: компилятор MSVC для PC-версии, отладчик

5. MSYS2 (<https://www.msys2.org/>)

- Установка в C:\\msys64 (стандартный путь)

- Зачем: PSn00bSDK release содержит mipsel-none-elf GCC toolchain для PS1; MSYS2 используется как удобная host shell среда, а не как источник toolchain.

6. Cursor IDE (<https://cursor.sh/>)

- Версия: последняя

- Расширения:

- С/С++ (Microsoft)

- CMake Tools (Microsoft)

- Clangd (llvm)

- GitLens

- Error Lens

7. DuckStation (<https://github.com/stenzek/duckstation>)

- Версия: последняя preview

- Зачем: эмулятор PS1 для тестирования

8. PCSX-Redux (<https://github.com/grumpycoders/pcsx-redux>)

- Версия: последняя

- Зачем: альтернативный эмулятор с GDB-сервером

### 3.2. MSYS2 --- КРОСС-ПЛАТФОРМЕННАЯ СБОРОЧНАЯ СРЕДА

> ЧТО ТАКОЕ MSYS2:

- Минималистичная Linux-среда для Windows.

- Содержит pacman --- менеджер пакетов (как в Arch Linux).

- Позволяет устанавливать кросс-компиляторы одной командой.

УСТАНОВКА КРОСС-КОМПИЛЯТОРА ДЛЯ PS1:

- Открываем MSYS2 MinGW x64 terminal

- Обновляем пакеты (pacman --Syu)

- Перезапускаем терминал, если потребуется

- Устанавливаем базовые инструменты (pacman --S base-devel git)

- Устанавливаем/активируем PSn00bSDK release toolchain для target mipsel-none-elf

- ВАЖНО: официального пакета в MSYS2 нет, используем PSn00bSDK toolchain

- См. раздел 3.4

PINNED MVP TOOLCHAIN: PSn00bSDK v0.24 (release), target mipsel-none-elf. A newer SDK requires an explicit ADR/update to this document and CI.

### 3.3. PS1 TOOLCHAIN (PSN00BSDK RELEASE)

> ВАРИАНТЫ ПОЛУЧЕНИЯ КРОСС-КОМПИЛЯТОРА:

1) Сборка собственного GCC/binutils toolchain из исходников --- advanced option; не входит в MVP setup.

 - Скачать GCC source, binutils source

 - Применить патчи для mipsel-elf

 - Скомпилировать (очень долго, \~4 часа)

 - НЕ РЕКОМЕНДУЕТСЯ для MVP

2) Использовать готовый PSn00bSDK release (рекомендуется для MVP): пакет содержит compiler toolchain, libraries и tools.

 -

 - Поддерживаемые host platforms определяются текущим PSn00bSDK release; MVP host: Windows 11.

 - РЕКОМЕНДУЕТСЯ для MVP

3) Docker --- optional CI/reproducibility option, не основной локальный workflow MVP.

 - Требует Docker Desktop

 - НЕ РЕКОМЕНДУЕТСЯ для соло-разработчика на Windows

> ВЫБОР: Вариант B (PSn00bSDK toolchain) УСТАНОВКА PSNOOBSDK RELEASE:
>
> 1\. Скачиваем PSn00bSDK

- git clone <https://github.com/Lameguy64/PSn00bSDK.git>

- cd PSn00bSDK

- Переходим на <https://github.com/Lameguy64/PSn00bSDK/releases>

- Скачиваем psn00bsdk-toolchain-windows-x64.zip

> 2. Распаковываем пакет целиком в C:\\PSn00bSDK\
>
> Структура:

- C:\\psn00b-sdk\

- toolchain\

- bin\

-

-

-

- ...

- lib\

- include\

> 4. Добавляем в PATH

- Панель управления -> Система -> Переменные среды -> PATH

- Добавляем: C:\\PSn00bSDK\\bin

> 5. Проверяем

- MSYS2 предоставляет POSIX-подобную среду и shell-инструменты для Windows; это не Linux runtime и не источник PS1 cross-compiler. PSn00bSDK release поставляет собственный mipsel-none-elf toolchain.

> ПРОВЕРКА РАБОТОСПОСОБНОСТИ:

```text
// test.c

int main() {

return 42;

}
```
> # Компилируем

-
-

> # Если нет ошибок --- toolchain работает

### 3.4. PSn00bSDK --- SDK ДЛЯ PS1

> ЧТО ТАКОЕ PSn00bSDK:

- Open-source SDK для PS1; core libraries/CMake are licensed under MPL-2.0. The release package also contains separately licensed tools/components; see PSn00bSDK LICENSE.md.

- Заменяет проприетарный PsyQ SDK от Sony.

- Содержит: библиотеки для GPU, SPU, CD-ROM, PAD, DMA.

- Uses BIOS-compatible interfaces and PSn00bSDK libraries; it is not a clean-room replacement for every BIOS service. Treat BIOS calls and SDK services as explicit dependencies of the PS1 backend.

УСТАНОВКА PSNOOBSDK:

Для contributors используется prebuilt PSn00bSDK v0.24 release. Release v0.24 соответствует commit 06e65be; contributor setup не собирает SDK из исходников. Сборка из source --- отдельная maintainer-задача.

- 1. Скачать точный v0.24 release archive для host OS из официальной страницы Releases PSn00bSDK и распаковать его, например, в C:\\PSn00bSDK.

- 2. Добавить C:\\PSn00bSDK\\bin\\ в PATH.

- 3. Задать PSN00BSDK_ROOT=C:\\PSn00bSDK и PSN00BSDK_LIBS=%PSN00BSDK_ROOT%\\lib\\libpsn00b.

> 4. Проверить наличие %PSN00BSDK_LIBS%\\cmake\\sdk.cmake и executable mipsel-none-elf-gcc в PATH.

- 5. Проверка SDK выполняется через canonical project command: cmake --preset ps1-release && cmake --build --preset ps1-release.

- Исходная сборка SDK из source допускается только для maintainers и должна использовать отдельно зафиксированный SDK commit/release asset и собственный CI job.

- cmake .. -G Ninja \

- -DCMAKE_TOOLCHAIN_FILE=$PSN00BSDK_LIBS/cmake/sdk.cmake \

- -DCMAKE_INSTALL_PREFIX=C:\\psn00b-sdk\\psn00bsdk

- cmake --build . --config Release cmake --install .

> 3. Структура установки:

- C:\\psn00b-sdk\\psn00bsdk\

- include\

- libpsn00b\

- gpu.h

- spu.h

- cdrom.h

- pad.h

- dma.h

- \...

- lib\

- libpsn00b.a

- libc.a

- libgcc.a

- bin\

- PSn00bSDK release provides the PS1 executable/image packaging tools. Use the tool names and paths supplied by the pinned release; do not hard-code a legacy directory layout.

- mkpsxiso.exe # Создатель ISO-образов

> 4\. Добавляем переменные среды
>
> set PSN00BSDK_ROOT=C:\\PSn00bSDK
> set PSN00BSDK_LIBS=%PSN00BSDK_ROOT%\\lib\\libpsn00b
>
> ПРОВЕРКА РАБОТОСПОСОБНОСТИ:

```text
SMOKE TEST PSNOOBSDK

Проверка v0.24: mipsel-none-elf-gcc --version; затем собрать минимальный target через canonical CMakePresets.json данного проекта. Не используйте самодельные GPU_Init/GPU_SetResolution snippets как normative API: PSn00bSDK не является drop-in replacement для каждого legacy SDK example.

Ожидаемый результат: создаётся PS1 executable target без ручной ELF-конвертации и без floating tool paths.
```
> 1\. Компилируем
>
> -I C:\\psn00b-sdk\\psn00bsdk\\include \
>
> hello_ps1.c -o hello_ps1.elf \
>
> -L C:\\psn00b-sdk\\psn00bsdk\\lib -lpsn00b
>
> 2\. Конвертируем в PS1 EXE
>
> Use the packaging tools supplied by the pinned PSn00bSDK release; exact executable paths are discovered from the release layout and are not hard-coded by the engine contract.
>
> 3\. Если нет ошибок --- PSn00bSDK работает

### 3.5. SDL3 И OPENGL ДЛЯ PC

> УСТАНОВКА SDL3:
>
> Вариант A: Через vcpkg (опционально, только convenience path)
>
> Устанавливаем vcpkg git clone <https://github.com/microsoft/vcpkg.git> C:\\vcpkg cd C:\\vcpkg .\\bootstrap-vcpkg.bat
>
> # Устанавливаем SDL3
>
> .\\vcpkg install sdl3:x64-windows
>
> # Интегрируем с CMake
>
> # Не требуется для canonical CMakePresets workflow.
>
> # Вариант B: Скачать prebuilt binaries
>
> # <https://github.com/libsdl-org/SDL/releases>
>
> # Скачать SDL3-devel-3.x.x-VC.zip
>
> # Распаковать в C:\\sdl3
>
> # Вариант C: Собрать из исходников git clone <https://github.com/libsdl-org/SDL.git> cd SDL mkdir build && cd build cmake .. -G Ninja -DCMAKE_INSTALL_PREFIX=C:\\sdl3 cmake --build . --config Release cmake --install .
>
> УСТАНОВКА OPENGL:

- OpenGL 3.3 runtime implementation поставляется драйвером/операционной системой; Windows SDK предоставляет системные import libraries (например, opengl32.lib). Отдельная установка OpenGL runtime для canonical build не требуется.

- Для современных функций (3.3+) используем glad или glbinding.

- РЕКОМЕНДАЦИЯ: glad (генератор загрузчика OpenGL) ГЕНЕРАЦИЯ GLAD:

- Переходим на <https://gen.glad.sh/>

- Выбираем

- API: OpenGL

- Profile: Core

- Version: 3.3

- Generator: C/C++

- Extensions: все

- Нажимаем "Generate"

- Скачиваем glad.zip

- Распаковываем в src/third_party/glad/

> СТРУКТУРА GLAD:
>
> src/third_party/glad/ include/ glad/gl.h
>
> KHR/khrplatform.h src/ gl.c
>
> ------------------------------------------------------------------------

### 3.6. CURSOR IDE --- НАСТРОЙКА

> РАСШИРЕНИЯ CURSOR:

- C/C++ (Microsoft) --- подсветка синтаксиса, IntelliSense

- CMake Tools (Microsoft) --- интеграция с CMake

- clangd (llvm) --- быстрая автодополнялка (альтернатива IntelliSense)

- GitLens --- история коммитов, blame

- Error Lens --- показывает ошибки прямо в коде

- CodeLLDB --- отладчик для C++

- C/C++ Extension Pack --- набор расширений

> НАСТРОЙКА CMAKE В CURSOR:
>
> // .vscode/settings.json
> {
> "cmake.useCMakePresets": "always",
> "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
> "files.associations": {
> "*.h": "cpp",
> "*.hpp": "cpp"
> }
> }
> IDE configuration MUST NOT define independent -DPLATFORM_*, -B, generator or build-directory conventions. Select one of the canonical presets: pc-debug, pc-release, pc-debug-render or ps1-release.
>
> НАСТРОЙКА ОТЛАДКИ:
>
> // .vscode/launch.json
>
> // .vscode/launch.json
> {
> "version": "0.2.0",
> "configurations": [
> {
> "name": "PC Debug (CMake Tools)",
> "type": "cppvsdbg",
> "request": "launch",
> "program": "${command:cmake.launchTargetPath}",
> "args": [],
> "stopAtEntry": false,
> "cwd": "${workspaceFolder}",
> "environment": [],
> "externalConsole": false,
> "preLaunchTask": "CMake: build selected preset"
> }
> ]
> }
> Use cppvsdbg for the documented MSVC/Visual Studio Build Tools setup. Do not hard-code a MinGW/MSYS2 debugger path in the canonical configuration.
>
> НАСТРОЙКА ЗАДАЧ:
>
> // .vscode/tasks.json
> {
> "version": "2.0.0",
> "tasks": [
> {
> "label": "CMake: build selected preset",
> "type": "shell",
> "command": "cmake",
> "args": ["--build", "--preset", "${input:preset}"],
> "group": {"kind": "build", "isDefault": true}
> }
> ],
> "inputs": [
> {
> "id": "preset",
> "type": "pickString",
> "description": "Canonical CMake build preset",
> "options": ["pc-debug", "pc-release", "pc-debug-render", "ps1-release"],
> "default": "pc-debug"
> }
> ]
> }

## 4. СИСТЕМА СБОРКИ (CMAKE + NINJA)

Test execution contract: default CTest runs host-only unit tests. Renderer/OpenGL integration tests require an explicitly provisioned graphics context (for example Xvfb/GLX on Linux) and are a separate CI/manual job; they are not silently run on headless runners.

> ------------------------------------------------------------------------

### 4.1. АРХИТЕКТУРА CMAKE-ПРОЕКТА

> ------------------------------------------------------------------------
>
> СТРУКТУРА CMAKE-ФАЙЛОВ: retro-engine/
>
> CMakeLists.txt # Главный файл cmake/ CMakePresets.json # Канонические пресеты PC/PS1 src/third_party/psn00bsdk/ # PSn00bSDK submodule
>
> SDL3 --- подключается как vendored CMake target SDL3::SDL3
>
> PSn00bSDK --- подключается через SDK-native sdk.cmake
>
> CompilerFlags.cmake # Общие флаги компиляции src/
>
> CMakeLists.txt # Сборка ядра core/ CMakeLists.txt hal/ CMakeLists.txt pc/ CMakeLists.txt ps1/ CMakeLists.txt render/
>
> CMakeLists.txt tools/ asset_converter/
>
> CMakeLists.txt examples/ hello_triangle/ CMakeLists.txt rotating_cube/
>
> CMakeLists.txt tests/
>
> CMakeLists.txt
>
> ------------------------------------------------------------------------

### 4.2. МУЛЬТИПЛАТФОРМЕННАЯ КОНФИГУРАЦИЯ

Platform selection is compile-time only. Exactly one target is configured per build: PLATFORM_PC or PLATFORM_PS1. The canonical entry point is CMakePresets.json; platform-specific source files are selected by src/hal/CMakeLists.txt.

```text
if(MSVC)

# MSVC (Windows PC) add_compile_options(/W4 /WX)

add_compile_options(/MP) # Многопоточная компиляция

add_compile_options(/Zi) # Debug info

else()

# GCC / Clang

add_compile_options(-Wall -Wextra -Wpedantic) add_compile_options(-Wno-unused-parameter)
```
4.2.1. CMAKE PRESETS (CANONICAL BUILD ENTRY POINT)

CMakePresets.json is the canonical contributor entry point. The repository MUST provide pc-debug, pc-release, pc-debug-render and ps1-release presets, and CI MUST invoke the same presets. pc-debug-render is identical to pc-debug except BUILD_RENDER_INTEGRATION_TESTS=ON and is used only with an explicitly provisioned graphics context.

Compiler diagnostics contract: MSVC uses /W4 /WX; GCC/Clang use -Wall -Wextra -Wpedantic -Werror where the SDK permits. Third-party headers may be isolated from these flags rather than disabling warnings globally.

CMakePresets.json example:

{
"version": 6,
"configurePresets": [
{"name":"pc-debug","generator":"Ninja","binaryDir":"${sourceDir}/build/pc-debug","cacheVariables":{"PLATFORM_PC":true,"BUILD_TESTS":true,"BUILD_RENDER_INTEGRATION_TESTS":false,"CMAKE_BUILD_TYPE":"Debug"}},
{"name":"pc-release","generator":"Ninja","binaryDir":"${sourceDir}/build/pc-release","cacheVariables":{"PLATFORM_PC":true,"BUILD_TESTS":true,"BUILD_RENDER_INTEGRATION_TESTS":false,"BUILD_EXAMPLES":true,"CMAKE_BUILD_TYPE":"Release"}},
{"name":"pc-debug-render","generator":"Ninja","binaryDir":"${sourceDir}/build/pc-debug-render","cacheVariables":{"PLATFORM_PC":true,"BUILD_TESTS":true,"BUILD_RENDER_INTEGRATION_TESTS":true,"CMAKE_BUILD_TYPE":"Debug"}},
{"name":"pc-asan","generator":"Ninja","binaryDir":"${sourceDir}/build/pc-asan","cacheVariables":{"PLATFORM_PC":true,"BUILD_TESTS":true,"BUILD_RENDER_INTEGRATION_TESTS":false,"CMAKE_BUILD_TYPE":"Debug","CMAKE_C_FLAGS":"-fsanitize=address -fno-omit-frame-pointer","CMAKE_CXX_FLAGS":"-fsanitize=address -fno-omit-frame-pointer"}},
{"name":"pc-bench","generator":"Ninja","binaryDir":"${sourceDir}/build/pc-bench","cacheVariables":{"PLATFORM_PC":true,"BUILD_TESTS":false,"BUILD_RENDER_INTEGRATION_TESTS":false,"BUILD_BENCHMARKS":true,"CMAKE_BUILD_TYPE":"Release"}},
{"name":"pc-coverage","generator":"Ninja","binaryDir":"${sourceDir}/build/pc-coverage","cacheVariables":{"PLATFORM_PC":true,"BUILD_TESTS":true,"BUILD_RENDER_INTEGRATION_TESTS":false,"CMAKE_CXX_FLAGS":"--coverage","CMAKE_BUILD_TYPE":"Debug"}},
{"name":"ps1-release","generator":"Ninja","binaryDir":"${sourceDir}/build/ps1-release","toolchainFile":"$env{PSN00BSDK_LIBS}/cmake/sdk.cmake","cacheVariables":{"PLATFORM_PS1":true,"BUILD_EXAMPLES":true,"CMAKE_BUILD_TYPE":"Release"}}
],
"buildPresets": [
{"name":"pc-debug","configurePreset":"pc-debug"},
{"name":"pc-release","configurePreset":"pc-release"},
{"name":"pc-debug-render","configurePreset":"pc-debug-render"},
{"name":"pc-asan","configurePreset":"pc-asan"},
{"name":"pc-bench","configurePreset":"pc-bench"},
{"name":"pc-coverage","configurePreset":"pc-coverage"},
{"name":"ps1-release","configurePreset":"ps1-release"}
],
"testPresets": [
{"name":"pc-debug","configurePreset":"pc-debug","output":{"outputOnFailure":true}},
{"name":"pc-release","configurePreset":"pc-release","output":{"outputOnFailure":true}},
{"name":"pc-debug-render","configurePreset":"pc-debug-render","output":{"outputOnFailure":true}},
{"name":"pc-asan","configurePreset":"pc-asan","output":{"outputOnFailure":true}},
{"name":"pc-coverage","configurePreset":"pc-coverage","output":{"outputOnFailure":true}}
]
}

Contributor commands: cmake --preset pc-debug && cmake --build --preset pc-debug; ctest --preset pc-debug. For release use pc-release. For PS1 use cmake --preset ps1-release && cmake --build --preset ps1-release. For optional quality jobs, the repository MAY also provide pc-asan, pc-bench and pc-coverage presets; these are derivative presets and do not replace the four canonical MVP build presets.

PS1 uses the pinned PSn00bSDK sdk.cmake and target mipsel-none-elf. Contributors do not create a custom MIPS compiler/sysroot file for the MVP.

### 4.3. СБОРКА PC-ВЕРСИИ

------------------------------------------------------------------------

Canonical PC build uses the same CMakePresets.json path as contributors and CI.

# root CMakeLists.txt (canonical architecture)
cmake_minimum_required(VERSION 3.25)
project(retro_engine LANGUAGES C CXX ASM)

option(PLATFORM_PC "Build PC backend" OFF)
option(PLATFORM_PS1 "Build PS1 backend" OFF)
if(PLATFORM_PC AND PLATFORM_PS1)
message(FATAL_ERROR "Select exactly one target platform")
elseif(NOT PLATFORM_PC AND NOT PLATFORM_PS1)
message(FATAL_ERROR "Select exactly one target platform")
endif()

if(PLATFORM_PC)
add_subdirectory(src/third_party/sdl3 EXCLUDE_FROM_ALL)
endif()
add_subdirectory(src)

# Asset converter is a host tool; it is never compiled with the PS1 cross compiler.
# MVP PS1 builds consume versioned/generated example assets from assets/build/.
if(PLATFORM_PC)
add_subdirectory(tools/asset_converter)
endif()

add_subdirectory(examples/hello_triangle)
add_subdirectory(examples/rotating_cube)
if(PLATFORM_PC)
# Canonical PC runtime targets are the documented demo executables.
# The release workflow packages rotating_cube as the reference PC runtime artifact.
set_target_properties(hello_triangle rotating_cube PROPERTIES
RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/game")
add_subdirectory(tests)
endif()

// src/CMakeLists.txt
if(PLATFORM_PC)
add_subdirectory(hal/pc)
else()
add_subdirectory(hal/ps1)
endif()

add_library(retro_engine STATIC
core/ecs/registry.cpp
core/math/math.cpp
core/log/logger.cpp
render/renderer.cpp
render/mesh.cpp
render/texture.cpp
io/input.cpp
io/filesystem.cpp
io/cdrom_stream.cpp
audio/audio.cpp
assets/asset_loader.cpp
assets/mesh_loader.cpp
assets/texture_loader.cpp
)
target_include_directories(retro_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(retro_engine PUBLIC retro_engine_hal)

Target graph contract: \`src/CMakeLists.txt\` first creates exactly one \`retro_engine_hal\` implementation (\`hal/pc\` or \`hal/ps1\`) selected by the canonical platform preset, then creates \`retro_engine\` and links the selected HAL. \`hello_triangle\` and \`rotating_cube\` link only to \`retro_engine\`; examples never link SDL3, OpenGL, PSn00bSDK or OS libraries directly.

См. §4.2.1: приведённый там \`CMakeLists.txt\` является единственным каноническим вариантом. В этом разделе используется тот же файл без изменений.

// src/hal/pc/CMakeLists.txt
find_package(OpenGL REQUIRED)
add_library(glad STATIC ${CMAKE_SOURCE_DIR}/src/third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/src/third_party/glad/include)
add_library(retro_engine_hal STATIC
platform_pc.cpp
render_opengl.cpp
input_sdl.cpp
audio_sdl.cpp
filesystem_pc.cpp
)
target_include_directories(retro_engine_hal PRIVATE ${CMAKE_SOURCE_DIR}/src/third_party/glad/include)
target_link_libraries(retro_engine_hal PUBLIC SDL3::SDL3 OpenGL::GL glad)

SDL3 is vendored/pinned and added exactly once by the PC configuration. OpenGL is supplied by the host driver/OS.
Canonical executable graph: hello_triangle and rotating_cube link only against retro_engine; retro_engine links the selected retro_engine_hal. rotating_cube is the release reference runtime target. Output binaries are placed in build/<preset>/game/.

PC dependency rule: SDL3 is pinned as a repository submodule/vendor dependency and added with add_subdirectory(); OpenGL is supplied by the host OS/driver. No host SDL package is required for the documented PC build.

 See the canonical backend target definitions in §4.3 (PC) and §4.4 (PS1). Backend CMake target declarations are intentionally not duplicated in this architecture table.
 --------------------------------------------------------------------------------------------------------------------------------------------------------------------------

### 4.4. СБОРКА PS1-ВЕРСИИ

------------------------------------------------------------------------

Canonical PS1 build: use the pinned PSn00bSDK v0.24 release and its SDK-native CMake toolchain at $env{PSN00BSDK_LIBS}/cmake/sdk.cmake. The SDK CMake integration supplies the MIPS compiler/sysroot and PS1 link rules.

Windows environment:

set PSN00BSDK_ROOT=C:\\PSn00bSDK
set PSN00BSDK_LIBS=%PSN00BSDK_ROOT%\\lib\\libpsn00b
set PATH=%PSN00BSDK_ROOT%\\bin;%PATH%

Configure and build:

cmake --preset ps1-release
cmake --build --preset ps1-release

// src/hal/ps1/CMakeLists.txt
psn00bsdk_add_executable(retro_engine_ps1 STATIC main_ps1.cpp)
target_link_libraries(retro_engine_ps1 PRIVATE retro_engine)
psn00bsdk_add_cd_image(
retro_engine_disc
retro-engine
${CMAKE_CURRENT_LIST_DIR}/cmake/ps1-disc.xml
DEPENDS retro_engine_ps1
)
The pinned SDK-native target produces the PS1 executable and invokes the documented image-packaging path; do not duplicate target declarations or manually invoke legacy ELF conversion commands.

The pinned SDK executable target generates retro_engine_ps1.elf, retro_engine_ps1.exe and retro_engine_ps1.map by default. The PS-EXE is a development/debug artifact. The disc target generates retro-engine.bin + retro-engine.cue through mkpsxiso; the .cue/.bin pair is the normative bootable runtime artifact for MVP filesystem and release acceptance. All release commands use the canonical CMakePresets.json flow.

ps1-disc.xml MUST include system.cnf, the generated retro_engine_ps1.exe, and the generated .mesh/.tex assets under an ISO9660 directory tree. system.cnf must use the exact executable filename and CD-ROM boot path expected by the PS1 BIOS.

PSn00bSDK v0.24 is pinned for the MVP. Updating the SDK requires changing the pin, rebuilding CI, and recording an ADR. Do not invoke ELF conversion tools directly when the SDK-native target already provides the required PS1 executable.

 See the canonical backend target definitions in §4.3 (PC) and §4.4 (PS1). Backend CMake target declarations are intentionally not duplicated in this architecture table.
 --------------------------------------------------------------------------------------------------------------------------------------------------------------------------

### 4.5. СБОРКА ИНСТРУМЕНТА КОНВЕРТАЦИИ АССЕТОВ

------------------------------------------------------------------------

Canonical host-tool build: the converter is built only in a PC/host configuration. The PS1 cross build consumes versioned files already present in assets/build/.

// tools/asset_converter/CMakeLists.txt
cmake_minimum_required(VERSION 3.25)
add_executable(asset_converter
main.cpp
mesh_converter.cpp
texture_converter.cpp
color_quantizer.cpp
)
target_compile_features(asset_converter PRIVATE cxx_std_14)
target_include_directories(asset_converter PRIVATE
${CMAKE_SOURCE_DIR}/src/third_party/cgltf
${CMAKE_SOURCE_DIR}/src/third_party/stb
)

SOURCE-TREE RULE: the PC asset converter intentionally generates the canonical example files into assets/build/. These outputs are deterministic and are either committed as versioned MVP assets for PS1 checkout or regenerated by the PC host build. A release build MUST verify that the generated files match the expected asset-format contract.

# Root PC configuration
add_subdirectory(tools/asset_converter)

add_custom_command(
OUTPUT
${CMAKE_SOURCE_DIR}/assets/build/cube.mesh
${CMAKE_SOURCE_DIR}/assets/build/texture.tex
COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/assets/build
COMMAND asset_converter
${CMAKE_SOURCE_DIR}/assets/source/cube.gltf
${CMAKE_SOURCE_DIR}/assets/build/cube.mesh
COMMAND asset_converter
${CMAKE_SOURCE_DIR}/assets/source/texture.png
${CMAKE_SOURCE_DIR}/assets/build/texture.tex
DEPENDS
asset_converter
${CMAKE_SOURCE_DIR}/assets/source/cube.gltf
${CMAKE_SOURCE_DIR}/assets/source/texture.png
)

add_custom_target(convert_assets ALL
DEPENDS
${CMAKE_SOURCE_DIR}/assets/build/cube.mesh
${CMAKE_SOURCE_DIR}/assets/build/texture.tex
)

## 5. УПРАВЛЕНИЕ ЗАВИСИМОСТЯМИ

> ------------------------------------------------------------------------

### 5.1. СТРАТЕГИЯ: VENDOR-ДИРЕКТОРИЯ

> ------------------------------------------------------------------------
>
> ПОЧЕМУ VENDOR, А НЕ ПАКЕТНЫЙ МЕНЕДЖЕР:
>
> ПРОБЛЕМЫ ПАКЕТНЫХ МЕНЕДЖЕРОВ (vcpkg, Conan):

- Требуют установки на каждой машине контрибьютора.

- Могут ломаться при обновлениях.

- Сложно работать с кросс-компиляцией (PS1).

- Лишние зависимости для open-source проекта.

> ПРЕИМУЩЕСТВА VENDOR:

- Pinned third-party sources are kept reproducible; system compiler/SDK binaries are pinned by documented version and checksum where practical.

- Версии зафиксированы (через git submodule или прямую копию).

- Работает с кросс-компиляцией.

- Нет внешних зависимостей в runtime.

> СТРУКТУРА VENDOR (ЕДИНСТВЕННАЯ CANONICAL LAYOUT):
>
> src/third_party/
>
> cgltf/ cgltf.h # Single-header (MIT)
>
> stb/ stb_image.h # Single-header (Public Domain) stb_image_write.h
>
> glad/ include/glad/gl.h src/gl.c
>
> psn00bsdk/ # Подмодуль или vendor-копия sdl3/ # Опционально, можно через vcpkg gtest/ # Только для тестов
>
> ЛИЦЕНЗИОННАЯ ЧИСТОТА:

- cgltf: MIT --- можно использовать

- stb: Public Domain --- можно использовать

- glad: MIT --- можно использовать

- PSn00bSDK: core libraries/CMake are MPL-2.0; mkpsxiso/dumpsxiso are GPL-2.0-or-later; review and preserve the applicable notices when redistributing.

- SDL3: zlib --- можно использовать

- Third-party licenses are reviewed individually; do not assume all licenses are interchangeable with MIT. Redistribution must preserve required notices and copyleft obligations.

> ------------------------------------------------------------------------

### 5.2. PSNOOBSDK

> ------------------------------------------------------------------------
>
> КАНОНИЧЕСКИЙ ВАРИАНТ: PSn00bSDK подключается как pinned submodule в src/third_party/psn00bsdk. Vendor-копия допустима только как содержательно идентичная frozen snapshot; локальные pull/update операции после pin запрещены.
>
> A\) Source submodule (maintainer-only): pin the PSn00bSDK repository to the exact v0.24 commit 06e65be (the signed v0.24 release commit). Do not run pull/update operations after checkout; keep the submodule at the pinned commit. Do not track the moving default branch.
>
> git submodule add <https://github.com/Lameguy64/PSn00bSDK.git> src/third_party/psn00bsdk
> cd src/third_party/psn00bsdk
> git checkout 06e65be
>
> cd ../../..
> # Do not run git pull/update operations after checking out the pinned commit.
>
> git add src/third_party/psn00bsdk git commit -m "Pin PSn00bSDK v0.24" B) Vendor-копия (если нужна стабильность): # Копируем конкретную версию git clone --branch v0.24 <https://github.com/Lameguy64/PSn00bSDK.git> temp cp -r temp src/third_party/psn00bsdk rm -rf temp
>
> # Фиксируем версию в VERSION.txt
>
> echo "PSn00bSDK pinned release tag: v0.24" > src/third_party/psn00bsdk/VERSION.txt ВЫБОР: Вариант A (git submodule) --- проще обновлять.
>
> ------------------------------------------------------------------------

### 5.3. SDL3

> ------------------------------------------------------------------------
>
> ДВА ВАРИАНТА:
>
> GTEST DEPENDENCY POLICY: vendored/submodule only for the canonical build:
>
> # Non-canonical option only: a system package manager may be used for local experiments. The canonical build always uses the pinned src/third_party/sdl3 submodule.
>
> # CMake автоматически найдет SDL3 через find_package(SDL3) B) Vendor-копия (для воспроизводимости):
>
> git submodule add <https://github.com/libsdl-org/SDL.git> src/third_party/sdl3
>
> # Собираем как часть проекта
>
> КАНОНИЧЕСКИЙ ВАРИАНТ: только pinned vendored/submodule SDL3 для local, CI and release builds. System package managers such as vcpkg are non-canonical troubleshooting options and MUST NOT be required by contributor setup.
>
> ------------------------------------------------------------------------

### 5.4. CGLTF, STB_IMAGE

> Это single-header библиотеки --- просто копируем .h файлы.
>
> УСТАНОВКА:
>
> # cgltf: retrieve a tagged release or commit-pinned archive; never fetch master/main in a reproducible build.
>
> -o src/third_party/cgltf/cgltf.h
>
> # stb: retrieve a tagged release or commit-pinned archive; never fetch master/main in a reproducible build.
>
> -o src/third_party/stb/stb_image.h curl -L <https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h> \
>
> -o src/third_party/stb/stb_image_write.h
>
> # Фиксируем версии
>
> # Store the exact upstream tag/commit and SHA-256 in VERSION.txt; CI verifies the recorded hash.
>
> ИСПОЛЬЗОВАНИЕ:
>
> // mesh_converter.cpp
>
> #define CGLTF_IMPLEMENTATION
>
> #include "cgltf.h"
>
> // texture_converter.cpp
>
> #define STB_IMAGE_IMPLEMENTATION
>
> #include "stb_image.h"
>
> ------------------------------------------------------------------------

### 5.5. GTEST ДЛЯ UNIT-ТЕСТОВ

------------------------------------------------------------------------

КАНОНИЧЕСКАЯ СТРАТЕГИЯ: GoogleTest хранится как git submodule в src/third_party/googletest и подключается через add_subdirectory(). Network FetchContent не используется в canonical build, чтобы локальная сборка и CI имели один и тот же набор исходников.

// tests/CMakeLists.txt
add_subdirectory(${CMAKE_SOURCE_DIR}/src/third_party/googletest ${CMAKE_BINARY_DIR}/googletest EXCLUDE_FROM_ALL)

add_executable(retro_engine_tests
math_test.cpp
memory_test.cpp
ecs_test.cpp
)

target_link_libraries(retro_engine_tests PRIVATE
retro_engine
GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(retro_engine_tests)

```text
add_executable(retro_engine_tests

math_test.cpp ecs_test.cpp memory_test.cpp
```
## 6. CI/CD НА GITHUB ACTIONS

> ------------------------------------------------------------------------

### 6.1. АРХИТЕКТУРА CI

> ------------------------------------------------------------------------
>
> WORKFLOWS:

- build-pc.yml --- validation build of the PC version on Windows, Linux and macOS

- build-ps1.yml --- сборка PS1-версии (cross-compile)

- tests.yml --- unit-тесты

- release.yml --- создание релиза при теге

> ТРИГГЕРЫ:

- push в main/master

- pull request в main/master

- tag v* (для релизов)

> МАТРИЦА СБОРКИ: PC CI покрывает windows-latest, ubuntu-latest и macos-latest в Release-конфигурации с системным toolchain каждого runner. Compiler-by-compiler cross-OS Cartesian matrix не является MVP acceptance contract; отдельные Debug/Clang presets существуют для локального и quality CI использования.

- ОС: windows-latest, ubuntu-latest, macos-latest

- Компилятор: MSVC, GCC, Clang

- Тип сборки: Debug, Release

> ------------------------------------------------------------------------

### 6.2. WORKFLOW: СБОРКА PC-ВЕРСИИ

// .github/workflows/build-pc.yml

name: Build PC

on:

push:

branches: [ main, master, develop ]

pull_request:

branches: [ main, master, develop ]

jobs:

build:

strategy:

matrix:

os: [ubuntu-latest, windows-latest, macos-latest]

runs-on: ${{ matrix.os }}

steps:

- uses: actions/checkout@v4

with:

submodules: recursive

- name: Install dependencies on Linux
if: runner.os == 'Linux'
shell: bash
run: sudo apt-get update && sudo apt-get install -y cmake ninja-build libgl1-mesa-dev

- name: Install dependencies on macOS
 if: runner.os == 'macOS'
 shell: bash
 run: brew install cmake ninja
- name: Install dependencies on Windows
 if: runner.os == 'Windows'
 shell: pwsh
 run: choco install cmake ninja -y

- name: Configure and build

run: \|

cmake --preset pc-release

cmake --build --preset pc-release

- name: Test

run: ctest --preset pc-release --output-on-failure

### 6.3. WORKFLOW: СБОРКА PS1-ВЕРСИИ

// .github/workflows/build-ps1.yml

name: Build PS1

on:

push:

branches: [ main, master, develop ]

pull_request:

branches: [ main, master, develop ]

jobs:

build:

runs-on: ubuntu-latest

steps:

- uses: actions/checkout@v4

with:

submodules: recursive

- name: Install host tools

run: sudo apt-get update && sudo apt-get install -y cmake ninja curl unzip

- name: Install pinned PSn00bSDK v0.24

env:

PSN00BSDK_RELEASE_URL: <https://github.com/Lameguy64/PSn00bSDK/releases/download/v0.24/PSn00bSDK-0.24-Linux.zip>
PSN00BSDK_SHA256_FILE: ci/psn00bsdk-v0.24-linux.sha256

run: \|

test -f "${PSN00BSDK_SHA256_FILE}" && test -n "$PSN00BSDK_RELEASE_URL"

mkdir -p "$RUNNER_TEMP/psn00bsdk"

curl -fL "$PSN00BSDK_RELEASE_URL" -o "$RUNNER_TEMP/psn00bsdk.zip"

CHECKSUM=$(awk '{print $1}' "${PSN00BSDK_SHA256_FILE}")
printf "%s %s\\n" "$CHECKSUM" "$RUNNER_TEMP/psn00bsdk.zip" \| sha256sum -c -

unzip -q "$RUNNER_TEMP/psn00bsdk.zip" -d "$RUNNER_TEMP/psn00bsdk"

SDK_CMAKE="$(find "$RUNNER_TEMP/psn00bsdk" -type f -path '*/lib/libpsn00b/cmake/sdk.cmake' -print -quit)"
test -n "$SDK_CMAKE"
SDK_LIBS="${SDK_CMAKE%/cmake/sdk.cmake}"
SDK_ROOT="$(cd "$SDK_LIBS/../.." && pwd)"
echo "PSN00BSDK_ROOT=$SDK_ROOT" >> "$GITHUB_ENV"
echo "PSN00BSDK_LIBS=$SDK_LIBS" >> "$GITHUB_ENV"
echo "$SDK_ROOT/bin" >> "$GITHUB_PATH"

test -n "$SDK_ROOT"

echo "PSN00BSDK_ROOT=$SDK_ROOT" >> "$GITHUB_ENV"

echo "PSN00BSDK_ROOT=$SDK_ROOT" >> "$GITHUB_ENV"
echo "PSN00BSDK_LIBS=$SDK_LIBS" >> "$GITHUB_ENV"
echo "$SDK_ROOT/bin" >> "$GITHUB_PATH"

- name: Configure and build

run: \|

test -f "$PSN00BSDK_LIBS/cmake/sdk.cmake"

cmake --preset ps1-release

cmake --build --preset ps1-release

- name: Upload PS1 artifacts

uses: actions/upload-artifact@v4

with:

name: retro-engine-ps1-${{ github.sha }}

path: build/ps1-release/

### 6.4. WORKFLOW: UNIT-ТЕСТЫ

> ------------------------------------------------------------------------
>
> // .github/workflows/tests.yml
>
> name: Unit Tests

CI policy: \`pc-debug\` and \`pc-release\` presets set \`BUILD_RENDER_INTEGRATION_TESTS=OFF\` by default because OpenGL integration tests require a compatible display/context. Headless CI runs unit/asset/invariant tests by default; renderer framebuffer tests run only in an explicitly provisioned graphics job.

> on:
> push:
> branches: [ main, master, develop ]
> pull_request:
> branches: [ main, master, develop ]
>
> jobs:
> test:
> runs-on: ubuntu-latest
> steps:
>
> - uses: actions/checkout@v4
> with:
> submodules: recursive
>
> - name: Install host build tools
> run: sudo apt-get update && sudo apt-get install -y cmake ninja-build libgl1-mesa-dev
>
> - name: Configure, build and test
> run: \|
> cmake --preset pc-debug
> cmake --build --preset pc-debug
> ctest --preset pc-debug --output-on-failure

-
-
-

-
-
-

### 6.5. WORKFLOW: СБОРКА РЕЛИЗА

> ------------------------------------------------------------------------

- // .github/workflows/release.yml
 name: Release

 on:
 push:
 tags: ["v*"]

 permissions:
 contents: write

 jobs:
 build:
 runs-on: ubuntu-latest
 steps:

 - uses: actions/checkout@v4
 with:
 submodules: recursive

 - name: Install host tools
 run: sudo apt-get update && sudo apt-get install -y cmake ninja-build libgl1-mesa-dev curl unzip

 - name: Install pinned PSn00bSDK v0.24
 env:
 PSN00BSDK_RELEASE_URL: <https://github.com/Lameguy64/PSn00bSDK/releases/download/v0.24/PSn00bSDK-0.24-Linux.zip>
 PSN00BSDK_SHA256_FILE: ci/psn00bsdk-v0.24-linux.sha256
 shell: bash
 run: \|
 set -euo pipefail
 test -f "${PSN00BSDK_SHA256_FILE}"
 mkdir -p "$RUNNER_TEMP/psn00bsdk"
 curl -fL "$PSN00BSDK_RELEASE_URL" -o "$RUNNER_TEMP/psn00bsdk.zip"
 CHECKSUM=$(awk '{print $1}' "${PSN00BSDK_SHA256_FILE}")
 printf "%s %s\\n" "$CHECKSUM" "$RUNNER_TEMP/psn00bsdk.zip" \| sha256sum -c -
 unzip -q "$RUNNER_TEMP/psn00bsdk.zip" -d "$RUNNER_TEMP/psn00bsdk"
 SDK_CMAKE="$(find "$RUNNER_TEMP/psn00bsdk" -type f -path '*/lib/libpsn00b/cmake/sdk.cmake' -print -quit)"
 test -n "$SDK_CMAKE"
 SDK_LIBS="${SDK_CMAKE%/cmake/sdk.cmake}"
 SDK_ROOT="$(cd "$SDK_LIBS/../.." && pwd)"
 echo "PSN00BSDK_ROOT=$SDK_ROOT" >> "$GITHUB_ENV"
 echo "PSN00BSDK_LIBS=$SDK_LIBS" >> "$GITHUB_ENV"
 echo "$SDK_ROOT/bin" >> "$GITHUB_PATH"

 - name: Build PC reference target
 shell: bash
 run: \|
 set -euo pipefail
 cmake --preset pc-release
 cmake --build --preset pc-release --target hello_triangle rotating_cube

 - name: Build PS1 and bootable disc image
 shell: bash
 run: \|
 set -euo pipefail
 test -f "$PSN00BSDK_LIBS/cmake/sdk.cmake"
 cmake --preset ps1-release
 cmake --build --preset ps1-release

 - name: Validate release artifacts
 shell: bash
 run: \|
 set -euo pipefail
 test -f build/pc-release/game/hello_triangle
 test -f build/pc-release/game/rotating_cube
 test -f build/ps1-release/retro_engine_ps1.exe
 test -f build/ps1-release/retro-engine.bin
 test -f build/ps1-release/retro-engine.cue

 - name: Prepare release payload
 shell: bash
 run: \|
 set -euo pipefail
 rm -rf release_payload
 mkdir -p release_payload/bin/pc release_payload/bin/ps1
 cp build/pc-release/game/hello_triangle release_payload/bin/pc/
 cp build/pc-release/game/rotating_cube release_payload/bin/pc/
 cp build/ps1-release/retro_engine_ps1.exe release_payload/bin/ps1/
 cp build/ps1-release/retro-engine.bin build/ps1-release/retro-engine.cue release_payload/bin/ps1/
 cp README.md LICENSE release_payload/
 ./scripts/create_release_archive.sh "${GITHUB_REF_NAME}" release_payload

 - name: Create GitHub Release
 uses: softprops/action-gh-release@v2
 with:
 files: retro-engine-${{ github.ref_name }}.tar.gz
 generate_release_notes: true

### 7.1. UNIT-ТЕСТЫ (МАТЕМАТИКА, ПАМЯТЬ, ECS)

------------------------------------------------------------------------

```cpp
// tests/math_test.cpp

#include <gtest/gtest.h>

#include "core/math/fixed.h"

#include "core/math/vec3.h"

#include <cmath>

#include <random>

TEST(FixedPoint, Conversion) {

EXPECT_EQ(fixed_from_int(5), 5 << 16);

EXPECT_EQ(fixed_to_int(fixed_from_int(5)), 5);

EXPECT_NEAR(fixed_to_float(fixed_from_float(3.14f)), 3.14f, 0.001f);

}

TEST(FixedPoint, Arithmetic) {

fixed_t a = fixed_from_int(10); fixed_t b = fixed_from_int(3);

EXPECT_EQ(fixed_add(a, b), fixed_from_int(13));

EXPECT_EQ(fixed_sub(a, b), fixed_from_int(7));

EXPECT_EQ(fixed_mul(a, b), fixed_from_int(30));

}

TEST(FixedPoint, AccuracyStressTest) {
// Inputs are exact raw 16.16 values; reference arithmetic uses double.
std::mt19937 rng(42);
std::uniform_int_distribution<int32_t> raw_a(-6 * FIXED_ONE, 6 * FIXED_ONE);
std::uniform_int_distribution<int32_t> raw_abs_b(FIXED_ONE / 2, 6 * FIXED_ONE);
double max_mul_error = 0.0;
double max_div_error = 0.0;
const int N = 10000;
for (int i = 0; i < N; ++i) {
const fixed_t a = raw_a(rng);
fixed_t b = raw_abs_b(rng);
if ((i & 1) != 0) b = -b;
const double af = static_cast<double>(a) / FIXED_ONE;
const double bf = static_cast<double>(b) / FIXED_ONE;
const double mul_ref = af * bf;
const double div_ref = af / bf;
max_mul_error = std::max(
max_mul_error,
std::abs(static_cast<double>(fixed_to_float(fixed_mul(a, b))) - mul_ref));
max_div_error = std::max(
max_div_error,
std::abs(static_cast<double>(fixed_to_float(fixed_div(a, b))) - div_ref));
}
EXPECT_LT(max_mul_error, 2.0 / FIXED_ONE);
EXPECT_LT(max_div_error, 2.0 / FIXED_ONE);
}

TEST(FixedPoint, Determinism) {
const fixed_t a = fixed_from_int(123);
const fixed_t b = fixed_from_int(456);
const fixed_t c1 = fixed_mul(a, b);
const fixed_t c2 = fixed_mul(a, b);
EXPECT_EQ(c1, c2);
EXPECT_EQ(c1, fixed_from_int(123 * 456));
}

```
```text
// Один и тот же вход -> один и тот же выход

fixed_t a = fixed_from_int(123); fixed_t b = fixed_from_int(456);
```
```cpp
TEST(Vec3, Operations) {

Vec3 a = vec3_make(fixed_from_int(1), fixed_from_int(2), fixed_from_int(3));

Vec3 b = vec3_make(fixed_from_int(4), fixed_from_int(5), fixed_from_int(6));

Vec3 c = vec3_add(a, b);

EXPECT_EQ(c.x, fixed_from_int(5));

EXPECT_EQ(c.y, fixed_from_int(7)); EXPECT_EQ(c.z, fixed_from_int(9));

fixed_t dot = vec3_dot(a, b);

EXPECT_EQ(dot, fixed_from_int(1*4 + 2*5 + 3*6)); // 32

}

// tests/memory_test.cpp

#include <gtest/gtest.h>

#include "core/memory/linear_allocator.h"

#include "core/memory/pool_allocator.h"

TEST(LinearAllocator, BasicAllocation) {

uint8_t buffer[1024]; LinearAllocator alloc(buffer, sizeof(buffer));

void* p1 = alloc.allocate(100);

EXPECT_NE(p1, nullptr);

EXPECT_EQ(alloc.used(), 100);

void* p2 = alloc.allocate(200);

EXPECT_NE(p2, nullptr);

EXPECT_EQ(alloc.used(), 300);

alloc.reset();

EXPECT_EQ(alloc.used(), 0);

}

TEST(LinearAllocator, OutOfMemory) {

uint8_t buffer[100]; LinearAllocator alloc(buffer, sizeof(buffer));

void* p1 = alloc.allocate(50); EXPECT_NE(p1, nullptr);

void* p2 = alloc.allocate(60);

EXPECT_EQ(p2, nullptr); // Не хватает памяти

}

TEST(PoolAllocator, BasicAllocation) {

struct TestObject { int x, y; };

Pool<TestObject, 10> pool;

TestObject* obj1 = pool.allocate();

EXPECT_NE(obj1, nullptr); obj1->x = 42;

TestObject* obj2 = pool.allocate();

EXPECT_NE(obj2, nullptr); obj2->x = 100;

pool.deallocate(obj1);

TestObject* obj3 = pool.allocate();

EXPECT_EQ(obj3, obj1); // Переиспользование памяти

}

// tests/ecs_test.cpp

#include <gtest/gtest.h>

#include "core/ecs/registry.h"

struct Position { int x, y; }; struct Velocity { int dx, dy; };

TEST(ECS, CreateEntity) {

Registry registry;

uint32_t e1 = registry.create_entity(); uint32_t e2 = registry.create_entity();

EXPECT_NE(e1, e2);

EXPECT_NE(e1, 0);

EXPECT_NE(e2, 0);

}

TEST(ECS, AddComponent) { Registry registry; uint32_t e = registry.create_entity(); registry.add_component<Position>(e, {10, 20});

EXPECT_TRUE(registry.has_component<Position>(e));

EXPECT_FALSE(registry.has_component<Velocity>(e));

Position* pos = registry.get_component<Position>(e);

EXPECT_EQ(pos->x, 10);

EXPECT_EQ(pos->y, 20);

}

TEST(ECS, View) {

Registry registry;

uint32_t e1 = registry.create_entity(); registry.add_component<Position>(e1, {1, 2}); registry.add_component<Velocity>(e1, {10, 20});

uint32_t e2 = registry.create_entity(); registry.add_component<Position>(e2, {3, 4}); // e2 не имеет Velocity

uint32_t e3 = registry.create_entity(); registry.add_component<Position>(e3, {5, 6}); registry.add_component<Velocity>(e3, {30, 40});

auto view = registry.view<Position, Velocity>();

int count = 0; for (auto entity : view) { count++;

EXPECT_TRUE(entity == e1 \|\| entity == e3);

}

EXPECT_EQ(count, 2); // Только e1 и e3 имеют оба компонента

}

------------------------------------------------------------------------

```
### 7.2. INTEGRATION-ТЕСТЫ (РЕНДЕРЕР, ЗАГРУЗКА АССЕТОВ)

------------------------------------------------------------------------

```cpp
// tests/integration/mesh_loader_test.cpp

#include <gtest/gtest.h>

#include "assets/mesh_loader.h"

#include "core/memory/linear_allocator.h"

TEST(MeshLoader, LoadValidMesh) {

uint8_t buffer[1024 * 1024]; LinearAllocator alloc(buffer, sizeof(buffer));

Mesh* mesh = mesh_load("assets/build/cube.mesh", &alloc);

ASSERT_NE(mesh, nullptr);

EXPECT_GT(mesh->vertex_count, 0);

EXPECT_GT(mesh->index_count, 0);

EXPECT_NE(mesh->vertices, nullptr);

EXPECT_NE(mesh->indices, nullptr);

}

TEST(MeshLoader, LoadInvalidFile) {

uint8_t buffer[1024 * 1024]; LinearAllocator alloc(buffer, sizeof(buffer));

Mesh* mesh = mesh_load("nonexistent.mesh", &alloc);

EXPECT_EQ(mesh, nullptr);

}

------------------------------------------------------------------------

```
### 7.3. SMOKE-ТЕСТЫ В ЭМУЛЯТОРЕ (ОПЦИОНАЛЬНО)

> АВТОМАТИЗАЦИЯ DUCKSTATION:
>
> # scripts/test_emulator.sh
>
> #!/bin/bash
>
> # Emulator smoke test is version-specific. Pin a known DuckStation build and validate its CLI flags before enabling CI. This script is illustrative, not a normative CI command.
>
> # и проверяем, что она не крашится в течение 10 секунд
>
> EMU_PATH="/c/Program Files/DuckStation/duckstation-qt-x64.exe"
>
> timeout 10 "$EMU_PATH" [PINNED_DUCKSTATION_ARGS] "$GAME_PATH" > "$LOG_FILE" 2>&1 EXIT_CODE=$?
>
> if [[ "$EXIT_CODE" -eq 124 ]]; then
> echo "PASS: Game ran for ${TIMEOUT} seconds without crashing"
> exit 0
> elif [[ "$EXIT_CODE" -eq 0 ]]; then
> echo "PASS: Game exited normally"
> exit 0
> else
> echo "FAIL: Game exited with code $EXIT_CODE" >&2
> exit "$EXIT_CODE"
> fi
>
> else
>
> echo "FAIL: Game crashed or exited prematurely"
>
> cat "$LOG_FILE" exit 1
>
> fi
>
> ИНТЕГРАЦИЯ С CI:
>
> # В .github/workflows/tests.yml добавить:

- name: Install DuckStationif: runner.os == 'Linux' run: \| wget <https://github.com/stenzek/duckstation/releases/latest/download/duckstation-qt-x64.AppImage> chmod +x duckstation-qt-x64.AppImage

- name: Run emulator smoke testrun: \|

> ./scripts/test_emulator.sh

## 8. УПРАВЛЕНИЕ ВЕРСИЯМИ И РЕЛИЗАМИ

> ------------------------------------------------------------------------

### 8.1. GIT FLOW ДЛЯ СОЛО-РАЗРАБОТЧИКА

> ------------------------------------------------------------------------
>
> УПРОЩЁННЫЙ GIT FLOW:
>
> Ветки:

- main (master) --- стабильная версия, всегда готова к релизу

- develop --- основная ветка разработки

- feature/* --- ветки для новых фич

- fix/* --- ветки для исправлений

- release/* --- ветки для подготовки релизаПОТОК РАБОТЫ:

- Начинаем новую фичу

> git checkout develop git checkout -b feature/ecs-system

- Работаем в ветке фичи

> git add .
>
> git commit -m "Add ECS registry" git push origin feature/ecs-system

- Создаём Pull Request в develop

- CI автоматически запускает тесты

- Ревью кода (если есть контрибьюторы)

- Merge в develop

- Готовим релиз

> git checkout develop git checkout -b release/v0.1.0
>
> # Исправляем баги, обновляем документацию git commit -m "Prepare release v0.1.0"

- Релизим

> git checkout main
>
> git merge --no-ff release/v0.1.0 git tag -a v0.1.0 -m "Release v0.1.0" git push origin main --tags git checkout develop git merge --no-ff release/v0.1.0

- Удаляем ветку релиза

> git branch -d release/v0.1.0
>
> ------------------------------------------------------------------------

### 8.2. SEMANTIC VERSIONING

> ------------------------------------------------------------------------
>
> ФОРМАТ: MAJOR.MINOR.PATCH
>
> MAJOR (X.0.0):

- Несовместимые изменения API

- Удаление старых фич

- Пример: v1.0.0 -> v2.0.0 (переписали ECS) MINOR (0.X.0):

- Новые фичи (обратно совместимые)

- Пример: v0.1.0 -> v0.2.0 (добавили поддержку Saturn) PATCH (0.0.X):

- Исправления багов

- Пример: v0.1.0 -> v0.1.1 (исправили утечку памяти) ПРАВИЛА ДЛЯ MVP:

- Пока API нестабильно --- MAJOR = 0

- Первый релиз: v0.1.0

- Стабильная версия: v1.0.0

> ------------------------------------------------------------------------

### 8.3. CHANGELOG

> ------------------------------------------------------------------------
>
> // CHANGELOG.md
>
> # Changelog
>
> Все значимые изменения проекта документируются в этом файле. Формат основан на [Keep a Changelog](<https://keepachangelog.com/>), и этот проект придерживается [Semantic Versioning](<https://semver.org/>).
>
> ## [Unreleased]
>
> ### Added

- Initial project structure

- CMake build system for PC and PS1

> ### Changed

- N/A

> ### Fixed

- N/A

> ## [0.1.0] - 2026-XX-XX
>
> ### Added

- Core: ECS (Entity Component System)

- Core: Fixed-point math (16.16)

- Core: Memory management (Linear, Pool, Stack allocators)

- HAL: PC implementation (SDL3 + OpenGL)

- HAL: PS1 implementation (PSn00bSDK)

- Renderer: Affine texture mapping

- Renderer: Painter's algorithm (no Z-buffer)

- Asset pipeline: glTF to .mesh converter

- Asset pipeline: PNG to .tex converter (8-bit indexed + RGB555+STP palette)

- Examples: hello_triangle and rotating_cube; both use the canonical PC preset output directory build/pc-release/game/.

- Documentation: README, ARCHITECTURE, BUILDING, API

> ### Known Issues

- Audio is stub (beep only)

- - Physics is intentionally limited to demo-level gravity/jump plus AABB collisions; no general physics engine.

- No networking

> ------------------------------------------------------------------------

### 8.4. GITHUB RELEASES

> АВТОМАТИЧЕСКИЕ РЕЛИЗЫ:
>
> При создании тега v* GitHub Actions автоматически:

- Собирает только те платформы, для которых в release workflow есть validated toolchain jobs; MVP release must publish PC + PS1 artifacts.

- Создаёт GitHub Release

- Прикладывает архивы с бинарниками

- Генерирует GitHub release notes согласно настройкам workflow; CHANGELOG.md входит в release payload как отдельный файл.

> РУЧНАЯ ПРОВЕРКА ПЕРЕД РЕЛИЗОМ:

- Прогнать все unit-тесты

> cmake --build --preset pc-debug
> ctest --preset pc-debug --output-on-failure

- Протестировать примеры на PC

> build/pc-release/game/hello_triangle build/pc-release/game/rotating_cube

- Протестировать примеры на PS1 (эмулятор)

> scripts\\run_ps1.bat

- Проверить документацию

- README.md актуален?

- BUILDING.md работает?

- Примеры компилируются?

## 5. Обновить CHANGELOG.md

- Перенести [Unreleased] в [X.Y.Z]

- Добавить дату

- Создать тег

> git tag -a vX.Y.Z -m "Release vX.Y.Z" git push origin main --tags

## 9. СКРИПТЫ АВТОМАТИЗАЦИИ

> ------------------------------------------------------------------------

### 9.1. SETUP_DEV.BAT --- ПЕРВОНАЧАЛЬНАЯ НАСТРОЙКА

// scripts/setup_dev.bat

\@echo off

setlocal

echo Retro Engine development setup

where git >nul 2>nul \|\| (echo ERROR: Git not found.& exit /b 1)

where cmake >nul 2>nul \|\| (echo ERROR: CMake not found.& exit /b 1)

where ninja >nul 2>nul \|\| (echo ERROR: Ninja not found.& exit /b 1)

git submodule update --init --recursive

if not exist "C:\\emulators\\DuckStation\\duckstation-qt-x64.exe" echo WARNING: DuckStation not found; PC setup can still continue.

cmake --preset pc-debug

if errorlevel 1 exit /b 1

cmake --build --preset pc-debug

if errorlevel 1 exit /b 1

ctest --preset pc-debug --output-on-failure

if errorlevel 1 exit /b 1

echo Setup complete.

endlocal

### 9.2. BUILD_PC.BAT --- БЫСТРАЯ СБОРКА PC

// scripts/build_pc.bat

\@echo off

setlocal

cmake --preset pc-release

if errorlevel 1 exit /b 1

cmake --build --preset pc-release

if errorlevel 1 exit /b 1

echo Build successful.

endlocal

### 9.3. BUILD_PS1.BAT --- БЫСТРАЯ СБОРКА PS1

// scripts/build_ps1.bat

\@echo off

setlocal

set "PSN00BSDK_ROOT=C:\\PSn00bSDK"

set "PSN00BSDK_LIBS=%PSN00BSDK_ROOT%\\lib\\libpsn00b"

if not exist "%PSN00BSDK_LIBS%\\cmake\\sdk.cmake" (echo ERROR: PSn00bSDK v0.24 toolchain not found.& exit /b 1)

cmake --preset ps1-release

if errorlevel 1 exit /b 1

cmake --build --preset ps1-release

if errorlevel 1 exit /b 1

echo Build successful.

endlocal

### 9.4. RUN_PC.BAT --- ЗАПУСК PC-ВЕРСИИ

// scripts/run_pc.bat

\@echo off

setlocal

set "EXE_PATH=build\\pc-release\\game\\rotating_cube.exe"

if not exist "%EXE_PATH%" (echo ERROR: PC example not found: %EXE_PATH% & exit /b 1)

start "RetroEngine PC" "%EXE_PATH%"

endlocal

### 9.5. RUN_PS1.BAT --- ЗАПУСК PS1-ВЕРСИИ В ЭМУЛЯТОРЕ

// scripts/run_ps1.bat

\@echo off

setlocal

set EMU_PATH=C:\\emulators\\DuckStation\\duckstation-qt-x64.exe

set "DISC_PATH=build\\ps1-release\\retro-engine.cue"

if not exist "%EMU_PATH%" (echo ERROR: DuckStation not found at %EMU_PATH% & exit /b 1)

if not exist "%DISC_PATH%" (echo ERROR: PS1 disc image not found: %DISC_PATH% & exit /b 1)

"%EMU_PATH%" "%DISC_PATH%"

set RESULT=%ERRORLEVEL%

endlocal & exit /b %RESULT%

### 9.6. TEST.BAT --- ЗАПУСК ТЕСТОВ

// scripts/test.bat

\@echo off

setlocal

cmake --preset pc-debug

if errorlevel 1 exit /b 1

cmake --build --preset pc-debug --target retro_engine_tests

if errorlevel 1 exit /b 1

ctest --preset pc-debug --output-on-failure

set RESULT=%ERRORLEVEL%

endlocal & exit /b %RESULT%

## 10. ДОКУМЕНТАЦИЯ ДЛЯ КОНТРИБЬЮТОРОВ

> ------------------------------------------------------------------------

### 10.1. СТРУКТУРА ДОКУМЕНТАЦИИ

> ------------------------------------------------------------------------
>
> docs/
>
> CONTRIBUTING.md # Как внести вклад
>
> CODE_OF_CONDUCT.md # Кодекс поведения
>
> BUILDING.md # Инструкция по сборке
>
> ARCHITECTURE.md # Архитектура движка
>
> API.md # API движка
>
> ASSETS.md # Работа с ассетами
>
> TESTING.md # Как тестировать
>
> STYLE_GUIDE.md # Стиль кода
>
> TROUBLESHOOTING.md # Частые проблемы
>
> ------------------------------------------------------------------------

### 10.2. CONTRIBUTING.MD (КЛЮЧЕВЫЕ РАЗДЕЛЫ)

> ------------------------------------------------------------------------
>
> // docs/CONTRIBUTING.md
>
> # Contributing to Retro Engine
>
> Спасибо за интерес к проекту! Этот документ поможет вам начать.
>
> ## Начало работы

- Форкните репозиторий

- Настройте среду разработки (см. scripts/setup_dev.bat)

- Создайте ветку для вашей фичи: git checkout -b feature/my-feature

- Внесите изменения

- Запустите тесты: scripts/test.bat

- Закоммитьте: git commit -m "Add my feature"

- Запушьте: git push origin feature/my-feature

- Создайте Pull Request

> ## Требования к коду

- C++14 стандарт

- Следуйте стилю из STYLE_GUIDE.md

- Все публичные функции должны иметь документацию (Doxygen)

- Unit-тесты для новой функциональности обязательны

- Код должен компилироваться на PC и PS1

> ## Процесс ревью

- CI автоматически запустит тесты

- Минимум 1 ревью от мейнтейнера (если есть)

- Все комментарии должны быть разрешены перед merge

- Squash merge в develop

> ## Что можно улучшить прямо сейчас
>
> См. issues с лейблом "good first issue" --- это задачи для новичков.
>
> ------------------------------------------------------------------------

### 10.3. STYLE_GUIDE.MD (КЛЮЧЕВЫЕ ПРАВИЛА)

> ------------------------------------------------------------------------
>
> // docs/STYLE_GUIDE.md
>
> # Style Guide
>
> ## Именование

- Классы/структуры: PascalCase (LinearAllocator, Vec3)

- Функции: snake_case (fixed_mul, vec3_add)

- Переменные: m_prefix для членов класса (m_offset, m_capacity)

- Константы: UPPER_SNAKE_CASE (FIXED_ONE, FIXED_BITS)

- Файлы: snake_case (linear_allocator.h)

> ## Форматирование

- 4 пробела (не табы)

- Максимум 100 символов в строке

- Открывающая скобка на той же строке

- Пробелы вокруг операторов

> ## Комментарии
>
> // Однострочный комментарий
>
> /* Многострочный комментарий */
>
> /// Doxygen-комментарий для публичного API
>
> /// \@param x Описание параметра
>
> /// \@return Описание возвращаемого значения
>
> ## Запрещено

- auto в публичном API (только в локальном коде)

- using namespace в заголовочных файлах

- Глобальные переменные (кроме синглтонов)

- new/delete в runtime (используйте аллокаторы)

- printf в релизной сборке (используйте Logger)

## 11. МОНИТОРИНГ И МЕТРИКИ

> ------------------------------------------------------------------------

### 11.1. МЕТРИКИ ЗДОРОВЬЯ ПРОЕКТА

> ------------------------------------------------------------------------
>
> ТЕХНИЧЕСКИЕ МЕТРИКИ (отслеживаются в CI):

- Размер бинарника PS1

- Цель: < 500 КБ

- Алерт: > 800 KiB; выход за acceptance ceiling является release blocker.

- Скрипт: scripts/check_binary_size.sh

- Использование RAM на PS1

- Цель: peak <= 1.5 MiB на W1; нормативная acceptance ceiling = 1.5 MiB.

- Алерт: > 1.5 MiB; выход за acceptance ceiling является release blocker.

-

- Время компиляции

- Цель: < 30 секунд (инкрементальная сборка PC)

- Алерт: > 2 минуты

- Мониторинг: GitHub Actions duration

- Покрытие тестами

- Цель: > 70% для core/

- Алерт: < 50%

- Инструмент: gcov + Codecov

> МЕТРИКИ СООБЩЕСТВА (отслеживаются вручную раз в месяц):

- Звезды на GitHub

- Количество форков

- Количество открытых issues

- Количество pull requests

- Активность в Discord (если создан)

> ------------------------------------------------------------------------

### 11.2. СКРИПТ ПРОВЕРКИ РАЗМЕРА БИНАРНИКА

> ------------------------------------------------------------------------
>
> // scripts/check_binary_size.sh
>
> #!/bin/bash
>
> # Проверка размера PS1-бинарника
>
> MAX_SIZE_KB=500
>
> EXE_PATH="${PS1_EXE_PATH}" if [ ! -f "$EXE_PATH" ]; then echo "ERROR: $EXE_PATH not found" exit 1
>
> fi
>
> SIZE_BYTES=$(stat -c%s "$EXE_PATH" 2>/dev/null \|\| stat -f%z "$EXE_PATH") SIZE_KB=$((SIZE_BYTES / 1024)) echo "Binary size: ${SIZE_KB} KB (max: ${MAX_SIZE_KB} KB)" if [ $SIZE_KB -gt $MAX_SIZE_KB ]; then echo "ERROR: Binary exceeds size limit!" exit 1
>
> fi
>
> echo "OK: Binary size is within limits"
>
> ------------------------------------------------------------------------

### 11.3. MEMORY PROFILER ДЛЯ PS1

> ------------------------------------------------------------------------

```cpp
// core/memory/memory_profiler.h

#pragma once

#include <stdint.h>

struct MemoryStats { uint32_t total_ram; uint32_t used_ram; uint32_t total_vram; uint32_t used_vram; uint32_t linear_alloc_used; uint32_t pool_alloc_used; uint32_t stack_alloc_used; };

class MemoryProfiler {

public:

static MemoryStats get_stats(); static void print_stats(); // Выводит в лог

// Алерт, если использование > 90% static bool check_thresholds(); };

// Использование в главном цикле:

#ifdef DEBUG

if (frame_count % 60 == 0) { // Раз в секунду

MemoryProfiler::print_stats(); if (!MemoryProfiler::check_thresholds()) {

Logger::log("WARNING: Memory usage critical!");

}

}

#endif

```
## 12. DISASTER RECOVERY

> ------------------------------------------------------------------------

### 12.1. ЧТО МОЖЕТ ПОЙТИ НЕ ТАК

> ------------------------------------------------------------------------

#### РИСК 1: Потеря локальной машины

- Вероятность: низкая

- Влияние: высокое

- Решение: весь код в GitHub, настройки в scripts/

#### РИСК 2: PSn00bSDK прекращает поддержку

- Вероятность: низкая при зафиксированном release package; лицензии компонентов указаны в LEGAL.md и должны проверяться при обновлении

- Влияние: высокое

- Решение: хранить зафиксированный release/package reference и checksum; vendor-копия допустима только при соблюдении лицензий компонентов PSn00bSDK.

#### РИСК 3: SDL3 меняет API

- Вероятность: средняя

- Влияние: среднее

- Решение: фиксируем версию SDL3, используем vendor

#### РИСК 4: GitHub удаляет репозиторий

- Вероятность: очень низкая

- Влияние: критическое

- Решение: локальный backup раз в неделю + mirror на GitLab

#### РИСК 5: Выгорание соло-разработчика

- Вероятность: высокая

- Влияние: критическое

- Решение: работа 10-15 часов в неделю, регулярные перерывы

> ------------------------------------------------------------------------

### 12.2. ПЛАН ВОССТАНОВЛЕНИЯ

> ------------------------------------------------------------------------
>
> ЕСЛИ ВСЁ ПОТЕРЯНО (кроме GitHub):

- Клонируем репозиторий

> git clone <https://github.com/yourname/retro-engine.git>

- Запускаем setup

> cd retro-engine scripts\\setup_dev.bat

- Проверяем сборку

> scripts\\build_pc.bat scripts\\test.bat

- Всё работает за 30 минут.

> ЕСЛИ GITHUB НЕДОСТУПЕН:

- Используем локальный backup

- Папка C:\\backup\\retro-engine\\ (обновляется скриптом)

- Или mirror на GitLab

- Восстанавливаем из backup

> xcopy C:\\backup\\retro-engine\\ C:\\projects\\retro-engine\\ /E /I

- Пересоздаём remote

> git remote set-url origin <new-url>
>
> ЕСЛИ КРОСС-КОМПИЛЯТОР СЛОМАЛСЯ:

- Переустанавливаем toolchain

> # Удаляем старый rmdir /s /q C:\\psn00b-sdk\\toolchain
>
> # Скачиваем новый
>
> # ... (см. scripts/setup_dev.bat)

## 2. Пересобираем PSn00bSDK

> cd src\\third_party\\psn00bsdk
>
> # ... (см. раздел 3.4)
>
> ------------------------------------------------------------------------

### 12.3. АВТОМАТИЧЕСКИЙ BACKUP

> ------------------------------------------------------------------------
>
> // scripts/backup.bat
>
> \@echo off
>
> REM Создание локального backup репозитория setlocal set BACKUP_DIR=C:\\backup\\retro-engine set TIMESTAMP=%DATE:\~-4%%DATE:\~3,2%%DATE:\~0,2%_%TIME:\~0,2%%TIME:\~3,2% set TIMESTAMP=%TIMESTAMP: =0%
>
> echo ======================================== echo Creating backup [%TIMESTAMP%] echo ======================================== REM Создаём директорию backup if not exist "%BACKUP_DIR%" mkdir "%BACKUP_DIR%" REM Архивируем репозиторий (исключая build/) tar -czf "%BACKUP_DIR%\\retro-engine-%TIMESTAMP%.tar.gz" \^
>
> --exclude="build" \^
>
> --exclude=".vs" \^
>
> --exclude=".cache" \^
>
> -C .. retro-engine
>
> REM Удаляем старые backup (оставляем последние 5) cd "%BACKUP_DIR%"
>
> for /f "skip=5" %%f in ('dir /b /o-d retro-engine-*.tar.gz') do del "%%f" echo.
>
> echo Backup created: %BACKUP_DIR%\\retro-engine-%TIMESTAMP%.tar.gz echo. endlocal
>
> РЕКОМЕНДАЦИЯ: Запускать scripts/backup.bat раз в неделю через Task Scheduler.

# ЭТАП 6: НЕПОСРЕДСТВЕННАЯ РАЗРАБОТКА (IMPLEMENTATION)

> Ретро-движок для консолей 5-го поколения + PC (SDL3)
>
> Дата: 2026
>
> Согласовано с: ETAP1-ETAP5

## 1. ВВЕДЕНИЕ И АДАПТАЦИЯ ПОД СОЛО-РАЗРАБОТЧИКА

> ИСХОДНЫЙ КОРПОРАТИВНЫЙ ПЛАН (из ТЗ):

- 5 итераций по 2 недели = 10 недель

- Команда из 4+ инженеров

- Все 5 консолей + PC

- Полноценное аудио и файловая системаАДАПТИРОВАННЫЙ ПЛАН (для соло):

- - 20 спринтов по 2 недели = 40 development weeks; recovery/buffer weeks are tracked separately and are not hidden inside the 40-week estimate.

- Один разработчик + AI-ассистенты

- MVP: только PC + PS1 (остальные консоли --- после MVP)

- Аудио: только заглушка в MVP (полноценное --- после MVP)

- Файловая система: минимальный read-only ISO9660 backend входит в MVP; полноценный streaming/CD-XA остаётся post-MVP ПОЧЕМУ 10 МЕСЯЦЕВ, А НЕ 10 НЕДЕЛЬ:

- Соло-разработчик работает 10-15 часов в неделю (не 40).

## 2. Нужно время на изучение PSn00bSDK, GTE, GPU PS1.

- Отладка на эмуляторах медленнее, чем на реальном железе.

- Нужен запас на непредвиденные проблемы.

- Выгорание --- реальный риск, нужно работать в устойчивом темпе.

> ПРИНЦИПЫ РАЗРАБОТКИ:

- "Working software over comprehensive documentation" --- код важнее бумаг.

- "Test-driven development" --- сначала тесты, потом код.

- "Continuous integration" --- каждый коммит проходит CI.

- "Incremental delivery" --- каждые 2 спринта --- работающий результат.

- "Fail fast" --- если спринт провален, анализируем и корректируем план.

## 2. ОБЗОР ИТЕРАЦИЙ: MVP VS БУДУЩИЕ ВЕРСИИ

> MVP (v0.1.0) --- 20 СПРИНТОВ (10 МЕСЯЦЕВ):
>
> ------------------------------------------------------------------------

#### Итерация 1: Ядро и PC (SDL3 + OpenGL)

- Спринты 1-6: Математика, память, ECS, HAL для PC

- Спринты 7-12: Рендерер, конвертер ассетов, Hello Triangle на PC

#### Итерация 2: PS1 (через PSn00bSDK)

- Спринты 13-16: HAL для PS1, Hello Triangle на PS1

- Спринты 17-18: Rotating Cube (демо MVP)

- Спринты 19-20: Полировка, документация, релиз v0.1.0 ИТОГО MVP: 20 спринтов = 40 недель 10 месяцев БУДУЩИЕ ВЕРСИИ (после MVP):

> ------------------------------------------------------------------------

#### Итерация 3: Sega Saturn (v0.2.0)

- 8-12 спринтов (4-6 месяцев)

- Master + Slave SH-2, VDP1 + VDP2

#### Итерация 4: N64, Jaguar, 3DO (v0.3.0 - v0.5.0)

- 12-18 спринтов (6-9 месяцев)

- Микрокод RCP, Blitter Jaguar, Cell engine 3DO

#### Итерация 5: Аудио и файловая система (v0.6.0 - v0.8.0)

- 8-12 спринтов (4-6 месяцев)

- XA (PS1), PCM (Saturn), ADPCM (N64)

- ISO9660/CDFS драйверы

> ИТОГО ПОСЛЕ MVP: 28-42 спринта = 56-84 недели 14-21 месяц ОБЩИЙ СРОК ПОЛНОГО РЕЛИЗА (v1.0.0): 24-31 месяц 2-2.5 года

## 3. ДЕТАЛЬНЫЙ ПЛАН СПРИНТОВ MVP (PC + PS1)

> ------------------------------------------------------------------------
>
> СПРИНТ 1-2: СТРУКТУРА РЕПОЗИТОРИЯ + МАТЕМАТИЧЕСКОЕ ЯДРО (4 НЕДЕЛИ)
>
> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Создать структуру репозитория и реализовать fixed-point математику с unit-тестами.
>
> ЗАДАЧИ:
>
> Неделя 1:
>
> [ ] Создать репозиторий на GitHub
>
> [ ] Настроить структуру директорий (см. ETAP4)
>
> [ ] Создать CMakeLists.txt (главный)
>
> [ ] Настроить .gitignore, .editorconfig
>
> [ ] Создать README.md (заглушка)
>
> [ ] Создать LICENSE (MIT)
>
> [ ] Первый коммит + push Неделя 2:
>
> [ ] Реализовать fixed_t (16.16 fixed-point)
>
> [ ] Реализовать fixed_add, fixed_sub, fixed_mul, fixed_div
>
> [ ] Реализовать fixed_sqrt (Newton's method)
>
> [ ] Написать unit-тесты для fixed-point [ ] Проверить точность (погрешность < 0.01) Неделя 3:
>
> [ ] Реализовать Vec2, Vec3, Vec4
>
> [ ] Реализовать операции: add, sub, scale, dot, cross, normalize
>
> [ ] Написать unit-тесты для векторов
>
> [ ] Проверить детерминированность (один вход = один выход) Неделя 4:
>
> [ ] Реализовать Mat3, Mat4
>
> [ ] Реализовать операции: identity, multiply, translate, rotate, scale
>
> [ ] Реализовать perspective, look_at [ ] Написать unit-тесты для матриц
>
> [ ] Настроить CI (GitHub Actions) для автоматического запуска тестов КРИТЕРИИ ПРИЁМКИ:
>
> Репозиторий публичен на GitHub
>
> Структура директорий соответствует ETAP4
>
> Все unit-тесты проходят (math_test.cpp)
>
> CI автоматически запускает тесты при push
>
> Fixed-point accuracy: acceptance is operation-specific; use deterministic test vectors and bounded absolute/relative error thresholds per operation.
>
> Детерминированность подтверждена тестами DEFINITION OF DONE:
>
> Код компилируется на Windows 11 (MSVC + MinGW)
>
> Код компилируется на Linux (GCC) и macOS (Clang) через CI
>
> Все тесты проходят (100% pass rate)
>
> Код отформатирован согласно STYLE_GUIDE.md
>
> Документация (Doxygen) для всех публичных функций
>
> Коммиты атомарные, с понятными сообщениями АРТЕФАКТЫ:

- src/core/math/fixed.h, fixed.cpp

- src/core/math/vec2.h, vec3.h, vec4.h

- src/core/math/mat3.h, mat4.h

- tests/math_test.cpp

- .github/workflows/tests.ymlРИСКИ:

- Риск: Погрешность fixed-point слишком высокая

> Митигация: Использовать 32.32 вместо 16.16 (если нужно)

- Риск: Недетерминированность на разных платформах

> Митигация: Тестировать на Windows, Linux, macOS через CI
>
> ------------------------------------------------------------------------
>
> СПРИНТ 3-4: СИСТЕМА ПАМЯТИ + ECS (4 НЕДЕЛИ)
>
> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Реализовать три аллокатора (Linear, Pool, Stack) и ECS (Registry).
>
> ЗАДАЧИ:
>
> Неделя 5:
>
> [ ] Реализовать LinearAllocator
>
> [ ] Реализовать выравнивание (alignment)
>
> Allocator benchmark: фиксировать baseline allocation latency на reference machine; <10 нс не является portable acceptance criterion.
>
> [ ] Реализовать PoolAllocator (шаблонный)
>
> [ ] Реализовать итерацию по используемым объектам
>
> [ ] Написать unit-тесты для PoolAllocator [ ] Проверить отсутствие фрагментации Неделя 7:
>
> [ ] Реализовать StackAllocator
>
> [ ] Реализовать reset_frame() для очистки кадра
>
> [ ] Написать unit-тесты для StackAllocator [ ] Интеграционный тест: все три аллокатора вместе Неделя 8:
>
> [ ] Реализовать Registry (ECS)
>
> [ ] Реализовать create_entity, destroy_entity
>
> [ ] Реализовать add_component, remove_component, get_component
>
> [ ] Реализовать view<T1, T2, ...> для итерации
>
> [ ] Написать unit-тесты для ECS
>
> [ ] Проверить cache-friendly итерацию (benchmark) КРИТЕРИИ ПРИЁМКИ:
>
> Все три аллокатора работают корректно
>
> ECS поддерживает до 10,000 сущностей
>
> Итерация по компонентам cache-friendly (benchmark)
>
> Все unit-тесты проходят (memory_test.cpp, ecs_test.cpp)
>
> Нет утечек памяти (проверено через AddressSanitizer) DEFINITION OF DONE:
>
> Core engine code compiles for PC and PS1; host unit tests execute on the PC build. PS1 acceptance uses PS1 emulator smoke/conformance tests.
>
> Все тесты проходят
>
> Benchmark: измерять и фиксировать baseline на reference machine; не использовать абсолютный <1 мкс target до появления benchmark methodology.
>
> AddressSanitizer не находит утечек
>
> Документация для всех публичных API АРТЕФАКТЫ:

- src/core/memory/linear_allocator.h

- src/core/memory/pool_allocator.h

- src/core/memory/stack_allocator.h

- src/core/ecs/registry.h, registry.cpp

- tests/memory_test.cpp, ecs_test.cppРИСКИ:

- Риск: ECS слишком медленный

> Митигация: Оптимизировать view<T> через sparse set

- Риск: Утечки памяти в PoolAllocator

> Митигация: AddressSanitizer + ручная проверка
>
> ------------------------------------------------------------------------
>
> СПРИНТ 5-6: HAL ДЛЯ PC (SDL3 + OPENGL) (4 НЕДЕЛИ)
>
> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Реализовать HAL для PC: окно, ввод, аудио (заглушка), файловая система.
>
> ЗАДАЧИ:
>
> Неделя 9:
>
> [ ] Установить SDL3 (через vcpkg или vendor)
>
> [ ] Установить glad (генератор OpenGL)
>
> [ ] Реализовать Platform_Init() (создание окна SDL3)
>
> [ ] Реализовать Platform_Shutdown()
>
> [ ] Реализовать Platform_PumpEvents() (обработка событий) Неделя 10:
>
> [ ] Реализовать Platform_SwapBuffers()
>
> [ ] Реализовать Platform_GetTimeMs() (для delta time)
>
> [ ] Реализовать Platform_SleepMs()
>
> [ ] Тест: окно открывается и закрывается без ошибок Неделя 11:
>
> [ ] Реализовать Input_IsKeyDown() (клавиатура)
>
> [ ] Реализовать Input_IsPadButtonDown() (геймпад)
>
> [ ] Реализовать Input_GetPadAxis() (аналоговые стики) [ ] Тест: ввод работает с клавиатуры и геймпада Неделя 12:
>
> [ ] Реализовать Audio_Init() (заглушка --- тишина)
>
> [ ] Реализовать Audio_PlayBeep() (простой бипер через SDL3 audio subsystem)
>
> [ ] Реализовать FileSystem_LoadFile() (чтение из файловой системы)
>
> [ ] Реализовать FileSystem_FreeFile()
>
> [ ] Тест: загрузка файла работает КРИТЕРИИ ПРИЁМКИ:
>
> Окно SDL3 открывается (1280x960)
>
> Ввод работает с клавиатуры (WASD)
>
> Ввод работает с геймпада (DualShock клон)
>
> Файловая система загружает файлы
>
> Аудио --- заглушка (тишина или бипер)
>
> Delta time работает (60 FPS) DEFINITION OF DONE:
>
> Код компилируется на Windows 11
>
> Окно открывается без ошибок
>
> Ввод работает (тест вручную)
>
> Файловая система загружает файлы (тест вручную)
>
> Нет крашей при закрытии окна АРТЕФАКТЫ:

- src/hal/pc/platform_pc.cpp

- src/hal/pc/input_sdl.cpp

- src/hal/pc/audio_sdl.cpp

- src/hal/pc/filesystem_pc.cppРИСКИ:

- Риск: изменения SDL3 API. Митигация: pin SDL3 version in CMake/vcpkg manifest and test the documented API surface in CI.

> Митигация: Использовать стабильную версию (3.2.x)

- Риск: Геймпад не определяется

> Митигация: Использовать SDL3 SDL_Gamepad API (например, SDL_GetGamepads / SDL_OpenGamepad / SDL_GetGamepadButton / SDL_GetGamepadAxis). Не использовать устаревший SDL_GameController API.
>
> ------------------------------------------------------------------------
>
> СПРИНТ 7-8: РЕНДЕРЕР С ЭМУЛЯЦИЕЙ PS1 (4 НЕДЕЛИ)
>
> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Реализовать рендерер на OpenGL 3.3 с эмуляцией PS1-ограничений.
>
> ЗАДАЧИ:
>
> Неделя 13:
>
> [ ] Реализовать Render_Init() (создание OpenGL контекста 3.3)
>
> [ ] Реализовать Render_Shutdown()
>
> [ ] Реализовать Render_BeginFrame(), Render_EndFrame()
>
> [ ] Реализовать Render_Clear() [ ] Тест: окно очищается цветом Неделя 14:
>
> [ ] Создать фреймбуфер 320x240 (внутреннее разрешение PS1)
>
> [ ] Реализовать вывод фреймбуфера на экран (nearest filtering)
>
> [ ] Написать вершинный шейдер (ps1_emu.vert)
>
> [ ] Написать фрагментный шейдер (ps1_emu.frag) [ ] Тест: рисуется треугольник (без текстур) Неделя 15:
>
> [ ] Реализовать эмуляцию квантования вершин (wobbling)
>
> [ ] Реализовать аффинное текстурирование (без perspective correction)
>
> [ ] Реализовать загрузку 8-bit текстур с палитрой (RGB555+STP) [ ] Тест: текстурированный треугольник с "wobbling" Неделя 16:
>
> [ ] Реализовать Render_DrawMesh() (отрисовка меша)
>
> [ ] Реализовать Render_SetCamera() (view + projection матрицы)
>
> [ ] Отключить Z-буфер (glDisable(GL_DEPTH_TEST))
>
> [ ] Реализовать Painter's algorithm (сортировка полигонов) [ ] Тест: несколько треугольников с правильной сортировкой КРИТЕРИИ ПРИЁМКИ:
>
> Рендерер работает в разрешении 320x240 (масштабируется до 1280x960)
>
> "Wobbling" вершин виден (как на PS1)
>
> Аффинное текстурирование работает (текстуры "плывут")
>
> 8-bit текстуры с палитрой работают
>
> Z-буфер отключен, Painter's algorithm работает
>
> W1: не менее 60 FPS на зафиксированной reference PC environment DEFINITION OF DONE:
>
> Код компилируется на Windows 11
>
> Рендерер работает без ошибок
>
> Визуальная проверка: результат сравнен с canonical PS1 reference set.
>
> W1: не менее 60 FPS на зафиксированной reference PC environment (нет просадок)
>
> RenderDoc показывает корректные draw calls АРТЕФАКТЫ:

- src/hal/pc/render_opengl.cpp

- src/render/shaders/ps1_emu.vert

- src/render/shaders/ps1_emu.frag

- src/render/renderer.cpp

- src/render/mesh.cpp, texture.cppРИСКИ:

- Риск: OpenGL 3.3 не поддерживается на старых видеокартах

> Митигация: Требовать OpenGL 3.3+ (большинство карт с 2010 года)

- Риск: Painter's algorithm слишком медленный

> Митигация: Оптимизировать через BSP-деревья (после MVP)
>
> ------------------------------------------------------------------------
>
> СПРИНТ 9-10: КОНВЕРТЕР АССЕТОВ (4 НЕДЕЛИ)
>
> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Реализовать C++14 утилиту для конвертации glTF -> .mesh и PNG -> .tex.
>
> ЗАДАЧИ:
>
> Неделя 17:
>
> [ ] Создать tools/asset_converter/ директорию [ ] Настроить CMakeLists.txt для утилиты
>
> [ ] Добавить cgltf (single-header) в third_party/
>
> [ ] Добавить stb_image (single-header) в third_party/ [ ] Тест: утилита компилируется Неделя 18:
>
> [ ] Реализовать mesh_converter.cpp (glTF -> .mesh)
>
> [ ] Реализовать конвертацию вершин (glTF float -> engine fixed_t 16.16)
>
> [ ] Реализовать конвертацию нормалей (float -> int8_t)
>
> [ ] Реализовать конвертацию UV (float -> uint8_t) [ ] Тест: конвертация простого куба из Blender Неделя 19:
>
> [ ] Реализовать texture_converter.cpp (PNG -> .tex)
>
> [ ] Реализовать color_quantizer.cpp (Median Cut алгоритм)
>
> [ ] Реализовать конвертацию в 8-bit indexed
>
> [ ] Реализовать конвертацию палитры в RGB555+STP [ ] Тест: конвертация PNG 256x256 Неделя 20:
>
> [ ] Интегрировать конвертер с CMake (add_custom_command)
>
> [ ] Проверить автоматическую конвертацию при PC-сборке; PS1-сборка потребляет уже сгенерированные assets из \`assets/build/\`.
>
> [ ] Реализовать обработку ошибок (невалидный glTF, битый PNG)
>
> [ ] Написать документацию (ASSETS.md) [ ] Тест: полная цепочка glTF -> .mesh -> рендеринг КРИТЕРИИ ПРИЁМКИ:
>
> Конвертер компилируется на Windows 11 glTF -> .mesh работает (куб из Blender) PNG -> .tex работает (256x256 текстура)
>
> Квантизация до 256 цветов работает (Median Cut)
>
> Палитра в RGB555+STP корректна
>
> Интеграция с CMake работает (автоматическая конвертация) DEFINITION OF DONE:
>
> Утилита компилируется без ошибок
>
> Конвертация работает для тестовых ассетов
>
> Документация (ASSETS.md) написана
>
> Примеры использования в README АРТЕФАКТЫ:

- tools/asset_converter/main.cpp

- tools/asset_converter/mesh_converter.cpp

- tools/asset_converter/texture_converter.cpp

- tools/asset_converter/color_quantizer.cpp

- src/third_party/cgltf/cgltf.h

- src/third_party/stb/stb_image.h

- docs/ASSETS.mdРИСКИ:

- Риск: cgltf не поддерживает некоторые glTF фичи

> Митигация: Ограничить поддержку (только базовые меши)

- Риск: Median Cut слишком медленный для больших текстур

> Митигация: Использовать Octree (быстрее)
>
> ------------------------------------------------------------------------
>
> СПРИНТ 11-12: HELLO TRIANGLE НА PC (4 НЕДЕЛИ)
>
> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Создать первый пример "Hello Triangle" на PC.
>
> ЗАДАЧИ:
>
> Неделя 21:
>
> [ ] Создать examples/hello_triangle/ директорию
>
> [ ] Настроить CMakeLists.txt для примера
>
> [ ] Создать triangle.gltf в Blender (простой треугольник)
>
> [ ] Создать texture.png (256x256, простой паттерн) [ ] Тест: ассеты конвертируются через asset_converter Неделя 22:
>
> [ ] Реализовать main.cpp для hello_triangle
>
> [ ] Загрузить triangle.mesh через FileSystem_LoadFile [ ] Загрузить texture.tex через FileSystem_LoadFile
>
> [ ] Реализовать RenderSystem (отрисовка сущностей с MeshRenderer)
>
> [ ] Настроить камеру (perspective + look_at)
>
> [ ] Отрисовать треугольник с текстурой
>
> [ ] Тест: треугольник виден на экране
>
> Неделя 24:
>
> [ ] Добавить управление камерой (WASD + мышь)
>
> [ ] Добавить FPS counter
>
> Hello Triangle работает на PC
>
> Треугольник загружается из .mesh файла
>
> Текстура загружается из .tex файла
>
> Треугольник отображается корректно
>
> Камера управляется с клавиатуры (WASD)
>
> W1: не менее 60 FPS на зафиксированной reference PC environment DEFINITION OF DONE:
>
> Пример компилируется и запускается
>
> Визуальная проверка: screenshot сравнен с эталонным reference set для соответствующего workload.
>
> README.md для примера написан
>
> W1: не менее 60 FPS на зафиксированной reference PC environment АРТЕФАКТЫ:

- examples/hello_triangle/main.cpp

- examples/hello_triangle/CMakeLists.txt

- examples/hello_triangle/README.md

- assets/source/triangle.gltf

- assets/source/texture.png

- assets/build/triangle.mesh (автогенерируемый)

- assets/build/texture.tex (автогенерируемый) РИСКИ:

- Риск: Камера работает некорректно

> Митигация: Использовать стандартную FPS-камеру (look_at + yaw/pitch)

- Риск: Текстура не видна

> Митигация: Проверить UV-координаты в Blender
>
> ------------------------------------------------------------------------
>
> СПРИНТ 13-14: HAL ДЛЯ PS1 (PSNOOBSDK) (4 НЕДЕЛИ)
>
> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Реализовать HAL для PS1 через PSn00bSDK.
>
> ЗАДАЧИ:
>
> Неделя 25:
>
> [ ] Собрать PSn00bSDK библиотеки
>
> [ ] Настроить CMakePresets.json через PSn00bSDK/lib/libpsn00b/cmake/sdk.cmake [ ] Тест: простой "Hello World" компилируется для PS1 Неделя 26:
>
> [ ] Реализовать Platform_Init() для PS1 (GPU, PAD, DMA)
>
> [ ] Реализовать Platform_Shutdown()
>
> [ ] Реализовать Platform_PumpEvents() (обработка pad)
>
> [ ] Реализовать Platform_SwapBuffers() (VSync + swap) [ ] Тест: PS1-бинарник запускается в DuckStation Неделя 27:
>
> [ ] Реализовать Input_IsPadButtonDown() (DualShock)
>
> [ ] Реализовать Input_GetPadAxis() (аналоговые стики)
>
> [ ] Реализовать Render_Init() для PS1 (GPU + Ordering Table)
>
> [ ] Реализовать Render_DrawMesh() через GTE + GPU
>
> [ ] Реализовать загрузку текстур в VRAM [ ] Тест: треугольник рисуется на PS1 (в эмуляторе) КРИТЕРИИ ПРИЁМКИ:
>
> Бинарник запускается в DuckStation
>
> Ввод с геймпада работает
>
> Треугольник рисуется на экране
>
> Текстуры загружаются в VRAM DEFINITION OF DONE:
>
> Бинарник работает в DuckStation
>
> Нет крашей в эмуляторе
>
> GTE используется для трансформаций
>
> Ordering Table используется для сортировки АРТЕФАКТЫ:

- src/hal/ps1/platform_ps1.cpp

- src/hal/ps1/render_gpu.cpp

- src/hal/ps1/input_pad.cpp

- src/hal/ps1/filesystem_cdrom.cpp

- CMakePresets.json + PSn00bSDK sdk.cmake РИСКИ:

- Риск: PSn00bSDK слишком сложный

> Митигация: Читать документацию + примеры из репозитория

- Риск: GTE работает некорректно

> Митигация: Сравнить результаты с PC-версией (fixed-point)

- Риск: Эмулятор врёт в таймингах

> Митигация: Использовать DuckStation (самый точный)
>
> ------------------------------------------------------------------------
>
> СПРИНТ 15-16: HELLO TRIANGLE НА PS1 (4 НЕДЕЛИ)
>
> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Запустить "Hello Triangle" на PS1 (в эмуляторе).
>
> ЗАДАЧИ:
>
> Неделя 29:
>
> [ ] Адаптировать hello_triangle для PS1
>
> [ ] Загрузить texture.tex с CD-ROM [ ] Тест: ассеты загружаются на PS1 Неделя 30:
>
> [ ] Реализовать RenderSystem для PS1 (через GTE + GPU)
>
> [ ] Настроить камеру (fixed-point math)
>
> [ ] Отрисовать треугольник с текстурой [ ] Тест: треугольник виден на экране PS1 Неделя 31:
>
> [ ] Добавить управление камерой с геймпада [ ] Тест: треугольник отображается, ввод с геймпада работает Неделя 32:
>
> [ ] Оптимизировать производительность (цель: 30 FPS)
>
> [ ] Проверить использование RAM (< 1.5 МБ)
>
> [ ] Проверить использование VRAM (< 800 КБ)
>
> [ ] Тест: W1: не менее 30 FPS на зафиксированной PS1 validation environment в DuckStation
>
> КРИТЕРИИ ПРИЁМКИ:
>
> Треугольник загружается из .mesh файла
>
> Текстура загружается из .tex файла
>
> Треугольник отображается корректно
>
> Камера управляется с геймпада
>
> W1: не менее 30 FPS на зафиксированной PS1 validation environment.
>
> Peak main RAM <= 1.5 MiB and peak VRAM usage < 800 KiB for the reference workload. DEFINITION OF DONE:
>
> Бинарник работает в DuckStation
>
> W1: не менее 30 FPS на зафиксированной PS1 validation environment.
>
> Нет крашей
>
> Использование RAM/VRAM в пределах лимитов
>
> Визуальная проверка: PC и PS1 результаты сравниваются по фиксированному reference set; различия, обусловленные PS1 rasterization, перечислены явно.

- examples/hello_triangle/ (адаптировано для PS1)

- Скриншоты PC vs PS1 (для сравнения) РИСКИ:

- Риск: 30 FPS не достигается

> Митигация: Уменьшить количество полигонов, оптимизировать GTE

- Риск: RAM переполняется

> Митигация: Использовать Memory Profiler, оптимизировать аллокаторы
>
> ------------------------------------------------------------------------
>
> СПРИНТ 17-18: ROTATING CUBE (ДЕМО MVP) (4 НЕДЕЛИ)
>
> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Создать финальную демку MVP --- "Rotating Cube" с управлением камерой.
>
> ЗАДАЧИ:
>
> Неделя 33:
>
> [ ] Создать examples/rotating_cube/ директорию
>
> [ ] Создать более сложную сцену (несколько кубов, разные текстуры)
>
> [ ] Реализовать загрузку нескольких мешей и текстур [ ] Тест: сцена загружается на PC и PS1 Неделя 34:
>
> [ ] Реализовать CollisionSystem (AABB коллизии)
>
> [ ] Добавить простую физику (гравитация, прыжки) [ ] Тест: кубы не проходят сквозь друг друга Неделя 35:
>
> [ ] Добавить vertex lighting (ambient + directional)
>
> [ ] Реализовать Gouraud shading (интерполяция цвета) [ ] Тест: освещение работает на PC и PS1 Неделя 36:
>
> [ ] Полировка: убрать баги, оптимизировать производительность
>
> [ ] Добавить debug overlay (FPS, RAM usage, triangle count)
>
> [ ] Тест: демка работает стабильно 10 минут без крашей КРИТЕРИИ ПРИЁМКИ:
>
> Rotating Cube проходит W1: не менее 60 FPS на зафиксированной reference PC environment и не менее 30 FPS на зафиксированной PS1 validation environment
>
> Несколько кубов с разными текстурами
>
> AABB коллизии работают
>
> Vertex lighting работает
>
> Debug overlay показывает FPS, RAM, triangles
>
> Стабильно работает 10 минут без крашей DEFINITION OF DONE:
>
> Демка компилируется и запускается на PC и PS1
>
> W1: не менее 60 FPS на зафиксированной reference PC environment; не менее 30 FPS на зафиксированной PS1 validation environment
>
> Нет крашей (тест 10 минут)
>
> Debug overlay работает
>
> README.md для демки написан
>
> Скриншоты и видео для демонстрации АРТЕФАКТЫ:

- examples/rotating_cube/main.cpp

- examples/rotating_cube/CMakeLists.txt

- examples/rotating_cube/README.md

- assets/source/ (несколько кубов, текстур)

- assets/build/ (сконвертированные ассеты)

- screenshots/ (скриншоты PC и PS1)

- video/ (видео демонстрации) РИСКИ:

- Риск: 30 FPS на PS1 не достигается

> Митигация: Уменьшить количество кубов, упростить текстуры

- Риск: Коллизии работают некорректно

> Митигация: Упростить AABB, добавить debug visualization
>
> ------------------------------------------------------------------------
>
> СПРИНТ 19-20: ПОЛИРОВКА, ДОКУМЕНТАЦИЯ, РЕЛИЗ v0.1.0 (4 НЕДЕЛИ)
>
> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Подготовить проект к публичному релизу v0.1.0.
>
> ЗАДАЧИ:
>
> Неделя 37:
>
> [ ] Обновить README.md (описание, скриншоты, инструкции)
>
> [ ] Написать ARCHITECTURE.md (описание архитектуры)
>
> [ ] Написать BUILDING.md (инструкция по сборке) [ ] Написать API.md (описание API) Неделя 38:
>
> [ ] Написать CONTRIBUTING.md (как внести вклад)
>
> [ ] Написать CODE_OF_CONDUCT.md
>
> [ ] Написать LEGAL.md (лицензии библиотек)
>
> [ ] Обновить CHANGELOG.md (все изменения с начала проекта) Неделя 39:
>
> [ ] Прогнать все unit-тесты (100% pass rate)
>
> [ ] Протестировать примеры на PC и PS1 (вручную)
>
> [ ] Проверить документацию (все ссылки работают) [ ] Исправить последние баги Неделя 40:
>
> [ ] Создать тег v0.1.0
>
> [ ] Создать GitHub Release (автоматически через CI)
>
> [ ] Написать announcement post (Reddit, Twitter, Discord)
>
> [ ] Ответить на первые issues от пользователей КРИТЕРИИ ПРИЁМКИ:
>
> Все unit-тесты проходят
>
> Примеры работают на PC и PS1
>
> Документация полная и актуальная
>
> GitHub Release создан
>
> Announcement post опубликован
>
> DEFINITION OF DONE:
>
> Репозиторий публичен
>
> Тег v0.1.0 создан
>
> GitHub Release с бинарниками
>
> Документация полная
>
> CI работает (все тесты проходят)
>
> Announcement post опубликован АРТЕФАКТЫ:

- README.md (обновлённый)

- docs/ARCHITECTURE.md

- docs/BUILDING.md

- docs/API.md

- docs/CONTRIBUTING.md

- docs/CODE_OF_CONDUCT.md

- docs/LEGAL.md

- CHANGELOG.md (v0.1.0)

- GitHub Release v0.1.0 РИСКИ:

- Риск: Документация неполная

> Митигация: Использовать чек-лист, проверять каждую страницу

- Риск: Критический баг перед релизом

> Митигация: Заложить 1 неделю на баг-фикс (спринт 39)

## 4. БУДУЩИЕ ИТЕРАЦИИ (ПОСЛЕ MVP)

> ------------------------------------------------------------------------
>
> ИТЕРАЦИЯ 3: SEGA SATURN (v0.2.0) --- 8-12 СПРИНТОВ (4-6 МЕСЯЦЕВ)
>
> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Добавить поддержку Sega Saturn.
>
> ОСОБЕННОСТИ SATURN:

- Два CPU: Master SH-2 + Slave SH-2

- VDP1 (полигоны) + VDP2 (фоновые слои)

- Квады вместо треугольников

- Нет Z-буфераСПРИНТЫ:

#### Спринт 21-22: Изучение Saturn, настройка toolchain (sh-elf-gcc)

#### Спринт 23-24: HAL для Saturn (Master SH-2)

#### Спринт 25-26: Параллелизм (Master + Slave SH-2)

#### Спринт 27-28: VDP1 (рисование полигонов)

#### Спринт 29-30: VDP2 (фоновые слои)

#### Спринт 31-32: Hello Triangle на Saturn

> КРИТЕРИИ ПРИЁМКИ:
>
> Saturn-бинарник компилируется
>
> Hello Triangle работает на Saturn (эмулятор)
>
> Master + Slave SH-2 используются
>
> VDP1 + VDP2 синхронизированы
>
> ------------------------------------------------------------------------
>
> ИТЕРАЦИЯ 4: N64, JAGUAR, 3DO (v0.3.0 - v0.5.0) --- 12-18 СПРИНТОВ (6-9 МЕСЯЦЕВ)
>
> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Добавить поддержку N64, Jaguar, 3DO.
>
> N64 (v0.3.0):

- Микрокод для RCP (RSP + RDP)

- Z-буфер (аппаратный)

- Perspective correct текстурирование

- 6-8 спринтовJaguar (v0.4.0):

- Прямое программирование Blitter (Tom)

- Five processors are present across the Jaguar architecture; the 68000 is the host/boot processor while Tom and Jerry contain additional programmable processors.

- 4-6 спринтов3DO (v0.5.0):

- Cell-based рендеринг

- ARM60 CPU

- 4-6 спринтов

> ------------------------------------------------------------------------
>
> ИТЕРАЦИЯ 5: АУДИО И ФАЙЛОВАЯ СИСТЕМА (v0.6.0 - v0.8.0) --- 8-12 СПРИНТОВ (4-6 МЕСЯЦЕВ)
>
> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Реализовать полноценное аудио и файловую систему.
>
> АУДИО (v0.6.0 - v0.7.0):

- XA аудио на PS1 (потоковое с CD-ROM)

- PCM на Saturn (через SCSP)

- ADPCM на N64 (через AI)

- 4-6 спринтов

> ФАЙЛОВАЯ СИСТЕМА (v0.8.0):

- ISO9660/CDFS драйверы

- Минимальная read-only ISO9660-backed загрузка файлов входит в MVP; background streaming/prefetching не является release gate v0.1.0.

- Поддержка save games (Memcard, EEPROM)

- 4-6 спринтов

## 5. DEFINITION OF DONE ДЛЯ КАЖДОГО СПРИНТА

> ОБЩИЕ КРИТЕРИИ ДЛЯ ВСЕХ СПРИНТОВ:
>
> Код компилируется без ошибок и предупреждений
>
> Все unit-тесты проходят (100% pass rate)
>
> CI проходит (GitHub Actions)
>
> Код отформатирован согласно STYLE_GUIDE.md
>
> Документация (Doxygen) для всех публичных функций
>
> Коммиты атомарные, с понятными сообщениями
>
> Код отревьюлен (если есть контрибьюторы)
>
> Нет известных критических багов
>
> СПЕЦИФИЧНЫЕ КРИТЕРИИ ПО СПРИНТАМ: См. раздел 3 (детальный план спринтов)

## 6. РИСКИ И МИТИГАЦИЯ

#### РИСК 1: Выгорание соло-разработчика

> Вероятность: ВЫСОКАЯ Влияние: КРИТИЧЕСКОЕ Митигация:

- Работать 10-15 часов в неделю (не больше)

- - Планировать отдельные recovery/buffer weeks при необходимости; они не считаются частью 40 development weeks.

- Отмечать достижения (каждый спринт --- демо)

- Иметь поддержку (Discord, Reddit, менторы)

#### РИСК 2: Технические проблемы с PSn00bSDK

> Вероятность: СРЕДНЯЯ Влияние: ВЫСОКОЕ Митигация:

- Читать документацию PSn00bSDK

- Задавать вопросы в Discord homebrew-сообщества

- Иметь fallback (использовать PCSX-Redux HLE)

#### РИСК 3: Эмуляторы врут в таймингах

> Вероятность: СРЕДНЯЯ Влияние: СРЕДНЕЕ Митигация:

- Использовать DuckStation (самый точный эмулятор)

- Купить реальную PS1 за $20-50 для финальной проверки

- Тестировать на нескольких ревизиях консоли (SCPH-1000, SCPH-9000)

- Сравнивать поведение эмулятора с реальным железом

#### РИСК 4: Недостаточно времени для MVP

> Вероятность: ВЫСОКАЯ Влияние: ВЫСОКОЕ Митигация:

- Чётко следовать плану спринтов (не распыляться)

- Отсекать фичи, если спринт затягивается (scope reduction)

- Использовать AI-ассистенты (Cursor) для ускорения рутины

- Иметь backup план: если не успеваем за 10 месяцев, релизим v0.0.9

#### РИСК 5: Критические баги перед релизом

> Вероятность: СРЕДНЯЯ Влияние: ВЫСОКОЕ Митигация:

- Заложить спринт 39 на баг-фикс (1 неделя запаса)

- Использовать AddressSanitizer для поиска утечек

- Тестировать 10 минут перед релизом (stress test)

- Иметь rollback план (откатить к предыдущему стабильному коммиту)

#### РИСК 6: Нет контрибьюторов после релиза

> Вероятность: СРЕДНЯЯ Влияние: СРЕДНЕЕ Митигация:

- Активно продвигать проект (Reddit, Twitter, Discord) - Писать туториалы и демо (привлекать внимание)

- Отвечать на issues быстро (показывать активность)

- Иметь "good first issue" для новичков

#### РИСК 7: SDL3 или PSn00bSDK ломают API

> Вероятность: НИЗКАЯ Влияние: СРЕДНЕЕ Митигация:

- Фиксировать версии зависимостей (vendor-стратегия)

- Использовать git submodule для контроля версий

- Иметь fallback на старые версии библиотек

#### РИСК 8: Производительность PS1 ниже ожидаемой

> Вероятность: СРЕДНЯЯ Влияние: СРЕДНЕЕ Митигация:

- Использовать GTE для всех 3D-трансформаций

- Оптимизировать Painter's algorithm (BSP-деревья)

- Уменьшить количество полигонов в демо- Иметь fallback: упростить графику (меньше текстур)

## 7. КРИТЕРИИ ЗАВЕРШЕНИЯ ЭТАПА

REPRODUCIBILITY GATE: PS1 CI and contributor setup use the same pinned PSn00bSDK release (v0.24) and the same documented target (mipsel-none-elf).

MVP ACCEPTANCE GATE (NORMATIVE)

A release candidate is accepted only when the reference workload, memory budgets, deterministic math vectors, PC/PS1 example scene, asset round-trip, and reproducible PC/PS1 builds all pass. Math conformance is checked against fixed test vectors, not by executing the same operation twice on one host. Community metrics do not affect technical acceptance.

> ЭТАП 6 считается завершённым, когда выполнены ВСЕ следующие условия:
>
> ТЕХНИЧЕСКИЕ КРИТЕРИИ:
>
> Все 20 спринтов завершены (40 недель работы)
>
> Код компилируется на PC (Windows 11, Linux, macOS)
>
> Все unit-тесты проходят (100% pass rate)
>
> CI работает (GitHub Actions) Примеры работают:

- hello_triangle: W1-equivalent PC performance >= 60 FPS on the recorded reference environment

- hello_triangle: W1-equivalent PS1 performance >= 30 FPS on the recorded validation environment

- rotating_cube: W1 >= 60 FPS on the recorded reference PC environment

- rotating_cube: W1 >= 30 FPS on the recorded PS1 validation environment

> Использование RAM на PS1 < 1.5 МБ
>
> Peak VRAM usage for the reference workload <= 800 KiB, measured as the sum of framebuffer allocation, CLUT/palette storage, texture pages and GPU-side drawing buffers actually resident in VRAM for W0/W1; the measurement method and layout MUST be documented by the PS1 backend.
>
> Размер бинарника PS1 < 500 КБ ФУНКЦИОНАЛЬНЫЕ КРИТЕРИИ:
>
> ECS работает (создание сущностей, компоненты, системы)
>
> Fixed-point математика работает (16.16)
>
> Система памяти работает (Linear, Pool, Stack аллокаторы)
>
> HAL работает (PC + PS1)
>
> Рендерер работает (affine текстурирование, Painter's algorithm)
>
> Конвертер ассетов работает (glTF -> .mesh, PNG -> .tex)
>
> Ввод работает (клавиатура + геймпад)
>
> Аудио --- заглушка (тишина или бипер)
>
> Минимальная файловая подсистема загружает .mesh/.tex из ISO9660 CD-ROM ДОКУМЕНТАЦИОННЫЕ КРИТЕРИИ:
>
> README.md полный (описание, скриншоты, инструкции) docs/ARCHITECTURE.md написан docs/BUILDING.md написан docs/API.md написан docs/ASSETS.md написан docs/CONTRIBUTING.md написан docs/CODE_OF_CONDUCT.md написан docs/LEGAL.md написан CHANGELOG.md обновлён (v0.1.0)
>
> Примеры имеют README.md РЕЛИЗНЫЕ КРИТЕРИИ:
>
> Тег v0.1.0 создан
>
> GitHub Release создан (с бинарниками)
>
> Announcement post опубликован (Reddit, Twitter, Discord)
>
> Первые issues от пользователей обработаны
>
> Нет критических багов (blocker/major) КАЧЕСТВЕННЫЕ КРИТЕРИИ:
>
> Код чистый (нет TODO/FIXME в релизной версии)
>
> Код отформатирован (clang-format)
>
> Код документирован (Doxygen)
>
> Нет утечек памяти (AddressSanitizer)
>
> Нет крашей (stress test 10 минут) Визуально корректно (скриншоты PC vs PS1) МЕТРИКИ УСПЕХА (после релиза, через 1 месяц):
>
> 100+ звезд на GitHub
>
> 10+ форков
>
> 5+ issues от пользователей
>
> 3+ pull requests от контрибьюторов
>
> 1+ игра, сделанная на движке (или обещание сделать)

## ИТОГИ ЭТАПА 6

> ЧТО МЫ ПОЛУЧИЛИ ПОСЛЕ ЭТАПА 6:

## 1. РАБОЧИЙ ДВИЖОК v0.1.0

- Ядро: ECS, fixed-point math, memory management

- HAL: PC (SDL3 + OpenGL) + PS1 (PSn00bSDK)

- Рендерер: affine текстурирование, Painter's algorithm

- Конвертер ассетов: glTF -> .mesh, PNG -> .tex

- Примеры: hello_triangle, rotating_cube

## 2. ПУБЛИЧНЫЙ РЕПОЗИТОРИЙ

- GitHub: github.com/yourname/retro-engine

- Лицензия: MIT

- Документация: полная

- CI/CD: GitHub Actions

## 3. СООБЩЕСТВО

- Первые пользователи

- Первые контрибьюторы

- Первые игры (или обещания)

- Обратная связь

## 4. ОПЫТ

- Понимание архитектуры ретро-движка

- Опыт работы с PS1 (GTE, GPU, Ordering Table)

- Опыт кроссплатформенной разработки

- Опыт open-source проекта

> ЧТО МЫ НЕ ПОЛУЧИЛИ (и это нормально):

- Поддержка Saturn/N64/Jaguar/3DO (будет в v0.2.0+)

- Полноценное аудио (будет в v0.6.0+)

- расширенный filesystem/streaming API (будет в v0.8.0+)

- Редактор уровней (будет в v1.0.0+)

- Скриптовый язык (будет в v2.0.0+)

# ЭТАП 7: ТЕСТИРОВАНИЕ И QA (TESTING & QUALITY ASSURANCE)

> Ретро-движок для консолей 5-го поколения + PC (SDL3)
>
> Дата: 2026
>
> Согласовано с: ETAP1-ETAP6

## 1. ВВЕДЕНИЕ И АДАПТАЦИЯ ПОД СОЛО-РАЗРАБОТЧИКА

> ИСХОДНЫЙ КОРПОРАТИВНЫЙ ПЛАН (из ТЗ):

- Отдельная QA-команда (3-5 инженеров)

- Реальное железо (девкиты, EPROM-картриджи, CD-R)

- Тестирование на разных ревизиях материнских плат

- Тестирование на ЭЛТ (CRT) через SCART/RF

- 48-часовой stress test

> АДАПТИРОВАННЫЙ ПЛАН (для соло):

- Один разработчик = один QA-инженер

- Только эмуляторы (нет реального железа для MVP)

- Автоматизация через CI/CD (GitHub Actions)

- Unit-тесты + Integration-тесты + Smoke-тесты в эмуляторе

- Stress test --- опционально (после MVP) ПОЧЕМУ ТАК:

- Реальное железо стоит $500-2000 (девкиты, программаторы, CRT).

## DuckStation is the primary PS1 emulator for MVP smoke testing; no universal percentage accuracy claim is made.

- Соло-разработчик не может тестировать 48 часов вручную.

## CI/CD reduces repeated manual verification; no fixed productivity percentage is assumed.

> ПРИНЦИПЫ ТЕСТИРОВАНИЯ:

- "Test early, test often" --- тесты на каждом коммите.

- "Automate everything" --- ручное тестирование только для UX.

- "Fail fast" --- ошибки должны ловиться на этапе компиляции.

- "Coverage matters" --- минимум 70% покрытия для core/.

- "Regression is evil" --- каждый баг = новый тест.

## 2. ПИРАМИДА ТЕСТИРОВАНИЯ ДЛЯ РЕТРО-ДВИЖКА

> Пирамида тестирования (снизу вверх):
>
> / E2E Tests \\ <- 5% (Smoke tests в эмуляторе)
>
> / Integration \\ <- 25% (Рендерер, ассеты, HAL) / Unit Tests \\ <- 70% (Математика, память, ECS)
>
> ОБЪЯСНЕНИЕ:

- Unit-тесты (70%): быстрые, изолированные, покрывают ядро.

- Integration-тесты (25%): проверяют взаимодействие компонентов.

- E2E-тесты (5%): smoke tests в эмуляторе (работает ли бинарник).

> ПОЧЕМУ НЕ БОЛЬШE E2E:

- E2E-тесты медленные (запуск эмулятора = 5-10 секунд).

- E2E-тесты хрупкие (зависят от таймингов эмулятора).

- E2E-тесты сложно автоматизировать (нужен headless-эмулятор).

## 3. UNIT-ТЕСТЫ

> ------------------------------------------------------------------------

### 3.1. ТЕСТИРОВАНИЕ МАТЕМАТИКИ (FIXED-POINT)

------------------------------------------------------------------------

ЦЕЛЬ: Убедиться, что fixed-point математика работает корректно и детерминированно на всех платформах.

```cpp
// tests/math/fixed_test.cpp

#include <gtest/gtest.h>

#include "core/math/fixed.h"

#include <cmath>

#include <random>

TEST(FixedPoint, Conversion) {

// Целые числа

EXPECT_EQ(fixed_from_int(0), 0);

EXPECT_EQ(fixed_from_int(1), 65536);

EXPECT_EQ(fixed_from_int(-1), -65536);

EXPECT_EQ(fixed_from_int(100), 6553600);

// Обратная конвертация

EXPECT_EQ(fixed_to_int(fixed_from_int(5)), 5);

EXPECT_EQ(fixed_to_int(fixed_from_int(-5)), -5);

// Float конвертация

EXPECT_NEAR(fixed_to_float(fixed_from_float(3.14f)), 3.14f, 0.001f);

EXPECT_NEAR(fixed_to_float(fixed_from_float(-2.71f)), -2.71f, 0.001f);

}

TEST(FixedPoint, Addition) {

fixed_t a = fixed_from_int(10); fixed_t b = fixed_from_int(20);

EXPECT_EQ(fixed_add(a, b), fixed_from_int(30));

EXPECT_EQ(fixed_add(a, fixed_from_int(-5)), fixed_from_int(5));

}

TEST(FixedPoint, Subtraction) {

fixed_t a = fixed_from_int(30); fixed_t b = fixed_from_int(10);

EXPECT_EQ(fixed_sub(a, b), fixed_from_int(20));

EXPECT_EQ(fixed_sub(b, a), fixed_from_int(-20));

}

TEST(FixedPoint, Multiplication) {

fixed_t a = fixed_from_int(5); fixed_t b = fixed_from_int(7);

EXPECT_EQ(fixed_mul(a, b), fixed_from_int(35));

```
```text
// Дробные числа

fixed_t c = fixed_from_float(2.5f); fixed_t d = fixed_from_float(4.0f);

float result = fixed_to_float(fixed_mul(c, d));

EXPECT_NEAR(result, 10.0f, 0.001f);

}
```
> TEST(FixedPoint, Division) {
>
> fixed_t a = fixed_from_int(100); fixed_t b = fixed_from_int(10);
>
> EXPECT_EQ(fixed_div(a, b), fixed_from_int(10));

```text
// Дробное деление

fixed_t c = fixed_from_float(10.0f); fixed_t d = fixed_from_float(3.0f);

float result = fixed_to_float(fixed_div(c, d));

EXPECT_NEAR(result, 3.333f, 0.01f);

}
```
```cpp
TEST(FixedPoint, DivisionByZero) {
#ifndef NDEBUG
EXPECT_DEATH((void)fixed_div(fixed_from_int(1), 0), "division by zero");
#else
EXPECT_EQ(fixed_div(fixed_from_int(1), 0), INT32_MAX);
EXPECT_EQ(fixed_div(fixed_from_int(-1), 0), INT32_MIN);
#endif
}

fixed_t a = fixed_from_int(10); fixed_t b = 0;

// Division by zero является ошибкой контракта API; фиксированное поведение должно быть определено явно (assert/guard)

// В зависимости от реализации

}

TEST(FixedPoint, Sqrt) {

fixed_t a = fixed_from_int(25); EXPECT_EQ(fixed_to_int(fixed_sqrt(a)), 5);

fixed_t b = fixed_from_int(2); float result = fixed_to_float(fixed_sqrt(b));

EXPECT_NEAR(result, 1.414f, 0.01f);

}

#include <algorithm>
TEST(FixedPoint, AccuracyStressTest) {
// Inputs are exact raw 16.16 values; reference arithmetic uses double.
std::mt19937 rng(42);
std::uniform_int_distribution<int32_t> raw_a(-6 * FIXED_ONE, 6 * FIXED_ONE);
std::uniform_int_distribution<int32_t> raw_abs_b(FIXED_ONE / 2, 6 * FIXED_ONE);
double max_mul_error = 0.0;
double max_div_error = 0.0;
const int N = 10000;
for (int i = 0; i < N; ++i) {
const fixed_t a = raw_a(rng);
fixed_t b = raw_abs_b(rng);
if ((i & 1) != 0) b = -b;

const double af = (double)a / (double)FIXED_ONE;
const double bf = (double)b / (double)FIXED_ONE;
const double mul_ref = af * bf;
const double div_ref = af / bf;
max_mul_error = std::max(max_mul_error, std::abs((double)fixed_to_float(fixed_mul(a, b)) - mul_ref));
max_div_error = std::max(max_div_error, std::abs((double)fixed_to_float(fixed_div(a, b)) - div_ref));
}
EXPECT_LT(max_mul_error, 2.0 / FIXED_ONE);
EXPECT_LT(max_div_error, 2.0 / FIXED_ONE);
}

```
```text
// Multiplication

fixed_t fa = fixed_from_float(a); fixed_t fb = fixed_from_float(b); fixed_t fc = fixed_mul(fa, fb); float result_mul = fixed_to_float(fc);

float expected_mul = a * b;

double error_mul = std::abs(result_mul - expected_mul); if (error_mul > max_error) max_error = error_mul;
```
> }
>
> TEST(FixedPoint, Determinism) {

```text
// Fixed raw vectors are compared against platform-independent expected raw results; this test is a single vector example.

fixed_t a = fixed_from_int(123); fixed_t b = fixed_from_int(456);
```
> fixed_t c1 = fixed_mul(a, b); fixed_t c2 = fixed_mul(a, b);
>
> EXPECT_EQ(c1, c2);
>
> EXPECT_EQ(c1, fixed_from_int(123 * 456));
>
> }
>
> ------------------------------------------------------------------------

### 3.2. ТЕСТИРОВАНИЕ ПАМЯТИ (АЛЛОКАТОРЫ)

------------------------------------------------------------------------

ЦЕЛЬ: Убедиться, что аллокаторы работают корректно, нет утечек, нет фрагментации.

```cpp
// tests/memory/linear_allocator_test.cpp

#include <gtest/gtest.h>

#include "core/memory/linear_allocator.h"

TEST(LinearAllocator, BasicAllocation) {

uint8_t buffer[1024]; LinearAllocator alloc(buffer, sizeof(buffer));

void* p1 = alloc.allocate(100);

EXPECT_NE(p1, nullptr);

EXPECT_EQ(alloc.used(), 100);

EXPECT_EQ(alloc.remaining(), 924);

void* p2 = alloc.allocate(200);

EXPECT_NE(p2, nullptr);

EXPECT_EQ(alloc.used(), 300);

// Указатели не должны перекрываться

EXPECT_NE(p1, p2);

EXPECT_TRUE((uint8_t*)p2 >= (uint8_t*)p1 + 100);

}

TEST(LinearAllocator, Alignment) {

uint8_t buffer[1024]; LinearAllocator alloc(buffer, sizeof(buffer));

// Выделяем 1 байт void* p1 = alloc.allocate(1); EXPECT_NE(p1, nullptr);

// Выделяем с выравниванием 8 байт void* p2 = alloc.allocate(10, 8); EXPECT_NE(p2, nullptr);

// p2 должен быть выровнен по 8 байт

EXPECT_EQ((uintptr_t)p2 % 8, 0);

}

TEST(LinearAllocator, OutOfMemory) {

uint8_t buffer[100]; LinearAllocator alloc(buffer, sizeof(buffer));

void* p1 = alloc.allocate(50); EXPECT_NE(p1, nullptr);

void* p2 = alloc.allocate(60);

EXPECT_EQ(p2, nullptr); // Не хватает памяти

}

TEST(LinearAllocator, Reset) {

uint8_t buffer[1024]; LinearAllocator alloc(buffer, sizeof(buffer));

alloc.allocate(100); alloc.allocate(200); EXPECT_EQ(alloc.used(), 300);

alloc.reset();

EXPECT_EQ(alloc.used(), 0);

EXPECT_EQ(alloc.remaining(), 1024);

// После reset можно снова выделять void* p = alloc.allocate(50);

EXPECT_NE(p, nullptr);

}

// tests/memory/pool_allocator_test.cpp

#include <gtest/gtest.h>

#include "core/memory/pool_allocator.h"

struct TestObject { int x, y; float z; };

TEST(PoolAllocator, BasicAllocation) {

Pool<TestObject, 10> pool;

TestObject* obj1 = pool.allocate();

EXPECT_NE(obj1, nullptr); obj1->x = 42;

TestObject* obj2 = pool.allocate();

EXPECT_NE(obj2, nullptr); obj2->x = 100;

// Объекты не должны перекрываться

EXPECT_NE(obj1, obj2);

}

TEST(PoolAllocator, Deallocation) { Pool<TestObject, 10> pool;

TestObject* obj1 = pool.allocate();

TestObject* obj2 = pool.allocate();

pool.deallocate(obj1);

// После deallocation можно снова выделить

TestObject* obj3 = pool.allocate();

EXPECT_EQ(obj3, obj1); // Переиспользование памяти

}

TEST(PoolAllocator, OutOfMemory) { Pool<TestObject, 2> pool;

TestObject* obj1 = pool.allocate();

TestObject* obj2 = pool.allocate();

TestObject* obj3 = pool.allocate();

EXPECT_NE(obj1, nullptr);

EXPECT_NE(obj2, nullptr);

EXPECT_EQ(obj3, nullptr); // Пул полон

}

TEST(PoolAllocator, Iteration) { Pool<TestObject, 10> pool;

TestObject* obj1 = pool.allocate(); obj1->x = 1;

TestObject* obj2 = pool.allocate(); obj2->x = 2;

TestObject* obj3 = pool.allocate(); obj3->x = 3;

pool.deallocate(obj2); // Удаляем средний

// Итерация по используемым объектам

int sum = 0; pool.for_each([&sum](TestObject* obj) {

sum += obj->x; });

EXPECT_EQ(sum, 4); // 1 + 3 (obj2 удалён)

}

// tests/memory/stack_allocator_test.cpp
#include <gtest/gtest.h>
#include "core/memory/stack_allocator.h"

TEST(StackAllocator, PushPop) {
uint8_t buffer[1024];
StackAllocator alloc(buffer, sizeof(buffer));
void* p1 = alloc.push(100);
ASSERT_NE(p1, nullptr);
const StackAllocator::Marker marker = alloc.mark();
void* p2 = alloc.push(200);
ASSERT_NE(p2, nullptr);
ASSERT_TRUE(alloc.pop(marker));
void* p3 = alloc.push(200);
EXPECT_EQ(p3, buffer + marker.offset);
}

TEST(StackAllocator, ForeignMarkerRejected) {
uint8_t bufferA[128];
uint8_t bufferB[128];
StackAllocator a(bufferA, sizeof(bufferA));
StackAllocator b(bufferB, sizeof(bufferB));
const StackAllocator::Marker marker = a.mark();
EXPECT_FALSE(b.pop(marker));
}

TEST(StackAllocator, ResetFrame) {

uint8_t buffer[1024]; StackAllocator alloc(buffer, sizeof(buffer));

alloc.push(100); alloc.push(200);

alloc.reset_frame();

EXPECT_EQ(alloc.used(), 0);

}

------------------------------------------------------------------------

```
### 3.3. ТЕСТИРОВАНИЕ ECS

------------------------------------------------------------------------

ЦЕЛЬ: Убедиться, что ECS работает корректно: создание сущностей, добавление/удаление компонентов, итерация.

```cpp
// tests/ecs/registry_test.cpp

#include <gtest/gtest.h>

#include "core/ecs/registry.h"

struct Position { int x, y; };

struct Velocity { int dx, dy; };

struct Health { int hp; };

TEST(ECS, CreateEntity) {

Registry registry;

uint32_t e1 = registry.create_entity(); uint32_t e2 = registry.create_entity();

EXPECT_NE(e1, e2);

EXPECT_NE(e1, 0); // 0 = invalid entity

EXPECT_NE(e2, 0);

}

TEST(ECS, DestroyEntity) {

Registry registry;

uint32_t e = registry.create_entity(); registry.add_component<Position>(e, {10, 20});

registry.destroy_entity(e);

EXPECT_FALSE(registry.has_component<Position>(e));

EXPECT_FALSE(registry.is_alive(e));

}

TEST(ECS, AddComponent) { Registry registry; uint32_t e = registry.create_entity(); registry.add_component<Position>(e, {10, 20});

EXPECT_TRUE(registry.has_component<Position>(e));

EXPECT_FALSE(registry.has_component<Velocity>(e));

Position* pos = registry.get_component<Position>(e);

EXPECT_EQ(pos->x, 10);

EXPECT_EQ(pos->y, 20);

}

TEST(ECS, RemoveComponent) { Registry registry; uint32_t e = registry.create_entity();

registry.add_component<Position>(e, {10, 20}); EXPECT_TRUE(registry.has_component<Position>(e));

registry.remove_component<Position>(e);

EXPECT_FALSE(registry.has_component<Position>(e));

}

TEST(ECS, ViewSingleComponent) {

Registry registry;

uint32_t e1 = registry.create_entity(); registry.add_component<Position>(e1, {1, 2});

uint32_t e2 = registry.create_entity(); registry.add_component<Position>(e2, {3, 4});

uint32_t e3 = registry.create_entity(); // e3 не имеет Position

auto view = registry.view<Position>();

int count = 0; for (auto entity : view) { count++;

EXPECT_TRUE(entity == e1 \|\| entity == e2);

}

EXPECT_EQ(count, 2);

}

TEST(ECS, ViewMultipleComponents) {

Registry registry;

uint32_t e1 = registry.create_entity(); registry.add_component<Position>(e1, {1, 2}); registry.add_component<Velocity>(e1, {10, 20});

uint32_t e2 = registry.create_entity(); registry.add_component<Position>(e2, {3, 4}); // e2 не имеет Velocity

uint32_t e3 = registry.create_entity(); registry.add_component<Position>(e3, {5, 6}); registry.add_component<Velocity>(e3, {30, 40});

auto view = registry.view<Position, Velocity>();

int count = 0; for (auto entity : view) { count++;

EXPECT_TRUE(entity == e1 \|\| entity == e3);

}

EXPECT_EQ(count, 2); // Только e1 и e3 имеют оба компонента

}

TEST(ECS, ViewWithModification) {

Registry registry;

uint32_t e1 = registry.create_entity(); registry.add_component<Position>(e1, {1, 2}); registry.add_component<Velocity>(e1, {10, 20});

uint32_t e2 = registry.create_entity(); registry.add_component<Position>(e2, {3, 4}); registry.add_component<Velocity>(e2, {30, 40});

// Проверяем результаты

Position* pos1 = registry.get_component<Position>(e1);

EXPECT_EQ(pos1->x, 11); // 1 + 10

EXPECT_EQ(pos1->y, 22); // 2 + 20

Position* pos2 = registry.get_component<Position>(e2);

EXPECT_EQ(pos2->x, 33); // 3 + 30

EXPECT_EQ(pos2->y, 44); // 4 + 40

}

TEST(ECS, Performance) { Registry registry;

```
```cpp
// Создаём 10000 сущностей const int N = 10000; for (int i = 0; i < N; ++i) {

uint32_t e = registry.create_entity(); registry.add_component<Position>(e, {i, i * 2}); registry.add_component<Velocity>(e, {1, 1});

}
```
> // Итерация по всем сущностям auto view = registry.view<Position, Velocity>();
>
> int count = 0; for (auto entity : view) { count++; }
>
> EXPECT_EQ(count, N);
>
> }
>
> ------------------------------------------------------------------------

### 3.4. ТЕСТИРОВАНИЕ УТИЛИТ

------------------------------------------------------------------------

```cpp
// tests/utils/logger_test.cpp

#include <gtest/gtest.h>

#include <fstream>

#include <cstdio>

TEST(Logger, BasicLogging) { const char* log_file = "test.log";

Logger::init(log_file);

Logger::log("Test message %d", 42);

Logger::shutdown();

// Проверяем, что файл создан и содержит сообщение

std::ifstream file(log_file); EXPECT_TRUE(file.good());

std::string line; std::getline(file, line); EXPECT_TRUE(line.find("Test message 42") != std::string::npos);

// Удаляем тестовый файл std::remove(log_file); }

```
## 4. INTEGRATION-ТЕСТЫ

> ------------------------------------------------------------------------

### 4.1. ТЕСТИРОВАНИЕ РЕНДЕРЕРА

------------------------------------------------------------------------

ЦЕЛЬ: Убедиться, что рендерер работает корректно: создание текстур, отрисовка мешей, работа с камерой.

```cpp
// tests/integration/renderer_test.cpp

#include <gtest/gtest.h>

#include "render/renderer.h"

#include "hal/platform.h"

uint64_t Render_TestReadbackHash();
bool Render_TestReadbackMatchesFile(const char* reference_path);

void SetUp() override {

ASSERT_TRUE(Platform_Init());

ASSERT_TRUE(Render_Init(320, 240));

}

void TearDown() override {

Render_Shutdown();

Platform_Shutdown();

}

};

TEST_F(RendererTest, CreateTexture) {

// Создаём тестовую текстуру 64x64 uint8_t pixels[64 * 64]; for (int i = 0; i < 64 * 64; ++i) { pixels[i] = i % 256; // 8-bit indexed }

```
```text
uint16_t palette[256];

for (int i = 0; i < 256; ++i) {

const uint16_t r5 = static_cast<uint16_t>((i >> 0) & 0x1F);

const uint16_t g5 = static_cast<uint16_t>((i >> 2) & 0x1F);

const uint16_t b5 = static_cast<uint16_t>((i >> 4) & 0x1F);

const bool stp = (i != 0); // test policy: entry 0 reserved as transparent black

palette[i] = static_cast<uint16_t>(r5 | (g5 << 5) | (b5 << 10) | (stp ? 0x8000u : 0u));

}

// RGB555+STP layout: R[4:0], G[9:5], B[14:10], STP[15].
```
TextureHandle tex = Render_CreateTexture(64, 64, pixels, palette);

EXPECT_NE(tex, 0);

Render_DestroyTexture(tex);

}

TEST_F(RendererTest, DrawTriangle) {

Vertex vertices[3] = {

```text
{0, 0, 0, 0, 0, 0, 0, 0, 255, 0, 0, 255},

{100, 0, 0, 0, 0, 0, 255, 0, 0, 255, 0, 255},

{50, 100, 0, 0, 0, 0, 128, 128, 0, 0, 255, 255}

};

Triangle indices[1] = {{0, 1, 2}}; Mat4 model; mat4_identity(&model);

Render_BeginFrame();

Render_Clear(0, 0, 0);

Render_DrawMesh(vertices, 3, indices, 1, &model);

Render_EndFrame();

EXPECT_TRUE(Render_TestReadbackMatchesFile("tests/visual/reference/triangle_320x240.bin"));

}

------------------------------------------------------------------------

```
### 4.2. ТЕСТИРОВАНИЕ ЗАГРУЗКИ АССЕТОВ

------------------------------------------------------------------------

```cpp
// tests/integration/mesh_loader_test.cpp

#include <gtest/gtest.h>

#include "assets/mesh_loader.h"

#include "core/memory/linear_allocator.h"

TEST(MeshLoader, LoadValidMesh) {

uint8_t buffer[1024 * 1024]; LinearAllocator alloc(buffer, sizeof(buffer));

Mesh* mesh = mesh_load("assets/build/cube.mesh", &alloc);

ASSERT_NE(mesh, nullptr);

EXPECT_GT(mesh->vertex_count, 0);

EXPECT_GT(mesh->index_count, 0);

EXPECT_NE(mesh->vertices, nullptr);

EXPECT_NE(mesh->indices, nullptr);

// Проверяем структурные invariants загрузчика.

EXPECT_EQ(mesh->index_count % 3, 0u);

for (uint32_t i = 0; i < mesh->index_count; ++i) {

EXPECT_LT(mesh->indices[i].v0, mesh->vertex_count);
EXPECT_LT(mesh->indices[i].v1, mesh->vertex_count);
EXPECT_LT(mesh->indices[i].v2, mesh->vertex_count);
}

TEST(MeshLoader, LoadInvalidFile) {

uint8_t buffer[1024 * 1024]; LinearAllocator alloc(buffer, sizeof(buffer));

Mesh* mesh = mesh_load("nonexistent.mesh", &alloc);

EXPECT_EQ(mesh, nullptr);

}

TEST(MeshLoader, LoadCorruptedFile) {

uint8_t buffer[1024 * 1024]; LinearAllocator alloc(buffer, sizeof(buffer));

// Создаём битый файл

FILE* f = fopen("corrupted.mesh", "wb"); uint32_t magic = 0xDEADBEEF; // Неправильный magic

fwrite(&magic, sizeof(magic), 1, f); fclose(f);

Mesh* mesh = mesh_load("corrupted.mesh", &alloc);

EXPECT_EQ(mesh, nullptr);

// Удаляем тестовый файл std::remove("corrupted.mesh"); }

------------------------------------------------------------------------

```
### 4.3. ТЕСТИРОВАНИЕ HAL

```cpp
// tests/integration/hal_test.cpp

#include <gtest/gtest.h>

#include "hal/platform.h"

TEST(HAL, InitShutdown) {

EXPECT_TRUE(Platform_Init());

Platform_Shutdown();

}

TEST(HAL, GetTimeMs) {

ASSERT_TRUE(Platform_Init());

uint32_t t1 = Platform_GetTimeMs();

Platform_SleepMs(100); uint32_t t2 = Platform_GetTimeMs();

EXPECT_GE(t2 - t1, 95); // Допускаем погрешность 5 мс

EXPECT_LE(t2 - t1, 150);

Platform_Shutdown();

}

TEST(HAL, InputKeyboard) {

ASSERT_TRUE(Platform_Init());

// Имитируем нажатие клавиши (через mock или реальный ввод)

// Это сложно автоматизировать, поэтому просто проверяем, что функция не крашится

bool key_state = Input_IsKeyDown(INPUT_KEY_W); EXPECT_TRUE(key_state == true \|\| key_state == false);

Platform_Shutdown();

}

```
## 5. HARDWARE QA (АДАПТИРОВАНО ПОД ЭМУЛЯТОРЫ)

> ------------------------------------------------------------------------

### 5.1. ОГРАНИЧЕНИЯ ЭМУЛЯТОРОВ

> ------------------------------------------------------------------------
>
> ПРОБЛЕМЫ ЭМУЛЯТОРОВ:

## 1. Тайминги DMA

- Эмуляторы могут не точно эмулировать задержки DMA.

- На реальном железе DMA может быть медленнее.

- Решение: использовать DuckStation (самый точный эмулятор PS1).

- Прерывания

- Эмуляторы могут не точно эмулировать порядок прерываний.

- На реальном железе прерывания могут приходить в другом порядке.

- Решение: тестировать на нескольких эмуляторах (DuckStation + PCSX-Redux).

## 3. GPU-баги

- Некоторые эмуляторы не эмулируют баги реального GPU PS1.

- Например, "wobbling" вершин может быть менее выражен.

- Решение: сравнивать скриншоты с реальным железом (если есть).

- Звук

- Эмуляторы могут не точно эмулировать SPU (Sound Processing Unit).

- Решение: для MVP звук --- заглушка, не критично.

## 5. CD-ROM

- Emulators do not reproduce mechanical seek/rotation latency exactly; performance-sensitive CD behavior must be validated with a configurable delay model and eventually real hardware.

- Real hardware access latency is workload- and seek-dependent; no single 300 ms seek-time constant is used by the engine.

- Решение: добавить искусственные задержки в эмуляторе для тестирования.

> ------------------------------------------------------------------------

### 5.2. ТЕСТИРОВАНИЕ НА DUCKSTATION (PS1)

> ------------------------------------------------------------------------
>
> АВТОМАТИЗАЦИЯ DUCKSTATION:
>
> # scripts/test_duckstation.sh
>
> #!/bin/bash
>
> # Запуск PS1-бинарника в DuckStation и проверка, что не крашится
>
> EMU_PATH="/c/Program Files/DuckStation/duckstation-qt-x64.exe"
>
> GAME_PATH="${PS1_EXE_PATH}"
>
> LOG_FILE="duckstation_test.log" TIMEOUT=30 # 30 секунд echo "Starting DuckStation test..." echo "Game: $GAME_PATH" echo "Timeout: ${TIMEOUT}s"
>
> # Запускаем эмулятор в headless-режиме (если поддерживается)
>
> DuckStation smoke automation is non-gating and version-specific. The normative CI gate is PS1 cross-compilation plus artifact validation; emulator QA is performed manually or only in a separately pinned, validated emulator workflow.
>
> if [ $EXIT_CODE -eq 124 ]; then echo "PASS: Game ran for ${TIMEOUT} seconds without crashing" exit 0
>
> elif [ $EXIT_CODE -eq 0 ]; then echo "PASS: Game exited normally"
>
> exit 0
>
> else
>
> echo "FAIL: Game crashed or exited with error code $EXIT_CODE"
>
> cat "$LOG_FILE" exit 1
>
> fi
>
> РУЧНОЕ ТЕСТИРОВАНИЕ:

- Запустить бинарник в DuckStation

> scripts\\run_ps1.bat

- Проверить

- [ ] Куб виден на экране

- [ ] Куб вращается плавно

- [ ] Камера управляется с геймпада

- [ ] W1: не менее 30 FPS на зафиксированной PS1 validation environment

- [ ] Нет артефактов графики- [ ] Нет крашей в течение 5 минут

- Сделать скриншоты

- F8 в DuckStation для скриншота

- Сравнить с PC-версией (должно быть похоже)

> ------------------------------------------------------------------------

### 5.3. ТЕСТИРОВАНИЕ НА PCSX-REDUX (PS1 С GDB)

> ------------------------------------------------------------------------
>
> ОТЛАДКА ЧЕРЕЗ GDB:
>
> (gdb) target remote localhost:3333
>
> (gdb) break main
>
> (gdb) continue
>
> # 3. Используем breakpoints, watch variables
>
> (gdb) print player.position
>
> (gdb) step
>
> (gdb) next
>
> ПРЕИМУЩЕСТВА PCSX-REDUX:

- Полноценный GDB-сервер

- Можно ставить breakpoints в runtime

- Можно смотреть регистры CPU, память

- Можно трассировать инструкцииНЕДОСТАТКИ:

- Медленнее, чем DuckStation

- Требует настройки GDB

- Не все баги воспроизводятся

> ------------------------------------------------------------------------

### 5.4. ПОДГОТОВКА К РЕАЛЬНОМУ ЖЕЛЕЗУ (БУДУЩЕЕ)

> КОГДА ПОНАДОБИТСЯ РЕАЛЬНОЕ ЖЕЛЕЗО:

- После релиза MVP (v0.1.0)

- Когда появятся баги, которые не ловятся в эмуляторе

- Перед физическим релизом игры (cartridge/CD) ЧТО НУЖНО КУПИТЬ:

- PS1 консоль (б/у, $20-50 на eBay)

- Рекомендация: SCPH-7000 или новее (компактная, надёжная)

- Избегать: SCPH-1000 (старая, могут быть проблемы)

## 2. Программатор EPROM/Flash ($50-100)

- Для загрузки бинарников на консоль

- Альтернатива: CD-R записывающий привод + CD-R диски

- Кабели

- AV-кабель (SCART или composite)

- Serial cable (для отладки через GDB)

- CRT-монитор (опционально, $50-100)

- Для проверки артефактов DAC

- LCD-мониторы могут скрывать некоторые багиТЕСТИРОВАНИЕ НА РАЗНЫХ РЕВИЗИЯХ:

> PS1 имеет несколько ревизий материнских плат:

- SCPH-1000 (ранняя, могут быть баги)

- SCPH-5000 (средняя)

- SCPH-7000 (поздняя, компактная)

- SCPH-9000 (финальная, самая стабильная)

> Нужно тестировать минимум на 2-3 ревизиях, чтобы убедиться, что код работает везде.
>
> ТЕСТИРОВАНИЕ НА ЭЛТ (CRT):
>
> ЭЛТ-мониторы показывают графику так, как она задумывалась:

- Правильные цвета (без артефактов DAC)

- Правильное разрешение (240p/480i)

- Правильные scanlinesПроверка на ЭЛТ:

- [ ] Нет мерцания

- [ ] Цвета корректны (сравнить с PC-версией)

- [ ] Нет артефактов текстур

- [ ] "Wobbling" вершин виден (как на скриншотах PS1 игр)

## 6. STRESS-TESTING

> ------------------------------------------------------------------------

### 6.1. ТЕСТ НА УТЕЧКИ ПАМЯТИ (48 ЧАСОВ)

> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Убедиться, что нет утечек памяти при длительной работе.
>
> ПОЧЕМУ 48 ЧАСОВ:

- На консолях без ОС утечка памяти = краш.

- 48 часа --- достаточно, чтобы обнаружить медленные утечки.

- Если за 48 часов нет утечек --- код стабилен.АВТОМАТИЗАЦИЯ:

> # scripts/stress_test_memory.sh
>
> #!/bin/bash
>
> # Запуск игры на 48 часов и мониторинг памяти
>
> EMU_PATH="/c/Program Files/DuckStation/duckstation-qt-x64.exe"
>
> GAME_PATH="${PS1_EXE_PATH}"
>
> LOG_FILE="stress_test.log"
>
> DURATION=172800 # 48 часов в секундах echo "Starting 48-hour stress test..." echo "Game: $GAME_PATH" echo "Duration: 48 hours" # Запускаем эмулятор
>
> The 48-hour PS1 stress test is manual/non-gating for MVP; do not use an undocumented emulator --batch contract as release evidence.
>
> if [[ "$EXIT_CODE" -eq 124 ]]; then
> echo "PASS: Game ran for ${DURATION} without crashing"
> exit 0
> elif [[ "$EXIT_CODE" -eq 0 ]]; then
> echo "PASS: Game exited normally"
> exit 0
> else
> echo "FAIL: Game exited with code $EXIT_CODE" >&2
> exit "$EXIT_CODE"
> fi
>
> else
>
> echo "FAIL: Game crashed after $((DURATION / 3600)) hours"
>
> cat "$LOG_FILE" exit 1
>
> fi
>
> РУЧНОЙ МОНИТОРИНГ ПАМЯТИ:
>
> В PCSX-Redux можно смотреть использование RAM в runtime:

- Debug -> Memory Viewer

- Смотреть на адрес 0x80010000 (начало heap)

- Проверять, что память не растёт со временемАЛЬТЕРНАТИВА ДЛЯ PC:

> На PC AddressSanitizer запускается через отдельный preset (например, \`pc-asan\`) либо локально как toolchain option. Canonical example:
> cmake --preset pc-asan
> cmake --build --preset pc-asan
> ctest --preset pc-asan --output-on-failure
>
> AddressSanitizer автоматически обнаружит утечки.
>
> ------------------------------------------------------------------------

### 6.2. ТЕСТ НА СТАБИЛЬНОСТЬ FPS

> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Убедиться, что FPS стабильный (нет просадок).
>
> АВТОМАТИЗАЦИЯ:
>
> # scripts/stress_test_fps.sh
>
> #!/bin/bash
>
> # Запуск игры на 1 час и мониторинг FPS
>
> EMU_PATH="/c/Program Files/DuckStation/duckstation-qt-x64.exe"
>
> GAME_PATH="${PS1_EXE_PATH}"
>
> LOG_FILE="fps_test.log" DURATION=3600 # 1 час
>
> echo "Starting 1-hour FPS test..."
>
> Manual FPS stress procedure: run the PS1 reference workload for 1 hour in the pinned emulator, record FPS from the application debug overlay, and require the documented threshold. No undocumented emulator --logfps CLI option is part of the gate.
>
> # Анализируем лог
>
> MIN_FPS=$(grep "FPS:" "$LOG_FILE" \| awk '{print $2}' \| sort -n \| head -1)
>
> MAX_FPS=$(grep "FPS:" "$LOG_FILE" \| awk '{print $2}' \| sort -n \| tail -1)
>
> AVG_FPS=$(grep "FPS:" "$LOG_FILE" \| awk '{sum+=$2; count++} END {print sum/count}') echo "Min FPS: $MIN_FPS" echo "Max FPS: $MAX_FPS" echo "Avg FPS: $AVG_FPS"
>
> if [[ -z "${MIN_FPS:-}" ]]; then
> echo "FAIL: no FPS samples found" >&2
> exit 1
> fi
> if (( MIN_FPS >= 25 )); then
> echo "PASS: FPS is stable (min >= 25)"
> exit 0
> else
> echo "FAIL: FPS below threshold" >&2
> exit 1
> fi
>
> else
>
> echo "FAIL: FPS dropped below 25" exit 1
>
> fi
>
> ------------------------------------------------------------------------

### 6.3. ТЕСТ НА КРАШИ

> ЦЕЛЬ: Убедиться, что игра не крашится при различных сценариях. СЦЕНАРИИ ТЕСТИРОВАНИЯ:

- Быстрое переключение уровней (100 раз)

- Загружать и выгружать уровни в цикле

- Проверять, что нет крашей

- Создание и удаление 10000 сущностей

- Создавать и удалять сущности в цикле

- Проверять, что нет утечек памяти

- Интенсивный ввод (нажимать все кнопки)

- Имитировать быстрый ввод с геймпада

- Проверять, что нет крашей

- Долгая игра (1 час без перезапуска)

- Играть 1 час без перезапуска

- Проверять, что нет крашейАВТОМАТИЗАЦИЯ:

> # scripts/stress_test_crash.sh
>
> #!/bin/bash
>
> # Запуск игры с различными сценариями
>
> EMU_PATH="/c/Program Files/DuckStation/duckstation-qt-x64.exe" GAME_PATH="${PS1_EXE_PATH}" echo "Running crash stress tests..."
>
> Manual crash stress procedure: exercise 100 level load/unload cycles through the application test mode; emulator command-line level injection is not a normative interface.
>
> fi
>
> done
>
> # Тест 2: Создание/удаление сущностей echo "Test 2: Entity creation/deletion..."
>
> Entity stress is executed by a test mode inside the application, not by undocumented emulator command-line arguments.
>
> fi
>
> Input stress is executed by an application test mode or manually; emulator CLI options are not part of the test contract.
>
> fi
>
> echo "PASS: All crash tests passed"

## 7. REGRESSION TESTING

> ------------------------------------------------------------------------

### 7.1. АВТОМАТИЧЕСКИЙ РЕГРЕССИОННЫЙ ТЕСТ

> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Убедиться, что новые изменения не сломали старую функциональность.
>
> АВТОМАТИЗАЦИЯ В CI/CD:

# .github/workflows/regression.yml
name: Regression Tests
on:
push:
branches: [ main, master, develop ]
pull_request:
branches: [ main, master, develop ]
jobs:
unit:
runs-on: ubuntu-latest
steps:

- uses: actions/checkout@v4
 with:
 submodules: recursive
- name: Install host dependencies
 run: sudo apt-get update && sudo apt-get install -y cmake ninja-build libgl1-mesa-dev
- name: Build
 run: \|
 cmake --preset pc-debug
 cmake --build --preset pc-debug
- name: Unit tests
 run: ctest --preset pc-debug --output-on-failure
 renderer-integration:
 runs-on: ubuntu-latest
 steps:
- uses: actions/checkout@v4
 with:
 submodules: recursive
- name: Install graphics dependencies
 run: sudo apt-get update && sudo apt-get install -y cmake ninja-build libgl1-mesa-dev xvfb mesa-utils
- name: Build
 run: \|
 cmake --preset pc-debug-render
 cmake --build --preset pc-debug-render
- name: Renderer integration tests
 run: xvfb-run -a ctest --preset pc-debug-render --output-on-failure -R integration

### 7.2. ВИЗУАЛЬНЫЙ РЕГРЕССИОННЫЙ ТЕСТ

> ЦЕЛЬ: Убедиться, что графика не изменилась (нет визуальных регрессий).
>
> ИНСТРУМЕНТЫ:

- RenderDoc (для PC) --- захват кадров, сравнение

- Скриншоты (для PS1) --- сравнение с эталонными скриншотамиАВТОМАТИЗАЦИЯ:

> # scripts/visual_regression.sh
>
> #!/bin/bash
>
> # Сравнение скриншотов с эталонными
>
> REFERENCE_DIR="tests/visual/reference"
> ACTUAL_DIR="tests/visual/actual"
> mkdir -p "$ACTUAL_DIR"
>
> # Запускаем игру и делаем скриншоты
>
> ./build/pc-release/game/rotating_cube --screenshot "$ACTUAL_DIR/pc.png"
>
> # Compare the reference and actual screenshots
>
> set +e
> compare -metric AE "$REFERENCE_DIR/pc.png" "$ACTUAL_DIR/pc.png" diff.png 2> diff_metric.txt
> COMPARE_EXIT=$?
> set -e
> if [[ "$COMPARE_EXIT" -gt 1 ]]; then
> cat diff_metric.txt
> exit "$COMPARE_EXIT"
> fi
> DIFF=$(identify -format "%[distortion]" diff.png)
> echo "Visual difference: $DIFF"
>
> # MVP gate: the threshold is project-defined and must be recorded with the reference image set.

```text
if [ $(echo "$DIFF > 100" | bc) -eq 1 ]; then echo "FAIL: Visual regression detected"

exit 1 else

echo "PASS: No visual regression"

exit 0
```
> MAX_DIFF=100
> DIFF_INT=${DIFF%.*}
> if (( DIFF_INT > MAX_DIFF )); then
> echo "FAIL: visual regression detected" >&2
> exit 1
> fi
> echo "PASS: no visual regression above threshold"

## 8. PERFORMANCE TESTING

> ------------------------------------------------------------------------

### 8.1. BENCHMARK НА PC

> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Измерить производительность на PC и сравнить с предыдущими версиями.
>
> // tests/benchmark/math_benchmark.cpp
>
> #include <benchmark/benchmark.h>
>
> #include "core/math/fixed.h"
>
> #include "core/math/vec3.h"
>
> #include "core/math/mat4.h"

```text
static void BM_FixedMul(benchmark::State& state) {

fixed_t a = fixed_from_int(123); fixed_t b = fixed_from_int(456);
```
for (auto _ : state) { fixed_t c = fixed_mul(a, b); benchmark::DoNotOptimize(c);

}

}

BENCHMARK(BM_FixedMul);

```cpp
static void BM_Vec3Normalize(benchmark::State& state) { Vec3 v = vec3_make(fixed_from_int(3), fixed_from_int(4), fixed_from_int(5));

for (auto _ : state) {

Vec3 n = vec3_normalize(v); benchmark::DoNotOptimize(n);

}

}

BENCHMARK(BM_Vec3Normalize);

```
```text
static void BM_Mat4Multiply(benchmark::State& state) {

Mat4 a, b; mat4_identity(&a); mat4_identity(&b);
```
```text
for (auto _ : state) {

Mat4 c;

mat4_multiply(&c, &a, &b); benchmark::DoNotOptimize(c);

}

}

BENCHMARK(BM_Mat4Multiply);

BENCHMARK_MAIN();
ЗАПУСК BENCHMARK:
cmake --preset pc-bench
cmake --build --preset pc-bench

./build/tests/benchmark/math_benchmark

------------------------------------------------------------------------

```
### 8.2. BENCHMARK НА PS1

> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Измерить производительность на PS1 (в эмуляторе).
>
> // tests/benchmark/ps1_benchmark.cpp
>
> #include "hal/platform.h"
>
> #include "core/math/fixed.h" #include "core/log/logger.h"
>
> void ps1_benchmark() {
>
> Logger::init("benchmark.log");

```cpp
// Benchmark: fixed_mul

uint32_t start = Platform_GetTimeMs(); for (int i = 0; i < 10000; ++i) { fixed_t a = fixed_from_int(i); fixed_t b = fixed_from_int(i + 1); fixed_t c = fixed_mul(a, b);

(void)c;

}

uint32_t end = Platform_GetTimeMs();

Logger::log("fixed_mul: %d ms for 10000 iterations", end - start);
```
> Logger::shutdown();
>
> }
>
> ------------------------------------------------------------------------

### 8.3. MEMORY PROFILING

> ЦЕЛЬ: Измерить использование памяти и найти узкие места.

```cpp
// core/memory/memory_profiler.h

#pragma once

#include <stdint.h>

struct MemoryStats { uint32_t total_ram; uint32_t used_ram; uint32_t total_vram; uint32_t used_vram; uint32_t linear_alloc_used; uint32_t pool_alloc_used; uint32_t stack_alloc_used; };

class MemoryProfiler { public:

static MemoryStats get_stats(); static void print_stats(); static bool check_thresholds(); };

ИСПОЛЬЗОВАНИЕ:

#ifdef DEBUG if (frame_count % 60 == 0) {

MemoryProfiler::print_stats(); if (!MemoryProfiler::check_thresholds()) {

Logger::log("WARNING: Memory usage critical!");

}

}

#endif

```
## 9. КРИТЕРИИ КАЧЕСТВА

> ------------------------------------------------------------------------

### 9.1. МЕТРИКИ КАЧЕСТВА КОДА

> ------------------------------------------------------------------------

- Покрытие тестами

- Цель: > 70% для core/

- Алерт: < 50%

- Инструмент: gcov + Codecov

- Статический анализ

- Цель: 0 ошибок, 0 предупреждений

- Инструмент: clang-tidy, cppcheck

- Стиль кода

- Цель: 100% соответствие STYLE_GUIDE.md

- Инструмент: clang-format

- Документация

- Цель: 100% публичных API документированы

- Инструмент: Doxygen

> ------------------------------------------------------------------------

### 9.2. МЕТРИКИ КАЧЕСТВА СБОРКИ

> ------------------------------------------------------------------------

- Время компиляции

- Цель: < 30 секунд (инкрементальная сборка PC)

- Алерт: > 2 минуты

- Размер бинарника

- Цель: < 500 КБ (PS1)

- Алерт: > 800 KiB; выход за acceptance ceiling является release blocker.

- Предупреждения компилятора

- Цель: 0 предупреждений

- Алерт: > 0 предупреждений

> ------------------------------------------------------------------------

### 9.3. МЕТРИКИ КАЧЕСТВА RUNTIME

## 1. FPS

- PC W1: >= 60 FPS on the recorded reference PC environment (стабильно)

- PS1 W1: >= 30 FPS on the recorded DuckStation validation environment (stable emulator validation; not a real-hardware performance guarantee) - Алерт: просадки < 25 FPS

- Использование RAM

- PS1 W1 peak main RAM <= 1.5 MiB

- Алерт: > 1.5 MiB; выход за acceptance ceiling является release blocker.

- Использование VRAM

- PS1: < 800 КБ из 1 МБ

- Алерт: > 800 KiB; выход за acceptance ceiling является release blocker.

- Время загрузки

- PC: < 1 секунда

- PS1 load time: measured against the reference asset set; no universal hardware <5 s requirement.

- Алерт: > 10 секунд

## 10. ИНСТРУМЕНТЫ И АВТОМАТИЗАЦИЯ

> ------------------------------------------------------------------------

### 10.1. GOOGLE TEST (GTEST)

> ------------------------------------------------------------------------
>
> УСТАНОВКА: \`git submodule update --init --recursive\` и canonical \`add_subdirectory(src/third_party/googletest ...)\`. Network FetchContent не используется.
>
> #include <gtest/gtest.h>
>
> TEST(TestSuite, TestName) {
>
> EXPECT_EQ(1, 1);
>
> ASSERT_TRUE(true);
>
> }
>
> ЗАПУСК:
>
> ctest --output-on-failure
>
> ------------------------------------------------------------------------

### 10.2. ADDRESSSANITIZER

> ------------------------------------------------------------------------
>
> ВКЛЮЧЕНИЕ:
>
> cmake --preset pc-asan
> cmake --build --preset pc-asan
> ctest --preset pc-asan --output-on-failure

- Обнаруживает утечки памяти- Обнаруживает use-after-free

- Обнаруживает buffer overflowНЕДОСТАТКИ:

- Замедляет выполнение в 2-3 раза

- Не работает на PS1 (только на PC)

> ------------------------------------------------------------------------

### 10.3. VALGRIND (ДЛЯ LINUX CI)

> ------------------------------------------------------------------------
>
> УСТАНОВКА:
>
> sudo apt-get install valgrind
>
> ЗАПУСК:
>
> valgrind --leak-check=full ./build/pc-release/game/hello_triangle.exe
>
> ПРЕИМУЩЕСТВА:

- Более точный, чем AddressSanitizer

- Показывает стек вызовов для утечекНЕДОСТАТКИ:

- Очень медленный (в 10-20 раз)

- Только для Linux

> ------------------------------------------------------------------------

### 10.4. CODECOV (ПОКРЫТИЕ ТЕСТАМИ)

> ИНТЕГРАЦИЯ С CI/CD: # .github/workflows/coverage.yml name: Code Coverage on: [push, pull_request] jobs:
>
> coverage: runs-on: ubuntu-latest steps:

- uses: actions/checkout@v4

- name: Install dependenciesrun: sudo apt-get install -y cmake ninja-build the pinned SDL3 source from src/third_party/sdl3 libgl1-mesa-dev lcov

- name: Build with coveragerun: \| cmake --preset pc-debug \

> cmake --preset pc-coverage
> cmake --build --preset pc-coverage
> ctest --preset pc-coverage --output-on-failure

- name: Run testsrun: cd build && ctest

- name: Generate coverage reportrun: \| cd build lcov --capture --directory . --output-file coverage.info lcov --remove coverage.info '/usr/*' --output-file coverage.info

- name: Upload to Codecovuses: codecov/codecov-action@v3 with:

> files: ./build/coverage.info
>
> ------------------------------------------------------------------------

### 10.5. СКРИПТЫ АВТОМАТИЗАЦИИ

> ------------------------------------------------------------------------
>
> # scripts/run_all_tests.sh
>
> #!/bin/bash
>
> # Запуск всех тестов echo "========================================" echo "Running all tests" echo "========================================"
>
> # Unit tests
> echo "[1/3] Running unit tests..."
> ctest --preset pc-debug --output-on-failure -R unit
> if [ $? -ne 0 ]; then echo "FAIL: Unit tests failed"; exit 1; fi
>
> fi
>
> # Integration tests
> echo "[2/3] Running integration tests..."
> cmake --preset pc-debug-render
> cmake --build --preset pc-debug-render
> xvfb-run -a ctest --preset pc-debug-render --output-on-failure -R integration
> if [ $? -ne 0 ]; then echo "FAIL: Integration tests failed"; exit 1; fi
>
> fi
>
> # Smoke tests (examples)
> echo "[3/3] Running smoke tests..."
> timeout 10 ./build/pc-debug/game/hello_triangle
> timeout 10 ./build/pc-debug/game/rotating_cube
> echo "========================================"
> echo "All tests passed"
>
> echo "========================================"

## 11. ТЕСТОВЫЕ СЦЕНАРИИ ДЛЯ MVP

> ТЕСТОВЫЕ СЦЕНАРИИ ДЛЯ ПРОВЕРКИ MVP:

- Hello Triangle на PC

- [ ] Окно открывается

- [ ] Треугольник виден

- [ ] Текстура на треугольнике

- [ ] W1: не менее 60 FPS на зафиксированной reference PC environment

- [ ] Нет крашей при закрытии

- Hello Triangle на PS1 (DuckStation)

- [ ] Бинарник загружается

- [ ] Треугольник виден

- [ ] Текстура на треугольнике

- [ ] W1: не менее 30 FPS на зафиксированной PS1 validation environment

- [ ] Нет крашей

- Rotating Cube на PC

- [ ] Куб вращается

- [ ] Камера управляется (WASD + мышь)

- [ ] Текстура на кубе

- [ ] W1: не менее 60 FPS на зафиксированной reference PC environment

- [ ] Нет крашей при длительной работе (10 минут)

- Rotating Cube на PS1 (DuckStation)

- [ ] Куб вращается

- [ ] Камера управляется (геймпад)

- [ ] Текстура на кубе

- [ ] W1: не менее 30 FPS на зафиксированной PS1 validation environment

- [ ] Нет крашей при длительной работе (10 минут)

- Загрузка ассетов

- [ ] cube.mesh загружается корректно- [ ] texture.tex загружается корректно

- [ ] Палитра применяется правильно

- [ ] Нет ошибок в логе

- Ввод

- [ ] Клавиатура работает (PC)

- [ ] Геймпад работает (PC)

- [ ] Геймпад работает (PS1)

- [ ] Аналоговые стики работают

- Память

- [ ] Нет утечек (AddressSanitizer)

- [ ] PS1 W1 peak main RAM <= 1.5 MiB (PS1) - [ ] PS1 W1 peak VRAM <= 800 KiB

- Производительность

- [ ] W1: не менее 60 FPS на зафиксированной reference PC environment

- [ ] W1: не менее 30 FPS на зафиксированной PS1 validation environment

- [ ] Нет просадок FPS

v0.1.0 release requires every condition below:

Performance gate: W0 verifies baseline renderer conformance; W1 is the release demo workload. Both workloads must meet their documented FPS/memory gates on the stated target environment; no claim of universal PS1 hardware timing is made until validated on real hardware.

1. PC Release builds pass on Windows, Linux and macOS CI.

2. PS1 Release build passes with pinned PSn00bSDK v0.24.

3. Unit tests pass; supported host configurations report no sanitizer findings.

4. Hello Triangle and Rotating Cube pass the documented PC and DuckStation scenarios using the bootable .cue/.bin disc image for PS1 filesystem acceptance.

5. Reference PS1 workload stays within <= 1.5 MiB main RAM and <= 800 KiB VRAM.

6. PS1 executable is <= 500 KiB and the release package contains both PC and PS1 artifacts.

7. Malformed asset tests demonstrate safe rejection of invalid v1 .mesh/.tex files, including invalid offsets and truncated CI8 payloads.

8. Renderer correctness is checked with deterministic framebuffer/reference comparisons, not crash-only tests.

9. No blocker or critical issue is open against the release commit.

### 11.1. НОРМАТИВНЫЙ RELEASE GATE

## 12. РИСКИ И МИТИГАЦИЯ

## РИСК 1: Эмуляторы врут в таймингах

> Вероятность: СРЕДНЯЯ Влияние: СРЕДНЕЕ Митигация:

- Использовать DuckStation (самый точный эмулятор)

- Тестировать на нескольких эмуляторах

- Купить реальную PS1 для финальной проверки

## РИСК 2: Unit-тесты не покрывают все баги

> Вероятность: ВЫСОКАЯ Влияние: СРЕДНЕЕ Митигация:

- Писать интеграционные тесты

- Делать smoke tests в эмуляторе

- Ручное тестирование перед релизом

## РИСК 3: Stress test занимает слишком много времени

> Вероятность: СРЕДНЯЯ Влияние: НИЗКОЕ Митигация:

- Автоматизировать через CI/CD

- Запускать ночью (не блокирует разработку)

- Для MVP --- опционально (после релиза)

## РИСК 4: Визуальные регрессии не обнаружены

> Вероятность: СРЕДНЯЯ Влияние: СРЕДНЕЕ Митигация:

- Использовать RenderDoc для PC

- Делать скриншоты на PS1

- Сравнивать с эталонными скриншотами

#### РИСК 5: Производительность ниже ожидаемой

> Вероятность: СРЕДНЯЯ Влияние: СРЕДНЕЕ Митигация:

- Использовать benchmarks

- Профилировать память

- Оптимизировать узкие места

## 13. КРИТЕРИИ ЗАВЕРШЕНИЯ ЭТАПА

> ЭТАП 7 считается завершённым, когда выполнены ВСЕ следующие условия:
>
> UNIT-ТЕСТЫ:
>
> Все unit-тесты проходят (100% pass rate)
>
> Покрытие тестами > 70% для core/
>
> Тесты запускаются автоматически в CI INTEGRATION-ТЕСТЫ:
>
> Все integration-тесты проходят
>
> Рендерер работает корректно
>
> Загрузка ассетов работает
>
> HAL работает (PC + PS1)
>
> HARDWARE QA:
>
> Hello Triangle работает на PC и PS1 (DuckStation)
>
> Rotating Cube работает на PC и PS1 (DuckStation)
>
> Нет крашей в эмуляторе (тест 10 минут)
>
> Скриншоты PC vs PS1 визуально похожи STRESS-TESTING:
>
> Нет утечек памяти (AddressSanitizer)
>
> FPS стабильный (нет просадок)
>
> Нет крашей при длительной работе (1 час) REGRESSION TESTING:
>
> Автоматические регрессионные тесты проходят
>
> Визуальные регрессии не обнаружены PERFORMANCE:
>
> W1: не менее 60 FPS на зафиксированной reference PC environment
>
> W1: не менее 30 FPS на зафиксированной PS1 validation environment
>
> PS1 W1 peak main RAM <= 1.5 MiB (PS1)
>
> PS1 W1 peak VRAM <= 800 KiB

### ДОКУМЕНТАЦИЯ

> Написан TESTING.md (как тестировать)
>
> Написаны тестовые сценарии для MVP Все тесты документированы

# ЭТАП 8: РЕЛИЗ, ДОКУМЕНТАЦИЯ И ПОДДЕРЖКА (RELEASE & MAINTENANCE)

> Ретро-движок для консолей 5-го поколения + PC (SDL3)
>
> Дата: 2026
>
> Согласовано с: ETAP1-ETAP7

## 1. ВВЕДЕНИЕ И АДАПТАЦИЯ ПОД СОЛО-РАЗРАБОТЧИКА

> ИСХОДНЫЙ КОРПОРАТИВНЫЙ ПЛАН (из ТЗ):

- Отдельная команда технической документации (3-5 человек)

- Корпоративный портал для дистрибуции

- B2B-отдел для продажи лицензий

- Отдельная команда поддержки (5-10 человек)

- SLA (Service Level Agreement) для клиентовАДАПТИРОВАННЫЙ ПЛАН (для соло):

- Один разработчик пишет всю документацию

- GitHub как единственная платформа дистрибуции

- Open-source (MIT license) --- бесплатно для всех

- GitHub Issues + Discord для поддержки

- Community-driven поддержка (контрибьюторы помогают)

> ПОЧЕМУ ТАК:

- Соло-разработчик не может содержать команду поддержки.

- Open-source модель привлекает контрибьюторов.

- GitHub --- стандарт для open-source проектов.

- Discord --- лучшая платформа для общения с сообществом.

> ПРИНЦИПЫ РЕЛИЗА:

- "Documentation is code" --- документация так же важна, как код.

- "Release early, release often" --- лучше релизить чаще с меньшими фичами.

- "Community first" --- сообщество важнее отдельных пользователей.

- "Transparency" --- все решения открыты, roadmap публичен.

- "Sustainability" --- проект должен быть устойчивым (не выгореть).

## 2. ДОКУМЕНТАЦИЯ

> ------------------------------------------------------------------------

### 2.1. API REFERENCE (DOXYGEN)

> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Создать полную документацию всех публичных API движка. ИНСТРУМЕНТ: Doxygen (стандарт для C/C++ проектов) УСТАНОВКА:

- Windows: скачать с <https://www.doxygen.org/download.html>

- Linux: sudo apt-get install doxygen graphviz

- macOS: brew install doxygen

> КОНФИГУРАЦИЯ:
>
> # Doxyfile (корневой файл конфигурации)
>
> PROJECT_NAME = "Retro Engine"
>
> PROJECT_NUMBER = "0.1.0"
>
> OUTPUT_DIRECTORY = docs/api
>
> INPUT = src/
>
> RECURSIVE = YES
>
> FILE_PATTERNS = *.h *.hpp *.cpp
>
> EXTRACT_ALL = YES
>
> EXTRACT_PRIVATE = NO
>
> EXTRACT_STATIC = YES
>
> GENERATE_HTML = YES GENERATE_LATEX = NO GENERATE_XML = YES

HAVE_DOT = YES

CALL_GRAPH = YES

CALLER_GRAPH = YES

ДОКУМЕНТАЦИЯ КОДА:

```text
// core/math/fixed.h

/// \@file fixed.h

/// \@brief Fixed-point math (16.16) for retro consoles

/// \@details This module provides fixed-point arithmetic operations /// optimized for consoles without FPU (PS1, Saturn, Jaguar).

/// All operations are deterministic across platforms.

/// \@brief Fixed-point type (16.16 format)

/// \@details 32-bit integer with 16 bits for integer part and 16 bits

/// for fractional part. Range: -32768.0 to 32767.99998

/// Precision: 1/65536 ≈ 0.000015 typedef int32_t fixed_t;

/// \@brief Convert integer to fixed-point

/// \@param v Integer value

/// \@return Fixed-point representation

/// \@example

/// \@code

/// fixed_t x = fixed_from_int(5); // x = 327680 (5 * 65536)

/// \@endcode

inline fixed_t fixed_from_int(int32_t v);

/// \@brief Convert float to fixed-point

/// \@param v Float value

/// \@return Fixed-point representation

/// \@warning Non-finite input is converted to 0. Finite values outside the representable range are clamped to INT32_MIN/INT32_MAX.
inline fixed_t fixed_from_float(float v);

/// \@brief Add two fixed-point numbers

/// \@param a First operand

/// \@param b Second operand

/// \@return Sum (a + b)

/// \@note Addition and subtraction use the documented saturating overflow contract. inline fixed_t fixed_add(fixed_t a, fixed_t b);

/// \@brief Multiply two fixed-point numbers

/// \@param a First operand

/// \@param b Second operand

/// \@return Product (a * b) /// \@details Uses 64-bit intermediate result to avoid overflow.

/// Uses a signed 64-bit intermediate and the fixed 2\^16 scale factor. /// On PS1, CPU mult produces its product in HI/LO; GTE MAC registers are a separate mechanism.

ГЕНЕРАЦИЯ ДОКУМЕНТАЦИИ: # Локальная генерация doxygen Doxyfile

# Результат: docs/api/html/index.html

# Интеграция с CI/CD (автоматическая генерация при релизе)

# .github/workflows/docs.yml name: Generate Documentation on:

push:

tags:

```
- 'v*'jobs:

> docs: runs-on: ubuntu-latest steps:

- uses: actions/checkout@v4

- name: Install Doxygenrun: sudo apt-get install -y doxygen graphviz

- name: Generate documentationrun: doxygen Doxyfile

- name: Deploy to GitHub Pagesuses: peaceiris/actions-gh-pages@v3 with:

> github_token: ${{ secrets.GITHUB_TOKEN }} publish_dir: ./docs/api/html
>
> ДОСТУП К ДОКУМЕНТАЦИИ:

- Локально: docs/api/html/index.html

- Онлайн: <https://yourname.github.io/retro-engine/>

> ------------------------------------------------------------------------

### 2.2. ТУТОРИАЛЫ ДЛЯ ПОЛЬЗОВАТЕЛЕЙ

> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Помочь новым пользователям начать работу с движком.
>
> СТРУКТУРА ТУТОРИАЛОВ:
>
> docs/tutorials/
>
> 01_getting_started.md # Установка и первый запуск
>
> 02_hello_triangle.md # Первый пример
>
> 03_exporting_from_blender.md # Экспорт моделей из Blender
>
> 04_creating_textures.md # Создание текстур
>
> 05_building_for_ps1.md # Сборка для PS1
>
> 06_ecs_basics.md # Основы ECS
>
> 07_custom_components.md # Создание своих компонентов
>
> 08_custom_systems.md # Создание своих систем
>
> 09_optimization.md # Оптимизация для PS1
>
> 10_debugging.md # Отладка на эмуляторе ПРИМЕР ТУТОРИАЛА:
>
> # docs/tutorials/01_getting_started.md
>
> # Getting Started
>
> This tutorial will guide you through installing Retro Engine and running your first example.
>
> ## Prerequisites
>
> Before you begin, ensure you have the following installed:

- **Git** (2.40 or later)

- **CMake** (3.25 or later)

- **Ninja** (1.11 or later)

- **Visual Studio 2022** (Windows) or **GCC** (Linux/macOS)

- Python не требуется: asset converter --- standalone C++14 tool

> ### Windows Installation

- Download and install Git from <https://git-scm.com/download/win>

- Download and install CMake from <https://cmake.org/download/>

- Download Ninja from <https://github.com/ninja-build/ninja/releases>

- Extract \`ninja.exe\` to \`C:\\Tools\\ninja\

- Add \`C:\\Tools\\ninja\\ to your PATH

- Install Visual Studio 2022 with "Desktop development with C++"

> ### Verify Installation
>
> Open a terminal and run:
>
> \`\`\`bash git --version # Should show 2.40+ cmake --version # Should show 3.25+ ninja --version # Should show 1.11+
>
> Installing Retro Engine Clone the Repository git clone <https://github.com/yourname/retro-engine.git> cd retro-engine
>
> Initialize Submodules
>
> Retro Engine uses git submodules for dependencies:
>
> git submodule update --init --recursive
>
> Build for PC
>
> cmake --preset pc-release
>
> # Build
>
> cmake --build --preset pc-release
>
> Run Hello Triangle # Windows
>
> build\\pc-release\\game\\hello_triangle.exe
>
> # Linux/macOS
>
> ./build/pc-release/game/hello_triangle
>
> You should see the Hello Triangle example. Use the documented controls to verify input and presentation.
>
> Building for PS1
>
> Install PSn00bSDK Toolchain
>
> Download the pinned v0.24 PSn00bSDK Windows release from <https://github.com/Lameguy64/PSn00bSDK/releases/tag/v0.24> and extract it to C:\\PSn00bSDK.
>
> Extract to C:\\PSn00bSDK\
>
> Add C:\\PSn00bSDK\\bin\\ to your PATH
>
> Build PS1 Version
>
> set PSN00BSDK_ROOT=C:\\PSn00bSDK
> set PSN00BSDK_LIBS=%PSN00BSDK_ROOT%\\lib\\libpsn00b
>
> cmake --preset ps1-release
> cmake --build --preset ps1-release
>
> Run the bootable PS1 disc in DuckStation
>
> Download DuckStation from <https://github.com/stenzek/duckstation/releases> Run the PS1 executable:
>
> # Windows
>
> "C:\\Program Files\\DuckStation\\duckstation-qt-x64.exe" build\\ps1-release\\retro-engine.cue
>
> Next Steps
>
> Hello Triangle Tutorial
>
> --- Create your first triangle
>
> Exporting from Blender
>
> --- Import your own models
>
> ECS Basics
>
> --- Learn the Entity Component System
>
> Troubleshooting
>
> CMake Configuration Fails Error: "Could not find SDL3"
> Solution: run git submodule update --init --recursive, then use cmake --preset pc-debug. The canonical PC build does not require a host SDL package-manager installation.
>
> C:\\vcpkg\\bootstrap-vcpkg.bat
>
> C:\\vcpkg\\vcpkg install sdl3:x64-windows # non-canonical fallback; canonical setup uses the SDL3 submodule
>
> PS1 Build Fails
>
> Solution: Ensure PSn00bSDK toolchain is in your PATH:
>
> DuckStation Crashes
>
> Error: Game crashes on startup
>
> Solution: verify the PSn00bSDK v0.24 toolchain and the generated PS1 executable first; do not tie DuckStation versioning to the PSn00bSDK version. Pin a known DuckStation build separately for CI smoke tests.
>
> Getting Help
>
> GitHub Issues: <https://github.com/yourname/retro-engine/issues>
>
> Discord: <https://discord.gg/retro-engine>
>
> Documentation: <https://yourname.github.io/retro-engine/>

### 2.3. ДЕМО-ПРОЕКТЫ

> ЦЕЛЬ: Показать возможности движка на реальных примерах.
>
> СТРУКТУРА ДЕМО-ПРОЕКТОВ:
>
> examples/ hello_triangle/ # Минимальный пример (треугольник) main.cpp CMakeLists.txt
>
> README.md
>
> assets/ triangle.gltf texture.png
>
> rotating_cube/ # Базовый пример (вращающийся куб) main.cpp CMakeLists.txt
>
> README.md
>
> assets/ cube.gltf texture.png
>
> multi_object_scene/ # Несколько объектов main.cpp CMakeLists.txt
>
> README.md
>
> assets/ cube.gltf sphere.gltf textures/ wood.png metal.png
>
> ecs_demo/ # Демонстрация ECS main.cpp CMakeLists.txt
>
> README.md
>
> components.h systems.h
>
> physics_demo/ # Простая физика (AABB коллизии) main.cpp CMakeLists.txt
>
> README.md
>
> texture_formats/ # Различные форматы текстур main.cpp CMakeLists.txt
>
> README.md
>
> assets/ 8bit.png
>
> 4bit.png camera_controls/ # Различные типы камер main.cpp CMakeLists.txt
>
> README.md
>
> ПРИМЕР README ДЛЯ ДЕМО-ПРОЕКТА: examples/rotating_cube/README.md
>
> Rotating Cube Demo
>
> This demo shows a textured cube rotating in 3D space with camera controls.
>
> Features
>
> Textured 3D cube (8-bit indexed texture with RGB555+STP palette)
>
> Smooth rotation animation
>
> Camera controls (WASD + mouse on PC, gamepad on PS1)
>
> Fixed-point math (16.16) for all transformations
>
> Painter's algorithm for depth sorting (no Z-buffer)
>
> Controls
>
> PC
>
> W/A/S/D --- Move camera
>
> Mouse --- Look around
>
> Space --- Move up
>
> Shift --- Move down
>
> ESC --- Exit

PS1

D-Pad --- Move camera

Left Stick --- Look around (DualShock)

Cross --- Move up

Circle --- Move down

Start --- Exit

Building

PC: cmake --preset pc-release && cmake --build --preset pc-release --target rotating_cube. PS1: cmake --preset ps1-release && cmake --build --preset ps1-release.

cmake --build --preset ps1-release --target retro_engine_ps1

Running PC

build/pc-release/game/rotating_cube.exe PS1 (DuckStation)

scripts\\run_ps1.bat

Performance

PC W1: >= 60 FPS on the recorded reference PC environment

PS1 W1: >= 30 FPS on the recorded DuckStation validation environment (stable emulator validation; not a real-hardware performance guarantee)

Memory usage (PS1): the reference workload must satisfy the normative peak budgets; sample values are not hardware guarantees.

Polygon count: 12 triangles (1 cube)

Source Code Overview

```cpp
// main.cpp

#include "core/ecs/registry.h"

#include "core/math/vec3.h"

#include "render/renderer.h"

#include "hal/platform.h"
#include "assets/asset_loader.h"
#include "core/log/logger.h"

// Components

struct Position { Vec3 value; }; struct Rotation { fixed_t yaw, pitch, roll; }; struct MeshRenderer { uint32_t mesh_id; uint32_t texture_id; }; struct Camera { fixed_t fov, near_plane, far_plane; };

// Systems class RotationSystem { public:

static void update(Registry& registry, fixed_t delta_time) {

auto view = registry.view<Rotation>();
for (auto it = view.begin(); it != view.end(); ++it) {

Rotation& rot = it.rotation();

rot.yaw = fixed_add(rot.yaw, fixed_mul(delta_time, fixed_from_int(90)));

}

}

};

int main() {
if (!Platform_Init()) return 1;
if (!Render_Init(320, 240)) { Platform_Shutdown(); return 1; }
Registry registry;
MeshHandle mesh = AssetLoader_LoadMesh("assets/build/cube.mesh");
TextureHandle texture = AssetLoader_LoadTexture("assets/build/texture.tex");
if (!mesh \|\| !texture) {
Logger::error("Failed to load rotating_cube assets");
Render_Shutdown();
Platform_Shutdown();
return 1;
}
uint32_t cube = registry.create_entity();
registry.add_component<Position>(cube, {0, 0, fixed_from_int(-5)});
registry.add_component<Rotation>(cube, {0, 0, 0});
registry.add_component<MeshRenderer>(cube, {mesh.id, texture.id});
registry.add_component<Camera>(cube, {fixed_from_int(60), fixed_from_float(0.1f), fixed_from_int(100)});
while (Platform_PumpEvents()) {
const fixed_t delta_time = SIM_DT;
RotationSystem::update(registry, delta_time);
Render_BeginFrame();
Render_Clear(0, 0, 32);
RenderSystem::update(registry);
Render_EndFrame();
}
Render_Shutdown();
Platform_Shutdown();
return 0;
}

Screenshots

Video

Watch on Youtube

```
### 2.4. АРХИТЕКТУРНАЯ ДОКУМЕНТАЦИЯ

> ЦЕЛЬ: Объяснить архитектуру движка для контрибьюторов. docs/ARCHITECTURE.md
>
> Architecture
>
> This document describes the architecture of Retro Engine.
>
> Overview
>
> Retro Engine is a cross-platform 3D game engine designed for retro consoles (PS1, Saturn, N64, Jaguar, 3DO) and PC. The architecture follows these principles:
>
> Data-Oriented Design (DOD) --- Data is stored sequentially for cache efficiency
>
> Minimal Abstractions --- Zero-cost abstractions, no virtual dispatch in hot paths
>
> Explicit Memory Management --- No malloc/free in runtime, custom allocators
>
> Determinism --- Fixed-point math ensures identical behavior across platforms
>
> Platform Abstraction via HAL --- Single codebase, platform-specific implementations
>
> Layers
>
> The engine has 4 dependency layers plus a Core/Foundation used by Systems and Services. Core has no dependency on HAL.
>
> +-----------------------------------------------------------+
>
> Core/Foundation
> ↓ (used by Systems and Services; no dependency on HAL)
> Layer 4: Game
> ↓
> Layer 3: Systems
> ↓
> Layer 2: Services
> ↓
> Layer 1: HAL (Hardware Abstraction Layer)
> Dependency Rule: Game/System/Service dependencies point only downward; Core is dependency-free from HAL and is available to higher layers.
>
> Core / Foundation Modules
>
> Entity Component System (ECS)
>
> The ECS follows the "Entity-Component-System" pattern:
>
> Entity --- Just an ID (uint32_t). No data.
>
> Component --- POD struct with data. Stored in arrays.
>
> System --- Function that operates on component arrays.
>
> // Entity using Entity = uint32_t;
>
> // Components (POD structs) struct Position { Vec3 value; }; struct Velocity { Vec3 value; };
>
> class MovementSystem {
> public:
> static void update(Registry& registry, fixed_t delta_time) {
> auto view = registry.view<Position, Velocity>();
> for (auto it = view.begin(); it != view.end(); ++it) {
> Position& pos = it.position();
> Velocity& vel = it.velocity();
> pos.value.x = fixed_add(pos.value.x, fixed_mul(vel.value.x, delta_time));
> pos.value.y = fixed_add(pos.value.y, fixed_mul(vel.value.y, delta_time));
> pos.value.z = fixed_add(pos.value.z, fixed_mul(vel.value.z, delta_time));
> }
> }
> };
>
> Memory Management
>
> The engine uses three custom allocators:
>
> Linear Allocator --- For level loading. Fast allocation, bulk deallocation.
>
> Pool Allocator --- For dynamic objects (bullets, enemies). Fixed-size slots. Stack Allocator --- For per-frame temporary data. Push/pop semantics.

```text
// Linear Allocator

// Large arenas must not be automatic stack objects on PS1.

static uint8_t level_buffer[448 * 1024]; // 1 MiB, reserved by the memory subsystem

LinearAllocator level_alloc(level_buffer, sizeof(level_buffer));

Mesh* mesh = (Mesh*)level_alloc.allocate(sizeof(Mesh));

// ... use mesh ... level_alloc.reset(); // Free all at once
```
> // Pool Allocator
>
> Pool<Bullet, 256> bullets; // Max 256 bullets
>
> Bullet* b = bullets.allocate();
>
> // ... use bullet ... bullets.deallocate(b);
>
> Fixed-Point Math
>
> Engine simulation math uses fixed-point 16.16 for determinism and bounded arithmetic; hardware backends may use native representations internally:
>
> typedef int32_t fixed_t; // 16.16 fixed-point
>
> fixed_t a = fixed_from_int(5); // 327680 fixed_t b = fixed_from_float(2.5f); // 163840 fixed_t c = fixed_mul(a, b); // 819200 (12.5) int32_t d = fixed_to_int(c); // 12
>
> Hardware Abstraction Layer (HAL)
>
> The HAL provides a unified API across platforms:
>
> // Unified API bool Platform_Init(void); void Render_DrawMesh(const Vertex* vertices, uint32_t count, const Mat4* model);

 // PC implementation: see canonical hal/pc/platform_pc.cpp in §3.3; it uses SDL3 + OpenGL 3.3 and initializes the GL context before loading GL entry points.
 -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 // PS1 implementation: see canonical hal/ps1/platform_ps1.cpp in §3.4; platform startup uses the pinned PSn00bSDK APIs and does not define a generic GPU initialization HAL call.

> Render Pipeline
```text

The render pipeline follows these steps:

TRANSFORM -> CLIP -> CULL -> SORT -> PACK -> SUBMIT. Projection-specific state is backend-owned; clipping occurs before projected coordinates are consumed.

CULL --- Backface culling after clipping; NCLIP provides screen-space signed-area/backface rejection on PS1 and does not replace frustum clipping.

SORT --- Deterministic back-to-front ordering of bounded opaque depth keys; PS1 maps depth to Ordering Table buckets, PC uses std::sort with Z-test disabled.

PACK --- Pack backend-neutral render commands into the fixed command buffer; overflow returns failure with a deterministic diagnostic.

SUBMIT --- Backend converts commands to native packets/OT entries; PS1 submission uses the SDK/DMA path, PC uses OpenGL draw calls.

Asset Pipeline

Assets are converted by the host-side asset_converter. PC development may invoke the converter automatically; PS1 cross-builds must not compile or execute the converter with the MIPS toolchain. MVP example .mesh/.tex files are versioned under assets/build/ so a clean PS1 checkout can build without a host-tool/cross-toolchain conflict.

```
MVP asset schema: .mesh v1 stores engine-space fixed_t vertex attributes and Triangle indices; .tex v1 stores CI8 index data plus 256-entry RGB555+STP CLUT for the MVP renderer. Endianness, field widths, and generated-file provenance are part of the v1 format contract.

> glTF/OBJ -> [asset_converter] -> .mesh (binary)
>
> PNG/JPG -> [asset_converter] -> .tex (8-bit indexed + RGB555+STP palette) The converter is a C++14 tool that runs during CMake build.
>
> Platform-Specific Optimizations
>
> PS1
>
> Use GTE for all 3D transformations
>
> Use Ordering Table for polygon sorting
>
> Use DMA for VRAM uploads
>
> Minimize overdraw (fillrate is bottleneck)
>
> PC
>
> Use OpenGL 3.3 with PS1 emulation shaders
>
> Vertex snapping for "wobbling" effect
>
> Affine texture mapping (no perspective correction)
>
> 320x240 internal resolution with nearest filtering
>
> Performance Targets
>
> PC W1: >= 60 FPS on the recorded reference PC environment
>
> PS1 W1: >= 30 FPS on the recorded DuckStation validation environment (stable emulator validation; not a real-hardware performance guarantee)
>
> Memory budget (PS1 W1): <= 1.5 MiB main RAM, <= 800 KiB VRAM
>
> PS-EXE size target (PS1): < 500 KiB; CD image size is tracked separately
>
> Future Extensions
>
> Saturn (v0.2.0)
>
> Master + Slave SH-2 parallelism
>
> VDP1 (polygons) + VDP2 (backgrounds)
>
> Quad-based rendering
>
> N64 (v0.3.0)
>
> Microcode for RCP (RSP + RDP)
>
> Hardware Z-buffer
>
> Perspective-correct texture mapping

### 2.5. ДОКУМЕНТАЦИЯ ДЛЯ КОНТРИБЬЮТОРОВ

> docs/CONTRIBUTING.md
>
> Contributing to Retro Engine
>
> Thank you for your interest in contributing to Retro Engine! This document will help you get started.
>
> Code of Conduct
>
> This project adheres to the Contributor Covenant Code of Conduct
>
> .
>
> By participating, you are expected to uphold this code.
>
> How to Contribute
>
> Reporting Bugs
>
> Check if the bug is already reported in GitHub Issues
>
> If not, create a new issue with:
>
> Clear title and description
>
> Steps to reproduce
>
> Expected vs actual behavior
>
> Platform (PC/PS1) and version
>
> Screenshots or logs if applicable
>
> Suggesting Features Open a GitHub Issue with the "enhancement" label
>
> Describe the feature and its benefits
>
> Discuss with maintainers before implementing
>
> Submitting Pull Requests
>
> Fork the repository
>
> Create a feature branch: git checkout -b feature/my-feature
>
> Make your changes
>
> Write/update tests
>
> Run tests: scripts/test.bat
>
> Commit with clear messages: git commit -m "Add my feature"
>
> Push to your fork: git push origin feature/my-feature
>
> Open a Pull Request
>
> Code Review Process
>
> CI will automatically run tests
>
> At least 1 maintainer will review your code
>
> Address all comments
>
> Squash merge into develop
>
> Development Setup See BUILDING.md
>
> for detailed instructions.
>
> Quick start: # Clone
>
> git clone <https://github.com/yourname/retro>
>
> Coding Standards
>
> See STYLE_GUIDE.md
>
> for detailed style guide.
>
> Key points:
>
> C++14 standard
>
> 4 spaces indentation (no tabs)
>
> PascalCase for classes, snake_case for functions
>
> Document all public APIs with Doxygen comments
>
> No new/delete in runtime (use allocators)
>
> No malloc/free in runtime
>
> Testing
>
> All code must have unit tests:
>
> // tests/math/fixed_test.cpp
>
> #include <gtest/gtest.h>
>
> #include "core/math/fixed.h"
>
> TEST(FixedPoint, Multiplication) {

```text
fixed_t a = fixed_from_int(5); fixed_t b = fixed_from_int(7);

EXPECT_EQ(fixed_mul(a, b), fixed_from_int(35));

}
```
> Run tests: scripts/test.bat
>
> Documentation
>
> Update Doxygen comments for public APIs
>
> Update tutorials if you change user-facing behavior
>
> Update ARCHITECTURE.md if you change core architecture
>
> Good First Issues
>
> Look for issues labeled "good first issue" --- these are beginner-friendly tasks.
>
> Communication
>
> GitHub Issues: Bug reports, feature requests
>
> Discord: General discussion, help
>
> Twitter: Announcements, updates
>
> License
>
> By contributing, you agree that your contributions will be licensed under the MIT License.

## 3. УПАКОВКА И ДИСТРИБУЦИЯ

### 3.1. СТРУКТУРА РЕЛИЗНОГО АРХИВА

> ЦЕЛЬ: Создать удобный архив для пользователей. СТРУКТУРА АРХИВА: retro-engine-v0.1.0/
>
> README.md # Главная информация
>
> LICENSE # MIT License
>
> CHANGELOG.md # История изменений LEGAL.md # Юридическая информация src/ # Исходный код движка core/ hal/ render/ io/
>
> audio/ assets/
>
> tools/ # Инструменты
>
> asset_converter/ # Host-only source + generated host binaries are not part of the canonical PS1 runtime tree
>
> README.md examples/ # Демо-проекты hello_triangle/ rotating_cube/
>
> README.md docs/ # Документация
>
> ARCHITECTURE.md
>
> BUILDING.md
>
> API.md ASSETS.md
>
> tutorials/ api/ # Doxygen HTML
>
> scripts/ # Скрипты автоматизации
>
> setup_dev.bat build_pc.bat build_ps1.bat run_pc.bat run_ps1.bat test.bat
>
> cmake/ # Дополнительные CMake-модули и скрипты
> CMakePresets.json # Канонические build presets
>
> assets/ # Примеры ассетов
>
> source/ cube.gltf texture.png build/ cube.mesh texture.tex
>
> tests/ # Unit-тесты math_test.cpp ecs_test.cpp memory_test.cpp
>
> CMakeLists.txt # Главный CMake
>
> Doxyfile # Doxygen конфигурация СОЗДАНИЕ АРХИВА:

#!/bin/bash

set -euo pipefail

VERSION="${1:?version required}"

PAYLOAD_DIR="${2:?payload directory required}"

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

ARCHIVE_NAME="retro-engine-${VERSION}.tar.gz"

TEMP_DIR="$(mktemp -d)"

trap 'rm -rf "$TEMP_DIR"' EXIT

mkdir -p "$TEMP_DIR/retro-engine-${VERSION}"

rsync -a "$PAYLOAD_DIR/" "$TEMP_DIR/retro-engine-${VERSION}/"

cd "$TEMP_DIR"

tar -czf "$ROOT_DIR/$ARCHIVE_NAME" "retro-engine-${VERSION}"

echo "Archive created: $ROOT_DIR/$ARCHIVE_NAME"

### 3.2. GITHUB RELEASES

> ЦЕЛЬ: Автоматически создавать релизы на GitHub.
>
> WORKFLOW:

.github/workflows/release.yml is defined canonically in §6.5 above. For MVP, the release payload contains Linux PC binaries and the PS1 bootable .cue/.bin image; Windows/macOS builds remain CI validation artifacts unless explicitly promoted to distribution scope.

## 4. POST-RELEASE SUPPORT

## 4. POST-RELEASE SUPPORT (ПРОДОЛЖЕНИЕ)

> ------------------------------------------------------------------------

### 4.1. СИСТЕМА ТИКЕТОВ (GITHUB ISSUES)

> ------------------------------------------------------------------------
>
> ЦЕЛЬ: Организовать обработку обращений от пользователей через GitHub Issues.
>
> НАСТРОЙКА REPOSITORY:
>
> # .github/ISSUE_TEMPLATE/bug_report.md
>
> ------------------------------------------------------------------------
>
> name: Bug Report about: Create a report to help us improve title: '[BUG] ' labels: 'bug' assignees: ''
>
> ------------------------------------------------------------------------
>
> **Describe the bug**
>
> A clear and concise description of what the bug is.
>
> **To Reproduce**
>
> Steps to reproduce the behavior:

- Build with '...'

- Run example '...'

- Do '...'

- See error

> **Expected behavior**
>
> A clear and concise description of what you expected to happen.
>
> **Screenshots**
>
> If applicable, add screenshots to help explain your problem.
>
> **Environment:**

- OS: [e.g. Windows 11, Ubuntu 22.04]

- Platform: [e.g. PC, PS1 (DuckStation)]

- Version: [e.g. v0.1.0]

- Compiler: [e.g. MSVC 2022, GCC 12]

> **Additional context**
>
> Add any other context about the problem here.
>
> # .github/ISSUE_TEMPLATE/feature_request.md
>
> ------------------------------------------------------------------------
>
> name: Feature Request about: Suggest an idea for this project title: '[FEATURE] ' labels: 'enhancement' assignees: ''
>
> ------------------------------------------------------------------------
>
> **Is your feature request related to a problem? Please describe.** A clear and concise description of what the problem is.
>
> **Describe the solution you'd like**
>
> A clear and concise description of what you want to happen.
>
> **Describe alternatives you've considered**
>
> A clear and concise description of any alternative solutions or features you've considered.
>
> **Additional context**
>
> Add any other context or screenshots about the feature request here.
>
> ЛАБЕЛЫ ДЛЯ ISSUES:

- \`bug\` --- Ошибка в коде

- \`enhancement\` --- Запрос новой функции

- \`documentation\` --- Проблема с документацией

- \`question\` --- Вопрос от пользователя

- \`good first issue\` --- Задача для новичков

- \`help wanted\` --- Нужна помощь сообщества

- \`priority:high\` --- Высокий приоритет

- \`priority:medium\` --- Средний приоритет

- \`priority:low\` --- Низкий приоритет

- \`platform:pc\` --- Относится к PC

- \`platform:ps1\` --- Относится к PS1

- \`status:investigating\` --- В процессе изучения

- \`status:in-progress\` --- В работе

- \`status:needs-info\` --- Нужна дополнительная информация

- \`status:wontfix\` --- Не будет исправленоWORKFLOW ОБРАБОТКИ ISSUES:

- Новый issue создан пользователем

- Автоматическая проверка (GitHub Actions)

- Заполнен ли шаблон?

- Указана ли версия?

- Приложены ли скриншоты?

- Мейнтейнер (ты) просматривает issue в течение 48 часов

- Классификация

- Назначить лейблы

- Назначить приоритет

- Назначить milestone (v0.1.1, v0.2.0, и т.д.)

- Действие

- Если баг: воспроизвести, исправить, закрыть

- Если вопрос: ответить, закрыть

- Если фича: обсудить, добавить в roadmap или закрыть

- Уведомление пользователя о решении

> АВТОМАТИЗАЦИЯ:
>
> # .github/workflows/issue-triage.yml name: Issue Triage on: issues: types: [opened]
>
> jobs:
>
> triage:
>
> runs-on: ubuntu-latest
>
> steps:
>
> - name: Check issue template uses: actions/github-script@v6 with:
>
> script: \|

```text
const body = context.payload.issue.body; if (!body.includes('**Describe the bug**') &&

!body.includes('**Is your feature request**')) {

await github.rest.issues.addLabels({

owner: context.repo.owner, repo: context.repo.repo,

issue_number: context.issue.number, labels: ['status:needs-info']

});

await github.rest.issues.createComment({

owner: context.repo.owner, repo: context.repo.repo,

issue_number: context.issue.number,

body: 'Please fill out the issue template completely.'

});

}
```
>
> ------------------------------------------------------------------------

### 4.2. КЛАССИФИКАЦИЯ БАГОВ

> ------------------------------------------------------------------------
>
> СЕРЬЁЗНОСТЬ (SEVERITY):

- **Blocker** --- Движок не компилируется или крашится при запуске

- Время реакции: 24 часа

- Пример: Segfault при инициализации GPU на PS1

- **Critical** --- Критическая функциональность не работает

- Время реакции: 1 неделя

- Пример: Текстуры не отображаются, ECS ломает сущности

- **Major** --- Важная функциональность работает некорректно

- Время реакции: 2 недели

- Пример: Неправильная сортировка полигонов, утечка памяти

- **Minor** --- Незначительная проблема

- Время реакции: 1 месяц

- Пример: Неправильное выравнивание текста в логе

- **Trivial** --- Косметическая проблема

- Время реакции: по возможности

- Пример: Опечатка в документацииПРИОРИТЕТ (PRIORITY):

- **P0 (Critical)** --- Блокирует релиз или работу пользователей

- **P1 (High)** --- Важный баг, но есть workaround

- **P2 (Medium)** --- Обычный баг, влияет на UX

- **P3 (Low)** --- Незначительный баг, можно отложить

> МАТРИЦА ПРИОРИТЕТОВ:

 Blocker Critical Major Minor Trivial
 --------------- ---------- ------- ------- --------- ----
 High Impact P0 P1 P1 P2 P3
 Medium Impact P1 P2 P2 P3 P3
 Low Impact P2 P3 P3 P3 P3

> ПРИМЕР КЛАССИФИКАЦИИ:
>
> Issue: "Rotating cube crashes after 5 minutes on PS1"

- Severity: Critical (краш)

- Impact: High (демо-пример, первый опыт пользователей)

- Priority: P1

> Issue: "Logger prints wrong timestamp format"

- Severity: Minor (не влияет на функциональность)

- Impact: Low (только отладка)

- Priority: P3

> ------------------------------------------------------------------------

### 4.3. ВЫПУСК ПАТЧЕЙ

> ------------------------------------------------------------------------
>
> СТРАТЕГИЯ ВЫПУСКА ПАТЧЕЙ:

- **Hotfix** --- Срочное исправление критического бага

- Ветка: \`hotfix/issue-123\` от \`main\`

- Время: 1-3 дня

- Релиз: v0.1.1 (PATCH)

- **Regular Patch** --- Плановое исправление багов

- Ветка: \`fix/issue-456\` от \`develop\`

- Время: 1-2 недели

- Релиз: v0.1.2 (PATCH)

- **Minor Release** --- Новые фичи + исправления

- Ветка: \`feature/new-feature\` от \`develop\`

- Время: 1-3 месяца

- Релиз: v0.2.0 (MINOR) WORKFLOW HOTFIX:

> \`\`\`bash
>
> # 1. Создать ветку hotfix от main
>
> git checkout main git checkout -b hotfix/issue-123 # 2. Исправить баг
>
> # ... код ...
>
> # 3. Написать тест для бага
>
> # ... тест ...
>
> # 4. Запустить все тесты scripts/test.bat
>
> # 5. Обновить CHANGELOG
>
> # Добавить запись в [Unreleased] # 6. Закоммитить git add .
>
> git commit -m "Fix: crash on PS1 after 5 minutes (#123)"
>
> # 7. Создать Pull Request в main git push origin hotfix/issue-123 # Открыть PR на GitHub # 8. После merge, создать тег git checkout main git pull origin main git tag -a v0.1.1 -m "Hotfix v0.1.1: Fix PS1 crash" git push origin main --tags # 9. Синхронизировать develop с main git checkout develop git merge main git push origin develop АВТОМАТИЧЕСКИЙ ПАТЧ через CI/CD:
>
> .github/workflows/hotfix.yml name: Hotfix Release on:
>
> push:
>
> tags:

- 'v*'
 jobs:

> hotfix: runs-on: ubuntu-latest steps:

- uses: actions/checkout@v4

- name: Build PCrun: \|

> cmake --preset pc-debug
>
> cmake --build --preset pc-debug

- name: Run tests
 run: ctest --preset pc-debug --output-on-failure

- name: Create releaseuses: softprops/action-gh-release@v2 with:

> generate_release_notes: true

### 4.4. ОБНОВЛЕНИЕ TOOLCHAIN

> ЦЕЛЬ: Поддерживать актуальность кросс-компиляторов и SDK.
>
> МОНИТОРИНГ ОБНОВЛЕНИЙ:
>
> PSn00bSDK:
>
> Подписаться на релизы: <https://github.com/Lameguy64/PSn00bSDK/releases> Проверять обновления раз в месяц SDL3:
>
> Подписаться на релизы: <https://github.com/libsdl-org/SDL/releases> Проверять обновления раз в месяц Кросс-компилятор mipsel-none-elf-gcc:
>
> Проверять обновления раз в 3 месяца
>
> Тестировать совместимость перед обновлением WORKFLOW ОБНОВЛЕНИЯ:
>
> # 1. Создать ветку: git checkout develop && git checkout -b update/psn00bsdk-v0.25
> # 2. Обновить pinned release URL и SHA-256 checksum для PSn00bSDK v0.25 в CI/docs; не менять target через произвольный sysroot.
> # 3. Проверить наличие sdk.cmake, собрать pc-debug/pc-release и ps1-release, затем прогнать ctest.
> # 4. Обновить LEGAL.md, BUILDING.md, CHANGELOG.md и ADR только после успешной проверки.
> # 5. Создать PR и сохранить предыдущий pin как rollback reference.
>
> # 3. Закоммитить обновление git add src/third_party/psn00bsdk git commit -m "Update PSn00bSDK to v0.25" # 4. Собрать и протестировать scripts/build_pc.bat scripts/test.bat scripts/build_ps1.bat scripts\\run_ps1.bat # 5. Если всё работает --- создать PR git push origin update/psn00bsdk-v0.25
>
> # Открыть PR на GitHub
>
> # 6. После merge, обновить документацию
>
> # Обновить BUILDING.md с новыми требованиями ПРОБЛЕМЫ СОВМЕСТИМОСТИ:
>
> Если новая версия SDK ломает API:
>
> Создать адаптационный слой:
>
> // hal/ps1/compat.h
>
> // Не привязывать engine API к именам конкретной версии SDK. Использовать отдельный adapter layer и проверку минимальной версии SDK в CMake/CI.
>
> // Новый API
>
> // SDK-specific symbol mapping belongs in the adapter implementation, not in the public engine HAL contract.
>
> #else
>
> // Старый API
>
> SDK API compatibility note: do not invent wrapper macros for GPU initialization. Bind the selected PSn00bSDK API only inside the version-pinned PS1 adapter and compile against the pinned headers.
>
> #endif
>
> Обновить документацию (BUILDING.md) с минимальной версией SDK
>
> Добавить в CHANGELOG.md запись об изменении

### 4.5. ОБРАТНАЯ СВЯЗЬ ОТ СООБЩЕСТВА

> КАНАЛЫ ОБРАТНОЙ СВЯЗИ:
>
> GitHub Issues --- Баги, фичи, вопросы
>
> GitHub Discussions --- Обсуждения, идеи, show-and-tell
>
> Discord --- Быстрая помощь, общение
>
> Twitter/X --- Анонсы, обновления
>
> Reddit --- Посты в r/ps1, r/homebrew, r/gamedev
>
> Email --- Для приватных вопросов (если указан) НАСТРОЙКА GITHUB DISCUSSIONS:
>
> Категории:
>
> Announcements --- Только для мейнтейнера (релизы, важные новости)
>
> General --- Общие обсуждения
>
> Ideas --- Предложения новых фич
>
> Show and Tell --- Пользователи показывают свои проекты
>
> Q&A --- Вопросы и ответы
>
> АВТОМАТИЧЕСКИЕ УВЕДОМЛЕНИЯ:
>
> .github/workflows/stale.yml name: Close stale issues on:
>
> schedule: cron: '0 0 * * *' # Каждый день
>
> jobs:
>
> stale: runs-on: ubuntu-latest steps:
>
> - uses: actions/stale@v8 with:
>
> days-before-stale: 30 days-before-close: 7 stale-issue-message: >
>
> This issue has been automatically marked as stale because it has not had recent activity. It will be closed if no further activity occurs.
>
> stale-pr-message: >
>
> This PR has been automatically marked as stale because it has not had recent activity. It will be closed if no further activity occurs.
>
> WORKFLOW ОБРАБОТКИ ОБРАТНОЙ СВЯЗИ:
>
> Пользователь пишет в Discord: "Не могу собрать для PS1"
>
> Помощь в Discord (быстрый ответ)
>
> Если это баг --- попросить создать GitHub Issue
>
> Если это фича --- добавить в Ideas Discussion
>
> Раз в неделю: просмотреть все Issues и Discussions
>
> Обновить roadmap на основе обратной связи

## 5. МАРКЕТИНГ И ПРОДВИЖЕНИЕ

### 5.1. ANNOUNCEMENT POST

> ЦЕЛЬ: Анонсировать релиз v0.1.0 и привлечь первых пользователей.
>
> ШАБЛОН ANNOUNCEMENT POST:
>
> Announcement: Retro Engine v0.1.0 --- Open-Source 3D Engine for PS1 and PC Hi everyone!
>
> I'm excited to announce the first public release of Retro Engine, an open-source 3D game engine designed for retro consoles (starting with PS1) and PC.
>
> What is Retro Engine?
>
> Retro Engine is a cross-platform game engine that lets you write game logic once in C++ and deploy it to both PC and PlayStation 1. It features:
>
> Entity Component System (ECS) for efficient game logic
>
> Fixed-point math optimized for consoles without FPU
>
> Authentic PS1 graphics (affine texture mapping, vertex wobbling)
>
> Asset pipeline with automatic conversion from glTF/PNG
>
> Complete documentation and tutorials
>
> Why I Built This
>
> I wanted to create a modern, well-documented engine for the homebrew community.
>
> Most existing tools are either outdated or poorly documented. Retro Engine aims to change that by providing:
>
> Clean, maintainable C++14 code
>
> Comprehensive documentation (API reference, tutorials)
>
> Automated build system (CMake + CI/CD)
>
> Active community support
>
> Getting Started
>
> Download from GitHub Releases
>
> Follow the Getting Started Tutorial
>
> Check out the API Documentation
>
> Demo
>
> Here's a video of the rotating cube demo running on both PC and PS1 (emulated):

## YouTube Video Link

> What's Next v0.2.0: Sega Saturn support v0.3.0: Nintendo 64 support v0.6.0: Real audio support (XA, PCM, ADPCM)
>
> Contributing
>
> Retro Engine is open-source (MIT license) and welcomes contributions! Check out the Contributing Guide to get started.
>
> Links
>
> GitHub: <https://github.com/yourname/retro-engine>
>
> Documentation: <https://yourname.github.io/retro-engine/>
>
> Discord: <https://discord.gg/retro-engine>
>
> Twitter: \@yourname
>
> Thanks to the homebrew community for inspiration and support!
>
> ГДЕ ОПУБЛИКОВАТЬ:
>
> Reddit:
>
> r/ps1 (2.5M members) r/homebrew (500K members) r/gamedev (3M members) r/opensource (1M members) r/cpp (800K members) Hacker News:
>
> <https://news.ycombinator.com/>
>
> Заголовок: "Retro Engine -- Open-source 3D engine for PS1 and PC" Twitter/X:
>
> Пост с видео демки
>
> Хэштеги: #gamedev #homebrew #ps1 #opensource #cpp
>
> Discord серверы:
>
> PS1 Homebrew Discord
>
> Retro Development Discord
>
> C++ Discord Форумы:
>
> <https://www.psxdev.net/> <https://gamedev.net/> <https://www.reddit.com/r/ps1/>

### 5.2. СОЦИАЛЬНЫЕ СЕТИ

> TWITTER/X СТРАТЕГИЯ: Частота постов: 2-3 раза в неделю Типы контента:
>
> Progress updates (40%) --- скриншоты, видео работы движка
>
> Technical deep-dives (30%) --- объяснение сложных моментов
>
> Community highlights (20%) --- проекты пользователей
>
> Announcements (10%) --- релизы, важные новости ПРИМЕРЫ ТВИТОВ:
>
> Твит 1 (Progress update):
>
> "Just got affine texture mapping working on PS1! The characteristic 'wobbling' is so nostalgic. #gamedev #ps1 #homebrew
>
> [Video of rotating cube]"
>
> Твит 2 (Technical deep-dive):
>
> "Fixed-point math tip: When multiplying two 16.16 numbers, use 64-bit intermediate to avoid overflow. On PS1, MIPS integer multiply provides a 64-bit intermediate; GTE has separate MAC accumulators. #cpp #retrodev
>
> [Code snippet screenshot]"
>
> Твит 3 (Community highlight):
>
> "Amazing! \@user just released their first PS1 game built with Retro Engine!
>
> Check it out: [link] #homebrew #ps1" DISCORD СЕРВЕР:
>
> Структура сервера:
>
> #announcements --- Только для мейнтейнера
>
> #general --- Общие обсуждения
>
> #help --- Помощь с проблемами
>
> #show-and-tell --- Пользователи показывают свои проекты
>
> #development --- Обсуждение разработки движка
>
> #off-topic --- Не по теме Боты:
>
> MEE6 --- Модерация, приветствие новых участников
>
> Carl-bot --- Роли, логирование
>
> GitHub Bot --- Уведомления о новых issues/PR

### 5.3. ДЕMONSTRАЦИОННЫЕ ВИДЕО

> ТИПЫ ВИДЕО:
>
> Release Trailer (1-2 минуты)
>
> Что такое Retro Engine
>
> Демонстрация возможностей
>
> Как начать использовать
>
> Ссылки на GitHub и документацию
>
> Tutorial Videos (5-10 минут)
>
> Пошаговые туториалы
>
> Загрузка моделей из Blender
>
> Создание первой игры
>
> Technical Deep-Dives (10-20 минут)
>
> Как работает GTE на PS1
>
> Fixed-point math объяснение
>
> Оптимизация для ретро-железа
>
> Devlogs (5-15 минут)
>
> Процесс разработки
>
> Проблемы и решения
>
> Планы на будущее ПЛАТФОРМЫ:
>
> YouTube --- Основная платформа
>
> Плейлист: "Retro Engine Tutorials"
>
> Плейлист: "Retro Engine Devlogs"
>
> Twitter/X --- Короткие клипы (30-60 секунд)
>
> Reddit --- GIF и короткие видео
>
> Discord --- Эксклюзивный контент для участников ОБОРУДОВАНИЕ ДЛЯ ВИДЕО:
>
> Запись экрана: OBS Studio (бесплатно)
>
> Монтаж: DaVinci Resolve (бесплатно) или Shotcut
>
> Микрофон: Любой USB-микрофон ($50-100)
>
> Веб-камера: Опционально (для devlogs)

### 5.4. РАБОТА С СООБЩЕСТВОМ

> СТРАТЕГИЯ ПОСТРОЕНИЯ СООБЩЕСТВА:
>
> Будь активен:
>
> Отвечай на вопросы в течение 24 часов
>
> Участвуй в обсуждениях Показывай, что ты заинтересован Будь прозрачен:
>
> Публикуй roadmap
>
> Объясняй решения
>
> Признавай ошибки
>
> Поощряй контрибьюторов:
>
> Благодарность в CHANGELOG
>
> "Contributor of the Month" в Discord Мерч (стикеры, футболки) --- когда появится бюджет Создай культуру:
>
> Code of Conduct
>
> Доброжелательность
>
> Помощь новичкам
>
> АКТИВНОСТИ ДЛЯ СООБЩЕСТВА:
>
> Game Jams:
>
> "Retro Engine Game Jam" --- раз в 6 месяцев
>
> Тема: ретро-игры на PS1 Призы: мерч, упоминание Showcase Events:
>
> "Retro Engine Showcase" --- раз в 3 месяца
>
> Пользователи показывают свои проекты
>
> Голосование за лучший проект Contributor Spotlights:
>
> Интервью с контрибьюторами
>
> Публикация в блоге/Twitter МЕТРИКИ СООБЩЕСТВА:
>
> Количество участников Discord
>
> Количество stars на GitHub
>
> Количество forks
>
> Количество pull requests
>
> Количество игр, сделанных на движке
>
> Активность в Issues/Discussions

## 6. МЕТРИКИ УСПЕХА ПОСЛЕ РЕЛИЗА

> ТЕХНИЧЕСКИЕ МЕТРИКИ:
>
> Стабильность:
>
> Цель: 0 критических багов в релизе Измерение: количество blocker/critical issues Производительность:
>
> Цель: W1: не менее 60 FPS на зафиксированной reference PC environment; не менее 30 FPS на зафиксированной PS1 validation environment Измерение: benchmarks Покрытие тестами:
>
> Цель: > 70% для core/ Измерение: Codecov Размер бинарника:
>
> Цель: < 500 KB для PS1
>
> Измерение: автоматическая проверка в CI
>
> МЕТРИКИ СООБЩЕСТВА (через 1 месяц после релиза): GitHub:
>
> 100+ stars
>
> 20+ forks
>
> 10+ issues (от пользователей) 5+ pull requests (от контрибьюторов) Discord:
>
> 50+ участников
>
> 10+ активных пользователей (ежедневно) Документация:
>
> 1000+ просмотров API docs 100+ просмотров туториалов Контент:
>
> 3+ игры, сделанные на движке
>
> 5+ туториалов от сообщества
>
> 10+ постов в Reddit/Twitter
>
> МЕТРИКИ ПОДДЕРЖКИ:
>
> Время реакции:
>
> Цель: < 48 часов на новый issue Измерение: GitHub metrics Resolution rate:
>
> Цель: > 80% issues решены в течение 1 месяца
>
> Измерение: GitHub metrics User satisfaction:
>
> Цель: > 4/5 средняя оценка Измерение: опросы в Discord

## 7. ПЛАН ПОДДЕРЖКИ

### 7.1. КРАТКОСРОЧНЫЙ ПЛАН (1-3 МЕСЯЦА ПОСЛЕ РЕЛИЗА)

> НЕДЕЛЯ 1-2:
>
> Ответить на все первые issues
>
> Исправить критические баги (если есть)
>
> Обновить документацию на основе обратной связи
>
> Активно продвигать в Reddit, Twitter, Discord НЕДЕЛЯ 3-4:
>
> Выпустить патч v0.1.1 (если нужны баг-фиксы)
>
> Написать 2-3 дополнительных туториала
>
> Провести первый "Showcase" в Discord
>
> Собрать обратную связь для roadmap МЕСЯЦ 2:
>
> Начать работу над v0.2.0 (Sega Saturn)
>
> Добавить "good first issue" для контрибьюторов
>
> Провести первый Game Jam
>
> Обновить roadmap на основе обратной связи МЕСЯЦ 3:
>
> Релиз v0.2.0 (если готов)
>
> Интервью с активными контрибьюторами
>
> Анализ метрик сообщества
>
> Планирование v0.3.0

### 7.2. СРЕДНЕСРОЧНЫЙ ПЛАН (3-12 МЕСЯЦЕВ)

> МЕСЯЦ 4-6:
>
> Поддержка Saturn (v0.2.0)
>
> Добавление реального аудио (XA, PCM)
>
> Улучшение документации
>
> Построение ядра контрибьюторов (5-10 активных) МЕСЯЦ 7-9:
>
> Поддержка N64 (v0.3.0)
>
> Добавление ISO9660/CDFS драйверов
>
> Оптимизация производительности
>
> Проведение второго Game Jam МЕСЯЦ 10-12:
>
> Поддержка Jaguar/3DO (v0.4.0 - v0.5.0)
>
> Полноценная файловая система
>
> Создание редактора уровней (прототип)
>
> Анализ первого года проекта

### 7.3. ДОЛГОСРОЧНЫЙ ПЛАН (1-2 ГОДА)

> ГОД 2:
>
> Стабилизация всех платформ (v0.6.0 - v0.8.0)
>
> Полноценный редактор уровней (v0.9.0)
>
> Скриптовый язык (Lua) (v1.0.0)
>
> Первая коммерческая игра на движке
>
> 1000+ stars на GitHub
>
> 50+ активных контрибьюторов
>
> 10+ игр, сделанных на движке
>
> ЦЕЛИ НА 2 ГОДА:
>
> Стать стандартом для homebrew-разработки на ретро-консолях
>
> Построить устойчивое сообщество (1000+ участников Discord)
>
> Получить признание в gamedev-сообществе
>
> Возможно, получить финансирование (GitHub Sponsors, Patreon)

## 8. РИСКИ И МИТИГАЦИЯ

## РИСК 1: Нет контрибьюторов после релиза

> Вероятность: СРЕДНЯЯ Влияние: СРЕДНЕЕ Митигация:

- Активно продвигать проект

- Создавать "good first issue"

- Отвечать на вопросы быстро

- Проводить Game Jams

## РИСК 2: Критические баги после релиза

> Вероятность: СРЕДНЯЯ Влияние: ВЫСОКОЕ Митигация:

- Иметь hotfix workflow

- Выпускать патчи быстро

- Коммуницировать прозрачно

## РИСК 3: Выгорание от поддержки

> Вероятность: ВЫСОКАЯ Влияние: КРИТИЧЕСКОЕ Митигация:

- Установить границы (не работать 24/7)

- Делегировать контрибьюторам

- Делать перерывы

- Помнить, что это open-source (не обязаловка)

## РИСК 4: Токсичное сообщество

> Вероятность: НИЗКАЯ Влияние: СРЕДНЕЕ Митигация:

- Code of Conduct

- Модерация Discord

- Бан токсичных пользователей

## РИСК 5: Форк проекта

> Вероятность: СРЕДНЯЯ
>
> Влияние: НИЗКОЕ (MIT license позволяет) Митигация:

- Принимать это как часть open-source

- Сотрудничать с форками

- Фокусироваться на своём roadmap

## РИСК 6: Конкуренты (другие движки)

> Вероятность: СРЕДНЯЯ Влияние: СРЕДНЕЕ Митигация:

- Фокусироваться на уникальности (кроссплатформенность ретро)

- Улучшать документацию

- Строить сообщество

## 9. КРИТЕРИИ ЗАВЕРШЕНИЯ ЭТАПА

> ЭТАП 8 считается завершённым, когда выполнены ВСЕ следующие условия:

## ДОКУМЕНТАЦИЯ

> API Reference сгенерирован (Doxygen)
>
> 10+ туториалов написаны
>
> Архитектурная документация полная
>
> Документация для контрибьюторов написана
>
> Все примеры имеют README

### УПАКОВКА

> Релизный архив создан
>
> GitHub Release опубликован
>
> CHANGELOG.md обновлён
>
> Лицензия и LEGAL.md в порядке

### ПОДДЕРЖКА

> GitHub Issues настроены (шаблоны, лейблы)
>
> Discord сервер создан
>
> Workflow обработки issues определён
>
> План поддержки на 3 месяца составлен

### МАРКЕТИНГ

> Announcement post опубликован (Reddit, Twitter, HN)
>
> Демо-видео создано и опубликовано
>
> Социальные сети настроены
>
> Первые 100 stars на GitHub

#### МЕТРИКИ (через 1 месяц)

> 100+ stars на GitHub
>
> 50+ участников Discord
>
> 10+ issues от пользователей
>
> 5+ pull requests от контрибьюторов
>
> 3+ игры, сделанные на движке (или обещания)

## ИТОГИ ЭТАПА 8

> ЧТО МЫ ПОЛУЧИЛИ ПОСЛЕ ЭТАПА 8:
>
> ПОЛНАЯ ДОКУМЕНТАЦИЯ:
>
> API Reference (Doxygen)
>
> 10+ туториалов
>
> Архитектурная документация
>
> Contributing guide
>
> ПУБЛИЧНЫЙ РЕЛИЗ:
>
> GitHub Release v0.1.0
>
> Релизный архив
>
> Announcement post
>
> Демо-видео
>
> СООБЩЕСТВО:
>
> Discord сервер
>
> GitHub Issues/Discussions
>
> Первые пользователи Первые контрибьюторы ПЛАН ПОДДЕРЖКИ:
>
> Workflow обработки issues
>
> План патчей
>
> Roadmap на 2 года
>
> Метрики успеха
>
> ЧТО МЫ БУДЕМ ДЕЛАТЬ ДАЛЬШЕ:
>
> Поддержка пользователей (issues, Discord)
>
> Выпуск патчей (v0.1.1, v0.1.2)
>
> Работа над v0.2.0 (Sega Saturn)
>
> Построение сообщества Проведение Game Jams

# REVISION SOURCES --- ТЕХНИЧЕСКАЯ ПРОВЕРКА

[S7] SDL3 Migration Guide --- official SDL documentation: SDL3 renamed SDL_GameController API to SDL_Gamepad and removed SDL_INIT_TIMER. <https://wiki.libsdl.org/SDL3/README-migration>
[S8] 3DO Development Repo --- hardware/memory documentation (non-normative future-platform reference): <https://3dodev.com/documentation/hardware/opera/memory_configurations>

[S1] PSX-SPX: PlayStation hardware specification and GTE/GPU documentation --- <https://psx-spx.consoledev.net/psx-spx.pdf>

[S2] PSX-SPX GPU / depth ordering --- <https://psx-spx.consoledev.net/graphicsprocessingunitgpu/>

[S3] PSn00bSDK --- v0.24 release and current repository/build documentation: <https://github.com/Lameguy64/PSn00bSDK/releases> ; <https://github.com/Lameguy64/PSn00bSDK>

[S4] CMake add_custom_command / OBJECT-library behavior --- <https://cmake.org/cmake/help/latest/command/add_custom_command.html>

[S5] Nintendo 64 technical details --- <https://www.nintendo.com/en-gb/Hardware/Nintendo-History/Nintendo-64/Technical-Details/Technical-Details-627050.html>

[S6] Atari Jaguar technical FAQ (community-maintained historical reference; non-normative for MVP): <https://www.atariage.com/Jaguar/faq/index.html?SystemID=JAGUAR> The reference explicitly documents Blitter hardware support for Z-buffering and Gouraud shading.

12.0. MVP CONFORMANCE MATRIX

The following matrix is normative for MVP implementation: PC and PS1 must consume the same engine-space asset data; only backend conversion, submission and hardware-specific state differ.

12.1. CANONICAL ENGINE CONTRACT (REVISION 1.3)

1. Engine scalar: fixed_t is signed 32-bit 16.16. This is the engine representation, not a hardware register format.

fixed_sqrt contract: negative input is invalid; debug builds assert, release builds return 0. The implementation uses integer/fixed-point arithmetic and has deterministic bounded iteration.

2. Overflow contract: fixed_add/sub/mul/div are saturating and must not depend on signed-overflow behavior. Division by zero returns a defined saturation value in release and asserts in debug.

3. Transform contract: engine Mat4 is never passed to platform hardware by raw cast. Each backend converts Mat4 into its native representation.

4. Asset contract: .mesh stores engine-space vertex data; platform GPU packet formats are generated only inside the backend.

Registry implementation requirement: component pools are selected by compile-time/static type IDs; no unordered_map, RTTI or virtual component base is allowed in the runtime ECS.

6. Renderer contract: Painter's Algorithm is the MVP depth strategy. It is valid for ordered opaque geometry but does not solve intersecting/cyclic surfaces; the reference scene must avoid those cases.

7. Depth contract: OT bucket mapping is bounded and monotonic. No bitmask/modulo mapping of arbitrary depth values is permitted.

8. PC contract: OpenGL 3.3 emulates selected PS1 visual behavior; it is a reference backend, not a hardware oracle.

9. Memory contract: 2 MiB PS1 main RAM is a hard physical limit. MVP acceptance ceiling is <= 1.5 MiB peak main RAM, leaving a separate 512 KiB physical reserve outside the engine budget.

10. Build contract: PS1 builds use the pinned PSn00bSDK CMake toolchain and target mipsel-none-elf; project CMake must not invent an incompatible compiler/sysroot contract.

REVISION 1.28 --- EDIT GATE
Historical traceability only. Superseded by v1.31. Earlier examples may be retained only where they match the current canonical contracts; conflicting legacy examples are non-normative and must not be copied into the repository.

13. ТЕХНИЧЕСКИЕ ИСТОЧНИКИ (ПРОВЕРКА СПЕЦИФИКАЦИЙ)

[S8] PSn00bSDK license terms by component: <https://github.com/Lameguy64/PSn00bSDK/blob/master/LICENSE.md>

[S7] Motorola M68000 Programmer's Reference Manual: <https://www.nxp.com/docs/en/reference-manual/M68000PRM.pdf>

[S6] Nintendo 64 technical details: <https://www.nintendo.com/en-gb/Hardware/Nintendo-History/Nintendo-64/Technical-Details/Technical-Details-627050.html>

[S5] SDL3 --- SDL_GL_CreateContext: <https://wiki.libsdl.org/SDL3/SDL_GL_CreateContext>

[S4] SDL3 --- SDL_GL_SetAttribute: <https://wiki.libsdl.org/SDL3/SDL_GL_SetAttribute>

[S3] PSn00bSDK --- current repository, releases and CMake reference: <https://github.com/Lameguy64/PSn00bSDK>

[S2] PSX-SPX --- Graphics Processing Unit / VRAM / DMA: <https://psx-spx.consoledev.net/graphicsprocessingunitgpu/>

[S1] PSX-SPX --- Geometry Transformation Engine (GTE): <https://psx-spx.consoledev.net/geometrytransformationenginegte/>

# REFERENCE SOURCES FOR HARDWARE AND BUILD CONTRACTS

PlayStation: <https://www.copetti.org/writings/consoles/playstation/> ; <https://psx-spx.consoledev.net/geometrytransformationenginegte/> ; <https://psx-spx.consoledev.net/graphicsprocessingunitgpu/> ; <https://psx-spx.consoledev.net/cdromfileformats/> ; <https://psx-spx.consoledev.net/soundprocessingunitspu/>

Sega Saturn: <https://www.copetti.org/writings/consoles/sega-saturn/>

Nintendo 64: <https://www.copetti.org/writings/consoles/nintendo-64/>

Atari Jaguar developer reference: <https://asterick.github.io/jag-docs/tom/object-processor.html>

3DO hardware documentation: <https://3dodev.com/documentation/hardware> ; <https://3dodev.com/documentation/hardware/opera/madam/matrix_engine> ; <https://3dodev.com/documentation/hardware/opera/240p_mode>

PSn00bSDK: <https://github.com/Lameguy64/PSn00bSDK> ; <https://github.com/Lameguy64/PSn00bSDK/blob/master/doc/cmake_reference.md>

SDL3 OpenGL API: <https://wiki.libsdl.org/SDL3/SDL_GL_SetAttribute> ; <https://wiki.libsdl.org/SDL3/SDL_GL_CreateContext> ; <https://wiki.libsdl.org/SDL3/SDL_CreateWindow>

# REVISION 1.43 --- EXECUTABLE CONTRACT CONSOLIDATION

Converted critical low-level invariants into explicit tests/validators; corrected allocator alignment and foreign-pointer validation; aligned Vec3 reductions with the wider-accumulator contract; replaced incorrect RGB555+STP test packing; added machine-readable normative manifest and executable release-gate requirements.
