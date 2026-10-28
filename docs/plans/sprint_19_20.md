# Спринт 19–20 — Полировка, документация, релиз v0.1.0 (недели 37–40)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§5).
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 19–20 |
| Недели | 37–40 — 19.07.2027 – 15.08.2027 |
| Веха | M8 — Релиз v0.1.0 |
| Цель | Публичный релиз: полный доксет, release gate, GitHub Release, анонс |
| Входные критерии | Спринт 17–18 завершён (rotating_cube, гейты FPS/памяти обеих платформ) ([`sprint_17_18.md`](sprint_17_18.md)) |
| На критическом пути | Да |
| Выходные артефакты | Полный доксет (README/ARCHITECTURE/BUILDING/API/ASSETS/CONTRIBUTING/CODE_OF_CONDUCT/LEGAL/STYLE_GUIDE/TESTING/TROUBLESHOOTING), `ci/normative_contracts.yaml`, `scripts/verify_release_contract.py`, `release.yml`, тег `v0.1.0`, GitHub Release, анонс, Discord |
| Следующий спринт | Пост-релизная поддержка (§12 родительского плана) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 30 | 45 | 70 | 46.7 | 6.7 | 60.0 | 40–60 ч | Укладывается; P-сценарий поглощает баг-фикс-буфер недели 39 |

## Задачи

**Неделя 37 — Основная документация:**
- [ ] [B] `README.md`: описание, скриншоты/GIF, quickstart (пресеты), ссылки на docs
- [ ] [B] `docs/ARCHITECTURE.md`: слои Core→Game, HAL-подход (CMake source selection, без runtime virtual dispatch в hot paths), pipeline TRANSFORM→CLIP→CULL→SORT→PACK→SUBMIT, ассет-пайплайн, платформенные оптимизации
- [ ] [B] `docs/BUILDING.md`: Windows/Linux/macOS установка (Git/CMake/Ninja/MSVC), PSn00bSDK pin, пресеты, submodules, PS1 .cue в DuckStation; troubleshooting (SDL3 not found → submodules; PS1 build → PATH/env)
- [ ] [B] `docs/API.md`: ECS, Renderer, Input, FileSystem, Math (по Doxygen)
- [ ] [B] `docs/ASSETS.md` — финализировать (байтовые форматы, CLI, интеграция)

**Неделя 38 — Сообщество + legal:**
- [ ] [B] `docs/CONTRIBUTING.md` (форк→ветка→тесты→PR; C++14; Doxygen; обязательные тесты; review-процесс; squash в develop)
- [ ] [B] `docs/CODE_OF_CONDUCT.md` (Contributor Covenant)
- [ ] [B] `docs/LEGAL.md`: лицензии всех зависимостей (SDL3 zlib; PSn00bSDK core MPL-2.0, mkpsxiso/dumpsxiso GPL-2.0+; cgltf MIT; stb PD/MIT; glad MIT; OpenGL — runtime от драйвера); правила сохранения notices
- [ ] [B] `docs/STYLE_GUIDE.md`, `docs/TESTING.md`, `docs/TROUBLESHOOTING.md`
- [ ] [B] CHANGELOG.md — перенести [Unreleased] → [0.1.0] + дата; известные issues (audio stub, demo-level physics, no networking)

**Неделя 39 — Полный QA-прогон + release gate:**
- [ ] [B] Реализовать `ci/normative_contracts.yaml` (манифест: presets; engine_scalar Q16.16 saturating/trunc; memory-бюджеты 1572864/819200/262144/65536; renderer 320×240, pipeline, depth disabled PC, no silent drop; assets v1 LE; release ps1_exe_max 512000; W0 conformance; W1 60/30/60s) и CI-валидацию манифеста против конфигурации
- [ ] [B] `scripts/verify_release_contract.py` (9 assert-ов):
  - [ ] presets == {pc-debug, pc-release, pc-debug-render, ps1-release}
  - [ ] ps1_executable_size ≤ 512000
  - [ ] ps1_peak_main_ram ≤ 1572864; ps1_peak_vram ≤ 819200
  - [ ] w0_framebuffer_hash == canonical_w0_hash
  - [ ] w1_pc_fps ≥ 60; w1_ps1_emulator_fps ≥ 30
  - [ ] all_asset_validation_cases_pass; no_renderer_overflow_is_silent
  - [ ] Публикация результата как CI-artifact; релиз без валидатора недействителен
- [ ] [B] Полный чек-лист release (см. §11 родительского плана): 3 ОС CI green; PS1 pinned build; unit+sanitizers; примеры PC+DuckStation через .cue; RAM/VRAM; EXE size; malformed assets; framebuffer conformance; 0 blocker/critical
- [ ] [B] Ручные прогоны: `ctest` (pc-debug/pc-release), примеры на PC, `run_ps1.bat` в DuckStation, проверка всех ссылок в доксах
- [ ] [B] Исправление найденных багов (заложенный баг-фикс-буфер)

**Неделя 40 — Релиз и анонс:**
- [ ] [B] `.github/workflows/release.yml` (по тегу `v*`): checkout submodules; PSn00bSDK pinned+sha; build PC (hello_triangle, rotating_cube) + PS1 (включая disc image); validate artifacts (все 5 файлов); payload `bin/pc`, `bin/ps1`, README, LICENSE; `create_release_archive.sh` → `retro-engine-<tag>.tar.gz`; softprops/action-gh-release + generate_release_notes
- [ ] [B] Тег: `git tag -a v0.1.0 -m "Release v0.1.0"`, push main --tags
- [ ] [B] GitHub Release опубликован (авто)
- [ ] [B] Announcement post (шаблон из ЭТАПА 8 §5.1) → Reddit (r/ps1, r/homebrew, r/gamedev, r/opensource, r/cpp), HN, Twitter/X (#gamedev #homebrew #ps1 #opensource #cpp), Discord-серверы homebrew, форумы (psxdev.net)
- [ ] [B] Демо-видео на YouTube (release trailer 1–2 мин)
- [ ] [B] Discord-сервер проекта: #announcements/#general/#help/#show-and-tell/#development/#off-topic + боты (модерация, GitHub bot)
- [ ] [B] Ответить на первые issues (SLA 48 ч)
- [ ] [B] GitHub Discussions: категории Announcements/General/Ideas/Show and Tell/Q&A

## Критерии приёмки спринта 19–20

- [ ] Все unit-тесты зелёные; примеры PC+PS1 работают
- [ ] Документация полна; ссылки проверены
- [ ] `verify_release_contract.py` зелёный; Release создан; анонс опубликован
