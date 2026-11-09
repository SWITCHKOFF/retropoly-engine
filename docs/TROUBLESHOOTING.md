# Troubleshooting — RetroPolyEngine

## Сборка

### `CMake Error: Could not find toolchain file: .../sdk.cmake`

**Контекст**: пресет `ps1-release`.

**Причина**: не заданы переменные среды PSn00bSDK.

**Решение**:

```bash
export PSN00BSDK_ROOT=$HOME/PSn00bSDK
export PSN00BSDK_LIBS=$PSN00BSDK_ROOT/lib/libpsn00b
export PATH=$PSN00BSDK_ROOT/bin:$PATH
```

### `src/third_party/sdl3 пуст (submodule не инициализирован)`

**Контекст**: любая PC-сборка.

**Решение**:

```sh
git submodule update --init --recursive
```

### `verify_vendored: FAIL: sdl3: pin сместился`

**Причина**: submodule обновился локально; пины фиксируются в
[`VERSION.txt`](../VERSION.txt).

**Решение**: откатить submodule на commit из `VERSION.txt` или оформить
отдельную задачу на обновление пина.

### Visual Studio: `Error running test executable`

**Причина**: обнаружение тестов GTest происходит в configure-фазе, а
SDL3-shared DLL не в PATH.

**Решение**: использовать канонический пресет (`pc-debug`/`pc-release`),
который линкует SDL3-static.

## Рендер / примеры

### Окно чёрное, но не падает

**Причина**: на PC `Render_EndFrame` блитит в default framebuffer, а swap
выполняется отдельно — `Platform_SwapBuffers()`.

**Решение**: убедитесь, что в главном цикле после `Render_EndFrame()`
вызывается `Platform_SwapBuffers()`.

### `RENDER_DIAG_COMMAND_OVERFLOW`

**Причина**: 64 KiB командный буфер кадра исчерпан.

**Решение**: уменьшить число треугольников кадра или разбить кадр; для
MVP сцена должна укладываться (W1 — 48 треугольников).

### `RENDER_DIAG_INVALID_PROJECTION`

**Причина**: проекционная матрица вырождена (например, `near == far`).

**Решение**: проверить параметры `mat4_perspective` (0 < fov < π,
aspect > 0, 0 < near < far).

## PS1

### DuckStation не видит диск

**Причина**: образ собран, но путь к `RETRO.cue` некорректен.

**Решение**: открывать `build/ps1-release/src/hal/ps1/RETRO.cue`, а не
`.bin`. Проверить, что `system.cnf` содержит
`BOOT = cdrom:\RETRO.EXE;1`.

### PS-EXE > 512 000 байт

**Решение**:

```sh
sh scripts/check_binary_size.sh
```

Если превышен — смотреть на размер статических данных/строк, отключать
ненужную отладку, проверять `-Os`/`Release`.

## Тесты

### `pc-debug-render` падает локально

**Причина**: нет GL-контекста/дисплея.

**Решение**:

```sh
xvfb-run -a ctest --preset pc-debug-render --output-on-failure -R integration
```

### Тесты W0-golden не совпадают хэш

**Причина**: другой GPU-драйвер / OSD; LSB-джиттер допустим, но не должен
влиять на 15-битный хэш.

**Решение**: если разница систематическая — перезапустить с
`RENDER_GOLDEN_REBASE=1` и оформить rebase с обоснованием в журнале.

## Связь

Если проблема не решена — откройте issue с лейблом `question` или
`bug`, приложив:

- ОС и версию;
- команду сборки и полный лог;
- git revision (`git rev-parse HEAD`).
