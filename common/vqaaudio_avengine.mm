// Native iPadOS VQA audio backend. The movie decoder still owns its original
// circular PCM buffer; AVAudioEngine only replaces the former OpenAL queue.
#include "vqaaudio.h"
#include "audio.h"
#include "ipados_audio_engine.h"
#include "vqafile.h"
#include "vqaloader.h"
#include "vqatask.h"

#include <chrono>
#include <cstring>

int AudioFlags;
int TimerIntCount;
int TimerMethod;
int VQATickCount;
int TickOffset;
unsigned VQAAudioPaused;
VQAHandle* AudioVQAHandle;

namespace
{
IPadAudioStream VQAStream = nullptr;

bool Queue_Audio()
{
    if (!AudioVQAHandle || !VQAStream || IPad_Audio_Free_Buffer_Count(VQAStream) <= 0) return false;
    VQAConfig* config = &AudioVQAHandle->Config;
    VQAAudio* audio = &AudioVQAHandle->VQABuf->Audio;

    if (!IPad_Audio_Queue(VQAStream, &audio->Buffer[audio->PlayPosition], config->HMIBufSize)) return false;
    audio->field_B8 = audio->field_B0;
    audio->field_B0 += config->HMIBufSize;
    if (audio->field_B0 >= audio->BuffBytes) audio->field_B0 = 0;

    audio->field_14 = audio->field_10 + 1;
    if (audio->field_14 >= audio->NumAudBlocks) audio->field_14 = 0;
    if (audio->IsLoaded[audio->field_14] != 1) {
        if (VQAMovieDone) ++audio->field_B4;
        ++audio->NumSkipped;
        config->DrawFlags &= 0xFB;
        return false;
    }

    audio->IsLoaded[audio->field_10] = 0;
    audio->PlayPosition += config->HMIBufSize;
    ++audio->field_10;
    if (audio->PlayPosition >= static_cast<unsigned>(config->AudioBufSize)) {
        audio->PlayPosition = 0;
        audio->field_10 = 0;
    }
    ++audio->field_B4;
    return true;
}

void VQA_AudioCallback()
{
    if (!VQAAudioPaused && VQAStream && IPad_Audio_Free_Buffer_Count(VQAStream) > 0) Queue_Audio();
}
}

int VQA_StartTimerInt(VQAHandle* handle, int a2)
{
    (void)a2;
    VQAAudio* audio = &handle->VQABuf->Audio;
    if (!(AudioFlags & VQA_AUDIO_FLAG_INTERRUPT_TIMER)) AudioFlags |= VQA_AUDIO_FLAG_UNKNOWN016;
    audio->Flags |= VQA_AUDIO_FLAG_UNKNOWN016;
    ++TimerIntCount;
    return 0;
}

void VQA_StopTimerInt(VQAHandle* handle)
{
    (void)handle;
    if (TimerIntCount > 0) --TimerIntCount;
    AudioFlags &= ~VQA_AUDIO_FLAG_INTERRUPT_TIMER;
}

int VQA_OpenAudio(VQAHandle* handle, void* hwnd)
{
    (void)hwnd;
    VQAConfig* config = &handle->Config;
    VQAAudio* audio = &handle->VQABuf->Audio;
    VQAHeader* header = &handle->Header;

    Start_Primary_Sound_Buffer(true);
    audio->field_10 = 0;
    audio->field_BC = 1;
    if (config->AudioRate == -1) {
        config->AudioRate = header->FPS == config->FrameRate
            ? audio->SampleRate
            : config->FrameRate * audio->SampleRate / header->FPS;
    }
    audio->field_C0 = 1;
    audio->field_BC = 0;
    audio->Flags |= VQA_AUDIO_FLAG_UNKNOWN001;
    AudioFlags |= VQA_AUDIO_FLAG_UNKNOWN001;
    return 0;
}

void VQA_CloseAudio(VQAHandle* handle)
{
    VQAAudio* audio = &handle->VQABuf->Audio;
    VQA_StopAudio(handle);
    AudioFlags &= ~(VQA_AUDIO_FLAG_UNKNOWN004 | VQA_AUDIO_FLAG_UNKNOWN008);
    audio->Flags &= ~(VQA_AUDIO_FLAG_UNKNOWN004 | VQA_AUDIO_FLAG_UNKNOWN008);
    audio->field_C0 = 0;
    audio->field_BC = 0;
    audio->Flags &= ~(VQA_AUDIO_FLAG_UNKNOWN001 | VQA_AUDIO_FLAG_UNKNOWN002);
    AudioFlags &= ~(VQA_AUDIO_FLAG_UNKNOWN001 | VQA_AUDIO_FLAG_UNKNOWN002 | VQA_AUDIO_FLAG_AUDIO_DMA_TIMER);
}

int VQA_StartAudio(VQAHandle* handle)
{
    VQAConfig* config = &handle->Config;
    VQAAudio* audio = &handle->VQABuf->Audio;
    AudioVQAHandle = handle;
    if (AudioFlags & VQA_AUDIO_FLAG_AUDIO_DMA_TIMER) return -1;

    if (VQAStream) IPad_Audio_Destroy_Stream(VQAStream);
    VQAStream = IPad_Audio_Create_Stream(audio->BitsPerSample, audio->Channels, audio->SampleRate, 2);
    if (!VQAStream) {
        AudioVQAHandle = nullptr;
        return -1;
    }

    audio->BuffBytes = config->HMIBufSize * 4;
    audio->field_B0 = 0;
    audio->field_B4 = 0;
    Queue_Audio();
    Queue_Audio();
    IPad_Audio_Set_Stream_Volume(VQAStream, config->Volume / 256.0f);
    IPad_Audio_Play(VQAStream);
    audio->Flags |= VQA_AUDIO_FLAG_AUDIO_DMA_TIMER;
    AudioFlags |= VQA_AUDIO_FLAG_AUDIO_DMA_TIMER;
    return 0;
}

void VQA_StopAudio(VQAHandle* handle)
{
    VQAAudio* audio = &handle->VQABuf->Audio;
    if (VQAStream) {
        IPad_Audio_Stop(VQAStream);
        IPad_Audio_Destroy_Stream(VQAStream);
        VQAStream = nullptr;
    }
    audio->Flags &= ~VQA_AUDIO_FLAG_AUDIO_DMA_TIMER;
    AudioFlags &= ~VQA_AUDIO_FLAG_AUDIO_DMA_TIMER;
    AudioVQAHandle = nullptr;
}

void VQA_PauseAudio()
{
    if (AudioVQAHandle && VQAStream && (AudioFlags & VQA_AUDIO_FLAG_AUDIO_DMA_TIMER) && !VQAAudioPaused) {
        IPad_Audio_Pause_Stream(VQAStream);
        VQAAudioPaused = VQA_GetTime(AudioVQAHandle);
    }
}

void VQA_ResumeAudio()
{
    if (AudioVQAHandle && VQAStream && (AudioFlags & VQA_AUDIO_FLAG_AUDIO_DMA_TIMER) && VQAAudioPaused) {
        IPad_Audio_Play(VQAStream);
        TickOffset -= VQA_GetTime(AudioVQAHandle) - VQAAudioPaused;
        VQAAudioPaused = 0;
    }
}

int VQA_CopyAudio(VQAHandle* handle)
{
    VQAConfig* config = &handle->Config;
    VQAAudio* audio = &handle->VQABuf->Audio;
    VQA_AudioCallback();

    if ((config->OptionFlags & 1) && audio->Buffer && audio->TempBufSize > 0) {
        int currentBlock = audio->AudBufPos / config->HMIBufSize;
        int nextBlock = (audio->TempBufSize + audio->AudBufPos) / config->HMIBufSize;
        if (static_cast<unsigned>(nextBlock) >= audio->NumAudBlocks) nextBlock -= audio->NumAudBlocks;
        if (audio->IsLoaded[nextBlock] == 1) return -10;

        if (nextBlock < currentBlock) {
            const int endSpace = config->AudioBufSize - audio->AudBufPos;
            const int remaining = audio->TempBufSize - endSpace;
            std::memcpy(&audio->Buffer[audio->AudBufPos], audio->TempBuf, endSpace);
            std::memcpy(audio->Buffer, &audio->TempBuf[endSpace], remaining);
            audio->AudBufPos = remaining;
            audio->TempBufSize = 0;
            for (unsigned i = currentBlock; i < audio->NumAudBlocks; ++i) audio->IsLoaded[i] = 1;
            for (int i = 0; i < nextBlock; ++i) audio->IsLoaded[i] = 1;
        } else {
            std::memcpy(&audio->Buffer[audio->AudBufPos], audio->TempBuf, audio->TempBufSize);
            audio->AudBufPos += audio->TempBufSize;
            audio->TempBufSize = 0;
            for (int i = currentBlock; i < nextBlock; ++i) audio->IsLoaded[i] = 1;
        }
    }
    return 0;
}

void VQA_SetTimer(VQAHandle* handle, int time, int method)
{
    if (method == -1) {
        if (AudioFlags & VQA_AUDIO_FLAG_AUDIO_DMA_TIMER) method = VQA_AUDIO_TIMER_METHOD_DMA;
        else if (AudioFlags & (VQA_AUDIO_FLAG_UNKNOWN016 | VQA_AUDIO_FLAG_UNKNOWN032)) method = VQA_AUDIO_TIMER_METHOD_INTERRUPT;
        else method = VQA_AUDIO_TIMER_METHOD_DOS;
    } else {
        if (!(AudioFlags & VQA_AUDIO_FLAG_AUDIO_DMA_TIMER) && method == VQA_AUDIO_TIMER_METHOD_DMA)
            method = VQA_AUDIO_TIMER_METHOD_INTERRUPT;
        if (!(AudioFlags & (VQA_AUDIO_FLAG_UNKNOWN016 | VQA_AUDIO_FLAG_UNKNOWN032))
            && method == VQA_AUDIO_TIMER_METHOD_INTERRUPT)
            method = VQA_AUDIO_TIMER_METHOD_DOS;
    }
    TimerMethod = method;
    TickOffset = 0;
    TickOffset = time - VQA_GetTime(handle);
}

unsigned VQA_GetTime(VQAHandle* handle)
{
    (void)handle;
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<unsigned>(TickOffset
        + 60 * std::chrono::duration_cast<std::chrono::milliseconds>(now).count() / 1000);
}

int VQA_TimerMethod()
{
    return TimerMethod;
}
