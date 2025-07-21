#include "winAsync.h"
#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <cassert>
#include <sstream>
#include <thread>
#include <mutex>
#include <filesystem>
#include <fstream>

class TestRunner {
public:
    template <typename Func>
    void Register(const std::string& testName, Func&& testFunc) {
        testCases.push_back({testName, std::forward<Func>(testFunc)});
    }

    int RunAll() {
        std::cout << "==================== RUNNING TESTS ====================" << std::endl;

        for (const auto& test : testCases) {
            std::cout << "[ RUNNING ] " << test.name << std::endl;
            try {
                test.func();
                std::cout << "[  PASSED ] " << test.name << std::endl << std::endl;
                passedCount++;
            } catch (const std::exception& e) {
                std::cerr << "[  FAILED ] " << test.name << std::endl;
                std::cerr << "\t-> Exception: " << e.what() << std::endl << std::endl;
                failedCount++;
            } catch (...) {
                std::cerr << "[  FAILED ] " << test.name << std::endl;
                std::cerr << "\t-> Unknown exception caught" << std::endl << std::endl;
                failedCount++;
            }
        }

        std::cout << "==================== TEST SUMMARY ====================" << std::endl;
        std::cout << "PASSED: " << passedCount << " | FAILED: " << failedCount << " | TOTAL: " << testCases.size() << std::endl;
        std::cout << "======================================================" << std::endl;

        return failedCount > 0 ? 1 : 0;
    }

private:
    struct TestCase {
        std::string name;
        std::function<void()> func;
    };

    std::vector<TestCase> testCases;
    int passedCount = 0;
    int failedCount = 0;
};

namespace TestCases {

static void Foo() {
    std::cout << "\tFoo: start" << std::endl;
    for (int i = 0; i < 2; ++i) {
        std::cout << "\tFoo: yield " << i << std::endl;
        Coroutine::YieldExecution();
    }
    std::cout << "\tFoo: end" << std::endl;
}

static void Bar() {
    std::cout << "\tBar: start" << std::endl;
    for (int i = 0; i < 5; ++i) {
        std::cout << "\tBar: yield " << i << std::endl;
        Coroutine::YieldExecution();
    }
    std::cout << "\tBar: end" << std::endl;
}

void BasicScheduling() {
    Scheduler scheduler;
    scheduler.Add(Foo);
    scheduler.Add(Bar);
    scheduler.Run();
}

static void TestVoidFunc() {}

static int TestIntFunc() {
    Coroutine::YieldExecution();
    return 1337;
}

static std::string TestStringFunc(const std::string& s, int n) {
    return s + std::to_string(n);
}

void ParameterPassing() {
    Scheduler scheduler;
    auto promiseVoid = scheduler.CreateCoroutine<void>(TestVoidFunc);
    auto promiseInt = scheduler.CreateCoroutine<int>(TestIntFunc);
    auto promiseString = scheduler.CreateCoroutine<std::string>(TestStringFunc, "ambr0se#", 1337);

    scheduler.Run();

    promiseVoid->GetResult();
    std::cout << "\tVoid return" << std::endl;

    assert(promiseInt->GetResult() == 1337);
    std::cout << "\tInt return: 1337" << std::endl;

    assert(promiseString->GetResult() == "ambr0se#1337");
    std::cout << "\tString return with params: ambr0se#1337" << std::endl;
}

static void ThrowingCoroutine() {
    Coroutine::YieldExecution();
    throw std::runtime_error("Test exception");
}

void ExceptionHandling() {
    Scheduler scheduler;
    auto promise = scheduler.CreateCoroutine<void>(ThrowingCoroutine);
    scheduler.Run();

    assert(promise->IsCompleted() && promise->HasException());
    try {
        promise->GetResult();
        throw std::runtime_error("Test failed: Expected an exception, but none was thrown.");
    } catch (const std::runtime_error& e) {
        if (std::string(e.what()) == "Test exception") {
            std::cout << "\tSuccessfully caught expected exception: " << e.what() << std::endl;
        } else {
            throw;
        }
    }
}

void AsyncSleep() {
    Scheduler scheduler;

    auto printWithTimestamp = [](const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm local_tm;
        localtime_s(&local_tm, &tt);

        std::stringstream ss;
        ss << "\t(" << local_tm.tm_hour << ":" << local_tm.tm_min << ":" << local_tm.tm_sec << "." << ms.count() << ") - " << message;
        std::cout << ss.str() << std::endl;
    };

    scheduler.CreateCoroutine<void>([&]() {
        std::cout << "\tTimer coroutine started" << std::endl;
        for (int i = 1; i <= 3; ++i) {
            printWithTimestamp("Timer will sleep for 1s");
            scheduler.AsyncSleep(1000);
        }
        std::cout << "\tTimer coroutine finished" << std::endl;
    });

    scheduler.CreateCoroutine<void>([&]() {
        printWithTimestamp("I/O coroutine started, simulating 2 second operation");
        scheduler.AsyncSleep(2000);
        printWithTimestamp("I/O coroutine finished after 2 seconds");
    });

    scheduler.Run();

    std::cout << "\tSimulated long I/O test completed" << std::endl;
}

static DWORD AsyncReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead) {
    Scheduler* scheduler = GetCurrentScheduler();
    if (!scheduler || !scheduler->GetRunningCoroutine()) {
        throw std::runtime_error("AsyncReadFile must be called from within a running coroutine");
    }

    IoOperation op;
    op.coroutine = scheduler->GetRunningCoroutine();

    if (!ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, nullptr, &op) && GetLastError() != ERROR_IO_PENDING) {
        throw std::runtime_error("ReadFile failed immediately with error: " + std::to_string(GetLastError()));
    }

    Coroutine::SuspendExecution();

    DWORD bytesRead = 0;
    GetOverlappedResult(hFile, &op, &bytesRead, TRUE);
    return bytesRead;
}

void AsyncIo() {
    const std::filesystem::path testFilePath = "io_test.txt";
    const std::string testContent = "Hello, Asynchronous World!";

    {
        std::ofstream outFile(testFilePath);
        outFile << testContent;
    }

    Scheduler scheduler;

    HANDLE hFile = CreateFileW(testFilePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to open test file for async reading");
    }

    scheduler.RegisterHandle(hFile);

    scheduler.CreateCoroutine<void>([&]() {
        std::cout << "\tIO Coroutine: Starting async read" << std::endl;
        char buffer[128] = {0};
        DWORD bytesRead = AsyncReadFile(hFile, buffer, sizeof(buffer) - 1);
        std::cout << "\tIO Coroutine: Read completed. Content: " << buffer << std::endl;

        assert(bytesRead == testContent.length());
        assert(std::string(buffer) == testContent);
        std::cout << "\tIO Coroutine: Content verification successful" << std::endl;
    });

    scheduler.CreateCoroutine<void>([]() {
        std::cout << "\tWorker Coroutine: Starting" << std::endl;
        for (int i = 0; i < 5; ++i) {
            std::cout << "\tWorker Coroutine: Still running... (" << i + 1 << "/5)" << std::endl;
            Scheduler::AsyncSleep(50);
        }
        std::cout << "\tWorker Coroutine: Finished" << std::endl;
    });

    scheduler.Run();

    CloseHandle(hFile);
    std::filesystem::remove(testFilePath);
}

void MultiThreadedScheduler() {
    std::mutex coutMutex;
    auto printThreadId = [&coutMutex]() {
        std::stringstream ss;
        ss << "\tTask executed by thread " << std::this_thread::get_id();
        
        std::lock_guard<std::mutex> lock(coutMutex);
        std::cout << ss.str() << std::endl;
    };

    Scheduler scheduler(4);

    std::cout << "\tSubmitting 10 tasks to a 4-thread scheduler" << std::endl;
    for (int i = 0; i < 10; ++i) {
        scheduler.Submit(printThreadId);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    scheduler.Stop();
    std::cout << "\tScheduler stopped" << std::endl;
}

void HybridSchedulingBenchmark() {
    Scheduler scheduler;

    scheduler.CreateCoroutine<void>([&]() {
        const int numCoroutineTasks = 5;
        const int numThreadPoolTasks = 5;
        std::vector<Task<int>> tasks;
        std::atomic<int> completedCount = 0;

        std::cout << "\tStarting hybrid scheduling benchmark with " << numCoroutineTasks + numThreadPoolTasks << " tasks." << std::endl;

        for (int i = 0; i < numCoroutineTasks; ++i) {
            tasks.push_back(CreateTask<int>([i, &completedCount]() {
                for (int j = 0; j < (i + 1) * 2; ++j) {
                    Coroutine::YieldExecution();
                }
                completedCount++;
                return i;
            }));
        }

        for (int i = 0; i < numThreadPoolTasks; ++i) {
            tasks.push_back(RunOnThreadPool<int>([i, &completedCount]() {
                int result = 0;
                for (int k = 0; k < 10000 * (i + 1); ++k) {
                    result += k;
                }
                completedCount++;
                return result;
            }));
        }

        std::cout << "\tAll tasks launched. Awaiting results..." << std::endl;

        for (auto& task : tasks) {
            Await(task);
        }

        std::cout << "\tAll tasks completed." << std::endl;
        assert(completedCount == numCoroutineTasks + numThreadPoolTasks);
    });

    scheduler.Run();
}

std::mutex g_std_mutex;
std::vector<int> g_shared_data;

void StdMutexDeadlockTest() {
    g_shared_data.clear();

    Scheduler scheduler;

    scheduler.Add([]{
        std::cout << "\t[Coroutine A] Trying to lock the mutex..." << std::endl;
        g_std_mutex.lock();
        std::cout << "\t[Coroutine A] Mutex locked" << std::endl;

        g_shared_data.push_back(1);
        std::cout << "\t[Coroutine A] Added data. Now sleeping for 100ms while holding the lock..." << std::endl;
        
        Scheduler::AsyncSleep(100);

        std::cout << "\t[Coroutine A] Woke up. Releasing mutex" << std::endl;
        g_std_mutex.unlock();
    });

    scheduler.Add([]{
        std::cout << "\t[Coroutine B] Waiting for 10ms before trying to lock..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        std::cout << "\t[Coroutine B] Trying to lock the mutex..." << std::endl;
        g_std_mutex.lock(); 
        std::cout << "\t[Coroutine B] Mutex locked" << std::endl;

        g_shared_data.push_back(2);
        std::cout << "\t[Coroutine B] Added data. Releasing mutex" << std::endl;
        g_std_mutex.unlock();
    });

    scheduler.Run();
    // The test passes because the deadlock is caught by the VEH...
}

} // namespace TestCases

int main() {
    auto testRunner = std::make_unique<TestRunner>();

    testRunner->Register("Basic Scheduling", TestCases::BasicScheduling);
    testRunner->Register("Parameter Passing and Return Values", TestCases::ParameterPassing);
    testRunner->Register("Exception Handling", TestCases::ExceptionHandling);
    testRunner->Register("Async Sleep", TestCases::AsyncSleep);
    testRunner->Register("Async IO", TestCases::AsyncIo);
    testRunner->Register("Multi-Threaded Scheduler", TestCases::MultiThreadedScheduler);
    testRunner->Register("Hybrid Scheduling Benchmark", TestCases::HybridSchedulingBenchmark);
    testRunner->Register("StdMutexDeadlockTest", TestCases::StdMutexDeadlockTest);

    return testRunner->RunAll();
}