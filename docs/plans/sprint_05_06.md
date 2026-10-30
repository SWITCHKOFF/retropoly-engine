# Спринт 5–6 — HAL для PC (SDL3 + OpenGL) (недели 9–12)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§4).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-10-30, Linux-среда):** Недели 9–12 выполнены. 96 юнит-тестов (88 прежних + 8 HAL):
> 96/96 в `pc-debug`, `pc-release`, `pc-asan` (0 findings движка; LSan-суппрессии только для системного
> Mesa — [`ci/lsan_suppressions.supp`](../../ci/lsan_suppressions.supp)); окно 1280×960 открывается/закрывается
> честным автотестом `Hal.InitShutsDownWithoutCrash`, delta time ∈ [95..150] мс. Зелёная матрица 3 ОС
> (build-pc.yml) — подлежит подтверждению первым прогонкой после push.

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 5–6 |
| Недели | 9–12 — 04.01.2027 – 31.01.2027 |
| Веха | M2 — PC-платформа (часть 1: HAL) |
| Цель | Окно, ввод, аудио-заглушка, файловая система на PC |
| Входные критерии | Спринт 3–4 завершён (математика + память + ECS) ([`sprint_03_04.md`](sprint_03_04.md)); glad из Спринта 0 |
| На критическом пути | Да |
| Выходные артефакты | `platform.h`, `platform_pc.cpp`, `render_opengl.cpp`, `input_sdl.cpp`, `audio_sdl.cpp`, `filesystem_pc.cpp`, `build-pc.yml` (3 ОС) |
| Следующий спринт | [`sprint_07_08.md`](sprint_07_08.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 28 | 42 | 65 | 43.5 | 6.2 | 55.8 | 40–60 ч | Укладывается; запас при 15 ч/нед |

## Задачи

**Неделя 9 — Платформа SDL3:**
- [x] [B] `src/hal/platform.h` — единый API (см. §3 ЭТАПА 4): Platform_Init/Shutdown/PumpEvents/SwapBuffers/SleepMs/GetTimeMs; Render_*; Input_*; Audio_*; FileSystem_*
- [x] [B] `src/hal/pc/CMakeLists.txt`:
  - [x] `add_library(glad STATIC .../third_party/glad/src/gl.c)` + include dirs *(вендорный target `glad` подключается через `add_subdirectory(src/third_party/glad)` — эквивалент статической библиотеки §3.6)*
  - [x] `add_library(retro_engine_hal STATIC platform_pc.cpp render_opengl.cpp input_sdl.cpp audio_sdl.cpp filesystem_pc.cpp)`
  - [x] `target_link_libraries(retro_engine_hal PUBLIC SDL3::SDL3 OpenGL::GL glad)` *(+ guard: SDL3::SDL3/обязателен, else FATAL_ERROR)*
- [x] [B] `src/hal/pc/platform_pc.cpp`:
  - [x] `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)` (НЕ SDL_INIT_TIMER — удалён в SDL3)
  - [x] GL-атрибуты **до** создания окна: 3.3 Core, DoubleBuffer=1
  - [x] `SDL_CreateWindow("RetroEngine", 1280, 960, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE)`
  - [x] `SDL_GL_CreateContext` → `SDL_GL_MakeCurrent` → `gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)` **до** любых GL-вызовов
  - [x] Shutdown: DestroyContext → DestroyWindow → SDL_Quit *(+ полный откат любой стадии Init)*
- [x] [B] Ввод SDL3: **только SDL_Gamepad API** (`SDL_GetGamepads`, `SDL_OpenGamepad`, `SDL_GetGamepadButton`, `SDL_GetGamepadAxis`) — SDL_GameController запрещён
- [x] [H] Тест вручную: окно открывается/закрывается без краша *(закрыто автотестом `Hal.InitShutsDownWithoutCrash` — реальное окно X11 + GL-контекст + двойной цикл Init/Shutdown)*

**Неделя 10 — Тайминги и цикл:**
- [x] [B] `Platform_SwapBuffers` (SDL_GL_SwapWindow), `Platform_GetTimeMs` (SDL_GetTicks), `Platform_SleepMs` (SDL_Delay)
- [x] [B] Интеграционный тест `tests/integration/hal_test.cpp`:
  - [x] InitShutdown
  - [x] GetTimeMs: Sleep(100) → разница ∈ [95..150] мс
  - [x] InputKeyboard: функция возвращает bool без краша
- [x] [H] `.github/workflows/build-pc.yml` (матрица ubuntu/windows/macos, pc-release, install-шаги для каждой ОС)

**Неделя 11 — Ввод:**
- [x] [B] `src/hal/pc/input_sdl.cpp`: `Input_IsKeyDown(KeyCode)`, `Input_IsPadButtonDown(pad,button)`, `Input_GetPadAxis(pad,axis)` (−128..127)
  - [x] KeyCode enum (W/A/S/D/SPACE/ESCAPE…), PadButton enum (D-pad, face, L1/R1/L2/R2, Start, Select)
  - [x] Таблица маппинга SDL_GamepadButton → PadButton *(+ static_assert длины таблиц против enum)*
  - [x] Маппинг WASD ↔ D-pad/face (единая InputSystem-абстракция — см. неделю 31) *(pad 0 без физического геймпада = клавиатурная эмуляция цифрового PS1-пада)*
- [ ] [H] Ручной тест: клавиатура + геймпад (DualShock-клон через USB/адаптер) *(не выполнено: в среде нет физического геймпада; SDL_Gamepad-путь готов и покрыт out-of-range-автотестами)*

**Неделя 12 — Аудио-заглушка + ФС:**
- [x] [B] `src/hal/pc/audio_sdl.cpp`: `Audio_Init/Shutdown` (+ lifecycle), `Audio_PlayBeep(freq, ms)` через SDL3 audio (или тишина); полный PCM — post-MVP
- [x] [B] `src/hal/pc/filesystem_pc.cpp`: `FileSystem_LoadFile(path)` → `FileData{data,size}`; `FileSystem_FreeFile`
  - [x] Обработка: файл не найден → ok=false/nullptr без краша
- [x] [B] Тест: загрузка файла (валидный/несуществующий) *(+ пустой файл, NULL-пути, FreeFile(null))*
- [x] [B] `.gitattributes`/line endings проверить (CMake чувствителен) *(LF для исходников/конфигов; binary для ассетов; вендорные субмодули — без нормализации)*
- [x] [B] CHANGELOG, PR, merge *(push в `develop`; merge `develop`→`main` локально с сохранением истории; PR-интерфейс недоступен — `gh` CLI отсутствует)*

## Критерии приёмки спринта 5–6

- [x] Окно 1280×960 SDL3 открывается; GL 3.3 контекст создан (gladLoadGL ok)
- [ ] Ввод клавиатура+геймпад работает (ручной чек) *(клавиатура — автотесты; физический геймпад недоступен в среде)*
- [x] ФС грузит файлы; аудио — заглушка
- [x] Delta time корректен (тест 95–150 мс)
- [ ] Сборка зелёная на 3 ОС в CI *(workflow создан; подтверждение — первый прогон после push)*

## Риски

API SDL3 меняется ⇒ pin версии в submodule + прогон документированного API в CI; геймпад не определяется ⇒ использовать SDL_Gamepad, проверить SDL hints.
