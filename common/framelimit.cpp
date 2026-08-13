#include "framelimit.h"
#include "wwmouse.h"
#include "settings.h"
#include "video.h"
#include <algorithm>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

#include "mssleep.h"

extern WWMouseClass* WWMouse;

#if defined(NEW_VIDEO_BUILD) && !defined(IPADOS_PORT) && !defined(MACOS_PORT)
void Video_Render_Frame();
#endif

void Frame_Limiter(FrameLimitFlags flags)
{
#if defined(IPADOS_PORT) || defined(MACOS_PORT)
    using Clock = std::chrono::steady_clock;
    static Clock::time_point next_frame = Clock::now();
    static int previous_limit = 0;

    const int frame_limit = Video_Get_Effective_Frame_Limit();
    const std::chrono::microseconds frame_interval(1000000 / std::max(1, frame_limit));
    Clock::time_point now = Clock::now();

    if (frame_limit != previous_limit || now > next_frame + frame_interval * 2) {
        next_frame = now;
        previous_limit = frame_limit;
    }

    if (now < next_frame) {
        if (flags & FrameLimitFlags::FL_NO_BLOCK) {
            us_sleep(1000);
            return;
        }
        if (!(flags & FrameLimitFlags::FL_FORCE_RENDER)) {
            us_sleep(static_cast<unsigned>(
                std::chrono::duration_cast<std::chrono::microseconds>(next_frame - now).count()));
            return;
        }
        us_sleep(static_cast<unsigned>(
            std::chrono::duration_cast<std::chrono::microseconds>(next_frame - now).count()));
    }

    Video_Render_Frame();
    now = Clock::now();
    do {
        next_frame += frame_interval;
    } while (next_frame <= now);
    return;
#else
    static auto frame_start = std::chrono::steady_clock::now();
#ifdef NEW_VIDEO_BUILD
    static auto render_avg = 0;

    auto render_start = std::chrono::steady_clock::now();
    auto render_remaining = std::chrono::duration_cast<std::chrono::milliseconds>(frame_start - render_start).count();

    if (!(flags & FrameLimitFlags::FL_FORCE_RENDER) && render_remaining > render_avg) {
        if (!(flags & FrameLimitFlags::FL_NO_BLOCK)) {
            ms_sleep(unsigned(render_remaining));
        } else {
            ms_sleep(1); // Unconditionally yield for minimum time.
        }
        return;
    }

    Video_Render_Frame();

    auto render_end = std::chrono::steady_clock::now();
    auto render_time = std::chrono::duration_cast<std::chrono::milliseconds>(render_end - render_start).count();

    // keep up some average so we have an idea if we need to skip a frame or not
    render_avg = (render_avg + render_time) / 2;
#endif

    if (Settings.Video.FrameLimit > 0 && !(flags & FrameLimitFlags::FL_NO_BLOCK)) {
#ifdef NEW_VIDEO_BUILD
        auto frame_end = render_end;
#else
        auto frame_end = std::chrono::steady_clock::now();
#endif
        unsigned int min_frame_time = 1000000 / Settings.Video.FrameLimit;
        auto cur_frame_time = std::chrono::duration_cast<std::chrono::microseconds>(frame_end - frame_start).count();
        if (cur_frame_time < min_frame_time) {
            frame_start += std::chrono::microseconds{min_frame_time};
            us_sleep(min_frame_time - cur_frame_time);
        } else {
            frame_start = frame_end;
        }
    }
#endif
}
