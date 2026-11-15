# ADR-016 — Atari Jaguar / Jaguar CD: связка зафиксирована, поддержка отложена

- **Статус:** Accepted (2026-11-15), **DEFERRED — до лучших времён**;
  старт — только по явному личному разрешению maintainer
- **Спринты:** S47–52 (v0.4.0) — **не активируются**
- **Связанные документы:** [`../plans/jaguar_emulator_sdk_answers.md`](../plans/jaguar_emulator_sdk_answers.md),
  [`../plans/jaguar_emulator_sdk_brief.md`](../plans/jaguar_emulator_sdk_brief.md),
  [`../plans/jaguar_legal_audit_v2.md`](../plans/jaguar_legal_audit_v2.md),
  [`../plans/jaguar_legal_hold.md`](../plans/jaguar_legal_hold.md),
  [`ADR-013-release-freeze.md`](ADR-013-release-freeze.md)

## 1. Контекст

Для Atari Jaguar / Jaguar CD были проведены лицензионный аудит и
183-вопросный бриф сравнения эмуляторов и SDK. Результат:

- готового MIT/BSD-эмулятора Jaguar CD нет;
- лучший открытый CD-эмулятор — `libretro/virtualjaguar-libretro`
  (GPL-3.0) со встроенными Atari boot ROM/CD BIOS;
- готового MIT-clean Jaguar CD SDK нет: `jaguar-sdk` без top-level
  LICENSE и с Atari CD BIOS/сканами, `jaglib` MIT кроме Atari
  `jagcore.h`, `rmvlib` LGPL-2.1, `rmac`/`rln` без явной лицензии,
  `cdi-maker` без LICENSE;
- технически лучший эмулятор (BigPEmu) закрыт и ограничивает
  commercial use/распространение; MAME для CD — `NOT_WORKING`;
- `vjag` (MIT) не умеет Jaguar CD, save states и boot ROM.

Одновременно действуют два требования maintainer:

1. движок и поставляемый SDK остаются **MIT**;
2. **Atari Jaguar CD — основной target**, а не «фаза после картриджей».

Ни один готовый кандидат не проходит все gate целиком. Юридический
gate остаётся `conditional` из-за встроенных BIOS, отсутствия
документированного разрешения Atari и неопределённости по
export-control/trademark.

## 2. Решение

1. **Добавление поддержки платформы Atari Jaguar / Jaguar CD
   откладывается «до лучших времён».** S47–52 не активируются; код,
   toolchain, ROM, CD-образы и payload-компоненты Jaguar не создаются.
   Планы S47–52 и research-документы остаются draft/reference, а не
   обязательствами по срокам и не backlog к исполнению.
   **Активация не происходит автоматически:** ни снятие LEGAL HOLD, ни
   закрытие лицензионных вопросов, ни наличие свободного времени не
   являются разрешением начать. Нужно отдельное явное личное разрешение
   maintainer — «начинаем Jaguar», с датой и объёмом работ.
2. На случай возобновления фиксируется следующая связка:
   - **Эмулятор:** `libretro/virtualjaguar-libretro` (GPL-3.0;
     master `8543bbf`, README: v3.9.0) — **только внешний процесс**,
     без линковки в engine/UI/SDK и без включения в release payload.
     HLE CD — по умолчанию; реальный BIOS — только пользовательский,
     не бандлится. Для поставки эмулятора — HLE-only форк без
     BIOS-массивов, отдельный GPL-3.0 процесс с source offer.
   - **SDK:** MIT-профиль — `m68k-elf-gcc` + binutils + `lyxass` (MIT)
     + собственный HAL/startup/linker + clean-room определения
     регистров. `jaglib` — optional после замены `jagcore.h`;
     `rmvlib` — optional LGPL, не default; `jaguar-sdk`, `rmac`/`rln`,
     Atari headers/BIOS — не вендорятся.
   - **CD:** собственный MIT-инструмент `jagcd` (boot track,
     `.cue/.bin`, manifest, size/budget check). `cdi-maker` — только
     после разрешения автора или замены; Atari CD BIOS не включается.
3. Политика остаётся прежней: `jaguar-sdk`/Atari docs/DOS-бинарники —
   только env-install пользователем; эмулятор и BIOS отсутствуют в
   CI-артефактах, SDK, UI-пакете и release payload; метрики — с
   пометкой «emulator validation, not hardware».
4. ADR-017 (детальная интеграция Jaguar) **не создаётся** до явного
   личного разрешения maintainer начать Jaguar. Если платформа будет
   возвращена, ADR-017 фиксирует toolchain-пины, HAL, CD-authoring и
   release-контракт; ADR-016 остаётся историческим основанием.

## 3. Последствия

- **v0.4.0 / Jaguar не является текущим milestone.** Дорожная карта не
  обещает Jaguar-релиз; после N64 следующими остаются аудио/ФС
  (v0.6.0–v0.8.0), 3DO исключён из roadmap.
- Планы `sprint_47_48.md`, `sprint_49_50.md`, `sprint_51_52.md`
  сохраняются как reference/draft; их даты и PERT-оценки не являются
  commitments, а чек-боксы **не выполняются** до личного разрешения
  maintainer.
- `docs/plans/jaguar_legal_hold.md` остаётся в силе; статус hold
  дополняется статусом **DEFERRED**.
- Любое возобновление требует: явного личного разрешения maintainer,
  юридической проверки, clean-room policy, лицензионных уточнений
  (`cdi-maker`, `rmac`/`rln`, `jaglib`, `rmvlib`) и проверки HLE-only
  сборки VJ.
- MIT-профиль проекта не меняется: Jaguar-компоненты не добавляются в
  default build/payload, поэтому текущий MIT-статус движка сохраняется.

## 4. Альтернативы

| Вариант | Почему отклонён |
|---|---|
| `vjag` (MIT) как основной эмулятор | Нет Jaguar CD, save states, boot ROM — не проходит CD-first требования |
| BigPEmu как основной | Закрытый; commercial/redistribution restrictions; нет macOS и документированного headless |
| MAME как acceptance-эмулятор | `jaguar`/`jaguarcd` = `MACHINE_NOT_WORKING`; только трассировка |
| `jaguar-sdk` как SDK | Нет top-level LICENSE; Atari samples/docs/CD BIOS; нельзя вендорить |
| Cartridge-only v0.4.0 | Противоречит требованию «Jaguar CD — основной target»; CD остался бы нерешённым |
| HLE-only форк VJ + поставка | Технически возможен, но требует отдельного форка/PR и GPL-поставки; отложен вместе с платформой |

## 5. Источники

- [`../plans/jaguar_emulator_sdk_answers.md`](../plans/jaguar_emulator_sdk_answers.md) — 183 ответа, связка и counts.
- [`../plans/jaguar_legal_audit_v2.md`](../plans/jaguar_legal_audit_v2.md) — лицензионный аудит.
- [`../plans/jaguar_legal_hold.md`](../plans/jaguar_legal_hold.md) — hold и условия возобновления.
- VJ: <https://github.com/libretro/virtualjaguar-libretro>
- `vjag`: <https://github.com/HelgeSverre/jaguar>
- `jaguar-sdk`: <https://github.com/cubanismo/jaguar-sdk>
- `lyxass`: <https://codeberg.org/42Bastian/lyxass>
- `jaglib`: <https://github.com/toarnold/jaglib>
- `rmvlib`: <https://github.com/theRemovers/rmvlib>
- `cdi-maker`: <https://github.com/cubanismo/cdi-maker>
- GCC RLE: <https://www.gnu.org/licenses/gcc-exception-3.1.html>
