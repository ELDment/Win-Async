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

Scheduler* GetCurrentScheduler();
void SetCurrentScheduler(Scheduler* scheduler);

struct ExceptionState;

struct IoOperation : public OVERLAPPED {
    IoOperation() {
        Internal = InternalHigh = 0;
        Offset = OffsetHigh = 0;
        hEvent = nullptr;
        coroutine = nullptr;
    }
    Coroutine* coroutine;
};

void CaptureException(ExceptionState* es, const EXCEPTION_RECORD& record);
bool HasException(const ExceptionState* es);
void RethrowIfExists(const ExceptionState* es);

class Coroutine {
public:
    enum class State {
        Ready,
        Running,
        Suspended,
        Finished
    };

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
    template <typename T>
    friend class CoroutinePromise;

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
    auto CreateCoroutine(Func&& func, Args&&... args) {
        auto promise = std::make_shared<CoroutinePromise<T>>();
        auto task = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);

        auto wrappedFunc = [promise, task]() mutable {
            try {
                if constexpr (std::is_void_v<T>) {
                    task();
                    promise->SetResult();
                } else {
                    promise->SetResult(task());
                }
            } catch (...) {
                /*
                    !!! The logic in this catch block MUST be empty, otherwise it will lead to undefined behavior!!!


                    **Exception handling is divided into two stages**
                    Stage 1 -> VEH
                        When the `task()` within our coroutine throws an exception, the VEH registered by the Scheduler will catch it
                        The VEH immediately identifies it as a C++ exception from the coroutine, then saves the exception information into the current coroutine's `exceptionState`
                        and immediately calls `SwitchToFiber()` to switch CPU control from the faulting coroutine back to the Scheduler's `Run()` loop
                        Finally, it returns `EXCEPTION_CONTINUE_EXECUTION`, telling the OS to ignore the exception and continue execution

                    Stage 2 -> try-catch
                        After the VEH has completed its work, the operating system will continue the stack unwinding process for the faulting coroutine's call stack
                        During this unwinding process, the exception will be caught by our catch block
                        Therefore, the task of this catch block is very simple, it just needs to silently "eat" the exception to prevent it from propagating further (which would terminate the program)


                    **Complete Event Chain**
                    `task()` throws an exception ->
                    `VectoredExceptionHandler()` captures exception info, stores it in `exceptionState`, then switches back to the scheduler ->
                    The coroutine's call stack unwinds, the catch block catches and "eats" the exception, allowing the function to end normally ->
                    `CoroutineTrampoline()` marks the coroutine's state as `Finished` ->
                    `Run()` calls the coroutine's `onDone` callback ->
                    `onDone()` checks `exceptionState`, finds an exception record ->
                    `promise->SetException()` is called
                */
            }
        };

        auto onDone = [promise](std::shared_ptr<ExceptionState> exState) {
            if (exState && HasException(exState.get())) {
                promise->SetException(exState);
            }
        };

        auto co = std::make_unique<Coroutine>(std::move(wrappedFunc), std::move(onDone), this);
        co->promiseHandle = promise;
        runnableQueue.push_back(co.get());
        coroutines.push_back(std::move(co));
        return promise;
    }

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

        bool operator>(const TimerNode& other) const {
            return wakeupTime > other.wakeupTime;
        }
    };
    std::priority_queue<TimerNode, std::vector<TimerNode>, std::greater<TimerNode>> timers;
    std::unordered_set<Coroutine*> sleepingCoroutines;

    bool isThreadPool = false;
    std::vector<std::thread> workers;
    std::deque<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop = false;
};

template <typename T>
class CoroutinePromise {
public:
    CoroutinePromise() = default;

    void SetResult(T value) {
        result = std::move(value);
        completed = true;
    }

    void SetException(std::shared_ptr<ExceptionState> exState) {
        exception = exState;
        completed = true;
    }

    bool IsCompleted() const {
        return completed;
    }

    T GetResult() {
        Scheduler* scheduler = GetCurrentScheduler();
        if (scheduler && scheduler->runningCoroutine) {
            while (!completed) {
                Coroutine::YieldExecution();
            }
        } else if (!completed) {
            throw std::runtime_error("Result not ready and not in a coroutine context to wait.");
        }

        if (exception) {
            RethrowIfExists(exception.get());
        }
        return *result;
    }

    bool HasException() const {
        return exception && ::HasException(exception.get());
    }

    void RethrowIfException() const {
        if (exception) {
            RethrowIfExists(exception.get());
        }
    }

private:
    bool completed = false;
    std::optional<T> result;
    std::shared_ptr<ExceptionState> exception;
};

template <>
class CoroutinePromise<void> {
public:
    CoroutinePromise() = default;

    void SetResult() {
        completed = true;
    }

    void SetException(std::shared_ptr<ExceptionState> exState) {
        exception = exState;
        completed = true;
    }

    bool IsCompleted() const {
        return completed;
    }

    void GetResult() {
        Scheduler* scheduler = GetCurrentScheduler();
        if (scheduler && scheduler->runningCoroutine) {
            while (!completed) {
                Coroutine::YieldExecution();
            }
        } else if (!completed) {
            throw std::runtime_error("Result not ready and not in a coroutine context to wait.");
        }

        if (exception) {
            RethrowIfExists(exception.get());
        }
    }

    bool HasException() const {
        return exception && ::HasException(exception.get());
    }

    void RethrowIfException() const {
        if (exception) {
            RethrowIfExists(exception.get());
        }
    }

private:
    bool completed = false;
    std::shared_ptr<ExceptionState> exception;
};