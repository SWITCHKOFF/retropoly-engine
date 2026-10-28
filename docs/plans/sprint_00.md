# Спринт 0 — Этап 0: подготовка среды и инфраструктуры (неделя 0)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§3).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-10-28, Linux-среда):** §3.3–3.7 выполнены (кроме шагов, требующих Windows-среды из §3.1–3.2 и GitHub remote — они помечены в тексте). Гейт сборки/ctest проверен здесь: GCC 15.2, CMake 4.4.4, Ninja 1.13.2.

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринт | 0 (подготовительный) |
| Недели | 0 — 02.11.2026 – 08.11.2026 (вне 40-недельного окна MVP) |
| Веха | M0 — Инфраструктура |
| Цель | Рабочая среда, pinned PS1-тулчейн, репозиторий, канонические CMake-пресеты, вендоринг зависимостей, зелёный CI на пустом каркасе |
| Входные критерии | Нет (старт проекта) |
| На критическом пути | Да (M0 → M1) |
| Выходные артефакты | Репозиторий `retro-engine`, каркас каталогов, `CMakePresets.json`, submodules, зелёный `.github/workflows/tests.yml` |
| Следующий спринт | [`sprint_01_02.md`](sprint_01_02.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Вывод |
|---|---|---|---|---|---|---|
| 12 | 18 | 30 | 19.0 | 3.0 | 25.0 | При 10–15 ч/нед может не уложиться в одну неделю — допускается растяжение на вторую «нулевую» неделю без сдвига окна MVP |

## Задачи

> Нумерация блоков соответствует §3 родительского плана.

### 3.1. Базовые инструменты Windows 11

- [ ] **Git for Windows** (2.40+)
  - [ ] Скачать с git-scm.com/download/win
  - [ ] Опция установки: «Checkout as-is, commit Unix-style line endings» (критично для CMake/CI)
  - [ ] Проверка: `git --version` в Git Bash
- [ ] **CMake** (3.25+)
  - [ ] Скачать с cmake.org/download
  - [ ] Включить «Add CMake to system PATH»
  - [ ] Проверка: `cmake --version` ≥ 3.25
- [ ] **Ninja** (1.11+)
  - [ ] Скачать `ninja.exe` с github.com/ninja-build/ninja/releases
  - [ ] Положить в `C:\Tools\ninja\`, добавить в PATH
  - [ ] Проверка: `ninja --version`
- [ ] **Visual Studio 2022 Build Tools**
  - [ ] Установить workload «Разработка классических приложений на C++»
  - [ ] Компоненты: MSVC v143 + Windows 11 SDK (10.0.22621.0+)
  - [ ] Проверка: открыть «Developer Command Prompt», `cl /?` работает
- [ ] **MSYS2** → `C:\msys64` (host shell, НЕ источник PS1-компилятора)
  - [ ] Установить с msys2.org
  - [ ] `pacman -Syu`, перезапуск терминала при необходимости
  - [ ] `pacman -S base-devel git`
- [ ] **Cursor IDE**
  - [ ] Установить с cursor.sh
  - [ ] Расширения: C/C++ (Microsoft), CMake Tools, clangd, GitLens, Error Lens, CodeLLDB, C/C++ Extension Pack
- [ ] **Эмуляторы**
  - [ ] DuckStation (последний preview) → `C:\emulators\DuckStation\`
  - [ ] PCSX-Redux (для GDB-отладки PS1-кода)

### 3.2. PS1-тулчейн (pinned PSn00bSDK v0.24)

- [ ] Скачать с github.com/Lameguy64/PSn00bSDK/releases архив `psn00bsdk-toolchain-windows-x64` для **v0.24** (release commit `06e65be`)
- [ ] Распаковать **целиком** в `C:\PSn00bSDK\` (структура: `bin\`, `lib\libpsn00b\`, `include\`)
- [ ] Переменные среды:
  - [ ] `PSN00BSDK_ROOT=C:\PSn00bSDK`
  - [ ] `PSN00BSDK_LIBS=%PSN00BSDK_ROOT%\lib\libpsn00b`
  - [ ] `PATH += C:\PSn00bSDK\bin`
- [ ] Проверки:
  - [ ] `mipsel-none-elf-gcc --version` выполняется
  - [ ] `%PSN00BSDK_LIBS%\cmake\sdk.cmake` существует
  - [ ] Смок-тест: скомпилировать `test.c` (`int main(){return 42;}`) компилятором из пакета; затем проверить сборку минимального target через `cmake --preset ps1-release` канонического пресета (после создания репозитория)
- [ ] **Правило:** обновление SDK = отдельная задача с ADR; локальные pull/update после pin запрещены.

### 3.3. OpenGL-загрузчик (glad)

- [x] Открыть gen.glad.sh: API=OpenGL, Profile=Core, Version=3.3, Generator=C/C++, Extensions=все *(выполнено официальным генератором glad2 CLI: `--api gl:core=3.3`, extensions=все, `--loader` — для Спринтов 7–8)*
- [x] Generate → распаковать в `src/third_party/glad/` (`include/glad/gl.h`, `include/KHR/khrplatform.h`, `src/gl.c`)
- [x] Проверить структуру каталога

### 3.4. Репозиторий (первичная структура)

- [ ] Создать репо на GitHub: имя `retro-engine`, лицензия MIT, public *(нужен доступ к GitHub/gh; после создания: `git remote add origin …`, push)*
- [x] `git clone` локально *(вместо clone — локальный `git init` в RetroPolyEngine; identity: user.name `switchkoff`, e-mail noreply — при необходимости сменить)*
- [x] Создать `.gitignore`: `build/`, `.vs/`, `.cache/`, `*.log`, `duckstation_test.log`, `coverage.info`
- [x] Создать `.editorconfig` (UTF-8, LF, 4 пробела)
- [x] Создать `LICENSE` (MIT)
- [x] Создать `README.md` (заглушка: 1 абзац + план-ссылки)
- [x] Создать `CHANGELOG.md` (Keep a Changelog: `[Unreleased]`)
- [x] Создать каркас каталогов по документации (§10 ЭТАПА 4):
  - [x] `src/core/{ecs,math,memory,log}/`, `src/core/types.h`
  - [x] `src/hal/` + `src/hal/pc/` + `src/hal/ps1/`
  - [x] `src/render/` (+ `shaders/`), `src/io/`, `src/audio/`, `src/assets/`
  - [x] `game/`, `tools/asset_converter/`, `examples/hello_triangle/`, `examples/rotating_cube/`
  - [x] `assets/source/`, `assets/build/`, `tests/`, `docs/`, `toolchain/`, `scripts/`, `ci/`, `benchmarks/`, `cmake/`
  - [x] `.github/workflows/`, `.github/ISSUE_TEMPLATE/`
- [ ] Первый commit + push в `main` *(commit выполнен: 565d3a0, ветки `main`/`develop` созданы; push — после создания remote)*
- [x] Создать ветку `develop`, далее работать в `develop`

### 3.5. Система сборки (каркас)

- [x] Создать корневой `CMakeLists.txt`:
  - [x] `cmake_minimum_required(VERSION 3.25)`, `project(retro_engine LANGUAGES C CXX ASM)`
  - [x] `option(PLATFORM_PC ...)`, `option(PLATFORM_PS1 ...)`
  - [x] Guard: обе платформы = FATAL_ERROR; ни одной = FATAL_ERROR
  - [x] `add_subdirectory(src)`; при PC — `add_subdirectory(src/third_party/sdl3 EXCLUDE_FROM_ALL)` и `add_subdirectory(tools/asset_converter)`
  - [x] `add_subdirectory(examples/hello_triangle)`, `add_subdirectory(examples/rotating_cube)`
  - [x] При PC: `set_target_properties(hello_triangle rotating_cube PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/game")` + `add_subdirectory(tests)`
- [x] Создать `CMakePresets.json` (version 6):
  - [x] `pc-debug` (Ninja, `build/pc-debug`, PLATFORM_PC, BUILD_TESTS, CMAKE_BUILD_TYPE=Debug)
  - [x] `pc-release` (+ BUILD_EXAMPLES, Release)
  - [x] `pc-debug-render` (BUILD_RENDER_INTEGRATION_TESTS=ON)
  - [x] `ps1-release` (toolchainFile `$env{PSN00BSDK_LIBS}/cmake/sdk.cmake`, PLATFORM_PS1, Release)
  - [x] Опционально сразу: `pc-asan`, `pc-bench`, `pc-coverage`
  - [x] Соответствующие `buildPresets` и `testPresets`
- [x] Создать `cmake/CompilerFlags.cmake`:
  - [x] MSVC: `/W4 /WX /MP /Zi`
  - [x] GCC/Clang: `-Wall -Wextra -Wpedantic -Werror` (где позволяет SDK); third-party headers изолировать от флагов
- [x] `src/CMakeLists.txt`:
  - [x] Выбор ровно одного HAL: `add_subdirectory(hal/pc)` ИЛИ `add_subdirectory(hal/ps1)`
  - [x] `add_library(retro_engine STATIC ...)` + `target_link_libraries(retro_engine PUBLIC retro_engine_hal)` *(до первых TU ядра (Спринт 1–2) — заглушечная `src/core/placeholder.cpp`, удалить вместе с ней)*
- [x] Проверка: `cmake --preset pc-debug && cmake --build --preset pc-debug` проходит (пока с пустыми/заглушечными targets) *(проверено: конфигурация + сборка зелёные, бинари в `build/pc-debug/game/`)*

### 3.6. Вендоринг зависимостей

- [x] SDL3: `git submodule add https://github.com/libsdl-org/SDL.git src/third_party/sdl3` → pin стабильный release-тег; подключение только через `add_subdirectory` (vcpkg — не-канонический fallback) *(pinned `release-3.4.18` = `829a65d7`)*
- [x] PSn00bSDK: `git submodule add ... src/third_party/psn00bsdk` → `git checkout 06e65be` (v0.24), НЕ обновлять после pin *(pinned v0.24 = `06e65bea3a778b2dae5af77a7935ae3868ddd4d3`)*
- [x] GoogleTest: submodule → `src/third_party/googletest` (canonical; FetchContent запрещён) *(pinned v1.18.0 = `063de7e9`)*
- [x] cgltf: скачать tagged release в `src/third_party/cgltf/cgltf.h` *(v1.15)*
- [x] stb: `stb_image.h`, `stb_image_write.h` (pinned archive) → `src/third_party/stb/` *(pinned master `003501f6`)*
- [x] Для каждого vendored компонента: записать точный тег/commit и SHA-256 в `VERSION.txt`; CI проверяет хэши *(проверка — `ci/verify_vendored.sh`, шаг в `tests.yml`; все пины/хэши совпали)*
- [x] Зафиксировать лицензии (для будущего LEGAL.md): cgltf=MIT, stb=PD/MIT, glad=MIT, SDL3=zlib, PSn00bSDK core=MPL-2.0 (mkpsxiso/dumpsxiso — GPL-2.0-or-later, сохранять notices) *(создан `LEGAL.md`)*

### 3.7. Первый CI

- [x] `.github/workflows/tests.yml` (ubuntu-latest):
  - [x] checkout@v4 c `submodules: recursive`
  - [x] `apt-get install cmake ninja-build libgl1-mesa-dev`
  - [x] `cmake --preset pc-debug && cmake --build --preset pc-debug && ctest --preset pc-debug --output-on-failure`
- [ ] Убедиться, что workflow зелёный на пустом каркасе *(требует push на GitHub; на этой Windows-независимой машине нет remote)*

## Критерии приёмки (гейт выхода из M0)

- [ ] Все чек-боксы выше закрыты *(остались: Windows-среда §3.1–3.2 (на Windows-машине), GitHub repo/remote/push, зелёный Actions)*
- [x] `cmake --preset pc-debug && cmake --build --preset pc-debug` проходит *(Linux: GCC 15.2, CMake 4.4.4, Ninja 1.13.2)*
- [ ] `ctest --preset pc-debug` зелёный; CI (`tests.yml`) зелёный на пустом каркасе *(ctest локально зелёный, exit 0; Actions — после первого push)*
- [ ] `cmake --preset ps1-release` конфигурируется с pinned SDK (смок-тест) *(нет mipsel-тулчейна в Linux-среде — смок-тест по §3.2 на Windows-машине)*
