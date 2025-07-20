# 💻 Win-Async

[简体中文项目介绍](README.zh-CN.md)

A C++ coroutine library implemented based on **Windows Fiber**, **Vectored Exception Handling (VEH)**, and **CXX17**.

> ⚠️ This is a **learning project** aimed at exploring the underlying mechanisms of coroutine scheduling, context switching, and exception handling. Since the core implementation relies on Windows Fiber, this project **only supports the Windows platform**. Additionally, this project does not provide production-level performance optimizations or long-term community support.

## ✨ Core Features

- 🚀 **Hybrid Scheduling Model**
  - 🤝 **Single-Threaded Cooperative Scheduling**: Suitable for I/O-bound tasks, enabling efficient cooperative multitasking via `Yield`
  - ⚡ **Multi-Threaded Parallel Scheduling**: Built-in thread pool to dispatch CPU-bound tasks to multiple cores for parallel processing using `Submit`
- ⏳ **Asynchronous Programming Support**
  - 🎁 **Future/Promise Pattern**: Safely retrieve execution results from outside the coroutine using `CoroutinePromise`
  - 😴 **Asynchronous Sleep**: Supports `AsyncSleep`, allowing coroutines to delay without blocking the thread
- 🛡️ **Robust Exception Handling**
  - 📦 **Cross-Coroutine Exception Propagation**: Safely catches exceptions within a coroutine and passes them to the `Promise`, preventing program crashes

## 🔧 Implementation Details

- **Context Switching**: Based on the Windows `Fiber` API
- **Exception Handling**: Captures exceptions in coroutines using Vectored Exception Handling (`VEH`)
- **Scheduling Loop**: Uses an event loop model, managing runnable coroutines with `std::deque` and handling asynchronous sleep tasks with `std::priority_queue`

## 🛠️ Quick Start

Build and run the benchmark using xmake

```powershell
# Configure build options
xmake f -m release
xmake f -m debug

# Build the project
xmake

# Run the benchmark
xmake run benchmark
```

## 🗺️ TODO

### 🚀 Cpp-Coroutine Development Roadmap

| Priority | Category | Key Tasks |
| :---: | :--- | :--- |
| ⚡ | **C++20 Coroutine Support** | - [ ] Design `Task<T>`<br>- [ ] Wrap Awaitables |
| ⚡⚡⚡⚡ | **Scheduler & Concurrency** | - [✅] Multi-threaded scheduler<br>- [ ] Integrate IOCP |
| ⚡⚡ | **Coroutine Sync Primitives** | - [ ] Async Mutex (`AsyncMutex`)<br>- [ ] Async Semaphore (`AsyncSemaphore`)<br>- [ ] Combinators (`WhenAll` / `WhenAny`) |
| ⚡⚡⚡ | **API** | - [ ] Cooperative Cancellation (`CancellationToken`) |