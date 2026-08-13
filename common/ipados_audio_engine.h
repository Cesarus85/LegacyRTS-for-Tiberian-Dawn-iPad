#ifndef TIBERIAN_DAWN_IPADOS_AUDIO_ENGINE_H
#define TIBERIAN_DAWN_IPADOS_AUDIO_ENGINE_H

#include <stddef.h>

typedef void* IPadAudioStream;

bool IPad_Audio_Initialize(void);
void IPad_Audio_Shutdown(void);
void IPad_Audio_Pause(void);
bool IPad_Audio_Resume(void);
bool IPad_Audio_Rebuild(void);

IPadAudioStream IPad_Audio_Create_Stream(int bits_per_sample, int channels, int rate, int queue_capacity);
void IPad_Audio_Destroy_Stream(IPadAudioStream stream);
bool IPad_Audio_Set_Stream_Format(IPadAudioStream stream, int bits_per_sample, int channels, int rate);
void IPad_Audio_Set_Stream_Volume(IPadAudioStream stream, float volume);
bool IPad_Audio_Queue(IPadAudioStream stream, const void* data, size_t bytes);
int IPad_Audio_Free_Buffer_Count(IPadAudioStream stream);
void IPad_Audio_Play(IPadAudioStream stream);
void IPad_Audio_Pause_Stream(IPadAudioStream stream);
void IPad_Audio_Stop(IPadAudioStream stream);
bool IPad_Audio_Is_Playing(IPadAudioStream stream);

#endif
