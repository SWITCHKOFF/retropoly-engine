# Consumer smoke SDK

Отдельный CMake-проект, доказывающий, что SDK самодостаточен:

```sh
cmake -S tests/sdk/consumer -B /tmp/rpe-consumer -G Ninja \
  -DCMAKE_PREFIX_PATH=/path/to/sdk \
  -DRETRO_SDK_DIR=/path/to/sdk
cmake --build /tmp/rpe-consumer
/tmp/rpe-consumer/sdk_probe --version
/tmp/rpe-consumer/hello_triangle_sdk/hello_triangle --version
```

Если `RetroPolyEngine::retro_engine`/`::hal`/`::log` или заголовки
`<retro_engine/...>` отсутствуют, configure падает — это release-gate проверка.
