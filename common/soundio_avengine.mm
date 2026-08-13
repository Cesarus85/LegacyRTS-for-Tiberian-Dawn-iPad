#include "soundio_imp.h"
#include "ipados_audio_engine.h"

#include <cstdlib>

enum
{
    AVAUDIO_BUFFER_COUNT = 2,
};

struct SampleTrackerTypeImp
{
    IPadAudioStream Stream;
    int BitsPerSample;
    int Channels;
    int Frequency;
};

void SoundImp_Buffer_Sample_Data(SampleTrackerTypeImp* st, const void* data, size_t datalen)
{
    if (st) IPad_Audio_Queue(st->Stream, data, datalen);
}

int SoundImp_Get_Sample_Free_Buffer_Count(SampleTrackerTypeImp* st)
{
    return st ? IPad_Audio_Free_Buffer_Count(st->Stream) : 0;
}

bool SoundImp_Init(int bits_per_sample, bool stereo, int rate, bool reverse_channels)
{
    (void)bits_per_sample;
    (void)stereo;
    (void)rate;
    (void)reverse_channels;
    return IPad_Audio_Initialize();
}

void SoundImp_PauseSound()
{
    IPad_Audio_Pause();
}

bool SoundImp_ResumeSound()
{
    return IPad_Audio_Resume();
}

SampleTrackerTypeImp* SoundImp_Init_Sample(int bits_per_sample, bool stereo, int rate)
{
    SampleTrackerTypeImp* st = static_cast<SampleTrackerTypeImp*>(std::calloc(1, sizeof(*st)));
    if (!st) return nullptr;
    st->BitsPerSample = bits_per_sample;
    st->Channels = stereo ? 2 : 1;
    st->Frequency = rate;
    st->Stream = IPad_Audio_Create_Stream(bits_per_sample, st->Channels, rate, AVAUDIO_BUFFER_COUNT);
    if (!st->Stream) {
        std::free(st);
        return nullptr;
    }
    return st;
}

void SoundImp_Set_Sample_Attributes(SampleTrackerTypeImp* st, int bits_per_sample, bool stereo, int rate)
{
    if (!st) return;
    const int channels = stereo ? 2 : 1;
    if (IPad_Audio_Set_Stream_Format(st->Stream, bits_per_sample, channels, rate)) {
        st->BitsPerSample = bits_per_sample;
        st->Channels = channels;
        st->Frequency = rate;
    }
}

void SoundImp_Set_Sample_Volume(SampleTrackerTypeImp* st, unsigned int volume)
{
    if (st) IPad_Audio_Set_Stream_Volume(st->Stream, volume / 65536.0f);
}

void SoundImp_Shutdown()
{
    IPad_Audio_Shutdown();
}

void SoundImp_Shutdown_Sample(SampleTrackerTypeImp* st)
{
    if (!st) return;
    IPad_Audio_Destroy_Stream(st->Stream);
    std::free(st);
}

void SoundImp_Start_Sample(SampleTrackerTypeImp* st)
{
    if (st) IPad_Audio_Play(st->Stream);
}

void SoundImp_Stop_Sample(SampleTrackerTypeImp* st)
{
    if (st) IPad_Audio_Stop(st->Stream);
}

bool SoundImp_Sample_Status(SampleTrackerTypeImp* st)
{
    return st && IPad_Audio_Is_Playing(st->Stream);
}
