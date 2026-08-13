#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Presents the first-run macOS importer when the legally supplied C&C Gold
// data is not installed. Returns false when the user chooses to quit.
bool TiberianDawn_PrepareGameData(void);

void TiberianDawn_ConfigureAudioSession(void);
bool TiberianDawn_RebuildAudioEngine(void);
void TiberianDawn_SetCompactWindowWarning(bool visible);
void TiberianDawn_ManageSaveGames(void);
int TiberianDawn_GetLanguagePreference(void);
int TiberianDawn_GetEffectiveLanguage(void);
void TiberianDawn_CycleLanguagePreference(void);
const char* TiberianDawn_LocalizedText(const char* key);
const char* TiberianDawn_LanguagePreferenceLabel(void);
const char* TiberianDawn_ClassicLanguageExtension(void);
void TiberianDawn_GetSafeAreaInsets(void* window,
                                   int output_width,
                                   int output_height,
                                   int* left,
                                   int* top,
                                   int* right,
                                   int* bottom);
int TiberianDawn_IsLowPowerModeEnabled(void);
int TiberianDawn_GetThermalState(void);

#ifdef __cplusplus
}
#endif
