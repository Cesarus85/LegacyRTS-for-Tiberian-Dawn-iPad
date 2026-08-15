#include "ipados_pencil.h"
#include "ipados_touch.h"
#include <cmath>

namespace
{
const float SqueezeTapTravelThreshold = 0.012f;
}

void IPadOS_Reset_Pencil_Squeeze(IPadOSPencilSqueezeState& state)
{
    state = IPadOSPencilSqueezeState();
}

IPadOSPencilSqueezeTransition IPadOS_Advance_Pencil_Squeeze(IPadOSPencilSqueezeState& state,
                                                            int event_code,
                                                            float x,
                                                            float y)
{
    IPadOSPencilSqueezeTransition result;
    if (event_code == IPADOS_EVENT_PENCIL_SQUEEZE_BEGIN) {
        state.active = true;
        state.last_x = x;
        state.last_y = y;
        state.travel = 0.0f;
        return result;
    }

    if (event_code == IPADOS_EVENT_PENCIL_SQUEEZE_MOVE) {
        if (!state.active) return result;
        result.delta_x = x - state.last_x;
        result.delta_y = y - state.last_y;
        state.last_x = x;
        state.last_y = y;
        state.travel += std::sqrt(result.delta_x * result.delta_x + result.delta_y * result.delta_y);
        state.pending_x += result.delta_x;
        state.pending_y += result.delta_y;
        if (!state.pan_started && state.travel >= SqueezeTapTravelThreshold) {
            state.pan_started = true;
        }
        if (state.pan_started && (state.pending_x != 0.0f || state.pending_y != 0.0f)) {
            result.delta_x = state.pending_x;
            result.delta_y = state.pending_y;
            state.pending_x = 0.0f;
            state.pending_y = 0.0f;
            result.actions |= IPADOS_PENCIL_SQUEEZE_PAN;
        } else {
            result.delta_x = 0.0f;
            result.delta_y = 0.0f;
        }
        return result;
    }

    if (event_code == IPADOS_EVENT_PENCIL_SQUEEZE_END) {
        if (state.active && !state.pan_started) {
            result.actions |= IPADOS_PENCIL_SQUEEZE_BACK;
        }
        IPadOS_Reset_Pencil_Squeeze(state);
        return result;
    }

    if (event_code == IPADOS_EVENT_PENCIL_SQUEEZE_CANCEL) {
        IPadOS_Reset_Pencil_Squeeze(state);
    }
    return result;
}
