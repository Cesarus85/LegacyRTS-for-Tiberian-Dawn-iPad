#include "ipados_pencil.h"
#include "ipados_touch.h"
#include <cassert>
#include <cmath>

int main()
{
    IPadOSPencilSqueezeState state;

    IPadOSPencilSqueezeTransition result =
        IPadOS_Advance_Pencil_Squeeze(state, IPADOS_EVENT_PENCIL_SQUEEZE_BEGIN, 0.25f, 0.50f);
    assert(state.active);
    assert(result.actions == IPADOS_PENCIL_SQUEEZE_NONE);

    result = IPadOS_Advance_Pencil_Squeeze(state, IPADOS_EVENT_PENCIL_SQUEEZE_END, 0.25f, 0.50f);
    assert(!state.active);
    assert(result.actions == IPADOS_PENCIL_SQUEEZE_BACK);

    IPadOS_Advance_Pencil_Squeeze(state, IPADOS_EVENT_PENCIL_SQUEEZE_BEGIN, 0.25f, 0.50f);
    result = IPadOS_Advance_Pencil_Squeeze(state, IPADOS_EVENT_PENCIL_SQUEEZE_MOVE, 0.254f, 0.504f);
    assert(result.actions == IPADOS_PENCIL_SQUEEZE_NONE);
    result = IPadOS_Advance_Pencil_Squeeze(state, IPADOS_EVENT_PENCIL_SQUEEZE_END, 0.254f, 0.504f);
    assert(result.actions == IPADOS_PENCIL_SQUEEZE_BACK);

    IPadOS_Advance_Pencil_Squeeze(state, IPADOS_EVENT_PENCIL_SQUEEZE_BEGIN, 0.20f, 0.30f);
    result = IPadOS_Advance_Pencil_Squeeze(state, IPADOS_EVENT_PENCIL_SQUEEZE_MOVE, 0.24f, 0.35f);
    assert(result.actions == IPADOS_PENCIL_SQUEEZE_PAN);
    assert(std::fabs(result.delta_x - 0.04f) < 0.0001f);
    assert(std::fabs(result.delta_y - 0.05f) < 0.0001f);
    result = IPadOS_Advance_Pencil_Squeeze(state, IPADOS_EVENT_PENCIL_SQUEEZE_END, 0.24f, 0.35f);
    assert(result.actions == IPADOS_PENCIL_SQUEEZE_NONE);

    IPadOS_Advance_Pencil_Squeeze(state, IPADOS_EVENT_PENCIL_SQUEEZE_BEGIN, 0.4f, 0.4f);
    IPadOS_Advance_Pencil_Squeeze(state, IPADOS_EVENT_PENCIL_SQUEEZE_CANCEL, 0.4f, 0.4f);
    assert(!state.active);
    result = IPadOS_Advance_Pencil_Squeeze(state, IPADOS_EVENT_PENCIL_SQUEEZE_MOVE, 0.8f, 0.8f);
    assert(result.actions == IPADOS_PENCIL_SQUEEZE_NONE);
    return 0;
}
