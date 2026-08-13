#include "audio_queue_state.h"

#include <algorithm>

AudioQueueState::AudioQueueState(int capacity)
    : Capacity(std::max(1, capacity))
    , Queued(0)
    , Playing(false)
    , CurrentGeneration(1)
{
}

bool AudioQueueState::Try_Queue(unsigned& generation)
{
    int queued = Queued.load(std::memory_order_acquire);
    while (queued < Capacity) {
        if (Queued.compare_exchange_weak(queued, queued + 1, std::memory_order_acq_rel)) {
            generation = CurrentGeneration.load(std::memory_order_acquire);
            return true;
        }
    }
    return false;
}

void AudioQueueState::Complete(unsigned generation)
{
    if (generation != CurrentGeneration.load(std::memory_order_acquire)) return;

    int queued = Queued.load(std::memory_order_acquire);
    while (queued > 0) {
        if (Queued.compare_exchange_weak(queued, queued - 1, std::memory_order_acq_rel)) {
            if (queued == 1) Playing.store(false, std::memory_order_release);
            return;
        }
    }
}

void AudioQueueState::Reset()
{
    CurrentGeneration.fetch_add(1, std::memory_order_acq_rel);
    Queued.store(0, std::memory_order_release);
    Playing.store(false, std::memory_order_release);
}

void AudioQueueState::Start()
{
    Playing.store(Queued.load(std::memory_order_acquire) > 0, std::memory_order_release);
}

void AudioQueueState::Pause()
{
    Playing.store(false, std::memory_order_release);
}

int AudioQueueState::Free_Count() const
{
    return Capacity - Queued.load(std::memory_order_acquire);
}

int AudioQueueState::Queued_Count() const
{
    return Queued.load(std::memory_order_acquire);
}

bool AudioQueueState::Is_Playing() const
{
    return Playing.load(std::memory_order_acquire) && Queued.load(std::memory_order_acquire) > 0;
}

unsigned AudioQueueState::Generation() const
{
    return CurrentGeneration.load(std::memory_order_acquire);
}
