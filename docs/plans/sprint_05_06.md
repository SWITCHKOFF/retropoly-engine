# Спринт 5–6 — HAL для PC (SDL3 + OpenGL) (недели 9–12)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§4).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).

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
- [ ] [B] `src/hal/platform.h` — единый API (см. §3 ЭТАПА 4): Platform_Init/Shutdown/PumpEvents/SwapBuffers/SleepMs/GetTimeMs; Render_*; Input_*; Audio_*; FileSystem_*
- [ ] [B] `src/hal/pc/CMakeLists.txt`:
  - [ ] `add_library(glad STATIC .../third_party/glad/src/gl.c)` + include dirs
  - [ ] `add_library(retro_engine_hal STATIC platform_pc.cpp render_opengl.cpp input_sdl.cpp audio_sdl.cpp filesystem_pc.cpp)`
  - [ ] `target_link_libraries(retro_engine_hal PUBLIC SDL3::SDL3 OpenGL::GL glad)`
- [ ] [B] `src/hal/pc/platform_pc.cpp`:
  - [ ] `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)` (НЕ SDL_INIT_TIMER — удалён в SDL3)
  - [ ] GL-атрибуты **до** создания окна: 3.3 Core, DoubleBuffer=1
  - [ ] `SDL_CreateWindow("RetroEngine", 1280, 960, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE)`
  - [ ] `SDL_GL_CreateContext` → `SDL_GL_MakeCurrent` → `gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)` **до** любых GL-вызовов
  - [ ] Shutdown: DestroyContext → DestroyWindow → SDL_Quit
- [ ] [B] Ввод SDL3: **только SDL_Gamepad API** (`SDL_GetGamepads`, `SDL_OpenGamepad`, `SDL_GetGamepadButton`, `SDL_GetGamepadAxis`) — SDL_GameController запрещён
- [ ] [H] Тест вручную: окно открывается/закрывается без краша

**Неделя 10 — Тайминги и цикл:**
- [ ] [B] `Platform_SwapBuffers` (SDL_GL_SwapWindow), `Platform_GetTimeMs` (SDL_GetTicks), `Platform_SleepMs` (SDL_Delay)
- [ ] [B] Интеграционный тест `tests/integration/hal_test.cpp`:
  - [ ] InitShutdown
  - [ ] GetTimeMs: Sleep(100) → разница ∈ [95..150] мс
  - [ ] InputKeyboard: функция возвращает bool без краша
- [ ] [H] `.github/workflows/build-pc.yml` (матрица ubuntu/windows/macos, pc-release, install-шаги для каждой ОС)

**Неделя 11 — Ввод:**
- [ ] [B] `src/hal/pc/input_sdl.cpp`: `Input_IsKeyDown(KeyCode)`, `Input_IsPadButtonDown(pad,button)`, `Input_GetPadAxis(pad,axis)` (−128..127)
  - [ ] KeyCode enum (W/A/S/D/SPACE/ESCAPE…), PadButton enum (D-pad, face, L1/R1/L2/R2, Start, Select)
  - [ ] Таблица маппинга SDL_GamepadButton → PadButton
  - [ ] Маппинг WASD ↔ D-pad/face (единая InputSystem-абстракция — см. неделю 31)
- [ ] [H] Ручной тест: клавиатура + геймпад (DualShock-клон через USB/адаптер)

**Неделя 12 — Аудио-заглушка + ФС:**
- [ ] [B] `src/hal/pc/audio_sdl.cpp`: `Audio_Init/Shutdown` (+ lifecycle), `Audio_PlayBeep(freq, ms)` через SDL3 audio (или тишина); полный PCM — post-MVP
- [ ] [B] `src/hal/pc/filesystem_pc.cpp`: `FileSystem_LoadFile(path)` → `FileData{data,size}`; `FileSystem_FreeFile`
  - [ ] Обработка: файл не найден → ok=false/nullptr без краша
- [ ] [B] Тест: загрузка файла (валидный/несуществующий)
- [ ] [B] `.gitattributes`/line endings проверить (CMake чувствителен)
- [ ] [B] CHANGELOG, PR, merge

## Критерии приёмки спринта 5–6

- [ ] Окно 1280×960 SDL3 открывается; GL 3.3 контекст создан (gladLoadGL ok)
- [ ] Ввод клавиатура+геймпад работает (ручной чек)
- [ ] ФС грузит файлы; аудио — заглушка
- [ ] Delta time корректен (тест 95–150 мс)
- [ ] Сборка зелёная на 3 ОС в CI

## Риски

API SDL3 меняется ⇒ pin версии в submodule + прогон документированного API в CI; геймпад не определяется ⇒ использовать SDL_Gamepad, проверить SDL hints.
