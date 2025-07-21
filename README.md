# 💻 Win-Async

[简体中文项目介绍](README.zh-CN.md)

A C++ coroutine library implemented based on `Windows Fiber`, `VEH`, `IOCP`, and `C++17`

> ⚠️ This is a **learning project** aimed at exploring the underlying mechanisms of coroutine scheduling, context switching, and exception handling. Since the core implementation relies on Windows Fiber, this project **only supports the Windows platform**. Additionally, this project does not provide production-level performance optimizations or long-term community support

## ✨ Core Features

- 🚀 **Hybrid Scheduling Model**
  - 🤝 **Cooperative (Single-Threaded)**: Ideal for I/O-bound tasks, using `Yield` for efficient multitasking
  - ⚡ **Parallel (Multi-Threaded)**: Built-in thread pool to offload CPU-bound tasks via `Submit`
- ⏳ **Asynchronous Programming Support**
  - 🎁 **Future/Promise Pattern**: Safely retrieve results from coroutines using `CoroutinePromise`
  - ✨ **Asynchronous I/O**: High-performance, event-driven I/O powered by **IOCP**, allowing coroutines to wait for I/O without blocking threads
  - 😴 **Asynchronous Sleep**: `AsyncSleep` allows non-blocking delays
- 🛡️ **Robust Exception Handling**
  - 📦 **Cross-Coroutine Exception Propagation**: Safely catches and forwards exceptions to the `Promise`

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