#include "app_lifecycle.h"

#include <atomic>

namespace
{
std::atomic<uint32_t> PendingEvents(0);
std::atomic<uint32_t> PendingTransition(APP_LIFECYCLE_NONE);
std::atomic<bool> AppInForeground(true);
uint32_t DeliveredTransition = APP_LIFECYCLE_ENTERED_FOREGROUND;
AppLifecycleHandler LifecycleHandler = nullptr;
}

void Queue_App_Lifecycle_Event(AppLifecycleEvent event)
{
    if (event == APP_LIFECYCLE_ENTERED_BACKGROUND || event == APP_LIFECYCLE_ENTERED_FOREGROUND) {
        AppInForeground.store(event == APP_LIFECYCLE_ENTERED_FOREGROUND, std::memory_order_release);
        PendingTransition.store(event, std::memory_order_release);
        return;
    }

    PendingEvents.fetch_or(event, std::memory_order_release);
}

bool Is_App_In_Foreground(void)
{
    return AppInForeground.load(std::memory_order_acquire);
}

uint32_t Consume_App_Lifecycle_Events(void)
{
    uint32_t events = PendingEvents.exchange(0, std::memory_order_acq_rel);
    const uint32_t transition = PendingTransition.exchange(APP_LIFECYCLE_NONE, std::memory_order_acq_rel);
    if (transition != APP_LIFECYCLE_NONE && transition != DeliveredTransition) {
        DeliveredTransition = transition;
        events |= transition;
    }
    return events;
}

void Set_App_Lifecycle_Handler(AppLifecycleHandler handler)
{
    LifecycleHandler = handler;
}

void Dispatch_App_Lifecycle_Event(AppLifecycleEvent event)
{
    if (LifecycleHandler != nullptr) {
        LifecycleHandler(event);
    }
}
