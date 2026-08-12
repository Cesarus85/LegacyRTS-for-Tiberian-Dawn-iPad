#ifndef IPADOS_TOUCH_H
#define IPADOS_TOUCH_H

enum IPadOSInputEventCode
{
    IPADOS_EVENT_PENCIL_HOVER = 0x5048,
    IPADOS_EVENT_PENCIL_DOUBLE_TAP = 0x5044,
    IPADOS_EVENT_PENCIL_SQUEEZE = 0x5053,
    IPADOS_EVENT_AUDIO_PAUSE = 0x4150,
    IPADOS_EVENT_AUDIO_RESUME = 0x4152,
    IPADOS_EVENT_AUDIO_ROUTE_CHANGED = 0x4143,
};

// Returns a short-lived, normalized two-finger pan delta. Positive values mean
// the fingers moved right/down and the tactical view should follow them.
bool Consume_IPadOS_Touch_Pan(float& delta_x, float& delta_y);
void Discard_IPadOS_Touch_Pan(void);

#endif
