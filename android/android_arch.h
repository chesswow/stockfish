// android_arch.h – SIMD feature selection for the Android Stockfish build.
//
// Force-included into every translation unit (see CMakeLists.txt). arm64 and x86_64 use the
// iOS header unchanged (onyx_arch.h derives the macros from the compiler's feature macros):
//   arm64-v8a    -> USE_NEON 8 + POPCNT; dot product only with -Pchesswow.engine.dotprod=true
//                   (adds -march=armv8.2-a+dotprod, so __ARM_FEATURE_DOTPROD is set).
//   x86_64       -> SSE4.1/SSSE3/SSE2/POPCNT, which the Android x86_64 ABI guarantees and the
//                   NDK enables by default; AVX2 is not part of the ABI and stays off.
// armeabi-v7a is not covered by onyx_arch.h (iOS has no 32-bit target): like upstream's
// ARCH=armv7-neon it gets USE_NEON 7 + POPCNT. NEON is mandatory for the Android
// armeabi-v7a ABI and enabled by the NDK by default.
#ifndef CHESSWOW_ANDROID_ARCH_H
#define CHESSWOW_ANDROID_ARCH_H

#include "onyx_arch.h"

#if defined(__arm__) && !defined(__aarch64__)
    #if defined(__ARM_NEON) || defined(__ARM_NEON__)
        #define USE_NEON 7
        #define USE_POPCNT 1
    #endif
#endif

// Die Umlenkung von exit() auf sf_engine_fatal() steht in onyx_arch.h und gilt damit für iOS und Android.

#endif  // CHESSWOW_ANDROID_ARCH_H
