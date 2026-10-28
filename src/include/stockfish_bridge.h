// stockfish_bridge.h – minimal C API around the embedded Stockfish UCI loop.
//
// The engine runs its normal UCI loop on a dedicated std::thread. Its
// std::cin / std::cout are redirected to thread-safe line queues, so the
// host talks to it exactly like a GUI talks to a stockfish process.

#ifndef STOCKFISH_BRIDGE_H
#define STOCKFISH_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/// Starts the UCI loop on its own thread. Idempotent: calling it while the
/// engine is already running is a no-op. If the engine has previously quit
/// (via the UCI "quit" command) it is started again.
void sf_start(void);

/// Like sf_start(), but reports whether the engine thread could be created:
/// 0 = running (started now or already before), -1 = thread creation failed.
int sf_try_start(void);

/// Replacement for Stockfish's fatal `exit()` calls where the engine runs inside an
/// app process (iOS and Android: redirected in onyx_arch.h). Ends only the engine: marks it
/// stopped, closes its queues (the reader sees EOF) and parks the calling thread for the
/// rest of the process lifetime. Never returns, never ends the process.
#ifdef __cplusplus
[[noreturn]]
#endif
void sf_engine_fatal(int code);

/// Queues one UCI command line (without trailing newline) for the engine.
void sf_send(const char* line);

/// Blocks until one output line (without newline) is available and copies it
/// (NUL-terminated) into `buf`, truncating to `cap - 1` bytes.
/// Returns the number of bytes copied, or -1 when the engine has terminated
/// and no more output is pending.
int sf_recv(char* buf, int cap);

/// 1 while the UCI loop thread is alive, 0 otherwise.
int sf_is_running(void);

#ifdef __cplusplus
}
#endif

#endif /* STOCKFISH_BRIDGE_H */
