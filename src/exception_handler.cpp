#include "coroutine.h"
#include <windows.h>

LONG WINAPI Scheduler::VectoredExceptionHandler(PEXCEPTION_POINTERS ExceptionInfo) {
    DebugPrint("[Scheduler::VectoredExceptionHandler] Vectored Exception Handler triggered.\n");
    Scheduler* scheduler = GetCurrentScheduler();
    if (scheduler && scheduler->runningCoroutine) {
        DebugPrint("[Scheduler::VectoredExceptionHandler] Exception occurred inside a coroutine.\n");
        if (ExceptionInfo->ExceptionRecord->ExceptionCode == 0xE06D7363) {
            // C++ 异常
            DebugPrint("[Scheduler::VectoredExceptionHandler] C++ exception detected. Capturing...\n");
            Coroutine* co = scheduler->runningCoroutine;
            CaptureException(co->exceptionState.get(), *ExceptionInfo->ExceptionRecord);

            DebugPrint("[Scheduler::VectoredExceptionHandler] Switching to main fiber to handle exception.\n");
            SwitchToFiber(scheduler->mainFiber);

            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }

    DebugPrint("[Scheduler::VectoredExceptionHandler] Exception not handled by our handler. Continuing search.\n");
    return EXCEPTION_CONTINUE_SEARCH;
}