#include "common/audio_queue_state.h"

#include <cassert>

int main()
{
    AudioQueueState queue(2);
    unsigned first = 0;
    unsigned second = 0;
    unsigned rejected = 0;
    assert(queue.Free_Count() == 2);
    assert(queue.Try_Queue(first));
    assert(queue.Try_Queue(second));
    assert(!queue.Try_Queue(rejected));
    assert(queue.Queued_Count() == 2);

    queue.Start();
    assert(queue.Is_Playing());
    queue.Complete(first);
    assert(queue.Free_Count() == 1);
    assert(queue.Is_Playing());
    queue.Complete(second);
    assert(queue.Free_Count() == 2);
    assert(!queue.Is_Playing());

    assert(queue.Try_Queue(first));
    const unsigned stale = first;
    queue.Reset();
    assert(queue.Free_Count() == 2);
    queue.Complete(stale);
    assert(queue.Free_Count() == 2);
    assert(queue.Generation() != stale);

    assert(queue.Try_Queue(first));
    queue.Start();
    queue.Pause();
    assert(!queue.Is_Playing());
    assert(queue.Queued_Count() == 1);
    return 0;
}
