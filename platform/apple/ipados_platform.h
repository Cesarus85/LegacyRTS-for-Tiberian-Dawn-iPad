#ifndef LEGACY_RTS_IPADOS_PLATFORM_H
#define LEGACY_RTS_IPADOS_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

// Performs the first-launch/migration workflow before the engine opens files.
// Returns false only when the user explicitly cancels without usable data.
bool LegacyRTS_PrepareGameData(void);

// Configures native audio behavior (silent switch, interruptions, routes).
void LegacyRTS_ConfigureAudioSession(void);

// Keeps a native, accessible warning synchronized with compact iPad windows.
void LegacyRTS_SetCompactWindowWarning(bool visible);

#ifdef __cplusplus
}
#endif

#endif
