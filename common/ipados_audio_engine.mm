#import <AVFAudio/AVFAudio.h>
#import <Foundation/Foundation.h>

#include "ipados_audio_engine.h"
#include "audio_queue_state.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>

@interface TiberianDawnAudioStreamState : NSObject
{
@public
    AVAudioPlayerNode* Node;
    AVAudioFormat* Format;
    std::shared_ptr<AudioQueueState> Queue;
    int BitsPerSample;
    int Channels;
    int Rate;
    int QueueCapacity;
    bool AutoPlay;
    bool Active;
}
- (instancetype)initWithBits:(int)bits channels:(int)channels rate:(int)rate capacity:(int)capacity;
@end

@implementation TiberianDawnAudioStreamState
- (instancetype)initWithBits:(int)bits channels:(int)channels rate:(int)rate capacity:(int)capacity
{
    self = [super init];
    if (self) {
        Node = [AVAudioPlayerNode new];
        Queue = std::make_shared<AudioQueueState>(capacity);
        BitsPerSample = bits;
        Channels = channels;
        Rate = rate;
        QueueCapacity = capacity;
        AutoPlay = false;
        Active = true;
    }
    return self;
}
@end

namespace
{
std::mutex AudioMutex;
AVAudioEngine* Engine = nil;
NSMutableArray<TiberianDawnAudioStreamState*>* Streams = nil;

AVAudioFormat* MakeFormat(int bits, int channels, int rate)
{
    if ((bits != 8 && bits != 16) || (channels != 1 && channels != 2) || rate <= 0) return nil;
    return [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                           sampleRate:rate
                                             channels:static_cast<AVAudioChannelCount>(channels)
                                          interleaved:NO];
}

bool StartEngineLocked()
{
    if (!Engine) return false;
    if (Engine.isRunning) return true;

    // AVAudioEngine lazily creates its hardware endpoints. Starting a fresh
    // engine before either endpoint has been materialized raises an AVFAudio
    // exception on physical iPad hardware even though simulator builds pass.
    // Accessing outputNode establishes the render endpoint before prepare.
    AVAudioOutputNode* output = Engine.outputNode;
    if (!output) return false;

    NSError* error = nil;
    [Engine prepare];
    if ([Engine startAndReturnError:&error]) return true;

    // A route change can invalidate render resources. Keep our queue model in
    // sync with AVAudioEngine if its reset discards scheduled player buffers.
    [Engine reset];
    for (TiberianDawnAudioStreamState* state in Streams) {
        [state->Node stop];
        state->Queue->Reset();
    }
    error = nil;
    [Engine prepare];
    const bool started = [Engine startAndReturnError:&error];
    if (!started) NSLog(@"Tiberian Dawn AVAudioEngine start failed: %@", error.localizedDescription);
    return started;
}

TiberianDawnAudioStreamState* State(IPadAudioStream stream)
{
    return (__bridge TiberianDawnAudioStreamState*)stream;
}

AVAudioPCMBuffer* ConvertPCM(TiberianDawnAudioStreamState* state, const void* data, size_t bytes)
{
    if (!state || !state->Format || !data || bytes == 0) return nil;
    const size_t bytesPerSample = static_cast<size_t>(state->BitsPerSample / 8);
    const size_t bytesPerFrame = bytesPerSample * static_cast<size_t>(state->Channels);
    if (bytesPerFrame == 0) return nil;

    const AVAudioFrameCount frames = static_cast<AVAudioFrameCount>(bytes / bytesPerFrame);
    if (frames == 0) return nil;
    AVAudioPCMBuffer* buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:state->Format frameCapacity:frames];
    if (!buffer || !buffer.floatChannelData) return nil;
    buffer.frameLength = frames;

    const uint8_t* source = static_cast<const uint8_t*>(data);
    for (AVAudioFrameCount frame = 0; frame < frames; ++frame) {
        for (int channel = 0; channel < state->Channels; ++channel) {
            const size_t sample = static_cast<size_t>(frame) * state->Channels + channel;
            float value = 0.0f;
            if (state->BitsPerSample == 8) {
                value = (static_cast<int>(source[sample]) - 128) / 128.0f;
            } else {
                const size_t offset = sample * 2;
                const uint16_t raw = static_cast<uint16_t>(source[offset])
                                   | (static_cast<uint16_t>(source[offset + 1]) << 8);
                value = static_cast<int16_t>(raw) / 32768.0f;
            }
            buffer.floatChannelData[channel][frame] = value;
        }
    }
    return buffer;
}
}

bool IPad_Audio_Initialize(void)
{
    std::lock_guard<std::mutex> lock(AudioMutex);
    if (!Engine) {
        Engine = [AVAudioEngine new];
        Streams = [NSMutableArray array];
    }
    return StartEngineLocked();
}

void IPad_Audio_Shutdown(void)
{
    std::lock_guard<std::mutex> lock(AudioMutex);
    if (!Engine) return;
    [Engine stop];
    for (TiberianDawnAudioStreamState* state in [Streams copy]) {
        [state->Node stop];
        state->Queue->Reset();
        state->Active = false;
    }
    [Streams removeAllObjects];
    Engine = nil;
    Streams = nil;
}

void IPad_Audio_Pause(void)
{
    std::lock_guard<std::mutex> lock(AudioMutex);
    [Engine pause];
}

bool IPad_Audio_Resume(void)
{
    std::lock_guard<std::mutex> lock(AudioMutex);
    return StartEngineLocked();
}

bool IPad_Audio_Rebuild(void)
{
    std::lock_guard<std::mutex> lock(AudioMutex);
    if (!Engine) {
        Engine = [AVAudioEngine new];
        Streams = [NSMutableArray array];
        return StartEngineLocked();
    }

    [Engine stop];
    AVAudioEngine* replacement = [AVAudioEngine new];
    for (TiberianDawnAudioStreamState* state in Streams) {
        const bool shouldPlay = state->AutoPlay;
        [state->Node stop];
        state->Queue->Reset();
        state->Node = [AVAudioPlayerNode new];
        state->AutoPlay = shouldPlay;
        [replacement attachNode:state->Node];
        [replacement connect:state->Node to:replacement.mainMixerNode format:state->Format];
    }
    Engine = replacement;
    return StartEngineLocked();
}

IPadAudioStream IPad_Audio_Create_Stream(int bits_per_sample, int channels, int rate, int queue_capacity)
{
    std::lock_guard<std::mutex> lock(AudioMutex);
    if (!Engine) {
        Engine = [AVAudioEngine new];
        Streams = [NSMutableArray array];
    }
    AVAudioFormat* format = MakeFormat(bits_per_sample, channels, rate);
    if (!format) return nullptr;

    // AVAudioEngine may reject detachNode while a VQA player is unwinding,
    // even though the node was originally attached and connected. Keep idle
    // player nodes in the graph and reuse an exact-format match instead. This
    // also avoids mutating the live render graph whenever a movie is skipped.
    for (TiberianDawnAudioStreamState* pooled in Streams) {
        if (!pooled->Active && pooled->BitsPerSample == bits_per_sample && pooled->Channels == channels
            && pooled->Rate == rate && pooled->QueueCapacity == queue_capacity) {
            pooled->Queue->Reset();
            pooled->AutoPlay = false;
            pooled->Active = true;
            return (__bridge_retained void*)pooled;
        }
    }

    TiberianDawnAudioStreamState* state = [[TiberianDawnAudioStreamState alloc]
        initWithBits:bits_per_sample channels:channels rate:rate capacity:queue_capacity];
    state->Format = format;
    [Engine attachNode:state->Node];
    [Engine connect:state->Node to:Engine.mainMixerNode format:format];
    [Streams addObject:state];
    if (!StartEngineLocked()) {
        [state->Node stop];
        state->Queue->Reset();
        state->Active = false;
        return nullptr;
    }
    return (__bridge_retained void*)state;
}

void IPad_Audio_Destroy_Stream(IPadAudioStream stream)
{
    if (!stream) return;
    TiberianDawnAudioStreamState* state = State(stream);
    {
        std::lock_guard<std::mutex> lock(AudioMutex);
        [state->Node stop];
        state->AutoPlay = false;
        state->Queue->Reset();
        state->Active = false;
    }
    (void)CFBridgingRelease(stream);
}

bool IPad_Audio_Set_Stream_Format(IPadAudioStream stream, int bits_per_sample, int channels, int rate)
{
    TiberianDawnAudioStreamState* state = State(stream);
    if (!state) return false;
    if (state->BitsPerSample == bits_per_sample && state->Channels == channels && state->Rate == rate) return true;
    AVAudioFormat* format = MakeFormat(bits_per_sample, channels, rate);
    if (!format) return false;

    std::lock_guard<std::mutex> lock(AudioMutex);
    [state->Node stop];
    state->AutoPlay = false;
    state->Queue->Reset();
    if (Engine) {
        [Engine disconnectNodeOutput:state->Node];
        [Engine connect:state->Node to:Engine.mainMixerNode format:format];
    }
    state->Format = format;
    state->BitsPerSample = bits_per_sample;
    state->Channels = channels;
    state->Rate = rate;
    return StartEngineLocked();
}

void IPad_Audio_Set_Stream_Volume(IPadAudioStream stream, float volume)
{
    TiberianDawnAudioStreamState* state = State(stream);
    if (state) state->Node.volume = std::max(0.0f, std::min(1.0f, volume));
}

bool IPad_Audio_Queue(IPadAudioStream stream, const void* data, size_t bytes)
{
    TiberianDawnAudioStreamState* state = State(stream);
    if (!state) return false;
    AVAudioPCMBuffer* buffer = ConvertPCM(state, data, bytes);
    if (!buffer) return false;

    unsigned generation = 0;
    if (!state->Queue->Try_Queue(generation)) return false;
    // The completion may arrive after a movie has been skipped and its stream
    // handle released. Retain only the queue state, not the player wrapper.
    std::shared_ptr<AudioQueueState> callbackQueue = state->Queue;
    [state->Node scheduleBuffer:buffer
             completionCallbackType:AVAudioPlayerNodeCompletionDataPlayedBack
                  completionHandler:^(AVAudioPlayerNodeCompletionCallbackType callbackType) {
        callbackQueue->Complete(generation);
    }];
    if (state->AutoPlay && IPad_Audio_Resume()) {
        state->Queue->Start();
        [state->Node play];
    }
    return true;
}

int IPad_Audio_Free_Buffer_Count(IPadAudioStream stream)
{
    TiberianDawnAudioStreamState* state = State(stream);
    return state ? state->Queue->Free_Count() : 0;
}

void IPad_Audio_Play(IPadAudioStream stream)
{
    TiberianDawnAudioStreamState* state = State(stream);
    if (!state || state->Queue->Queued_Count() == 0) return;
    if (!IPad_Audio_Resume()) return;
    state->AutoPlay = true;
    state->Queue->Start();
    [state->Node play];
}

void IPad_Audio_Pause_Stream(IPadAudioStream stream)
{
    TiberianDawnAudioStreamState* state = State(stream);
    if (!state) return;
    [state->Node pause];
    state->AutoPlay = false;
    state->Queue->Pause();
}

void IPad_Audio_Stop(IPadAudioStream stream)
{
    TiberianDawnAudioStreamState* state = State(stream);
    if (!state) return;
    [state->Node stop];
    state->AutoPlay = false;
    state->Queue->Reset();
}

bool IPad_Audio_Is_Playing(IPadAudioStream stream)
{
    TiberianDawnAudioStreamState* state = State(stream);
    return state && state->Queue->Is_Playing();
}
