# Спринт 23–24 — HAL Saturn на Master SH-2 (недели 45–48)

> Детальный план спринта. Родительский документ: [`DEVELOPMENT_PLAN.md`](../DEVELOPMENT_PLAN.md) (§13).
> Источник задач: [`RetroEngine_Engineering_Documentation_v1.43.md`](../RetroEngine_Engineering_Documentation_v1.43.md) §4 «Будущие итерации» — Saturn, S21–32.
> Легенда: **[B]** blocking · **[H]** high · **[M]** medium (можно перенести).
> **Статус (2026-11-09):** план не начат; пост-MVP v0.2.0.

## Карточка спринта

| Поле | Значение |
|---|---|
| Спринты | 23–24 |
| Недели | 45–48 — 13.09.2027 – 10.10.2027 |
| Веха | v0.2.0 — Sega Saturn (итерация 3) |
| Цель | Рабочий HAL на Master SH-2: lifecycle, video init, ввод, CD-ROM ФС, время/логи/память; рендер — только очистка экрана |
| Входные критерии | Спринт 21–22 завершён (toolchain, preset, skeleton) ([`sprint_21_22.md`](sprint_21_22.md)) |
| На критическом пути | Да |
| Выходные артефакты | `src/hal/saturn/platform_saturn.cpp`, `input_saturn.cpp`, `filesystem_saturn.cpp`, `render_saturn.cpp` (skeleton), bootable-образ, `build-saturn.yml`, тесты, обновлённые docs |
| Следующий спринт | [`sprint_25_26.md`](sprint_25_26.md) |

## PERT-оценка

| O (ч) | M (ч) | P (ч) | E = (O+4M+P)/6 | σ = (P−O)/6 | E+2σ (95%) | Доступно | Вывод |
|---|---|---|---|---|---|---|---|
| 35 | 55 | 80 | 55.8 | 7.5 | 70.8 | 40–60 ч | ⚠ 93% номинала; риск toolchain/память; ранний резерв и перенос части [H] при просадке |

## Задачи

**Неделя 45 — Lifecycle и видео:**

- [ ] [B] `Platform_Init`: сброс VDP1/VDP2, инициализация дисплея 320×224/NTSC, VBlank; полный откат при любой ошибке
- [ ] [B] `Platform_Shutdown`: остановка прерываний, освобождение engine-owned ресурсов, идемпотентность
- [ ] [B] `Platform_PumpEvents`, `Platform_SwapBuffers`, `Platform_GetTimeMs`, `Platform_SleepMs`: контракты как у PC/PS1, но без host-зависимостей
- [ ] [B] Логирование: sink в debug-консоль эмулятора и/или VDP2-текст; `Log_Message` без `printf` в runtime
- [ ] [B] Память: арены в WRAM-L/WRAM-H, `LinearAllocator`/`StackAllocator`/`PoolAllocator`, `MemoryProfiler`; static_asserts бюджета
- [ ] [B] Тест lifecycle: повторный `Platform_Init/Shutdown`, вызовы после shutdown, ошибка инициализации — без крашей

**Неделя 46 — Ввод:**

- [ ] [B] `input_saturn.cpp`: чтение 6-button pad через SMPC; active-low семантика; D-pad, A/B/C/X/Y/Z, L/R, Start
- [ ] [B] Маппинг в `INPUT_PAD_*`: не ломать PS1-раскладку; для Saturn-специфичных кнопок — расширение enum только перед `INPUT_PAD_COUNT`
- [ ] [B] `Platform_PumpEvents` опрашивает pad ровно один раз за кадр; состояние «нет пада» и «пад отключён» — безопасные значения
- [ ] [B] Тесты маппинга: направления, лицевые кнопки, шоулдеры, Start, idle, disconnect
- [ ] [B] Документация раскладки в `docs/SATURN.md` и `docs/API.md` (раздел Input)

**Неделя 47 — CD-ROM ФС и образ:**

- [ ] [B] `filesystem_saturn.cpp`: read-only ISO9660, `FileSystem_LoadFile`, 2048-байтовые секторы, имена 8.3, проверка размера, выделение из арены
- [ ] [B] `FileSystem_FreeFile`: корректный возврат буфера; при ошибке чтения — 0 байт persistent-памяти
- [ ] [B] Сборка bootable-образа: `saturn-disc.xml`/скрипт toolchain; включить `assets/build/*.mesh` и `*.tex`; артефакт CI
- [ ] [B] Тесты: валидный файл, отсутствующий файл, обрезанный файл, неверный путь, host-путь (должен быть отвергнут)
- [ ] [B] Документация: формат образа, имена файлов, ограничения CD-ROM в `docs/BUILDING.md`/`docs/ASSETS.md`

**Неделя 48 — Render skeleton и гейт:**

- [ ] [B] `render_saturn.cpp`: `Render_Init/Shutdown`, `Render_BeginFrame/EndFrame`, `Render_Clear` через VDP2 backdrop; command buffer — каркас
- [ ] [B] Lifecycle-контракт: ровно один `Render_Init` после `Platform_Init`; нарушение — диагностика, не silent crash
- [ ] [B] Backend-контракты: переполнение command buffer/VRAM возвращает `false` + диагностику; silent drop запрещён
- [ ] [B] Golden: фиксированный цвет очистки, readback/скриншот, детерминированный хэш
- [ ] [B] CI: `build-saturn.yml` собирает образ и публикует artifact; ручной прогон 5 минут в pinned-эмуляторе
- [ ] [B] CHANGELOG + документация; проверить, что PC/PS1 тесты зелёные

## Критерии приёмки спринта 23–24

- [ ] `Platform_Init/Shutdown` идемпотентны; Saturn-бинарник стабильно работает 5 минут в эмуляторе
- [ ] Ввод с 6-button pad работает; маппинг совместим с существующим `INPUT_PAD_*`
- [ ] `.mesh`/`.tex` читаются с CD-образа; malformed/отсутствующие файлы безопасно отвергаются
- [ ] `Render_Clear` даёт стабильный golden; переполнения возвращают ошибку, а не молчаливый drop
- [ ] CI `build-saturn.yml` зелёный; PC/PS1 сборки и тесты не регрессировали

## Риски

- **Шины и банки WRAM-L/WRAM-H.** Митигация: ранний ledger, статические арены, измерение фактического использования.
- **SMPC/пад.** Митигация: начать с одного пада, сверить с SDK-примером, тесты на disconnect.
- **CD-ROM чтение.** Митигация: синхронный read-only путь для v0.2.0; background streaming не входит в scope.
- **Отсутствие stdout.** Митигация: debug-консоль эмулятора + VDP2-текст; логирование не должно требовать host-ФС.
