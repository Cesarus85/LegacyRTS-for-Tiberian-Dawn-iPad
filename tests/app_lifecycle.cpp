#include "common/app_lifecycle.h"

#include <cassert>
#include <iostream>
#include <vector>

namespace
{
std::vector<AppLifecycleEvent> Delivered;

void Record(AppLifecycleEvent event)
{
    Delivered.push_back(event);
}
}

int main()
{
    assert(Is_App_In_Foreground());
    assert(Consume_App_Lifecycle_Events() == APP_LIFECYCLE_NONE);

    Queue_App_Lifecycle_Event(APP_LIFECYCLE_ENTERED_FOREGROUND);
    assert(Consume_App_Lifecycle_Events() == APP_LIFECYCLE_NONE);

    Queue_App_Lifecycle_Event(APP_LIFECYCLE_ENTERED_BACKGROUND);
    Queue_App_Lifecycle_Event(APP_LIFECYCLE_ENTERED_BACKGROUND);
    assert(!Is_App_In_Foreground());
    assert(Consume_App_Lifecycle_Events() == APP_LIFECYCLE_ENTERED_BACKGROUND);
    assert(Consume_App_Lifecycle_Events() == APP_LIFECYCLE_NONE);

    Queue_App_Lifecycle_Event(APP_LIFECYCLE_ENTERED_FOREGROUND);
    assert(Is_App_In_Foreground());
    assert(Consume_App_Lifecycle_Events() == APP_LIFECYCLE_ENTERED_FOREGROUND);

    // A whole rapid cycle between engine ticks must retain both edges so the
    // background autosave cannot be lost.
    Queue_App_Lifecycle_Event(APP_LIFECYCLE_ENTERED_BACKGROUND);
    Queue_App_Lifecycle_Event(APP_LIFECYCLE_ENTERED_FOREGROUND);
    assert(Is_App_In_Foreground());
    const uint32_t rapid_cycle = Consume_App_Lifecycle_Events();
    assert((rapid_cycle & APP_LIFECYCLE_ENTERED_BACKGROUND) != 0);
    assert((rapid_cycle & APP_LIFECYCLE_ENTERED_FOREGROUND) != 0);

    Queue_App_Lifecycle_Event(APP_LIFECYCLE_LOW_MEMORY);
    Queue_App_Lifecycle_Event(APP_LIFECYCLE_TERMINATING);
    const uint32_t independent = Consume_App_Lifecycle_Events();
    assert((independent & APP_LIFECYCLE_LOW_MEMORY) != 0);
    assert((independent & APP_LIFECYCLE_TERMINATING) != 0);

    Set_App_Lifecycle_Handler(Record);
    Dispatch_App_Lifecycle_Event(APP_LIFECYCLE_ENTERED_BACKGROUND);
    Dispatch_App_Lifecycle_Event(APP_LIFECYCLE_ENTERED_FOREGROUND);
    assert(Delivered.size() == 2);
    assert(Delivered[0] == APP_LIFECYCLE_ENTERED_BACKGROUND);
    assert(Delivered[1] == APP_LIFECYCLE_ENTERED_FOREGROUND);
    Set_App_Lifecycle_Handler(nullptr);

    std::cout << "iPad lifecycle hardening matrix passed\n";
    return 0;
}
