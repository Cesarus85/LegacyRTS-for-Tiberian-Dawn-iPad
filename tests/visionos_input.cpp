#include "visionos_input.h"

#include "ipados_touch.h"

#include <cassert>
#include <cmath>

int main()
{
    assert(VisionOS_Edge_Direction(0.5f, 0.5f) == SDIR_NONE);
    assert(VisionOS_Edge_Direction(0.02f, 0.5f) == SDIR_W);
    assert(VisionOS_Edge_Direction(0.98f, 0.02f) == SDIR_NE);
    assert(VisionOS_Edge_Direction(0.02f, 0.98f) == SDIR_SW);

    const VisionOSInputConfiguration narrow = VisionOS_Input_Configuration(0, 0);
    const VisionOSInputConfiguration balanced = VisionOS_Input_Configuration(1, 1);
    const VisionOSInputConfiguration wide = VisionOS_Input_Configuration(2, 2);
    assert(narrow.enter_edge_band < balanced.enter_edge_band);
    assert(wide.enter_edge_band > balanced.enter_edge_band);
    assert(narrow.scroll_speed_multiplier < balanced.scroll_speed_multiplier);
    assert(wide.scroll_speed_multiplier > balanced.scroll_speed_multiplier);

    VisionOSInputState state;
    VisionOSInputTransition result =
        VisionOS_Advance_Input(state, VISIONOS_EVENT_PRIMARY_TAP, 0.5f, 0.5f);
    assert(result.actions == (VISIONOS_ACTION_LEFT_PRESS | VISIONOS_ACTION_LEFT_RELEASE));

    result = VisionOS_Advance_Input(state, VISIONOS_EVENT_DRAG_BEGIN, 0.3f, 0.3f);
    assert(state.drag_button_down);
    assert(result.actions == VISIONOS_ACTION_LEFT_PRESS);
    result = VisionOS_Advance_Input(state, VISIONOS_EVENT_DRAG_BEGIN, 0.4f, 0.4f);
    assert(result.actions == VISIONOS_ACTION_NONE);
    VisionOS_Reset_Input(state);
    assert(!state.drag_button_down);
    assert(!state.edge_scroll_active);
    result = VisionOS_Advance_Input(state, VISIONOS_EVENT_DRAG_BEGIN, 0.4f, 0.4f);
    assert(state.drag_button_down);
    assert(result.actions == VISIONOS_ACTION_LEFT_PRESS);
    result = VisionOS_Advance_Input(state, VISIONOS_EVENT_DRAG_MOVE, 0.7f, 0.7f);
    assert(state.drag_button_down);
    assert(!state.edge_scroll_active);
    assert(result.actions == VISIONOS_ACTION_NONE);
    result = VisionOS_Advance_Input(state, VISIONOS_EVENT_DRAG_MOVE, 0.98f, 0.5f);
    assert(state.edge_scroll_active);
    assert(state.edge_scroll_mode == VISIONOS_EDGE_SCROLL_SELECTION);
    assert(state.edge_scroll_direction == SDIR_E);
    const float edge_intensity = state.edge_scroll_intensity;
    assert(edge_intensity > balanced.minimum_scroll_intensity);
    VisionOS_Advance_Input(state, VISIONOS_EVENT_DRAG_MOVE, 0.90f, 0.5f);
    assert(state.edge_scroll_active); // Hysteresis prevents edge jitter.
    assert(state.edge_scroll_direction == SDIR_E);
    assert(state.edge_scroll_intensity <= edge_intensity);
    VisionOS_Advance_Input(state, VISIONOS_EVENT_DRAG_MOVE, 0.75f, 0.5f);
    assert(!state.edge_scroll_active);
    result = VisionOS_Advance_Input(state, VISIONOS_EVENT_DRAG_CANCEL, 0.7f, 0.7f);
    assert(!state.drag_button_down);
    assert(!state.edge_scroll_active);
    assert(result.actions == VISIONOS_ACTION_LEFT_RELEASE);
    result = VisionOS_Advance_Input(state, VISIONOS_EVENT_DRAG_END, 0.7f, 0.7f);
    assert(result.actions == VISIONOS_ACTION_NONE);

    result = VisionOS_Advance_Input(state, VISIONOS_EVENT_LONG_PRESS_BEGIN, 0.98f, 0.02f);
    assert(state.edge_scroll_active);
    assert(state.edge_scroll_direction == SDIR_NE);
    assert(state.edge_scroll_mode == VISIONOS_EDGE_SCROLL_HOLD);
    assert(state.edge_scroll_intensity > 0.0f);
    assert(result.actions == VISIONOS_ACTION_NONE);
    VisionOS_Advance_Input(state, VISIONOS_EVENT_LONG_PRESS_MOVE, 0.5f, 0.5f);
    assert(!state.edge_scroll_active);
    VisionOS_Advance_Input(state, VISIONOS_EVENT_LONG_PRESS_MOVE, 0.02f, 0.5f);
    assert(state.edge_scroll_active);
    assert(state.edge_scroll_direction == SDIR_W);
    VisionOS_Advance_Input(state, VISIONOS_EVENT_LONG_PRESS_END, 0.02f, 0.5f);
    assert(!state.edge_scroll_active);
    assert(state.edge_scroll_direction == SDIR_NONE);

    result = VisionOS_Advance_Input(state, VISIONOS_EVENT_LONG_PRESS_BEGIN, 0.5f, 0.5f);
    assert(result.actions == (VISIONOS_ACTION_RIGHT_PRESS | VISIONOS_ACTION_RIGHT_RELEASE));
    return 0;
}
