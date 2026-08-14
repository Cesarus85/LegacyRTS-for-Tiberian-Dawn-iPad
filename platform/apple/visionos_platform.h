#ifndef TIBERIAN_DAWN_VISIONOS_PLATFORM_H
#define TIBERIAN_DAWN_VISIONOS_PLATFORM_H

// The first native visionOS milestone shares the complete UIKit data-import,
// save-management, localization, audio-session, and lifecycle contract with
// iPadOS. Keeping this forwarding header explicit gives visionOS a stable
// platform boundary for later spatial UI services without duplicating APIs.
#include "ipados_platform.h"

#endif
