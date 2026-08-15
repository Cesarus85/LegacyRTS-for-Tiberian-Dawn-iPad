#include "visionos_input.h"

#include "ipados_touch.h"

#include <algorithm>
#include <cmath>

namespace
{
bool Direction_Has_West(ScrollDirType direction)
{
    return direction == SDIR_W || direction == SDIR_NW || direction == SDIR_SW;
}

bool Direction_Has_East(ScrollDirType direction)
{
    return direction == SDIR_E || direction == SDIR_NE || direction == SDIR_SE;
}

bool Direction_Has_North(ScrollDirType direction)
{
    return direction == SDIR_N || direction == SDIR_NE || direction == SDIR_NW;
}

bool Direction_Has_South(ScrollDirType direction)
{
    return direction == SDIR_S || direction == SDIR_SE || direction == SDIR_SW;
}

float Edge_Proximity(float distance, float band)
{
    if (band <= 0.0f) return 0.0f;
    return std::max(0.0f, std::min(1.0f, (band - distance) / band));
}

void Stop_Edge_Scroll(VisionOSInputState& state)
{
    state.edge_scroll_active = false;
    state.edge_scroll_direction = SDIR_NONE;
    state.edge_scroll_intensity = 0.0f;
    state.edge_scroll_mode = VISIONOS_EDGE_SCROLL_NONE;
}

void Update_Edge_Scroll(VisionOSInputState& state,
                        float x,
                        float y,
                        VisionOSEdgeScrollMode mode,
                        const VisionOSInputConfiguration& configuration)
{
    const float enter = configuration.enter_edge_band;
    const float exit = std::max(enter, configuration.exit_edge_band);
    const ScrollDirType previous = state.edge_scroll_mode == mode ? state.edge_scroll_direction : SDIR_NONE;

    const bool west = x <= enter || (Direction_Has_West(previous) && x <= exit);
    const bool east = x >= 1.0f - enter || (Direction_Has_East(previous) && x >= 1.0f - exit);
    const bool north = y <= enter || (Direction_Has_North(previous) && y <= exit);
    const bool south = y >= 1.0f - enter || (Direction_Has_South(previous) && y >= 1.0f - exit);

    ScrollDirType direction = SDIR_NONE;
    if (east && north) direction = SDIR_NE;
    else if (east && south) direction = SDIR_SE;
    else if (west && north) direction = SDIR_NW;
    else if (west && south) direction = SDIR_SW;
    else if (east) direction = SDIR_E;
    else if (west) direction = SDIR_W;
    else if (north) direction = SDIR_N;
    else if (south) direction = SDIR_S;

    if (direction == SDIR_NONE) {
        Stop_Edge_Scroll(state);
        return;
    }

    float proximity = 0.0f;
    if (west) proximity = std::max(proximity, Edge_Proximity(x, enter));
    if (east) proximity = std::max(proximity, Edge_Proximity(1.0f - x, enter));
    if (north) proximity = std::max(proximity, Edge_Proximity(y, enter));
    if (south) proximity = std::max(proximity, Edge_Proximity(1.0f - y, enter));

    const float curved = std::pow(proximity, 1.45f);
    const float minimum = std::max(0.0f, std::min(1.0f, configuration.minimum_scroll_intensity));
    state.edge_scroll_active = true;
    state.edge_scroll_direction = direction;
    state.edge_scroll_intensity =
        (minimum + (1.0f - minimum) * curved) * configuration.scroll_speed_multiplier;
    state.edge_scroll_mode = mode;
}
}

VisionOSInputConfiguration VisionOS_Input_Configuration(int edge_sensitivity, int scroll_speed)
{
    VisionOSInputConfiguration configuration;
    switch (edge_sensitivity) {
    case 0:
        configuration.enter_edge_band = 0.06f;
        configuration.exit_edge_band = 0.078f;
        break;
    case 2:
        configuration.enter_edge_band = 0.12f;
        configuration.exit_edge_band = 0.155f;
        break;
    default:
        break;
    }

    switch (scroll_speed) {
    case 0:
        configuration.scroll_speed_multiplier = 0.70f;
        break;
    case 2:
        configuration.scroll_speed_multiplier = 1.40f;
        break;
    default:
        break;
    }
    return configuration;
}

ScrollDirType VisionOS_Edge_Direction(float x, float y, float band)
{
    const bool west = x <= band;
    const bool east = x >= 1.0f - band;
    const bool north = y <= band;
    const bool south = y >= 1.0f - band;
    if (east && north) return SDIR_NE;
    if (east && south) return SDIR_SE;
    if (west && north) return SDIR_NW;
    if (west && south) return SDIR_SW;
    if (east) return SDIR_E;
    if (west) return SDIR_W;
    if (north) return SDIR_N;
    if (south) return SDIR_S;
    return SDIR_NONE;
}

void VisionOS_Reset_Input(VisionOSInputState& state)
{
    state = VisionOSInputState();
}

VisionOSInputTransition VisionOS_Advance_Input(VisionOSInputState& state,
                                               int event_code,
                                               float normalized_x,
                                               float normalized_y,
                                               const VisionOSInputConfiguration& configuration)
{
    VisionOSInputTransition transition;
    switch (event_code) {
    case VISIONOS_EVENT_PRIMARY_TAP:
        transition.actions = VISIONOS_ACTION_LEFT_PRESS | VISIONOS_ACTION_LEFT_RELEASE;
        break;
    case VISIONOS_EVENT_DRAG_BEGIN:
        Stop_Edge_Scroll(state);
        if (!state.drag_button_down) {
            state.drag_button_down = true;
            transition.actions = VISIONOS_ACTION_LEFT_PRESS;
        }
        break;
    case VISIONOS_EVENT_DRAG_MOVE:
        if (state.drag_button_down) {
            Update_Edge_Scroll(state,
                               normalized_x,
                               normalized_y,
                               VISIONOS_EDGE_SCROLL_SELECTION,
                               configuration);
        }
        break;
    case VISIONOS_EVENT_DRAG_END:
    case VISIONOS_EVENT_DRAG_CANCEL:
        Stop_Edge_Scroll(state);
        if (state.drag_button_down) {
            state.drag_button_down = false;
            transition.actions = VISIONOS_ACTION_LEFT_RELEASE;
        }
        break;
    case VISIONOS_EVENT_LONG_PRESS_BEGIN:
        Update_Edge_Scroll(state,
                           normalized_x,
                           normalized_y,
                           VISIONOS_EDGE_SCROLL_HOLD,
                           configuration);
        if (!state.edge_scroll_active) {
            transition.actions = VISIONOS_ACTION_RIGHT_PRESS | VISIONOS_ACTION_RIGHT_RELEASE;
        }
        break;
    case VISIONOS_EVENT_LONG_PRESS_MOVE:
        Update_Edge_Scroll(state,
                           normalized_x,
                           normalized_y,
                           VISIONOS_EDGE_SCROLL_HOLD,
                           configuration);
        break;
    case VISIONOS_EVENT_LONG_PRESS_END:
        Stop_Edge_Scroll(state);
        break;
    default:
        break;
    }
    return transition;
}
