#include "winAsync.h"
#include <windows.h>
#include <stdexcept>
#include <algorithm>
#include <chrono>

namespace {
    thread_local Scheduler* currentScheduler = nullptr;
}

Scheduler* GetCurrentScheduler() {
    return currentScheduler;
}

void SetCurrentScheduler(Scheduler* scheduler) {
    currentScheduler = scheduler;
}

Scheduler::Scheduler() : runningCoroutine(nullptr), vehHandle(nullptr), pendingException(nullptr), isThreadPool(false), stop(false), iocpHandle(nullptr) {
    if (currentScheduler) {
        throw std::runtime_error("Only one scheduler per thread is allowed.");
    }

    currentScheduler = this;
    mainFiber = ConvertThreadToFiber(nullptr);
    iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (!iocpHandle) {
        throw std::runtime_error("Failed to create IOCP handle");
    }
    vehHandle = AddVectoredExceptionHandler(1, VectoredExceptionHandler);

    DebugPrint("[Scheduler::Scheduler] Scheduler created and VEH registered\n");
}

Scheduler::Scheduler(size_t numThreads) : runningCoroutine(nullptr), vehHandle(nullptr), pendingException(nullptr), isThreadPool(true), stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back(&Scheduler::WorkerLoop, this);
    }
}

Scheduler::~Scheduler() {
    if (vehHandle) {
        RemoveVectoredExceptionHandler(vehHandle);
        DebugPrint("[Scheduler::~Scheduler] VEH unregistered\n");
    }

    if (iocpHandle) {
        CloseHandle(iocpHandle);
    }

    if (isThreadPool) {
        Stop();
    } else {
        currentScheduler = nullptr;
        ConvertFiberToThread();
    }
}

void Scheduler::Submit(std::function<void()> func) {
    if (!isThreadPool) {
        throw std::runtime_error("Submit is only for thread pool schedulers.");
    } else {
        std::unique_lock<std::mutex> lock(queueMutex);
        tasks.push_back(std::move(func));
    }
    condition.notify_one();
}

void Scheduler::Add(std::function<void()> func) {
    auto co = std::make_unique<Coroutine>(std::move(func), nullptr, this);
    runnableQueue.push_back(co.get());
    coroutines.push_back(std::move(co));
}

void Scheduler::RegisterHandle(HANDLE handle) {
    if (CreateIoCompletionPort(handle, iocpHandle, 0, 0) != iocpHandle) {
        throw std::runtime_error("Failed to associate handle with IOCP");
    }
}

void Scheduler::Stop() {
    if (!isThreadPool) {
        return;
    } else {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void Scheduler::Run() {
    DebugPrint("[Scheduler::Run] Starting scheduler with %zu initial coroutines\n", coroutines.size());

    while (!coroutines.empty()) {
        auto now = std::chrono::steady_clock::now();
        while (!timers.empty() && timers.top().wakeupTime <= now) {
            TimerNode node = timers.top();
            timers.pop();
            runnableQueue.push_back(node.coroutine);
            sleepingCoroutines.erase(node.coroutine);
        }

        while (!runnableQueue.empty()) {
            Coroutine* co = runnableQueue.front();
            runnableQueue.pop_front();

            if (co->state != Coroutine::State::Finished) {
                DebugPrint("[Scheduler::Run] Resuming coroutine %p in state %d\n", co, static_cast<int>(co->state));
                Resume(co);
            }
        }

        for (const auto& co : coroutines) {
            if (co->state == Coroutine::State::Suspended) {
                if (sleepingCoroutines.find(co.get()) == sleepingCoroutines.end()) {
                    runnableQueue.push_back(co.get());
                }
            }
        }

        coroutines.erase(std::remove_if(coroutines.begin(), coroutines.end(),
            [](const std::unique_ptr<Coroutine>& co) {
                if (co->state == Coroutine::State::Finished) {
                    DebugPrint("[Scheduler::Run] Cleaning up finished coroutine %p\n", co.get());
                    if (co->onDone) {
                        co->onDone(co->exceptionState);
                    }
                    return true;
                }
                return false;
            }), coroutines.end());

        if (coroutines.empty()) {
            DebugPrint("[Scheduler::Run] No more coroutines to run. Exiting.\n");
            break;
        }

        if (runnableQueue.empty()) {
            DWORD timeout = INFINITE;
            if (!timers.empty()) {
                auto nextWakeup = timers.top().wakeupTime;
                auto timeToWait = std::chrono::duration_cast<std::chrono::milliseconds>(nextWakeup - std::chrono::steady_clock::now());
                if (timeToWait.count() > 0) {
                    timeout = static_cast<DWORD>(timeToWait.count());
                } else {
                    timeout = 0;
                }
            }
            
            DWORD bytesTransferred;
            ULONG_PTR completionKey;
            OVERLAPPED* overlapped;

            DebugPrint("[Scheduler::Run] Waiting for IO events with timeout %u ms\n", timeout);
            BOOL result = GetQueuedCompletionStatus(iocpHandle, &bytesTransferred, &completionKey, &overlapped, timeout);

            if (result && overlapped) {
                IoOperation* op = static_cast<IoOperation*>(overlapped);
                DebugPrint("[Scheduler::Run] IO completed for coroutine %p, resuming.\n", op->coroutine);
                runnableQueue.push_back(op->coroutine);
            } else if (!result && overlapped) {
                // IO operation failed
                IoOperation* op = static_cast<IoOperation*>(overlapped);
                // For now, just resume it. A more robust implementation would handle the error.
                DebugPrint("[Scheduler::Run] IO failed for coroutine %p, resuming.\n", op->coroutine);
                runnableQueue.push_back(op->coroutine);
            } else {
                DebugPrint("[Scheduler::Run] Wait timed out or woken up.\n");
            }
        }
    }
}

void Scheduler::Resume(Coroutine* co) {
    if (!co) return;

    runningCoroutine = co;
    co->state = Coroutine::State::Running;

    SwitchToFiber(co->fiber);
    DebugPrint("[Scheduler::Resume] Returned from coroutine context. Checking for exceptions.\n");

    runningCoroutine = nullptr;
    if (co->HasException()) {
        DebugPrint("[Scheduler::Resume] Coroutine has an exception. Setting pendingException.\n");
        pendingException = co;
        co->state = Coroutine::State::Finished;
    }
}

Coroutine* Scheduler::GetRunningCoroutine() const {
    return runningCoroutine;
}

Coroutine* Scheduler::PollException() {
    DebugPrint("[Scheduler::PollException] Polling for exception. Found: %p\n", pendingException);
    Coroutine* co = pendingException;
    pendingException = nullptr;
    return co;
}

void Scheduler::WorkerLoop() {
    Scheduler localScheduler;
    SetCurrentScheduler(&localScheduler);

    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });
            if (stop && tasks.empty()) {
                return;
            }
            task = std::move(tasks.front());
            tasks.pop_front();
        }

        localScheduler.Add(std::move(task));
        localScheduler.Run();
    }
}

void Scheduler::AsyncSleep(uint32_t milliseconds) {
    Scheduler* scheduler = GetCurrentScheduler();
    if (!scheduler || !scheduler->runningCoroutine) return;

    auto wakeupTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    Coroutine* co = scheduler->runningCoroutine;
    scheduler->timers.push({wakeupTime, co});
    scheduler->sleepingCoroutines.insert(co);
    
    Coroutine::YieldExecution();
}