#ifndef TIBERIAN_DAWN_APPLE_RELAY_TRANSPORT_H
#define TIBERIAN_DAWN_APPLE_RELAY_TRANSPORT_H

#if defined(IPADOS_PORT) || defined(MACOS_PORT)

#include "wsproto.h"

enum TiberianDawnMultiplayerChoice
{
    TIBERIAN_MULTIPLAYER_CANCEL = 0,
    TIBERIAN_MULTIPLAYER_SKIRMISH = 1,
    TIBERIAN_MULTIPLAYER_NETWORK = 2
};

// Presents the shared Apple transport selection. Internet setup is completed
// before this returns; the legacy lobby then runs over the selected transport.
int TiberianDawn_SelectMultiplayerTransport(void);
WinsockInterfaceClass* TiberianDawn_CreateNetworkTransport(void);
void TiberianDawn_ResetNetworkTransport(void);

#endif
#endif
