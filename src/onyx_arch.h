// onyx_arch.h – SIMD feature selection for the embedded Stockfish build.
//
// Force-included into every Stockfish translation unit (see Package.swift). Upstream
// passes these macros from the Makefile per ARCH; SwiftPM cannot vary flags per CPU
// architecture, so they are derived here from the compiler's own feature macros.
//
// iOS arm64: plain ARMv8 NEON. The deployment target (iOS 17) still includes A12
//   devices, which lack the dot-product extension, so USE_NEON_DOTPROD must stay off
//   unless the compiler was explicitly told the target has it.
// macOS arm64: Package.swift adds `-Xarch_arm64 -march=armv8.2-a+dotprod`; every Apple
//   Silicon Mac supports it, so __ARM_FEATURE_DOTPROD is set there.
// x86_64 (Intel Mac, Intel simulator): Package.swift adds AVX2/SSE4.1/POPCNT via
//   -Xarch_x86_64; every Mac that runs macOS 14 supports them.
#ifndef ONYX_ARCH_H
#define ONYX_ARCH_H

#if defined(__aarch64__) || defined(__arm64__)
    #define USE_NEON 8
    #if defined(__ARM_FEATURE_DOTPROD)
        #define USE_NEON_DOTPROD 1
    #endif
    #define USE_POPCNT 1
#elif defined(__x86_64__)
    #if defined(__AVX2__)
        #define USE_AVX2 1
    #endif
    #if defined(__SSE4_1__)
        #define USE_SSE41 1
    #endif
    #if defined(__SSSE3__)
        #define USE_SSSE3 1
    #endif
    #if defined(__SSE2__)
        #define USE_SSE2 1
    #endif
    #if defined(__POPCNT__)
        #define USE_POPCNT 1
    #endif
#endif

// Stockfish beendet sich bei fatalen Fehlern mit exit(EXIT_FAILURE) (Netz nicht geladen bei "go",
// Speicher für Hash/Netz/Threads nicht zu bekommen, defekte Endspieldatei, Thread nicht startbar).
// In einer App läuft die Engine aber im App-Prozess: exit() von einem Hintergrund-Thread beendet die
// ganze App, und die dabei laufenden statischen Destruktoren anderer Bibliotheken treffen auf noch
// laufende Threads (typisch SIGABRT oder SIGSEGV). Deshalb wird exit in allen Stockfish-Quellen auf
// sf_engine_fatal() umgelenkt (bridge.cpp, iOS und Android): nur die Engine gilt als beendet, der Leser sieht EOF, die
// App meldet "Engine nicht verfügbar" und kann neu starten. <cstdlib> kommt vorher, damit die
// Deklaration von ::exit selbst unberührt bleibt; std::exit wird über die using-Deklaration umgelenkt.
#ifdef __cplusplus
    #include <cstdlib>
extern "C" [[noreturn]] void sf_engine_fatal(int code);
namespace std {
using ::sf_engine_fatal;
}
    #define exit(code) sf_engine_fatal(code)
#endif

#endif  // ONYX_ARCH_H
