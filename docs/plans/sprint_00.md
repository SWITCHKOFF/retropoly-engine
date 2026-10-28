# Спринт 0 — Этап 0: подготовка среды и инфраструктуры (неделя 0)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§3).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).

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

- [ ] Открыть gen.glad.sh: API=OpenGL, Profile=Core, Version=3.3, Generator=C/C++, Extensions=все
- [ ] Generate → распаковать в `src/third_party/glad/` (`include/glad/gl.h`, `include/KHR/khrplatform.h`, `src/gl.c`)
- [ ] Проверить структуру каталога

### 3.4. Репозиторий (первичная структура)

- [ ] Создать репо на GitHub: имя `retro-engine`, лицензия MIT, public
- [ ] `git clone` локально
- [ ] Создать `.gitignore`: `build/`, `.vs/`, `.cache/`, `*.log`, `duckstation_test.log`, `coverage.info`
- [ ] Создать `.editorconfig` (UTF-8, LF, 4 пробела)
- [ ] Создать `LICENSE` (MIT)
- [ ] Создать `README.md` (заглушка: 1 абзац + план-ссылки)
- [ ] Создать `CHANGELOG.md` (Keep a Changelog: `[Unreleased]`)
- [ ] Создать каркас каталогов по документации (§10 ЭТАПА 4):
  - [ ] `src/core/{ecs,math,memory,log}/`, `src/core/types.h`
  - [ ] `src/hal/` + `src/hal/pc/` + `src/hal/ps1/`
  - [ ] `src/render/` (+ `shaders/`), `src/io/`, `src/audio/`, `src/assets/`
  - [ ] `game/`, `tools/asset_converter/`, `examples/hello_triangle/`, `examples/rotating_cube/`
  - [ ] `assets/source/`, `assets/build/`, `tests/`, `docs/`, `toolchain/`, `scripts/`, `ci/`, `benchmarks/`, `cmake/`
  - [ ] `.github/workflows/`, `.github/ISSUE_TEMPLATE/`
- [ ] Первый commit + push в `main`
- [ ] Создать ветку `develop`, далее работать в `develop`

### 3.5. Система сборки (каркас)

- [ ] Создать корневой `CMakeLists.txt`:
  - [ ] `cmake_minimum_required(VERSION 3.25)`, `project(retro_engine LANGUAGES C CXX ASM)`
  - [ ] `option(PLATFORM_PC ...)`, `option(PLATFORM_PS1 ...)`
  - [ ] Guard: обе платформы = FATAL_ERROR; ни одной = FATAL_ERROR
  - [ ] `add_subdirectory(src)`; при PC — `add_subdirectory(src/third_party/sdl3 EXCLUDE_FROM_ALL)` и `add_subdirectory(tools/asset_converter)`
  - [ ] `add_subdirectory(examples/hello_triangle)`, `add_subdirectory(examples/rotating_cube)`
  - [ ] При PC: `set_target_properties(hello_triangle rotating_cube PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/game")` + `add_subdirectory(tests)`
- [ ] Создать `CMakePresets.json` (version 6):
  - [ ] `pc-debug` (Ninja, `build/pc-debug`, PLATFORM_PC, BUILD_TESTS, CMAKE_BUILD_TYPE=Debug)
  - [ ] `pc-release` (+ BUILD_EXAMPLES, Release)
  - [ ] `pc-debug-render` (BUILD_RENDER_INTEGRATION_TESTS=ON)
  - [ ] `ps1-release` (toolchainFile `$env{PSN00BSDK_LIBS}/cmake/sdk.cmake`, PLATFORM_PS1, Release)
  - [ ] Опционально сразу: `pc-asan`, `pc-bench`, `pc-coverage`
  - [ ] Соответствующие `buildPresets` и `testPresets`
- [ ] Создать `cmake/CompilerFlags.cmake`:
  - [ ] MSVC: `/W4 /WX /MP /Zi`
  - [ ] GCC/Clang: `-Wall -Wextra -Wpedantic -Werror` (где позволяет SDK); third-party headers изолировать от флагов
- [ ] `src/CMakeLists.txt`:
  - [ ] Выбор ровно одного HAL: `add_subdirectory(hal/pc)` ИЛИ `add_subdirectory(hal/ps1)`
  - [ ] `add_library(retro_engine STATIC ...)` + `target_link_libraries(retro_engine PUBLIC retro_engine_hal)`
- [ ] Проверка: `cmake --preset pc-debug && cmake --build --preset pc-debug` проходит (пока с пустыми/заглушечными targets)

### 3.6. Вендоринг зависимостей

- [ ] SDL3: `git submodule add https://github.com/libsdl-org/SDL.git src/third_party/sdl3` → pin стабильный release-тег; подключение только через `add_subdirectory` (vcpkg — не-канонический fallback)
- [ ] PSn00bSDK: `git submodule add ... src/third_party/psn00bsdk` → `git checkout 06e65be` (v0.24), НЕ обновлять после pin
- [ ] GoogleTest: submodule → `src/third_party/googletest` (canonical; FetchContent запрещён)
- [ ] cgltf: скачать tagged release в `src/third_party/cgltf/cgltf.h`
- [ ] stb: `stb_image.h`, `stb_image_write.h` (pinned archive) → `src/third_party/stb/`
- [ ] Для каждого vendored компонента: записать точный тег/commit и SHA-256 в `VERSION.txt`; CI проверяет хэши
- [ ] Зафиксировать лицензии (для будущего LEGAL.md): cgltf=MIT, stb=PD/MIT, glad=MIT, SDL3=zlib, PSn00bSDK core=MPL-2.0 (mkpsxiso/dumpsxiso — GPL-2.0-or-later, сохранять notices)

### 3.7. Первый CI

- [ ] `.github/workflows/tests.yml` (ubuntu-latest):
  - [ ] checkout@v4 c `submodules: recursive`
  - [ ] `apt-get install cmake ninja-build libgl1-mesa-dev`
  - [ ] `cmake --preset pc-debug && cmake --build --preset pc-debug && ctest --preset pc-debug --output-on-failure`
- [ ] Убедиться, что workflow зелёный на пустом каркасе

## Критерии приёмки (гейт выхода из M0)

- [ ] Все чек-боксы выше закрыты
- [ ] `cmake --preset pc-debug && cmake --build --preset pc-debug` проходит
- [ ] `ctest --preset pc-debug` зелёный; CI (`tests.yml`) зелёный на пустом каркасе
- [ ] `cmake --preset ps1-release` конфигурируется с pinned SDK (смок-тест)
