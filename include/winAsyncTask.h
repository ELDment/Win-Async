#pragma once

#include "winAsync.h"

class CoroutinePromiseBase {
public:
    CoroutinePromiseBase() : completed(false) {}

    void SetException(std::shared_ptr<ExceptionState> exState) {
        exception = exState;
        completed = true;
    }

    bool IsCompleted() const { return completed; }

    bool HasException() const {
        return exception && ::HasException(exception.get());
    }

    void RethrowIfException() const {
        if (exception) {
            RethrowIfExists(exception.get());
        }
    }

protected:
    bool completed;
    std::shared_ptr<ExceptionState> exception;
};

template <typename T>
class CoroutinePromise : public CoroutinePromiseBase {
public:
    void SetResult(T value) {
        result = std::move(value);
        completed = true;
    }

    T GetResult() {
        RethrowIfException();
        return *result;
    }

private:
    std::optional<T> result;
};

template <>
class CoroutinePromise<void> : public CoroutinePromiseBase {
public:
    void SetResult() { completed = true; }

    void GetResult() { RethrowIfException(); }
};

template <typename T>
class Task {
public:
    explicit Task(std::shared_ptr<CoroutinePromise<T>> p) : promise(std::move(p)) {}
    std::shared_ptr<CoroutinePromise<T>> GetPromise() const { return promise; }
private:
    std::shared_ptr<CoroutinePromise<T>> promise;
};

template <typename T, typename Func, typename... Args>
Task<T> CreateTask(Func&& func, Args&&... args) {
    Scheduler* scheduler = GetCurrentScheduler();
    if (!scheduler) {
        throw std::runtime_error("CreateTask must be called from within a running coroutine context.");
    }
    auto promise = scheduler->CreateCoroutine<T>(std::forward<Func>(func), std::forward<Args>(args)...);
    return Task<T>(promise);
}

template <typename T>
T Await(Task<T>& task) {
    auto promise = task.GetPromise();
    while (!promise->IsCompleted()) {
        Coroutine::YieldExecution();
    }
    return promise->GetResult();
}

inline void Await(Task<void>& task) {
    auto promise = task.GetPromise();
    while (!promise->IsCompleted()) {
        Coroutine::YieldExecution();
    }
    promise->GetResult();
}

template <typename T, typename Func, typename... Args>
Task<T> RunOnThreadPool(Func&& func, Args&&... args) {
    auto promise = std::make_shared<CoroutinePromise<T>>();
    auto task = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);

    auto work = [promise, task]() mutable {
        try {
            if constexpr (std::is_void_v<T>) {
                task();
                promise->SetResult();
            } else {
                promise->SetResult(task());
            }
        } catch (...) {
            // Exception is handled by VEH, this just prevents crash
        }
    };

    Scheduler::GetThreadPool().Submit(std::move(work));
    return Task<T>(promise);
}

template <typename T, typename Func, typename... Args>
std::shared_ptr<CoroutinePromise<T>> Scheduler::CreateCoroutine(Func&& func, Args&&... args) {
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
            // Exception is handled by VEH, this just prevents crash
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