#include "coroutine.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <functional>
#include <cassert>
#include <memory>
#include <sstream>
#include <fstream>

struct TestCase {
    std::string name;
    std::function<void()> func;
};

std::vector<TestCase> testRegistry;

void RegisterTest(const std::string& name, std::function<void()> func) {
    testRegistry.push_back({name, func});
}

void Foo() {
    std::cout << "\tFoo: start" << std::endl;
    for (int i = 0; i < 2; ++i) {
        std::cout << "\tFoo: yield " << i << std::endl;
        Coroutine::YieldExecution();
    }
    std::cout << "\tFoo: end" << std::endl;
}

void Bar() {
    std::cout << "\tBar: start" << std::endl;
    for (int i = 0; i < 5; ++i) {
        std::cout << "\tBar: yield " << i << std::endl;
        Coroutine::YieldExecution();
    }
    std::cout << "\tBar: end" << std::endl;
}

void TestBasicScheduling() {
    Scheduler scheduler;
    scheduler.Add(Foo);
    scheduler.Add(Bar);
    scheduler.Run();
}

void TestVoidFunc() {}

int TestIntFunc() {
    Coroutine::YieldExecution();
    return 1337;
}

std::string TestStringFunc(const std::string& s, int n) {
    return s + std::to_string(n);
}

void TestParameterPassing() {
    Scheduler scheduler;
    auto promiseVoid = scheduler.CreateCoroutine<void>(TestVoidFunc);
    auto promiseInt = scheduler.CreateCoroutine<int>(TestIntFunc);
    auto promiseString = scheduler.CreateCoroutine<std::string>(TestStringFunc, "ambr0se#", 1337);

    scheduler.Run();

    promiseVoid->GetResult();
    std::cout << "\tVoid return [OK]" << std::endl;

    assert(promiseInt->GetResult() == 1337);
    std::cout << "\tInt return: 1337 [OK]" << std::endl;

    assert(promiseString->GetResult() == "ambr0se#1337");
    std::cout << "\tString return with params: ambr0se#1337 [OK]" << std::endl;
}

void ThrowingCoroutine() {
    Coroutine::YieldExecution();
    throw std::runtime_error("Test exception");
}

void TestExceptionHandling() {
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

void TestAsyncSleep() {
    Scheduler scheduler;

    auto printWithTimestamp = [](const std::string& message) {
        std::cout << "\t(" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() << ") - " << message << std::endl;
    };

    scheduler.CreateCoroutine<void>([&]() {
        std::cout << "\tTimer coroutine started." << std::endl;
        for (int i = 1; i <= 3; ++i) {
            printWithTimestamp("Timer: " + std::to_string(i) + "s");
            scheduler.AsyncSleep(1000);
        }
        std::cout << "\tTimer coroutine finished." << std::endl;
    });

    scheduler.CreateCoroutine<void>([&]() {
        printWithTimestamp("I/O coroutine started, simulating 2 second operation.");
        scheduler.AsyncSleep(2000);
        printWithTimestamp("I/O coroutine finished after 2 seconds.");
    });

    scheduler.Run();

    std::cout << "\tSimulated long I/O test completed." << std::endl;
}

DWORD AsyncReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead) {
    Scheduler* scheduler = GetCurrentScheduler();
    if (!scheduler || !scheduler->GetRunningCoroutine()) {
        throw std::runtime_error("AsyncReadFile must be called from within a running coroutine.");
    }

    IoOperation op;
    op.coroutine = scheduler->GetRunningCoroutine();

    BOOL result = ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, nullptr, &op);
    DWORD lastError = GetLastError();

    if (!result && lastError != ERROR_IO_PENDING) {
        char errorMsg[256];
        sprintf_s(errorMsg, "ReadFile failed immediately with error: %lu", lastError);
        throw std::runtime_error(errorMsg);
    }

    Coroutine::SuspendExecution();

    DWORD bytesRead = 0;
    GetOverlappedResult(hFile, &op, &bytesRead, TRUE);
    return bytesRead;
}

void TestAsyncIo() {
    const char* testFileName = "io_test.txt";
    const char* testContent = "Hello, Asynchronous World!";

    {
        std::ofstream outFile(testFileName);
        outFile << testContent;
    }

    Scheduler scheduler;

    HANDLE hFile = CreateFileA(testFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to open test file for async reading.");
    }

    scheduler.RegisterHandle(hFile);

    scheduler.CreateCoroutine<void>([&]() {
        std::cout << "\tIO Coroutine: Starting async read." << std::endl;
        char buffer[128] = {0};
        DWORD bytesRead = AsyncReadFile(hFile, buffer, sizeof(buffer) - 1);
        std::cout << "\tIO Coroutine: Read completed. Content: '" << buffer << "'" << std::endl;

        assert(bytesRead == strlen(testContent));
        assert(strcmp(buffer, testContent) == 0);
        std::cout << "\tIO Coroutine: Content verification successful." << std::endl;
    });

    scheduler.CreateCoroutine<void>([]() {
        std::cout << "\tWorker Coroutine: Starting." << std::endl;
        for (int i = 0; i < 5; ++i) {
            std::cout << "\tWorker Coroutine: Still running... (" << i + 1 << "/5)" << std::endl;
            Scheduler::AsyncSleep(50);
        }
        std::cout << "\tWorker Coroutine: Finished." << std::endl;
    });

    scheduler.Run();

    CloseHandle(hFile);
    remove(testFileName);
}

void TestMultiThreadedScheduler() {
    auto printThreadId = []() {
        std::stringstream ss;
        ss << "\tTask executed by thread: " << std::this_thread::get_id() << std::endl;
        std::cout << ss.str();
    };

    Scheduler scheduler(4);

    for (int i = 0; i < 10; ++i) {
        scheduler.Submit(printThreadId);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    scheduler.Stop();
}

int main() {
    RegisterTest("Basic Scheduling", TestBasicScheduling);
    RegisterTest("Exception Handling", TestExceptionHandling);
    RegisterTest("Parameter Passing and Return Values", TestParameterPassing);
    RegisterTest("Async Sleep", TestAsyncSleep);
    RegisterTest("Multi-Threaded Scheduler", TestMultiThreadedScheduler);
    RegisterTest("Async IO", TestAsyncIo);

    int passed = 0;
    int failed = 0;

    std::cout << "==================== RUNNING TESTS ====================" << std::endl;

    for (const auto& test : testRegistry) {
        std::cout << "[ RUNNING ] " << test.name << std::endl;
        try {
            test.func();
            std::cout << "[ PASSED ] " << test.name << std::endl << std::endl;
            passed++;
        } catch (const std::exception& e) {
            std::cerr << "[ FAILED ] " << test.name << std::endl;
            std::cerr << "\t-> Exception: " << e.what() << std::endl << std::endl;
            failed++;
        } catch (...) {
            std::cerr << "[ FAILED ] " << test.name << std::endl;
            std::cerr << "\t-> Unknown exception caught." << std::endl << std::endl;
            failed++;
        }
    }

    std::cout << "==================== TEST SUMMARY ====================" << std::endl;
    std::cout << "PASSED: " << passed << " | FAILED: " << failed << " | TOTAL: " << testRegistry.size() << std::endl;
    std::cout << "======================================================" << std::endl;

    return failed > 0 ? 1 : 0;
}