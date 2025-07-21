# 💻 Win-Async

[English Project Introduction](README.md)

基于 `Windows Fiber`、`VEH`、`IOCP` 及 `C++17` 实现的 C++ 协程库。

> ⚠️ 本项目是一个**练手项目**，旨在研究协程底层的调度、上下文切换和异常处理机制。由于核心实现依赖了 Windows Fiber，本项目**仅支持 Windows 平台**。同时，本项目不会提供生产级的性能优化或长期的社区支持。

## ✨ 核心功能

- 🚀 **混合调度模型**
  - 🤝 **单线程协作式调度**: 通过 `Yield` 实现高效的协作式多任务。
  - ⚡ **多线程并行调度**: 内置线程池，通过 `Submit` 将 CPU 密集型任务分发至多核并行处理。
- ⏳ **异步编程支持**
  - 🎁 **Future/Promise 模式**: 通过 `CoroutinePromise` 从协程外部安全地获取执行结果。
  - 🚀 **异步 I/O**: 基于 **IOCP** 实现高效的事件驱动 I/O，允许协程在等待 I/O 操作完成时挂起，而不阻塞任何线程。
  - 😴 **异步休眠**: 支持 `AsyncSleep`，允许协程在不阻塞线程的情况下进行延时操作。
- 🛡️ **强大的异常处理**
  - 📦 **跨协程异常传递**: 能够安全地捕获协程内部异常，并将其传递给 `Promise`，防止程序崩溃。

## 🔧 实现原理

- **上下文切换**: 基于 Windows `Fiber` API。
- **异常捕获**: 通过向量化异常处理 (`VEH`) 捕获协程中的异常。
- **调度循环**: 采用 **IOCP** 事件驱动模型，统一处理协程切换、定时器和异步 I/O 事件。

## 🛠️ 快速开始

### 使用 xmake

```powershell
# 配置编译选项（Release 模式）
xmake f -m release

# 构建项目
xmake

# 运行基准测试
xmake run benchmark
```

### 使用 CMake

```powershell
# 创建并进入构建目录
mkdir build; cd build

# 生成构建系统
cmake ..

# 构造项目（Release 模式）
cmake --build . --config Release

# 运行基准测试
./Release/benchmark.exe
```

## 🗺️ 未来计划

### 🚀 Cpp-Coroutine 未来开发计划

| 优先级 | 分类 | 主要任务 |
| :---: | :--- | :--- |
| ⚡⚡ | **C++20 协程支持** | [√] 设计 `Task<T>`<br>[√] 封装 Awaitables |
| ⚡⚡⚡⚡ | **调度器与并发** | [√] 多线程调度器<br>[√] 集成 IOCP |
| ⚡⚡ | **协程同步原语** | [ ] 异步互斥锁（`AsyncMutex`）<br>[ ] 异步信号量（`AsyncSemaphore`）<br>[ ] 组合器（`WhenAll` / `WhenAny`） |
| ⚡ | **API** | [ ] 协作式取消（`CancellationToken`） |