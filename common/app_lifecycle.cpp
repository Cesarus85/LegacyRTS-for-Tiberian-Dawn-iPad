#include "app_lifecycle.h"

#include <atomic>

namespace
{
std::atomic<uint32_t> PendingEvents(0);
std::atomic<bool> AppInForeground(true);
AppLifecycleHandler LifecycleHandler = nullptr;
}

void Queue_App_Lifecycle_Event(AppLifecycleEvent event)
{
    if (event == APP_LIFECYCLE_ENTERED_BACKGROUND || event == APP_LIFECYCLE_ENTERED_FOREGROUND) {
        const bool foreground = event == APP_LIFECYCLE_ENTERED_FOREGROUND;
        const bool previous = AppInForeground.exchange(foreground, std::memory_order_acq_rel);
        if (previous != foreground) {
            // Preserve both edges if a rapid background/foreground cycle occurs
            // before the engine's next event pump. Duplicate UIKit/SDL notices
            // for the same state are still collapsed.
            PendingEvents.fetch_or(event, std::memory_order_release);
        }
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
    return PendingEvents.exchange(0, std::memory_order_acq_rel);
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
