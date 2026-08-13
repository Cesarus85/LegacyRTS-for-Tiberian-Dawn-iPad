#ifndef TIBERIAN_DAWN_IPADOS_PLATFORM_H
#define TIBERIAN_DAWN_IPADOS_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

// Performs the first-launch/migration workflow before the engine opens files.
// Returns false only when the user explicitly cancels without usable data.
bool TiberianDawn_PrepareGameData(void);

// Configures native audio behavior (silent switch, interruptions, routes).
void TiberianDawn_ConfigureAudioSession(void);
bool TiberianDawn_RebuildAudioEngine(void);

// Keeps a native, accessible warning synchronized with compact iPad windows.
void TiberianDawn_SetCompactWindowWarning(bool visible);

// Presents native import/export for manual saves via Files and iCloud Drive.
// The call returns after the manager is closed.
void TiberianDawn_ManageSaveGames(void);

// Runtime language preference: 0 = iPad system, 1 = German, 2 = English.
int TiberianDawn_GetLanguagePreference(void);
int TiberianDawn_GetEffectiveLanguage(void);
void TiberianDawn_CycleLanguagePreference(void);
const char* TiberianDawn_LocalizedText(const char* key);
const char* TiberianDawn_LanguagePreferenceLabel(void);
const char* TiberianDawn_ClassicLanguageExtension(void);

#ifdef __cplusplus
}
#endif

#endif
