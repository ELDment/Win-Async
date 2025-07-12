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
    ~Scheduler();

    void Add(std::function<void()> func);
    void Run();
    void Resume(Coroutine* co);
    Coroutine* PollException();
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
                ！！！此处catch逻辑必须为空，否则程序将会出现未定义行为！！！

                
                异常捕获分为两个阶段：

                第一阶段：VEH
                当我们的协程内的task()抛出异常时，Scheduler注册的VEH会捕获到这个异常，VEH会立刻识别出该异常为协程中的C++异常，
                然后将异常信息保存到当前协程的exceptionState中，并立即调用SwitchToFiber()将CPU的控制权从出错的协程切换回调度器的Run()循环。
                最后返回EXCEPTION_CONTINUE_EXECUTION，让操作系统无视异常继续执行。

                第二阶段：try-catch
                在VEH完成了工作后，操作系统会继续“展开”出错的协程的函数调用栈（stack unwinding）
                在“展开”过程中，这个异常会被我们的catch块捕获。
                因此，本catch块的任务很简单————他只需要安静地“吞掉”这个异常，防止异常继续向上传播（导致程序终止）即可。


                完整事件链：

                task() 抛出异常 ->
                VectoredExceptionHandler() 捕获异常信息，存入exceptionState，然后切换回调度器 ->
                协程的调用栈展开，catch块捕获并“吞掉”异常，wait() 函数正常结束 ->
                CorotineTrampoline() 将该协程的状态标记为Finished ->
                Run() 调用该协程的onDone回调 ->
                onDone() 检查exceptionState，发现里面有异常记录 ->
                调用promise->SetException()
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
    static LONG WINAPI VectoredExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo);

    friend class Coroutine;

    void* mainFiber;
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