#pragma once

#include "wwkeyboard.h"

#include <cstdint>

enum VisionOSInputAction : uint8_t
{
    VISIONOS_ACTION_NONE = 0,
    VISIONOS_ACTION_LEFT_PRESS = 1 << 0,
    VISIONOS_ACTION_LEFT_RELEASE = 1 << 1,
    VISIONOS_ACTION_RIGHT_PRESS = 1 << 2,
    VISIONOS_ACTION_RIGHT_RELEASE = 1 << 3,
};

enum VisionOSEdgeScrollMode : uint8_t
{
    VISIONOS_EDGE_SCROLL_NONE = 0,
    VISIONOS_EDGE_SCROLL_HOLD,
    VISIONOS_EDGE_SCROLL_SELECTION,
};

struct VisionOSInputConfiguration
{
    float enter_edge_band = 0.085f;
    float exit_edge_band = 0.11f;
    float minimum_scroll_intensity = 0.32f;
    float scroll_speed_multiplier = 1.0f;
};

struct VisionOSInputState
{
    bool drag_button_down = false;
    bool edge_scroll_active = false;
    ScrollDirType edge_scroll_direction = SDIR_NONE;
    float edge_scroll_intensity = 0.0f;
    VisionOSEdgeScrollMode edge_scroll_mode = VISIONOS_EDGE_SCROLL_NONE;
};

struct VisionOSInputTransition
{
    uint8_t actions = VISIONOS_ACTION_NONE;
};

VisionOSInputConfiguration VisionOS_Input_Configuration(int edge_sensitivity, int scroll_speed);
ScrollDirType VisionOS_Edge_Direction(float normalized_x, float normalized_y, float edge_band = 0.085f);
void VisionOS_Reset_Input(VisionOSInputState& state);
VisionOSInputTransition VisionOS_Advance_Input(VisionOSInputState& state,
                                               int event_code,
                                               float normalized_x,
                                               float normalized_y,
                                               const VisionOSInputConfiguration& configuration = {});
