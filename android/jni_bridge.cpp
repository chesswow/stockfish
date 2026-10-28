// jni_bridge.cpp – JNI wrapper around the C bridge of the vendored Stockfish (bridge.cpp:
// sf_start / sf_send / sf_recv / sf_is_running). Lines cross the boundary as UTF-8 byte
// arrays, so file paths with any characters survive (NewStringUTF expects modified UTF-8).
//
// Nothing here may throw across the JNI boundary: the library is built with -fno-exceptions, so
// a C++ exception would end in std::terminate (SIGABRT). Thread creation therefore goes through
// sf_try_start() (pthread, error code instead of std::system_error), and Stockfish's fatal exit()
// calls only end the engine (sf_engine_fatal, see android_arch.h).

#include <jni.h>

#include <string>
#include <vector>

#include "stockfish_bridge.h"

namespace {
constexpr int kLineCapacity = 1 << 16;
}

extern "C" {

// JNI_TRUE when the UCI loop runs (started now or before), JNI_FALSE when no thread could be created.
JNIEXPORT jboolean JNICALL Java_app_chesswow_engine_StockfishNative_nativeStart(JNIEnv*, jclass) {
    return sf_try_start() == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_app_chesswow_engine_StockfishNative_nativeSend(JNIEnv* env, jclass, jbyteArray line) {
    if (line == nullptr)
        return;
    const jsize n = env->GetArrayLength(line);
    std::string s(static_cast<size_t>(n), '\0');
    if (n > 0)
        env->GetByteArrayRegion(line, 0, n, reinterpret_cast<jbyte*>(&s[0]));
    sf_send(s.c_str());
}

// Blocks until the engine prints a line; null once the engine has terminated.
JNIEXPORT jbyteArray JNICALL Java_app_chesswow_engine_StockfishNative_nativeReceive(JNIEnv* env, jclass) {
    thread_local std::vector<char> buf(kLineCapacity);
    const int n = sf_recv(buf.data(), kLineCapacity);
    if (n < 0)
        return nullptr;
    jbyteArray out = env->NewByteArray(n);
    // NewByteArray failed (OutOfMemoryError pending): Kotlin sees the exception, not a crash.
    if (out != nullptr && n > 0)
        env->SetByteArrayRegion(out, 0, n, reinterpret_cast<const jbyte*>(buf.data()));
    return out;
}

JNIEXPORT jboolean JNICALL Java_app_chesswow_engine_StockfishNative_nativeIsRunning(JNIEnv*, jclass) {
    return sf_is_running() != 0 ? JNI_TRUE : JNI_FALSE;
}

}  // extern "C"
