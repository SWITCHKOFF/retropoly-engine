# Contributing to RetroPolyEngine

Спасибо за интерес к проекту! Этот документ описывает рабочий процесс,
требования к коду и review.

## Быстрый старт

1. Форкните репозиторий.
2. Создайте ветку от `develop`:
   ```sh
   git checkout -b feature/short-name
   ```
3. Внесите изменения атомарно (код + тесты + документация).
4. Убедитесь, что все тесты зелёные:
   ```sh
   cmake --preset pc-debug && cmake --build --preset pc-debug
   ctest --preset pc-debug --output-on-failure
   cmake --preset pc-debug-render && cmake --build --preset pc-debug-render
   xvfb-run -a ctest --preset pc-debug-render --output-on-failure
   ```
5. Отформатируйте код:
   ```sh
   clang-format -i src/... tests/... game/... tools/...
   ```
6. Откройте Pull Request в `develop`.

## Ветвление

| Префикс | Назначение |
|---|---|
| `feature/*` | Новая функциональность |
| `fix/*` | Исправление бага |
| `docs/*` | Только документация |
| `release/*` | Подготовка релиза |
| `hotfix/*` | Срочный фикс от `main` |

Релизы мержатся в `main` + тег `v*`; обычная разработка — squash-PR в
`develop`.

## Требования к коду

- **C++14** для движка и тестов.
- **Без плавающей запятой в игровой логике**; допустимы только конверсии
  `fixed_from_float`/`fixed_to_float`, host-only инструменты и тесты.
- **Без heap в runtime**: `new`/`delete`/`malloc`/`free` запрещены в ядре.
- **Без прямого ввода-вывода**: диагностика только через `src/core/log/log.h`.
- **Doxygen** для всех публичных функций.
- **SPDX-заголовок MIT** в начале каждого файла.

## Стиль

См. [`STYLE_GUIDE.md`](STYLE_GUIDE.md). Ключевые моменты:

- 4 пробела, LF, ≤100 символов в строке.
- Имена: `PascalCase` для типов, `snake_case` для функций/переменных,
  `UPPER_SNAKE` для констант/макросов.
- Фигурные скобки: K&R для стейтментов, перенос для определений типов/функций.
- `auto` запрещён в публичном API; `using namespace` запрещён в заголовках.

## Тесты

- Каждый модуль — свой test suite.
- Эталонные значения вычисляются независимо (`double`-модель), не той же
  функцией движка.
- Детерминизм проверяется на raw `fixed_t`.
- Новая функциональность покрывается unit/integration/visual тестами.

## Review-процесс

- PR должен быть зелёным в CI (`tests`, `build-pc`, `build-ps1`).
- Сообщения коммитов на русском или английском, первая строка ≤72 символов.
- Self-review допустим при отсутствии других контрибьюторов.
- Merge в `develop` — squash.

## Release checklist (для мейнтейнеров)

См. [`DEVELOPMENT_PLAN.md`](DEVELOPMENT_PLAN.md) §11 и
[`plans/sprint_19_20.md`](plans/sprint_19_20.md).

## Вопросы

Открывайте [GitHub Discussions](../../discussions) или issue с лейблом
`question`.
