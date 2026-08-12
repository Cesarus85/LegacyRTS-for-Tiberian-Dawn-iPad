#ifndef APP_LIFECYCLE_H
#define APP_LIFECYCLE_H

#include <stdint.h>

enum AppLifecycleEvent : uint32_t
{
    APP_LIFECYCLE_NONE = 0,
    APP_LIFECYCLE_TERMINATING = 1U << 0,
    APP_LIFECYCLE_LOW_MEMORY = 1U << 1,
    APP_LIFECYCLE_ENTERED_BACKGROUND = 1U << 2,
    APP_LIFECYCLE_ENTERED_FOREGROUND = 1U << 3
};

typedef void (*AppLifecycleHandler)(AppLifecycleEvent event);

void Queue_App_Lifecycle_Event(AppLifecycleEvent event);
uint32_t Consume_App_Lifecycle_Events(void);
void Set_App_Lifecycle_Handler(AppLifecycleHandler handler);
void Dispatch_App_Lifecycle_Event(AppLifecycleEvent event);
bool Is_App_In_Foreground(void);

#endif
