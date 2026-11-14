# DRAFT — RetroPolyEngine v0.2.1 (release candidate, не опубликовано)

> **Статус: withdrawn / deferred.** GitHub Release v0.2.1 удалён/отозван
> 2026-11-14; любая публикация релизов отложена до v0.9.x. Файл сохранён
> как черновик release notes для будущей ручной приёмки. `release.yml`
> создаёт только draft и использует auto-generated notes; этот файл не
> публикуется автоматически.

Release candidate: устанавливаемый SDK (PC/PS1/Saturn) и UI client для
создания, сборки и запуска проектов. GitHub Release v0.2.0 также отозван
и не является SDK-релизом.

## Что внутри

| Asset | Содержимое |
|---|---|
| `retro-engine-v0.2.1.tar.gz` | Комбинированный payload: `sdk/`, `ui/`, optional `demos/`, `checksums.sha256`, `release_contract_report.json`, README/LICENSE/LEGAL |
| `retro-engine-sdk-0.2.1-pc-<os>-<arch>.{tar.gz,zip}` | PC SDK: публичные заголовки, библиотеки, CMake package, `asset_converter`, примеры, шаблон проекта |
| `retro-engine-sdk-0.2.1-ps1-mips.{tar.gz,zip}` | PS1 SDK: headers/libs + `hello_triangle_ps1` + toolchain wrapper |
| `retro-engine-sdk-0.2.1-saturn-sh2.{tar.gz,zip}` | Saturn SDK: headers/libs + `hello_triangle_saturn` + toolchain wrapper |
| `retro-engine-ui-0.2.1-<os>-<arch>.{tar.gz,zip}` (+ macOS `.dmg`) | UI client: launcher/editor, samples, DejaVuSans, manifest/checksums |
| `checksums.sha256` | SHA-256 всех release assets |
| `release_contract_report.json` | Полный отчёт release gate v2 |
| `payload_verification_report.json` | Проверка `sdk/` + `ui/` payload, checksums и запрета BIOS/ROM |

## Быстрый старт

### SDK

```sh
tar -xzf retro-engine-sdk-0.2.1-pc-linux-x86_64.tar.gz
cd sdk
sha256sum -c checksums.sha256

cmake --preset sdk-release
cmake --build --preset sdk-release
```

Свой проект:

```cmake
find_package(RetroPolyEngine CONFIG REQUIRED)
target_link_libraries(my_game PRIVATE RetroPolyEngine::retro_engine)
```

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/sdk
cmake --build build
```

### UI client

```sh
tar -xzf retro-engine-ui-0.2.1-linux-x86_64.tar.gz
./ui/retro-engine-ui
```

1. **Новый проект** → имя/путь/target PC.
2. Указать SDK PC (каталог с `sdk_manifest.json`).
3. Импортировать `samples/cube.gltf` и `samples/texture.png`.
4. **Собрать** → **Запустить**.

## Поддерживаемые платформы

| Компонент | Hosts | Targets |
|---|---|---|
| SDK | Linux x86_64, Windows x86_64, macOS arm64 | PC, PS1, Saturn |
| UI client | Linux x86_64, Windows x86_64, macOS arm64 | PC/PS1/Saturn build & run |

macOS x86_64 собирается из исходников, но не входит в assets v0.2.1.

PS1 требует PSn00bSDK v0.24, Saturn — Jo Engine v2026.2 (pinned в
`VERSION.txt`); toolchains не входят в SDK-архивы.

## Проверки release gate v2

- `build-pc`, `build-ps1`, `build-saturn`, `tests`, `sdk-package`,
  `ui-client` — success на release-коммите.
- Clean consumer: SDK распакован в пустой каталог, `hello_triangle` и
  `rotating_cube` собираются через `find_package(RetroPolyEngine)`.
- Clean UI E2E: SDK-архив + UI-пакет → New Project → import
  `cube.gltf`/`texture.png` → Build PC → Run.
- `verify_release_contract.py` — `all_ok: true` (SDK/UI/payload).
- Negative-tests: без `sdk/`, `ui/`, checksums/report; BIOS/ROM/секреты и
  build-каталоги в payload → gate fail.
- PS1/Saturn: сборка примеров из SDK-пакета; ручной запуск `.cue` в
  DuckStation (PS1) и Mednafen 1.32.1 (Saturn).

## Checksums

```sh
sha256sum -c checksums.sha256
# для отдельного архива — sidecar рядом с файлом:
sha256sum -c retro-engine-sdk-0.2.1-pc-linux-x86_64.tar.gz.sha256
```

## Known limitations

- PS1/Saturn валидация выполнена в эмуляторах (DuckStation/Mednafen), не на
  реальном железе; производительность на hardware не заявляется.
- PS1/Saturn toolchains нужно установить отдельно (pinned версии).
- UI client — MVP: project/target/import/build/run, без полноценного IDE.
- Комбинированный payload содержит native-host SDK+UI; другие hosts
  скачиваются отдельными assets.
- v0.2.0 остаётся отозванным; не использовать его как SDK-релиз.

## Ссылки

- [docs/SDK.md](https://github.com/RetroPolyEngine/retro-engine/blob/main/docs/SDK.md)
- [docs/UI_CLIENT.md](https://github.com/RetroPolyEngine/retro-engine/blob/main/docs/UI_CLIENT.md)
- [docs/RELEASING.md](https://github.com/RetroPolyEngine/retro-engine/blob/main/docs/RELEASING.md)
- [CHANGELOG.md](https://github.com/RetroPolyEngine/retro-engine/blob/main/CHANGELOG.md)
