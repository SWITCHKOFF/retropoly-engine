# DRAFT — RetroPolyEngine v0.3.0 (Nintendo 64, не опубликовано)

> **Статус: closed as draft / frozen (2026-11-15).** v0.3.0 — development-релиз N64; любая
> публикация отложена до **v0.9.x** ([ADR-013](adr/ADR-013-release-freeze.md)).
> GitHub Release/тег не создаются; `release.yml` формирует только draft
> после ручной приёмки maintainer + агент. v0.2.x остаются withdrawn.

## Что внутри

| Артефакт | Содержимое |
|---|---|
| `retro-engine-v0.3.0.tar.gz` | Комбинированный payload: `sdk/` + `sdk/platforms/n64/` + `ui/` + optional `demos/n64/`, `checksums.sha256`, `release_contract_report.json` |
| `retro-engine-sdk-0.3.0-pc-<os>-<arch>.{tar.gz,zip}` | PC SDK: headers/libs/CMake package/`asset_converter`/examples |
| `retro-engine-sdk-0.3.0-ps1-mips.{tar.gz,zip}` | PS1 SDK (PSn00bSDK v0.24, внешний) |
| `retro-engine-sdk-0.3.0-saturn-sh2.{tar.gz,zip}` | Saturn SDK (Jo Engine v2026.2, внешний) |
| `retro-engine-sdk-0.3.0-n64-mips64.{tar.gz,zip}` | N64 SDK: HAL, `RetroPolyEngine::n64_rsp_tnl`, `hello_triangle_n64`, toolchain wrapper; требует `N64_INST` + pinned libdragon |
| `retro-engine-ui-0.3.0-<os>-<arch>.{tar.gz,zip,dmg}` | UI client: target PC/PS1/Saturn/N64, import/build/run |

`demos/n64/` (optional): `hello_triangle.z64`, `rotating_cube.z64`,
`.map`, `.sha256`, `retro_n64.dfs`, listing. ROM не заменяет SDK/UI.

## N64 (S39–46)

- pinned toolchain: mips64-elf-gcc 16.2.0 + libdragon
  `e356bf3f56f7afbf7e5246329562f145965cfdfc` (MIT), `.z64` big-endian,
  DFS read-only (`mkdfs`, deterministic), 4 MiB ledger.
- RSP T&L overlay (Q16.16, 512 слотов) + RSP dot-lighting; RDPQ с
  аппаратным 18-bit Z; perspective-correct CI8 + TMEM-полосы 32×64
  (2560 B) + 256-entry TLUT; bilinear.
- `hello_triangle.z64` — 1 треугольник + интерактивная камера
  (stick/D-pad/A-reset); `rotating_cube.z64` — W1 4 куба/48 tri,
  fixed camera. Overlay: FPS/frame, peak RDRAM, TMEM, TNL/RDP, draw
  calls, W0-hash.
- W0 5/5 golden; CPU T&L fallback даёт тот же visible hash
  (`0x9e92e718`). W1 ≈59 FPS в gopher64 1.2.1 (gate ≥30; target 60 на
  железе, software RDP).
- `asset_converter --platform n64`: host-only, 64×64 CI8+CLUT,
  детерминизм; `.mesh`/`.tex` v1 не изменяются.
- N64 SDK: `find_package(RetroPolyEngine)`, clean consumer
  `hello_triangle_n64` → `.z64`; UI target N64 (gopher64).
- ROM/DFS: 376 832/360 448 B и 68 864 B (≪32 MiB); heap в
  `src/hal/n64` запрещён; peak RDRAM 966 492 B (limit 3.5 MiB).

## Быстрый старт (N64)

```sh
export N64_INST=$HOME/n64-sdk/toolchain/opt/libdragon
export LIBDRAGON_ROOT=$HOME/n64-sdk/libdragon
cmake --preset n64-release
cmake --build --preset n64-release
GOPHER64_BIN=$PWD/gopher64 scripts/run_n64.sh
# rotating_cube
GOPHER64_BIN=$PWD/gopher64 scripts/run_n64.sh \
  build/n64-release/src/hal/n64/rotating_cube.z64
```

SDK:

```sh
tar -xzf retro-engine-sdk-0.3.0-n64-mips64.tar.gz
cmake -S sdk/examples/hello_triangle_n64 -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/sdk/cmake/n64-toolchain.cmake \
  -DCMAKE_PREFIX_PATH=$PWD/sdk
cmake --build build        # hello_triangle_n64.z64
```

## Поддерживаемые платформы

| Host | Targets |
|---|---|
| Linux x86_64 | PC, PS1 (PSn00bSDK), Saturn (Jo Engine), N64 (libdragon) |
| Windows x86_64 | PC; PS1/Saturn/N64 — экспериментально (toolchain env) |
| macOS arm64 | PC; cross-targets требуют внешний toolchain |

N64 host — Linux x86_64 (CI); Windows-сборка N64 экспериментальна,
macOS не проверяется. BIOS/ROM/ключи, Jaguar-артефакты и эмуляторы
(включая gopher64) в payload не входят; для N64 BIOS не требуется.

## Известные ограничения

- Нет N64 audio/AI, записи на картридж/64DD, PAL/480i.
- RSP T&L — без скиннинга/ucode-clipping; lighting — dot + CPU
  color/ambient; RDP-команды ставит CPU (ADR-015).
- Текстуры N64 ≤64×64, без мипмапов; TMEM 4 KiB.
- DFS — read-only bare UPPERCASE 8.3, ≤128 KiB/файл.
- Expansion Pak определяется, арены не растут; 8 MiB — reference
  environment, не требование.
- UI N64 run требует gopher64 1.2.1; import — PC SDK
  `asset_converter --platform n64`.
- Публикация релизов запрещена до v0.9.x; v0.2.x withdrawn.

## Проверка

```sh
sha256sum -c checksums.sha256
sha256sum -c sdk/checksums.sha256
sha256sum -c sdk/platforms/n64/checksums.sha256
sha256sum -c ui/checksums.sha256
```

Gate: `scripts/verify_release_contract.py` (`all_ok: true`),
`scripts/release_gate_negative_tests.py` (включая missing/bad N64 SDK,
Jaguar/emulator — 18/18), `scripts/verify_n64_budgets.py`,
`scripts/verify_n64_assets.py`.
Remote CI: PR #5 `2191d3a` — green (`build-n64`, `tests`, `sdk-package`,
`ui-client`, `build-pc/ps1/saturn`); release gate dispatch `43720489809` —
success, payload `all_ok`, `forbidden: []`, `publish` skipped; merge
`2f7d9f4` — develop green.
Бенчмарк: [`benchmarks/n64_hello_triangle_v0_3_0.yaml`](benchmarks/n64_hello_triangle_v0_3_0.yaml).
