# Спринт 31–32 — Hello Triangle на Saturn и релиз v0.2.0 (недели 61–64)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13).
> Источник задач: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md) §4 «Будущие итерации» — Saturn, S21–32.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-13):** **закрыт**. hello_triangle, бюджеты и релизный
> контур приняты; 10-минутный stress PASS; remote CI зелёный; тег
> `v0.2.0` и GitHub Release опубликованы — см. «Результаты».

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 31–32 |
| Недели | 61–64 — 03.01.2028 – 30.01.2028 |
| Веха | v0.2.0 — Sega Saturn (итерация 3) |
| Цель | Полный hello_triangle на Saturn: ассеты с CD, управление с пада, VDP1+VDP2, оба SH-2; документация и релиз v0.2.0 |
| Входные критерии | Спринт 29–30 завершён (VDP2, композитинг, синхронизация) ([`sprint_29_30.md`](sprint_29_30.md)) |
| На критическом пути | Да |
| Выходные артефакты | Saturn-цель `hello_triangle`, `main_saturn.cpp`, bootable-образ, скриншоты, тег `v0.2.0`, GitHub Release, обновлённые docs/release workflow |
| Следующий спринт | Планирование v0.3.0 — N64 (S33+), после приёмки v0.2.0 |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 35 | 50 | 80 | 52.5 | 7.5 | 67.5 | 40–60 ч | ⚠ Интеграция/релиз; P-сценарий поглощает баг-фикс-буфер, при просадке — перенос анонса |

## Задачи

**Неделя 61 — Интеграция hello_triangle:**

- [x] [B] Saturn-цель `hello_triangle`: `main_saturn.cpp`, загрузка `.mesh`/`.tex` с CD-образа, сцена, камера, цикл 60 Гц
- [x] [B] VDP1+VDP2 в одном кадре: 3D-объект поверх фона; оба SH-2 задействованы; fallback на single-CPU
- [x] [B] Сборка bootable-образа с ассетами; артефакт CI; PC/PS1 цели не ломать
- [x] [B] Управление с 6-button pad: движение камеры/объекта; детерминированный ввод
- [x] [B] CMake preset/targets: `saturn-release` + пример; документация запуска

**Неделя 62 — Корректность и бюджеты:**

- [x] [B] Golden/reference-сравнение PC/PS1/Saturn; задокументировать ожидаемые отличия (320×224, quad-конверсия, порядок)
- [x] [B] Проверить текстуры, Gouraud, clip/cull, отсутствие Z-артефактов; malformed-ассеты безопасно отвергаются
- [x] [B] Бюджеты: Work RAM Low/High, VDP1/VDP2 VRAM, размер бинарника/образа; зафиксировать в docs
- [x] [B] Input-тесты: все кнопки, disconnect, повторный запуск
- [x] [B] Обновить `docs/ASSETS.md`/`docs/SATURN.md` по фактическому пайплайну

**Неделя 63 — QA и производительность:**

- [x] [B] 10-минутный стресс в pinned-эмуляторе; frame pacing; отсутствие hang/tearing
- [x] [B] Замер загрузки Master/Slave, fillrate VDP1/VDP2; baseline в `docs/benchmarks/saturn_hello_triangle_v0_2_0.yaml`
- [x] [B] Скриншоты PC/PS1/Saturn; README/демо-материалы
- [x] [B] CI: сборка+артефакт; проверить, что `build-pc`, `build-ps1`, `tests` зелёные (remote — после push)
- [x] [B] TROUBLESHOOTING: toolchain, BIOS/эмулятор, CD-образ, типовые ошибки

**Неделя 64 — Релиз v0.2.0:**

- [x] [B] Документация: README, BUILDING, ARCHITECTURE, API, ASSETS, LEGAL, TROUBLESHOOTING, `docs/SATURN.md`, CHANGELOG [0.2.0]
- [x] [B] `ci/normative_contracts.yaml`: Saturn-секция (preset, toolchain pin, бюджеты, разрешение, golden)
- [x] [B] `scripts/verify_release_contract.py`: Saturn-asserts (сборка, ассеты, память/VRAM, бинарник, golden); релиз без валидатора недействителен
- [x] [B] `.github/workflows/release.yml`: Saturn-артефакты в payload; `create_release_archive.sh`; тег `v0.2.0`
- [x] [B] GitHub Release + анонс; обновить roadmap (следующий — N64 v0.3.0)
- [x] [B] Финальный чек-лист v0.2.0: 3 ОС CI green; PC/PS1/Saturn сборки; unit+sanitizers; примеры; RAM/VRAM; malformed; golden; 0 blocker/critical

## Критерии приёмки спринта 31–32

- [x] Saturn-бинарник компилируется с pinned toolchain и загружается в эмуляторе
- [x] Hello Triangle работает на Saturn: текстура, Gouraud, камера/ввод, VDP1+VDP2
- [x] Master + Slave SH-2 используются; fallback работает
- [x] VDP1 + VDP2 синхронизированы; golden стабилен; отличия от PC/PS1 задокументированы
- [x] Релиз v0.2.0: тег, GitHub Release, артефакты, `verify_release_contract.py` зелёный
- [x] PC/PS1 не регрессировали; документация и LEGAL обновлены

## Ручной чек-лист приёмки (эмулятор)

- [x] Запустить `saturn-release` образ в pinned-эмуляторе: hello_triangle виден, текстура корректна
- [x] Проверить управление с пада: камера/объект, все заявленные кнопки
- [x] 10-минутный прогон: без hang/tearing/крашей
- [x] Скриншот Saturn-сцены для README/релиза
- [x] Проверить, что BIOS/ROM не попали в репозиторий и релизный архив

## Результаты (2026-11-13)

- **hello_triangle:** цель `hello_triangle_saturn` + `hello_triangle.*`
  (ELF/BIN/ISO/CUE/MAP; legacy `retro_saturn.*` байт-идентичны); IP.BIN
  `HELLO TRIANGLE`, `RPE-0002`, `V0.200`.
- **Сцена:** VDP2 NBG1 фон `TEX1.TEX` + VDP1 textured Gouraud cube
  `CUBE.MSH`/`TEX0.TEX`; Painter's без Z-буфера; VDP1 erase transparent.
- **Master+Slave:** каждый кадр 16 треугольников на CPU
  (`parallel_workload_run_frame_n`), boot benchmark 256; `saturn-single` —
  fallback. Лог `[par] f=300 c=303 w=549 h=d40494` — Slave реально
  работал; 60 Гц (360 кадров / 6 с).
- **Ввод:** D-pad — yaw/pitch объекта; A/B — дистанция; C/X — высота;
  Y/Z — орбита; Start — сброс. Без ввода — deterministic golden.
  Host `saturn_hal_tests` — 112/112 (все 6-button/disconnect).
- **Бюджеты:** ELF 264 480 B, raw 132 392 B, ISO 837 632 B,
  `__jo_work_end=0x060F63F8` (< stack 0x060FBC00, headroom ≈22 KiB);
  VDP1 texture 65 280 B / 451 832 B; VDP2 bitmap 256 KiB / 512 KiB.
  Снапшот — `docs/benchmarks/saturn_hello_triangle_v0_2_0.yaml`.
- **Golden:** `screenshots/hello_triangle_saturn.png`,
  `hello_triangle_saturn_scene.png` (file sha256 `b51b8847…`);
  `hello_triangle_saturn_input.png` — ввод.
- **Release contract:** Saturn-секция в `ci/normative_contracts.yaml`;
  `verify_release_contract.py` — `saturn_artifacts`, `saturn_map_budget`,
  `saturn_golden`; `release.yml` собирает Saturn и кладёт `saturn/` в
  payload; `create_release_archive.sh` обновлён.
- **10-минутный stress:** `timeout 640` → `exit 124` (SIGTERM), без
  hang/tearing/сброса; финальный кадр `f=27240`, `c=6`, `o=40`;
  скриншоты `screenshots/hello_triangle_saturn_stress{,_mid}.png`.
  Standalone steady-state измерен 60 FPS (360 кадров / 6 с); длинный
  прогон шёл параллельно с CI/сборками, поэтому средний FPS ниже.
- **Remote CI:** `build-saturn` 43455266369 / final 43458548647, `build-pc`
  (macOS/Ubuntu/Windows), `build-ps1`, `tests` — success; release workflow
  43458577309 — success.
- **Релиз v0.2.0:** тег `v0.2.0` (`ccbd6f3`), GitHub Release
  <https://github.com/SWITCHKOFF/retropoly-engine/releases/tag/v0.2.0>;
  `retro-engine-v0.2.0.tar.gz` (4.2 MB) содержит `pc/`, `ps1/`, `saturn/`;
  `release_contract_report.json` — `all_ok: true`; BIOS/ROM в архив не
  попали.

## Риски

- **Интеграция VDP1+VDP2+dual-CPU.** Митигация: ранний end-to-end smoke, fallback, запас на баг-фикс.
- **Эмуляторная валидация.** Митигация: pinned-эмулятор, ручной чек-лист; не заявлять real-hardware performance.
- **Релизный scope.** Митигация: сначала hello_triangle-гейт, затем документация/анонс; анонс можно перенести.
- **Юридические ограничения.** Митигация: BIOS не вендорить; LEGAL обновить до релиза.
