# ModernP2P

Desktop (Windows / macOS / Linux) peer-to-peer file transfer app built with Qt 6.11.0.

## Layout

| Path | Purpose |
|---|---|
| `src/core` | Device code generation and core logic |
| `qml/` | UI (staging area, peer code input, Requests, local code) |
| `tests/` | QtTest unit tests |
| `.github/workflows/` | CI builds for all three platforms |

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Requires Qt 6.11.0 and CMake 3.25+.
