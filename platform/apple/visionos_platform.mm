// Native visionOS platform boundary.
//
// V1 intentionally reuses the proven UIKit implementation. The file remains a
// separate translation unit so spatial-window, ornament, and immersion support
// can be added here without forking the game engine or the iPad target.
#include "visionos_platform.h"
#include "ipados_platform.mm"
