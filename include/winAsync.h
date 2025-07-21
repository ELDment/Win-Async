#pragma once

#include <windows.h>
#include <functional>
#include <vector>
#include <deque>
#include <stdexcept>
#include <memory>
#include <chrono>
#include <queue>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <cstdarg>
#include <cstdio>

#ifdef DEBUG_COROUTINE
inline void DebugPrint(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
}
#else
#define DebugPrint(...) ((void)0)
#endif

class Coroutine;
class Scheduler;
struct ExceptionState;
template <typename T>
class CoroutinePromise;
template <typename T>
class Task;

Scheduler* GetCurrentScheduler();
void SetCurrentScheduler(Scheduler* scheduler);

struct IoOperation : public OVERLAPPED {
    IoOperation();
    Coroutine* coroutine;
};

void CaptureException(ExceptionState* es, const EXCEPTION_RECORD& record);
bool HasException(const ExceptionState* es);
void RethrowIfExists(const ExceptionState* es);

class Coroutine {
public:
    enum class State { Ready, Running, Suspended, Finished };

    Coroutine(std::function<void()> f, std::function<void(std::shared_ptr<ExceptionState>)> onDoneCallback, Scheduler* s);
    ~Coroutine();

    void Resume();
    static void YieldExecution();
    static void SuspendExecution();
    bool HasException() const;
    void RethrowExceptionIfAny();

private:
    friend class Scheduler;
    friend void CoroutineTrampoline(void* arg);

    std::function<void()> func;
    std::function<void(std::shared_ptr<ExceptionState>)> onDone;
    State state;
    Scheduler* scheduler;
    void* fiber = nullptr;
    std::shared_ptr<ExceptionState> exceptionState;
    std::shared_ptr<void> promiseHandle;
};

class Scheduler {
public:
    Scheduler();
    explicit Scheduler(size_t numThreads);
    ~Scheduler();

    void Add(std::function<void()> func);
    void Submit(std::function<void()> func);
    void Run();
    void Stop();
    void RegisterHandle(HANDLE handle);
    void Resume(Coroutine* co);
    Coroutine* PollException();
    Coroutine* GetRunningCoroutine() const;
    static void AsyncSleep(uint32_t milliseconds);

    template <typename T, typename Func, typename... Args>
    std::shared_ptr<CoroutinePromise<T>> CreateCoroutine(Func&& func, Args&&... args);

private:
    void WorkerLoop();
    static LONG WINAPI VectoredExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo);

    friend class Coroutine;

    void* mainFiber;
    HANDLE iocpHandle;
    Coroutine* runningCoroutine;
    std::vector<std::unique_ptr<Coroutine>> coroutines;
    void* vehHandle;
    Coroutine* pendingException;

    std::deque<Coroutine*> runnableQueue;
    struct TimerNode {
        std::chrono::steady_clock::time_point wakeupTime;
        Coroutine* coroutine;
        bool operator>(const TimerNode& other) const { return wakeupTime > other.wakeupTime; }
    };
    std::priority_queue<TimerNode, std::vector<TimerNode>, std::greater<TimerNode>> timers;
    std::unordered_set<Coroutine*> sleepingCoroutines;

    bool isThreadPool = false;
    std::vector<std::thread> workers;
    std::deque<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop = false;

public:
    static Scheduler& GetThreadPool();
};

#include "winAsyncTask.h"

inline IoOperation::IoOperation() {
    Internal = InternalHigh = 0;
    Offset = OffsetHigh = 0;
    hEvent = nullptr;
    coroutine = nullptr;
}