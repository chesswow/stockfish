// bridge.cpp – runs Stockfish's UCI loop in-process on a background thread,
// with std::cin / std::cout redirected to thread-safe line queues.
//
// This replaces main.cpp. The start-up sequence mirrors main():
//   Attacks::init(); Position::init();
//   UCIEngine uci(CommandLine(argc, argv));
//   Tune::init(uci.engine_options());
//   uci.loop();

#include "include/stockfish_bridge.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <pthread.h>
#include <streambuf>
#include <string>
#include <thread>
#include <utility>

#ifdef __ANDROID__
    #include <android/log.h>
#endif

#include "attacks.h"
#include "misc.h"
#include "position.h"
#include "tune.h"
#include "uci.h"

namespace {

// A blocking FIFO of complete lines.
class LineQueue {
   public:
    void push(std::string line) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::move(line));
        }
        cv_.notify_one();
    }

    // Blocks until a line is available or the queue is closed.
    // Returns false only when closed and drained.
    bool pop(std::string& out) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&] { return closed_ || !queue_.empty(); });
        if (queue_.empty())
            return false;
        out = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        cv_.notify_all();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
        closed_ = false;
    }

   private:
    std::mutex              mutex_;
    std::condition_variable cv_;
    std::deque<std::string> queue_;
    bool                    closed_ = false;
};

// std::streambuf feeding std::cin from a LineQueue. `underflow` blocks until
// a line is available; each line is delivered with a trailing '\n' so that
// std::getline works unchanged. When the queue is closed it reports EOF,
// which makes UCIEngine::loop() treat it as "quit".
class InputLineBuf : public std::streambuf {
   public:
    explicit InputLineBuf(LineQueue& q) :
        queue_(q) {}

   protected:
    int_type underflow() override {
        if (gptr() < egptr())
            return traits_type::to_int_type(*gptr());

        std::string line;
        if (!queue_.pop(line))
            return traits_type::eof();

        current_ = std::move(line);
        current_.push_back('\n');
        char* base = &current_[0];
        setg(base, base, base + current_.size());
        return traits_type::to_int_type(*gptr());
    }

   private:
    LineQueue&  queue_;
    std::string current_;
};

// std::streambuf collecting std::cout into complete lines and pushing each
// finished line into a LineQueue. Stockfish writes from several threads but
// always through sync_cout/sync_endl (mutex-protected), so lines never
// interleave; we still guard our own state so partial writes are safe.
class OutputLineBuf : public std::streambuf {
   public:
    explicit OutputLineBuf(LineQueue& q) :
        queue_(q) {}

   protected:
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof()))
            return traits_type::not_eof(ch);
        std::lock_guard<std::mutex> lock(mutex_);
        append(static_cast<char>(traits_type::to_char_type(ch)));
        return ch;
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (std::streamsize i = 0; i < n; ++i)
            append(s[i]);
        return n;
    }

    int sync() override { return 0; }  // lines are flushed on '\n'

   private:
    void append(char c) {
        if (c == '\n')
        {
            // Strip a trailing '\r' for good measure.
            if (!pending_.empty() && pending_.back() == '\r')
                pending_.pop_back();
            queue_.push(std::move(pending_));
            pending_.clear();
        }
        else
            pending_.push_back(c);
    }

    LineQueue&  queue_;
    std::mutex  mutex_;
    std::string pending_;
};

struct Bridge {
    LineQueue     input;
    LineQueue     output;
    InputLineBuf  inBuf{input};
    OutputLineBuf outBuf{output};

    std::mutex lifecycle;
    // pthread instead of std::thread: the engine is built with -fno-exceptions, so a failing
    // std::thread constructor (std::system_error) would end in std::terminate -> SIGABRT.
    pthread_t       thread{};
    bool            hasThread      = false;
    bool            running        = false;
    // Incremented per start; a late end of an older instance must not touch the current one.
    unsigned        generation     = 0;
    // Set by sf_engine_fatal(): the engine thread of that instance is parked and must never be
    // joined (it does not return), only detached.
    bool            fatal          = false;
    bool            streamsSwapped = false;
    std::streambuf* savedCin       = nullptr;
    std::streambuf* savedCout      = nullptr;
};

Bridge& bridge() {
    static Bridge* b = new Bridge();  // intentionally leaked: lives for the process
    return *b;
}

struct EngineStart {
    Bridge*  bridge;
    unsigned generation;
};

void engine_main(Bridge* b, unsigned generation) {
    using namespace Stockfish;

    Attacks::init();
    Position::init();

    static char  arg0[] = "stockfish";
    static char* argv[] = {arg0, nullptr};

    {
        auto cli = CommandLine(1, argv);
        auto uci = std::make_unique<UCIEngine>(std::move(cli));
        Tune::init(uci->engine_options());
        uci->loop();  // returns after "quit" or EOF on std::cin
    }

    // Mark the engine as stopped before closing the output: once the reader sees EOF,
    // sf_start() must already be able to start a fresh instance (QA r1 ENG-15).
    // An older instance (ended after a fatal error) leaves the current one alone.
    {
        std::lock_guard<std::mutex> lock(b->lifecycle);
        if (b->generation != generation)
            return;
        b->running = false;
    }
    b->output.close();
}

void* engine_thread(void* arg) {
    EngineStart start = *static_cast<EngineStart*>(arg);
    delete static_cast<EngineStart*>(arg);
    engine_main(start.bridge, start.generation);
    return nullptr;
}

// Stack of the UCI loop thread. Searches run on Stockfish's own threads (8 MB); the loop only
// parses commands and loads the net, but gets more than the platform default (iOS 512 KB).
constexpr size_t kEngineThreadStack = 4 * 1024 * 1024;

}  // namespace

extern "C" {

int sf_try_start(void) {
    Bridge& b = bridge();
    std::lock_guard<std::mutex> lock(b.lifecycle);
    if (b.running)
        return 0;

    if (b.hasThread)
    {
        // A previous instance has quit: reap it. After a fatal error its thread is parked
        // forever, so joining would block this caller; it is detached instead.
        if (b.fatal)
            pthread_detach(b.thread);
        else
            pthread_join(b.thread, nullptr);
        b.hasThread = false;
    }
    b.fatal = false;

    b.input.reset();
    b.output.reset();

    if (!b.streamsSwapped)
    {
        std::ios::sync_with_stdio(false);
        b.savedCin  = std::cin.rdbuf(&b.inBuf);
        b.savedCout = std::cout.rdbuf(&b.outBuf);
        b.streamsSwapped = true;
    }

    auto* start = new (std::nothrow) EngineStart{&b, b.generation + 1};
    if (start == nullptr)
        return -1;

    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0)
    {
        delete start;
        return -1;
    }
    pthread_attr_setstacksize(&attr, kEngineThreadStack);
    const int rc = pthread_create(&b.thread, &attr, engine_thread, start);
    pthread_attr_destroy(&attr);
    if (rc != 0)
    {
        delete start;
        return -1;
    }

    b.generation += 1;
    b.hasThread = true;
    b.running   = true;
    return 0;
}

void sf_start(void) { (void) sf_try_start(); }

void sf_engine_fatal(int code) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_ERROR, "ChessWOW-Engine",
                        "Stockfish fatal error (exit %d): engine stopped, app keeps running", code);
#else
    (void) code;
#endif
    Bridge& b = bridge();
    {
        std::lock_guard<std::mutex> lock(b.lifecycle);
        if (b.running)
        {
            b.running = false;
            b.fatal   = true;
            // The current instance is dead from now on; its own end (if any) must not
            // touch a later instance.
            b.generation += 1;
        }
    }
    // The reader sees EOF and reports the engine as gone; a UCI loop still waiting for
    // input sees EOF as well and stops reading.
    b.output.close();
    b.input.close();
    // Never return (the callers are [[noreturn]] exit paths) and never end the process:
    // park this thread for the rest of the process lifetime.
    for (;;)
        std::this_thread::sleep_for(std::chrono::hours(1));
}

void sf_send(const char* line) {
    if (line == nullptr)
        return;
    bridge().input.push(std::string(line));
}

int sf_recv(char* buf, int cap) {
    if (buf == nullptr || cap <= 0)
        return -1;

    std::string line;
    if (!bridge().output.pop(line))
        return -1;

    int n = static_cast<int>(line.size());
    if (n > cap - 1)
        n = cap - 1;
    std::memcpy(buf, line.data(), static_cast<size_t>(n));
    buf[n] = '\0';
    return n;
}

int sf_is_running(void) {
    Bridge& b = bridge();
    std::lock_guard<std::mutex> lock(b.lifecycle);
    return b.running ? 1 : 0;
}

}  // extern "C"
