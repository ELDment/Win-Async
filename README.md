# 💻 Win-Async

[简体中文项目介绍](README.zh-CN.md)

A C++ coroutine library implemented based on `Windows Fiber`, `VEH`, `IOCP`, and `C++17`

> ⚠️ This is a **learning project** aimed at exploring the underlying mechanisms of coroutine scheduling, context switching, and exception handling. Since the core implementation relies on Windows Fiber, this project **only supports the Windows platform**. Additionally, this project does not provide production-level performance optimizations or long-term community support

## ✨ Core Features

- 🚀 **Powerful Hybrid Scheduling Model**
  - 🤝 **Cooperative I/O Coroutines**: Integrates `IOCP` to elegantly handle high-concurrency, non-blocking I/O operations
  - ⚡ **Parallel CPU-Bound Tasks**: A built-in thread pool allows offloading high-latency tasks via `RunOnThreadPool`, preventing them from blocking the main I/O event loop

- 💻 **Modern Asynchronous API (`Task`/`Await`)**
  - 💡 **Synchronous-Style Coding**: Write asynchronous logic that reads like synchronous code using `CreateTask` and `Await`
  - 🎁 **Seamless Result & Exception Propagation**: `Task<T>` transparently delivers results or exceptions from any coroutine (whether an I/O coroutine or a thread pool task) to the caller

- 🛡️ **Robust Exception Safety**
  - 📦 **Unified Exception Handling**: Automatically captures exceptions from any context (Fiber or thread pool) using Vectored Exception Handling (VEH) and safely propagates them through the `Promise`

## 🔧 How It Works

- **Context Switching**: Based on the Windows `Fiber` API
- **Exception Handling**: Utilizes Vectored Exception Handling (`VEH`)
- **Scheduling Loop**: An **IOCP-driven** event loop that unifies coroutine scheduling, timers, and asynchronous I/O events

## 🛠️ Quick Start

### Using xmake

```powershell
# Configure build options (Release mode)
xmake f -m release

# Build the project
xmake

# Run the benchmark
xmake run benchmark
```

### Using CMake

```powershell
# Create and enter the build directory
mkdir build; cd build

# Generate the build system
cmake ..

# Build the project (Release mode)
cmake --build . --config Release

# Run the benchmark
./Release/benchmark.exe
```

## 🗺️ TODO

### 🚀 Cpp-Coroutine Development Roadmap

| Priority | Category | Key Tasks |
| :---: | :--- | :--- |
| ⚡⚡ | **Fiber-based Task/Await** | [√] Design `Task<T>`<br>[√] Implement `Await` |
| ⚡⚡⚡⚡ | **Scheduler & Concurrency** | [√] Multi-threaded scheduler<br>[√] Integrate IOCP |
| ⚡⚡ | **Coroutine Sync Primitives** | [ ] Async Mutex (`AsyncMutex`)<br>[ ] Async Semaphore (`AsyncSemaphore`)<br>[ ] Combinators (`WhenAll` / `WhenAny`) |
| ⚡ | **API** | [ ] Cooperative Cancellation (`CancellationToken`) |