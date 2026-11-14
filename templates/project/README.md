# Шаблон проекта RetroPolyEngine

Минимальный проект, который собирается против установленного SDK:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/retro-engine-sdk
cmake --build build
./build/retro_project
```

Шаблон использует только публичный API (`<retro_engine/retro_engine.h>`);
внутренние заголовки движка не требуются.
