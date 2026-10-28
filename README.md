# Stockfish in ChessWOW

This repository contains the complete source code of the chess engine that ships in
[ChessWOW](https://chesswow.app) for iOS, iPadOS, macOS and Android.

The engine is [Stockfish](https://stockfishchess.org) 19, unmodified apart from the
changes listed below. Stockfish is free software by the Stockfish developers (see
[AUTHORS](AUTHORS)), licensed under the GNU General Public License version 3. This
repository is distributed under the same license, see [COPYING](COPYING).

## Upstream version

- Source: https://github.com/official-stockfish/Stockfish
- Tag: `sf_19` (commit `edb0d9db6731067ec50ce619ff372b463bc4dd5d`)
- Neural network: `nn-1a298aa575a0.nnue`, unchanged, from https://tests.stockfishchess.org/api/nn/nn-1a298aa575a0.nnue

## Changes to Stockfish

The engine runs inside the app process instead of as a separate program. Everything
needed for that is in these files:

| File | Change |
|---|---|
| `src/main.cpp` | removed, replaced by `src/bridge.cpp` |
| `src/Makefile`, `src/universal/` | removed, the build uses `Package.swift` or `android/CMakeLists.txt` instead |
| `src/bridge.cpp`, `src/include/stockfish_bridge.h` | new: run the UCI loop on a background thread and exchange UCI lines with the app through queues instead of stdin and stdout |
| `src/onyx_arch.h` | new: selects the SIMD code paths (NEON, AVX2, ...) from the compiler's feature macros, which the upstream Makefile does through its `ARCH` setting. It also redirects `exit()` in the engine code to `sf_engine_fatal()`, which stops only the engine thread |
| `src/uci.cpp`, `src/uci.h` | an invalid command reports `info string CRITICAL ERROR` but no longer calls `exit(1)`, because that would close the app. After an invalid position the engine falls back to the start position |
| `android/jni_bridge.cpp`, `android/android_arch.h` | new: JNI wrapper around `bridge.cpp` and the SIMD selection for Android |

All other files in `src/` are identical to `sf_19`. You can check this with

```
git clone --depth 1 --branch sf_19 https://github.com/official-stockfish/Stockfish.git
diff -r Stockfish/src src
```

## Building

The network file is not part of the repository (about 94 MB). Download it first:

```
./fetch-net.sh
```

**iOS and macOS:** `Package.swift` is a Swift package with the same compiler flags the app
uses. Add it as a local package in Xcode or run `swift build -c release`.

**Android:** `android/CMakeLists.txt` builds `libchesswow_stockfish.so` with the Android NDK,
for example from Gradle with `externalNativeBuild { cmake { path "android/CMakeLists.txt" } }`.

## Contact

Questions about this code: contact@chesswow.app
