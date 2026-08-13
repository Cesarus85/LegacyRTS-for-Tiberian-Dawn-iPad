#ifndef TIBERIAN_DAWN_AUDIO_QUEUE_STATE_H
#define TIBERIAN_DAWN_AUDIO_QUEUE_STATE_H

#include <atomic>

class AudioQueueState
{
public:
    explicit AudioQueueState(int capacity = 2);

    bool Try_Queue(unsigned& generation);
    void Complete(unsigned generation);
    void Reset();
    void Start();
    void Pause();

    int Free_Count() const;
    int Queued_Count() const;
    bool Is_Playing() const;
    unsigned Generation() const;

private:
    int Capacity;
    std::atomic<int> Queued;
    std::atomic<bool> Playing;
    std::atomic<unsigned> CurrentGeneration;
};

#endif
