#include "coroutine.h"
#include <windows.h>

struct ExceptionState {
    bool hasException = false;
    EXCEPTION_RECORD exceptionRecord;
};

void CaptureException(ExceptionState* es, const EXCEPTION_RECORD& record) {
    es->hasException = true;
    es->exceptionRecord = record;
}

bool HasException(const ExceptionState* es) {
    return es && es->hasException;
}

void RethrowIfExists(const ExceptionState* es) {
    if (es && es->hasException) {
        RaiseException(
            es->exceptionRecord.ExceptionCode,
            es->exceptionRecord.ExceptionFlags,
            es->exceptionRecord.NumberParameters,
            es->exceptionRecord.ExceptionInformation
        );
    }
}

void CoroutineTrampoline(void* arg);

Coroutine::Coroutine(std::function<void()> f, std::function<void(std::shared_ptr<ExceptionState>)> onDoneCallback, Scheduler* s)
    : func(std::move(f)), onDone(std::move(onDoneCallback)), state(State::Ready), scheduler(s), exceptionState(std::make_shared<ExceptionState>()) {
    DebugPrint("[Coroutine::Coroutine] Created fiber\n");
    fiber = CreateFiber(0, (LPFIBER_START_ROUTINE)CoroutineTrampoline, this);
}

Coroutine::~Coroutine() = default;

bool Coroutine::HasException() const {
    return ::HasException(exceptionState.get());
}

void Coroutine::RethrowExceptionIfAny() {
    DebugPrint("[Coroutine::RethrowExceptionIfAny] Rethrowing exception...\n");
    ::RethrowIfExists(exceptionState.get());
}

void Coroutine::Resume() {
    DebugPrint("[Coroutine::Resume] Before scheduler->Resume, state=%d\n", static_cast<int>(state));
    if (scheduler) {
        scheduler->Resume(this);
    }
    DebugPrint("[Coroutine::Resume] After scheduler->Resume, state=%d\n", static_cast<int>(state));
}

void Coroutine::YieldExecution() {
    Scheduler* s = GetCurrentScheduler();
    if (!s || !s->runningCoroutine) {
        return;
    }

    Coroutine* co = s->runningCoroutine;
    if (co->state != Coroutine::State::Finished) {
        co->state = Coroutine::State::Suspended;
    }

    SwitchToFiber(s->mainFiber);
}

void CoroutineTrampoline(void* arg) {
    Coroutine* co = static_cast<Coroutine*>(arg);
    co->func();
    co->state = Coroutine::State::Finished;
    Coroutine::YieldExecution();
}