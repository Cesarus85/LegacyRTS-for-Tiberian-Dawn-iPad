#ifndef IPADOS_TOUCH_H
#define IPADOS_TOUCH_H

enum IPadOSInputEventCode
{
    IPADOS_EVENT_PENCIL_HOVER = 0x5048,
    IPADOS_EVENT_PENCIL_HOVER_END = 0x5045,
    IPADOS_EVENT_PENCIL_DOUBLE_TAP = 0x5044,
    IPADOS_EVENT_PENCIL_SQUEEZE_BEGIN = 0x5051,
    IPADOS_EVENT_PENCIL_SQUEEZE_MOVE = 0x5052,
    IPADOS_EVENT_PENCIL_SQUEEZE_END = 0x5053,
    IPADOS_EVENT_PENCIL_SQUEEZE_CANCEL = 0x5054,
    IPADOS_EVENT_AUDIO_PAUSE = 0x4150,
    IPADOS_EVENT_AUDIO_RESUME = 0x4152,
    IPADOS_EVENT_AUDIO_ROUTE_CHANGED = 0x4143,
    IPADOS_EVENT_AUDIO_RESET = 0x4158,
    VISIONOS_EVENT_PRIMARY_TAP = 0x5654,
    VISIONOS_EVENT_DRAG_BEGIN = 0x5642,
    VISIONOS_EVENT_DRAG_MOVE = 0x564D,
    VISIONOS_EVENT_DRAG_END = 0x5645,
    VISIONOS_EVENT_DRAG_CANCEL = 0x5643,
    VISIONOS_EVENT_LONG_PRESS_BEGIN = 0x564C,
    VISIONOS_EVENT_LONG_PRESS_MOVE = 0x5656,
    VISIONOS_EVENT_LONG_PRESS_END = 0x5652,
    VISIONOS_EVENT_LOOK_SCROLL = 0x5647,
};

// Returns a short-lived, normalized two-finger pan delta. Positive values mean
// the fingers moved right/down and the tactical view should follow them.
bool Consume_IPadOS_Touch_Pan(float& delta_x, float& delta_y);
void Discard_IPadOS_Touch_Pan(void);

// Consumes the system-managed visionOS 26 Look-to-Scroll displacement. The
// values follow content direction: positive X/Y moves the camera east/south.
bool Consume_VisionOS_Look_Scroll(float& delta_x, float& delta_y);
void Discard_VisionOS_Look_Scroll(void);

#endif
