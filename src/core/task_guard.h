#pragma once
// Protect small value snapshots only. Never hold across I/O, heap or rendering.
#ifdef NATIVE_BUILD
#include <mutex>
using TaskMutex = std::recursive_mutex;
class TaskGuard {
    TaskMutex& m;
public:
    explicit TaskGuard(TaskMutex& mutex):m(mutex){m.lock();}
    ~TaskGuard(){m.unlock();}
};
#else
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
struct TaskMutex { portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; };
class TaskGuard {
    TaskMutex& m;
public:
    explicit TaskGuard(TaskMutex& mutex):m(mutex){portENTER_CRITICAL(&m.mux);}
    ~TaskGuard(){portEXIT_CRITICAL(&m.mux);}
};
#endif
