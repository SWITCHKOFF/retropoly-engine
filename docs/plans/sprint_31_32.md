# Спринт 31–32 — Hello Triangle на Saturn и релиз v0.2.0 (недели 61–64)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13).
> Источник задач: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md) §4 «Будущие итерации» — Saturn, S21–32.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-11):** входные критерии S29–30 выполнены; план готов к
> старту (hello_triangle Saturn + релиз v0.2.0).

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

- [ ] [B] Saturn-цель `hello_triangle`: `main_saturn.cpp`, загрузка `.mesh`/`.tex` с CD-образа, сцена, камера, цикл 60 Гц
- [ ] [B] VDP1+VDP2 в одном кадре: 3D-объект поверх фона; оба SH-2 задействованы; fallback на single-CPU
- [ ] [B] Сборка bootable-образа с ассетами; артефакт CI; PC/PS1 цели не ломать
- [ ] [B] Управление с 6-button pad: движение камеры/объекта; детерминированный ввод
- [ ] [B] CMake preset/targets: `saturn-release` + пример; документация запуска

**Неделя 62 — Корректность и бюджеты:**

- [ ] [B] Golden/reference-сравнение PC/PS1/Saturn; задокументировать ожидаемые отличия (320×224, quad-конверсия, порядок)
- [ ] [B] Проверить текстуры, Gouraud, clip/cull, отсутствие Z-артефактов; malformed-ассеты безопасно отвергаются
- [ ] [B] Бюджеты: Work RAM Low/High, VDP1/VDP2 VRAM, размер бинарника/образа; зафиксировать в docs
- [ ] [B] Input-тесты: все кнопки, disconnect, повторный запуск
- [ ] [B] Обновить `docs/ASSETS.md`/`docs/SATURN.md` по фактическому пайплайну

**Неделя 63 — QA и производительность:**

- [ ] [B] 10-минутный стресс в pinned-эмуляторе; frame pacing; отсутствие hang/tearing
- [ ] [B] Замер загрузки Master/Slave, fillrate VDP1/VDP2; baseline в `docs/benchmarks/saturn_reference_environment.yaml`
- [ ] [B] Скриншоты PC/PS1/Saturn; README/демо-материалы
- [ ] [B] CI: сборка+артефакт; проверить, что `build-pc`, `build-ps1`, `tests` зелёные
- [ ] [B] TROUBLESHOOTING: toolchain, BIOS/эмулятор, CD-образ, типовые ошибки

**Неделя 64 — Релиз v0.2.0:**

- [ ] [B] Документация: README, BUILDING, ARCHITECTURE, API, ASSETS, LEGAL, TROUBLESHOOTING, `docs/SATURN.md`, CHANGELOG [0.2.0]
- [ ] [B] `ci/normative_contracts.yaml`: Saturn-секция (preset, toolchain pin, бюджеты, разрешение, golden)
- [ ] [B] `scripts/verify_release_contract.py`: Saturn-asserts (сборка, ассеты, память/VRAM, бинарник, golden); релиз без валидатора недействителен
- [ ] [B] `.github/workflows/release.yml`: Saturn-артефакты в payload; `create_release_archive.sh`; тег `v0.2.0`
- [ ] [B] GitHub Release + анонс; обновить roadmap (следующий — N64 v0.3.0)
- [ ] [B] Финальный чек-лист v0.2.0: 3 ОС CI green; PC/PS1/Saturn сборки; unit+sanitizers; примеры; RAM/VRAM; malformed; golden; 0 blocker/critical

## Критерии приёмки спринта 31–32

- [ ] Saturn-бинарник компилируется с pinned toolchain и загружается в эмуляторе
- [ ] Hello Triangle работает на Saturn: текстура, Gouraud, камера/ввод, VDP1+VDP2
- [ ] Master + Slave SH-2 используются; fallback работает
- [ ] VDP1 + VDP2 синхронизированы; golden стабилен; отличия от PC/PS1 задокументированы
- [ ] Релиз v0.2.0: тег, GitHub Release, артефакты, `verify_release_contract.py` зелёный
- [ ] PC/PS1 не регрессировали; документация и LEGAL обновлены

## Ручной чек-лист приёмки (эмулятор)

- [ ] Запустить `saturn-release` образ в pinned-эмуляторе: hello_triangle виден, текстура корректна
- [ ] Проверить управление с пада: камера/объект, все заявленные кнопки
- [ ] 10-минутный прогон: без hang/tearing/крашей
- [ ] Скриншот Saturn-сцены для README/релиза
- [ ] Проверить, что BIOS/ROM не попали в репозиторий и релизный архив

## Риски

- **Интеграция VDP1+VDP2+dual-CPU.** Митигация: ранний end-to-end smoke, fallback, запас на баг-фикс.
- **Эмуляторная валидация.** Митигация: pinned-эмулятор, ручной чек-лист; не заявлять real-hardware performance.
- **Релизный scope.** Митигация: сначала hello_triangle-гейт, затем документация/анонс; анонс можно перенести.
- **Юридические ограничения.** Митигация: BIOS не вендорить; LEGAL обновить до релиза.
