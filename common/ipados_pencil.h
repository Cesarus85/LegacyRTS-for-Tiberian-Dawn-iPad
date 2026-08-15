#ifndef IPADOS_PENCIL_H
#define IPADOS_PENCIL_H

struct IPadOSPencilSqueezeState
{
    bool active = false;
    float last_x = 0.0f;
    float last_y = 0.0f;
    float travel = 0.0f;
    float pending_x = 0.0f;
    float pending_y = 0.0f;
    bool pan_started = false;
};

enum IPadOSPencilSqueezeAction
{
    IPADOS_PENCIL_SQUEEZE_NONE = 0,
    IPADOS_PENCIL_SQUEEZE_PAN = 1,
    IPADOS_PENCIL_SQUEEZE_BACK = 2,
};

struct IPadOSPencilSqueezeTransition
{
    unsigned actions = IPADOS_PENCIL_SQUEEZE_NONE;
    float delta_x = 0.0f;
    float delta_y = 0.0f;
};

IPadOSPencilSqueezeTransition IPadOS_Advance_Pencil_Squeeze(IPadOSPencilSqueezeState& state,
                                                            int event_code,
                                                            float x,
                                                            float y);
void IPadOS_Reset_Pencil_Squeeze(IPadOSPencilSqueezeState& state);

#endif
