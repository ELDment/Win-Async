# 💻 Win-Async

[English Project Introduction](README.md)

基于 `Windows Fiber`、`VEH`、`IOCP` 及 `C++17` 实现的 C++ 协程库。

> ⚠️ 本项目是一个**练手项目**，旨在研究协程底层的调度、上下文切换和异常处理机制。由于核心实现依赖了 Windows Fiber，本项目**仅支持 Windows 平台**。同时，本项目不会提供生产级的性能优化或长期的社区支持。

## ✨ 核心功能

- 🚀 **强大的混合调度模型**
  - 🤝 **协作式 I/O 协程**：集成 `IOCP` ，使得本项目可以优雅地处理高并发非阻塞的 I/O 操作。
  - ⚡ **并行 CPU 密集型任务**：内置的线程池，可通过 `RunOnThreadPool` 将高耗时 I/O 操作外派，防止其阻塞主 I/O 事件循环。

- 💻 **现代的异步 API (`Task`/`Await`)**
  - 💡 **同步风格的编码**：使用 `CreateTask` 和 `Await`，可以像编写同步代码一样编写异步逻辑。
  - 🎁 **无缝的结果与异常传递**：`Task<T>` 能透明地将任何协程（无论是 I/O 协程还是线程池任务）的结果或异常传递给调用方。

- 🛡️ **健壮的异常安全**
  - 📦 **统一的异常处理**：利用向量化异常处理（VEH）自动捕获来自任何上下文（Fiber 或线程池）的异常，并通过`Promise`安全地进行传递。

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