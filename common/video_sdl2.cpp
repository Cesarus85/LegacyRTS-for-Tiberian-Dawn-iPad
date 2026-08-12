//
// Copyright 2020 Electronic Arts Inc.
//
// TiberianDawn.DLL and RedAlert.dll and corresponding source code is free
// software: you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.

// TiberianDawn.DLL and RedAlert.dll and corresponding source code is distributed
// in the hope that it will be useful, but with permitted additional restrictions
// under Section 7 of the GPL. See the GNU General Public License in LICENSE.TXT
// distributed with this program. You should have received a copy of the
// GNU General Public License along with permitted additional restrictions
// with this program. If not, see https://github.com/electronicarts/CnC_Remastered_Collection

/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D   A S S O C I A T E S   **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Westwood Win32 Library                   *
 *                                                                         *
 *                    File Name : DDRAW.CPP                                *
 *                                                                         *
 *                   Programmer : Philip W. Gorrow                         *
 *                                                                         *
 *                   Start Date : October 10, 1995                         *
 *                                                                         *
 *                  Last Update : October 10, 1995   []                    *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*=========================================================================*/
/* The following PRIVATE functions are in this file:                       */
/*=========================================================================*/

/*= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =*/

#include "gbuffer.h"
#include "palette.h"
#include "video.h"
#include "wwkeyboard.h"
#include "wwmouse.h"
#include "settings.h"
#include "debugstring.h"
#include "app_lifecycle.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>
#include <SDL.h>

#ifdef IPADOS_PORT
#include "../platform/apple/ipados_platform.h"
#include <mach/mach.h>
#include <sys/resource.h>
#endif

extern WWKeyboardClass* Keyboard;
static SDL_Window* window;
static SDL_Renderer* renderer;
static SDL_Palette* palette;
static Uint32 pixel_format;
static SDL_Rect render_dst;
static int renderer_output_w;
static int renderer_output_h;
static int window_points_w;
static int window_points_h;

#ifdef IPADOS_PORT
extern "C" void TiberianDawnForiPad_GetSafeAreaInsets(SDL_Window* sdl_window,
                                              int output_width,
                                              int output_height,
                                              int* left,
                                              int* top,
                                              int* right,
                                              int* bottom);
extern "C" int TiberianDawnForiPad_IsLowPowerModeEnabled(void);
extern "C" int TiberianDawnForiPad_GetThermalState(void);
#endif

#ifdef IPADOS_PORT
namespace
{
std::atomic<uint32_t> pending_input_timestamp(0);
uint64_t palette_generation = 1;
uint64_t cursor_generation = 1;
Uint8 previous_palette[256 * 3] = {0};
bool has_previous_palette = false;
bool force_present = true;
bool render_active = true;
bool last_frame_idle = false;
bool low_power_mode = false;
int thermal_state = 0;
Uint32 power_state_checked_at = 0;

struct TouchFeedbackState
{
    int x = 0;
    int y = 0;
    bool active = false;
    bool pencil = false;
    Uint32 expires_at = 0;
    uint64_t generation = 1;
};

TouchFeedbackState touch_feedback;

struct VideoTelemetry
{
    Uint32 interval_started_at = 0;
    uint64_t attempts = 0;
    uint64_t presents = 0;
    uint64_t uploads = 0;
    uint64_t unchanged_skips = 0;
    uint64_t inactive_skips = 0;
    double present_ms = 0.0;
    double input_latency_ms = 0.0;
    double max_input_latency_ms = 0.0;
    uint64_t input_samples = 0;
    double previous_cpu_seconds = 0.0;
};

VideoTelemetry telemetry;

const char* Thermal_State_Name(int state)
{
    static const char* const names[] = {"nominal", "fair", "serious", "critical"};
    return state >= 0 && state < 4 ? names[state] : "unknown";
}

void Update_Power_State()
{
    const Uint32 now = SDL_GetTicks();
    if (power_state_checked_at != 0 && now - power_state_checked_at < 1000) {
        return;
    }

    power_state_checked_at = now;
    const bool new_low_power_mode = TiberianDawnForiPad_IsLowPowerModeEnabled() != 0;
    const int new_thermal_state = TiberianDawnForiPad_GetThermalState();
    if (new_low_power_mode != low_power_mode || new_thermal_state != thermal_state) {
        low_power_mode = new_low_power_mode;
        thermal_state = new_thermal_state;
        DBG_INFO("iPad power policy changed: low_power=%s thermal=%s",
                 low_power_mode ? "on" : "off",
                 Thermal_State_Name(thermal_state));
    }
}

double Process_CPU_Seconds()
{
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0.0;
    }
    return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0 + usage.ru_stime.tv_sec
           + usage.ru_stime.tv_usec / 1000000.0;
}

double Resident_Megabytes()
{
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count)
        != KERN_SUCCESS) {
        return 0.0;
    }
    return info.resident_size / (1024.0 * 1024.0);
}

void Report_Telemetry_If_Due()
{
    const Uint32 now = SDL_GetTicks();
    if (telemetry.interval_started_at == 0) {
        telemetry.interval_started_at = now;
        telemetry.previous_cpu_seconds = Process_CPU_Seconds();
        return;
    }

    const Uint32 elapsed_ms = now - telemetry.interval_started_at;
    if (elapsed_ms < 5000) {
        return;
    }

    const double elapsed_seconds = elapsed_ms / 1000.0;
    const double cpu_seconds = Process_CPU_Seconds();
    const double cpu_percent = elapsed_seconds > 0.0
                                   ? (cpu_seconds - telemetry.previous_cpu_seconds) * 100.0 / elapsed_seconds
                                   : 0.0;
    const double average_present_ms = telemetry.presents > 0 ? telemetry.present_ms / telemetry.presents : 0.0;
    const double average_latency_ms = telemetry.input_samples > 0
                                          ? telemetry.input_latency_ms / telemetry.input_samples
                                          : 0.0;

    DBG_INFO("iPad perf: target=%d fps actual=%.1f uploads=%.1f/s unchanged=%llu inactive=%llu "
             "present=%.2fms cpu=%.1f%% resident=%.1fMB touch=%.1f/%.1fms low_power=%s thermal=%s",
             Video_Get_Effective_Frame_Limit(),
             telemetry.presents / elapsed_seconds,
             telemetry.uploads / elapsed_seconds,
             static_cast<unsigned long long>(telemetry.unchanged_skips),
             static_cast<unsigned long long>(telemetry.inactive_skips),
             average_present_ms,
             cpu_percent,
             Resident_Megabytes(),
             average_latency_ms,
             telemetry.max_input_latency_ms,
             low_power_mode ? "on" : "off",
             Thermal_State_Name(thermal_state));

    telemetry = VideoTelemetry();
    telemetry.interval_started_at = now;
    telemetry.previous_cpu_seconds = cpu_seconds;
}
}
#endif

static struct
{
    int GameW;
    int GameH;
    bool Clip;
    float ScaleX{1.0f};
    float ScaleY{1.0f};
    void* Raw;
    int W;
    int H;
    int HotX;
    int HotY;
    float X;
    float Y;
    SDL_Cursor* Pending;
    SDL_Cursor* Current;
    SDL_Surface* Surface;
} hwcursor;

#define ARRAY_SIZE(x) int(sizeof(x) / sizeof(x[0]))
#define MAKEFORMAT(f)                                                                                                  \
    {                                                                                                                  \
        SDL_PIXELFORMAT_##f, #f                                                                                        \
    }
Uint32 SettingsPixelFormat()
{
    /*
    ** Known good RGB formats for both the surface and texture.
    */
    static struct
    {
        Uint32 format;
        std::string name;
    } formats[] = {
        MAKEFORMAT(ARGB8888),
        MAKEFORMAT(RGBA8888),
        MAKEFORMAT(ABGR8888),
        MAKEFORMAT(BGRA8888),
        MAKEFORMAT(RGB24),
        MAKEFORMAT(BGR24),
        MAKEFORMAT(RGB888),
        MAKEFORMAT(BGR888),
        MAKEFORMAT(RGB555),
        MAKEFORMAT(BGR555),
        MAKEFORMAT(RGB565),
        MAKEFORMAT(BGR565),
    };

    std::string str = Settings.Video.PixelFormat;

    for (auto& c : str) {
        c = toupper(c);
    }

    for (int i = 0; i < ARRAY_SIZE(formats); i++) {
        if (str.compare(formats[i].name) == 0) {
            return formats[i].format;
        }
    }

    return SDL_PIXELFORMAT_UNKNOWN;
}

static void Update_HWCursor();

static void Update_HWCursor_Settings()
{
    if (window == nullptr || renderer == nullptr || hwcursor.GameW <= 0 || hwcursor.GameH <= 0) {
        return;
    }

    /*
    ** Renderer coordinates are drawable pixels, while SDL mouse coordinates
    ** are UIKit points. Keep both sizes so Retina windows and Stage Manager
    ** use the same mapping as normalized touch input.
    */
    int previous_output_w = renderer_output_w;
    int previous_output_h = renderer_output_h;
    SDL_GetRendererOutputSize(renderer, &renderer_output_w, &renderer_output_h);
    SDL_GetWindowSize(window, &window_points_w, &window_points_h);

    if (renderer_output_w <= 0 || renderer_output_h <= 0 || window_points_w <= 0 || window_points_h <= 0) {
        return;
    }

    int safe_left = 0;
    int safe_top = 0;
    int safe_right = 0;
    int safe_bottom = 0;
#ifdef IPADOS_PORT
    TiberianDawnForiPad_GetSafeAreaInsets(window,
                                renderer_output_w,
                                renderer_output_h,
                                &safe_left,
                                &safe_top,
                                &safe_right,
                                &safe_bottom);
#endif

    safe_left = std::max(0, std::min(safe_left, renderer_output_w));
    safe_top = std::max(0, std::min(safe_top, renderer_output_h));
    safe_right = std::max(0, std::min(safe_right, renderer_output_w - safe_left));
    safe_bottom = std::max(0, std::min(safe_bottom, renderer_output_h - safe_top));

    const int available_w = std::max(1, renderer_output_w - safe_left - safe_right);
    const int available_h = std::max(1, renderer_output_h - safe_top - safe_bottom);

    /*
    ** Update screen boxing settings.
    */
    float ar = (float)hwcursor.GameW / hwcursor.GameH;
#ifdef IPADOS_PORT
    /* The classic 640x400 surface must never be distorted on iPad. */
    if (Settings.Video.PresentationMode == 1) {
        const int integer_scale = std::max(1, std::min(available_w / hwcursor.GameW, available_h / hwcursor.GameH));
        render_dst.w = hwcursor.GameW * integer_scale;
        render_dst.h = hwcursor.GameH * integer_scale;
    } else {
        render_dst.w = available_w;
        render_dst.h = static_cast<int>(render_dst.w / ar + 0.5f);
        if (render_dst.h > available_h) {
            render_dst.h = available_h;
            render_dst.w = static_cast<int>(render_dst.h * ar + 0.5f);
        }
    }
    render_dst.x = safe_left + (available_w - render_dst.w) / 2;
    render_dst.y = safe_top + (available_h - render_dst.h) / 2;
    TiberianDawnForiPad_SetCompactWindowWarning(window_points_w < 640 || window_points_h < 400);
#else
    if (Settings.Video.Boxing) {
        size_t colonPos = Settings.Video.BoxingAspectRatio.find(":");
        std::string arW;
        std::string arH;

        /*
        ** If we don't have a valid string for aspect ratio, default back to 4:3.
        */
        if (colonPos == std::string::npos) {
            arW = "4";
            arH = "3";
        } else {
            size_t arLen = Settings.Video.BoxingAspectRatio.length();
            arW = Settings.Video.BoxingAspectRatio.substr(0, colonPos);
            arH = Settings.Video.BoxingAspectRatio.substr(colonPos + 1, arLen - colonPos);
        }

        ar = std::stof(arW) / std::stof(arH);

        render_dst.w = renderer_output_w;
        render_dst.h = render_dst.w / ar;
        if (render_dst.h > renderer_output_h) {
            render_dst.h = renderer_output_h;
            render_dst.w = render_dst.h * ar;
        }
        render_dst.x = (renderer_output_w - render_dst.w) / 2;
        render_dst.y = (renderer_output_h - render_dst.h) / 2;
    } else {
        render_dst.w = renderer_output_w;
        render_dst.h = renderer_output_h;
        render_dst.x = 0;
        render_dst.y = 0;
    }
#endif

    hwcursor.ScaleX = render_dst.w / (float)hwcursor.GameW;
    hwcursor.ScaleY = render_dst.h / (float)hwcursor.GameH;

    if (previous_output_w != renderer_output_w || previous_output_h != renderer_output_h) {
        DBG_INFO("Video layout: drawable %dx%d, viewport %dx%d at %d,%d",
                 renderer_output_w,
                 renderer_output_h,
                 render_dst.w,
                 render_dst.h,
                 render_dst.x,
                 render_dst.y);
    }

    /*
    ** Ensure cursor clip is in the desired state.
    */
    Set_Video_Cursor_Clip(hwcursor.Clip);

    /*
    ** Update visible cursor scaling.
    */
    Update_HWCursor();
}

class SurfaceMonitorClassDummy : public SurfaceMonitorClass
{

public:
    SurfaceMonitorClassDummy()
    {
    }

    virtual void Restore_Surfaces()
    {
    }

    virtual void Set_Surface_Focus(bool in_focus)
    {
    }

    virtual void Release()
    {
    }
};

SurfaceMonitorClassDummy AllSurfacesDummy;           // List of all direct draw surfaces
SurfaceMonitorClass& AllSurfaces = AllSurfacesDummy; // List of all direct draw surfaces

/***********************************************************************************************
 * Set_Video_Mode -- Initializes Direct Draw and sets the required Video Mode                  *
 *                                                                                             *
 * INPUT:           int width           - the width of the video mode in pixels                *
 *                  int height          - the height of the video mode in pixels               *
 *                  int bits_per_pixel  - the number of bits per pixel the video mode supports *
 *                                                                                             *
 * OUTPUT:     none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/26/1995 PWG : Created.                                                                 *
 *=============================================================================================*/
bool Set_Video_Mode(int w, int h, int bits_per_pixel)
{
#ifdef IPADOS_PORT
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");
#endif
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_ShowCursor(SDL_DISABLE);

    int win_w = w;
    int win_h = h;
    int win_flags = 0;
    Uint32 requested_pixel_format = SettingsPixelFormat();

#ifdef IPADOS_PORT
    Settings.Video.Windowed = true;
    Settings.Mouse.RawInput = false;
    win_flags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
#else
    if (!Settings.Video.Windowed) {
        /*
        ** Native fullscreen if no proper width and height set.
        */
        if (Settings.Video.Width < w || Settings.Video.Height < h) {
            win_w = Settings.Video.Width = 0;
            win_h = Settings.Video.Height = 0;
            win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        } else {
            win_w = Settings.Video.Width;
            win_h = Settings.Video.Height;
            win_flags |= SDL_WINDOW_FULLSCREEN;
        }
    } else if (Settings.Video.WindowWidth > w || Settings.Video.WindowHeight > h) {
        win_w = Settings.Video.WindowWidth;
        win_h = Settings.Video.WindowHeight;
    } else {
        Settings.Video.WindowWidth = win_w;
        Settings.Video.WindowHeight = win_h;
    }
#endif

    window =
        SDL_CreateWindow("Vanilla Conquer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, win_w, win_h, win_flags);
    if (window == nullptr) {
        DBG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        Reset_Video_Mode();
        return false;
    }

    DBG_INFO("Created SDL2 %s window in %dx%d", (win_flags ? "fullscreen" : "windowed"), win_w, win_h);

    pixel_format = SDL_GetWindowPixelFormat(window);
    if (pixel_format == SDL_PIXELFORMAT_UNKNOWN || SDL_BITSPERPIXEL(pixel_format) < 16) {
        DBG_ERROR("SDL2 window pixel format unsupported: %s (%d bpp)",
                  SDL_GetPixelFormatName(pixel_format),
                  SDL_BITSPERPIXEL(pixel_format));
        Reset_Video_Mode();
        return false;
    }

    DBG_INFO("  pixel format: %s (%d bpp)", SDL_GetPixelFormatName(pixel_format), SDL_BITSPERPIXEL(pixel_format));

    DBG_INFO("SDL2 drivers available: (user preference '%s')", Settings.Video.Driver.c_str());
    int renderer_index = -1;
    for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
        SDL_RendererInfo info;
        if (SDL_GetRenderDriverInfo(i, &info) == 0) {
            if (Settings.Video.Driver.compare(info.name) == 0) {
                renderer_index = i;
            }

            DBG_INFO(" %s%s", info.name, (i == renderer_index ? " (selected)" : ""));
        }
    }

    Uint32 renderer_flags = SDL_RENDERER_TARGETTEXTURE;
#ifdef IPADOS_PORT
    renderer_flags |= SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
#endif
    renderer = SDL_CreateRenderer(window, renderer_index, renderer_flags);
    if (renderer == nullptr) {
        DBG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        Reset_Video_Mode();
        return false;
    }

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) != 0) {
        DBG_ERROR("SDL_GetRendererInfo failed: %s", SDL_GetError());
        Reset_Video_Mode();
        return false;
    }

    DBG_INFO("Initialized SDL2 driver '%s'", info.name);
    DBG_INFO("  flags:");
    if (info.flags & SDL_RENDERER_SOFTWARE) {
        DBG_INFO("    SDL_RENDERER_SOFTWARE");
    }
    if (info.flags & SDL_RENDERER_ACCELERATED) {
        DBG_INFO("    SDL_RENDERER_ACCELERATED");
    }
    if (info.flags & SDL_RENDERER_PRESENTVSYNC) {
        DBG_INFO("    SDL_RENDERER_PRESENT_VSYNC");
    }
    if (info.flags & SDL_RENDERER_TARGETTEXTURE) {
        DBG_INFO("    SDL_RENDERER_TARGETTEXTURE");
    }

    DBG_INFO("  max texture size: %dx%d", info.max_texture_width, info.max_texture_height);

    DBG_INFO("  %d texture formats supported: (user preference '%s')",
             info.num_texture_formats,
             SDL_GetPixelFormatName(requested_pixel_format));

    /*
    ** Pick the first pixel format or the user requested one. It better be RGB.
    */
    pixel_format = SDL_PIXELFORMAT_UNKNOWN;
    for (int i = 0; i < info.num_texture_formats; i++) {
        if ((pixel_format == SDL_PIXELFORMAT_UNKNOWN && i == 0) || info.texture_formats[i] == requested_pixel_format) {
            pixel_format = info.texture_formats[i];
        }
    }

    for (int i = 0; i < info.num_texture_formats; i++) {
        DBG_INFO("    %s%s",
                 SDL_GetPixelFormatName(info.texture_formats[i]),
                 (pixel_format == info.texture_formats[i] ? " (selected)" : ""));
    }

    /*
    ** Set requested scaling algorithm.
    */
    if (!SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, Settings.Video.Scaler.c_str(), SDL_HINT_OVERRIDE)) {
        DBG_WARN("  scaler '%s' is unsupported");
    } else {
        DBG_INFO("  scaler set to '%s'", Settings.Video.Scaler.c_str());
    }

    if (palette == nullptr) {
        palette = SDL_AllocPalette(256);
    }

    /*
    ** Set mouse scaling options.
    */
    hwcursor.GameW = w;
    hwcursor.GameH = h;
    hwcursor.X = w / 2;
    hwcursor.Y = h / 2;
    Update_HWCursor_Settings();

    /*
    ** Init gamepad.
    */
    if (Settings.Mouse.ControllerEnabled
#ifdef IPADOS_PORT
        || true
#endif
    ) {
#ifdef IPADOS_PORT
        Settings.Mouse.ControllerEnabled = true;
#endif
        SDL_Init(SDL_INIT_GAMECONTROLLER);
        Keyboard->Open_Controller();
    }

    return true;
}

void Toggle_Video_Fullscreen()
{
#ifdef IPADOS_PORT
    /* iPadOS owns window geometry, including Stage Manager full screen. */
    Refresh_Video_Layout();
    return;
#else
    Settings.Video.Windowed = !Settings.Video.Windowed;

    if (!Settings.Video.Windowed) {
        if (Settings.Video.Width == 0 || Settings.Video.Height == 0) {
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        } else {
            SDL_SetWindowSize(window, Settings.Video.Width, Settings.Video.Height);
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
        }
    } else {
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowSize(window, Settings.Video.WindowWidth, Settings.Video.WindowHeight);
    }

    Update_HWCursor_Settings();
#endif
}

void Refresh_Video_Layout()
{
#ifdef IPADOS_PORT
    force_present = true;
#endif
    Update_HWCursor_Settings();
}

void Get_Video_Scale(float& x, float& y)
{
    x = hwcursor.ScaleX;
    y = hwcursor.ScaleY;
}

void Set_Video_Cursor_Clip(bool clipped)
{
    hwcursor.Clip = clipped;

    if (window) {
#ifdef IPADOS_PORT
        /* Pointer confinement and relative mode conflict with iPad windowing. */
        SDL_SetWindowGrab(window, SDL_FALSE);
        SDL_SetRelativeMouseMode(SDL_FALSE);
        return;
#else
        int relative;

        if (Settings.Video.Windowed) {
            SDL_SetWindowGrab(window, hwcursor.Clip ? SDL_TRUE : SDL_FALSE);
            relative = SDL_SetRelativeMouseMode(
                Settings.Mouse.ControllerEnabled || (Settings.Mouse.RawInput && hwcursor.Clip) ? SDL_TRUE : SDL_FALSE);

            /*
            ** When grabbing with raw input, move in-game cursor where the real cursor was and vice versa.
            */
            if (Settings.Mouse.RawInput) {
                if (hwcursor.Clip) {
                    int x, y;
                    SDL_GetMouseState(&x, &y);
                    hwcursor.X = x / hwcursor.ScaleX;
                    hwcursor.Y = y / hwcursor.ScaleY;
                } else {
                    SDL_WarpMouseInWindow(window, hwcursor.X * hwcursor.ScaleX, hwcursor.Y * hwcursor.ScaleY);
                }
            }
        } else {
            SDL_SetWindowGrab(window, SDL_TRUE);
            relative = SDL_SetRelativeMouseMode(Settings.Mouse.RawInput ? SDL_TRUE : SDL_FALSE);
        }

        if (relative < 0) {
            DBG_ERROR("Raw input not supported, disabling.");
            Settings.Mouse.RawInput = false;
        }
#endif
    }
}

void Move_Video_Mouse(float xrel, float yrel)
{
    if (Keyboard->Is_Gamepad_Active() || hwcursor.Clip || !Settings.Video.Windowed) {
        hwcursor.X += xrel * (Settings.Mouse.Sensitivity / 100.0f);
        hwcursor.Y += yrel * (Settings.Mouse.Sensitivity / 100.0f);
    }

    if (hwcursor.X >= hwcursor.GameW) {
        hwcursor.X = hwcursor.GameW - 1;
    } else if (hwcursor.X < 0) {
        hwcursor.X = 0;
    }

    if (hwcursor.Y >= hwcursor.GameH) {
        hwcursor.Y = hwcursor.GameH - 1;
    } else if (hwcursor.Y < 0) {
        hwcursor.Y = 0;
    }
}

void Get_Video_Mouse(int& x, int& y)
{
    if (Keyboard->Is_Gamepad_Active() || (Settings.Mouse.RawInput && (hwcursor.Clip || !Settings.Video.Windowed))) {
        x = hwcursor.X;
        y = hwcursor.Y;
    } else {
        int window_x = 0;
        int window_y = 0;
        SDL_GetMouseState(&window_x, &window_y);
#ifdef IPADOS_PORT
        Set_Video_Mouse_Window(window_x, window_y, x, y);
#else
        x = window_x / hwcursor.ScaleX;
        y = window_y / hwcursor.ScaleY;
#endif
    }
}

#ifdef IPADOS_PORT
static void Set_Video_Mouse_Output(float pixel_x, float pixel_y, int& game_x, int& game_y)
{
    const float scale_x = render_dst.w > 0 ? render_dst.w / static_cast<float>(hwcursor.GameW) : 1.0f;
    const float scale_y = render_dst.h > 0 ? render_dst.h / static_cast<float>(hwcursor.GameH) : 1.0f;

    game_x = static_cast<int>((pixel_x - render_dst.x) / scale_x);
    game_y = static_cast<int>((pixel_y - render_dst.y) / scale_y);
    game_x = std::max(0, std::min(game_x, hwcursor.GameW - 1));
    game_y = std::max(0, std::min(game_y, hwcursor.GameH - 1));
    hwcursor.X = game_x;
    hwcursor.Y = game_y;
}

void Set_Video_Mouse_Window(float window_x, float window_y, int& game_x, int& game_y)
{
    if (renderer_output_w <= 0 || renderer_output_h <= 0 || window_points_w <= 0 || window_points_h <= 0) {
        Refresh_Video_Layout();
    }

    const float pixel_x = window_x * renderer_output_w / static_cast<float>(std::max(1, window_points_w));
    const float pixel_y = window_y * renderer_output_h / static_cast<float>(std::max(1, window_points_h));
    Set_Video_Mouse_Output(pixel_x, pixel_y, game_x, game_y);
}

void Set_Video_Mouse_Normalized(float normalized_x, float normalized_y, int& game_x, int& game_y)
{
    if (renderer_output_w <= 0 || renderer_output_h <= 0) {
        Refresh_Video_Layout();
    }

    Set_Video_Mouse_Output(normalized_x * renderer_output_w, normalized_y * renderer_output_h, game_x, game_y);
}
#endif

/***********************************************************************************************
 * Reset_Video_Mode -- Resets video mode and deletes Direct Draw Object                        *
 *                                                                                             *
 * INPUT:		none                                                                            *
 *                                                                                             *
 * OUTPUT:     none                                                                            *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/26/1995 PWG : Created.                                                                 *
 *=============================================================================================*/
void Reset_Video_Mode(void)
{
    if (hwcursor.Pending) {
        SDL_FreeCursor(hwcursor.Pending);
        hwcursor.Pending = nullptr;
    }

    if (hwcursor.Current) {
        SDL_FreeCursor(hwcursor.Current);
        hwcursor.Current = nullptr;
    }

    if (hwcursor.Surface) {
        SDL_FreeSurface(hwcursor.Surface);
        hwcursor.Surface = nullptr;
    }

    SDL_DestroyRenderer(renderer);
    renderer = nullptr;

    SDL_FreePalette(palette);
    palette = nullptr;

    SDL_DestroyWindow(window);
    window = nullptr;

    Keyboard->Close_Controller();
}

static void Update_HWCursor()
{
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    int scaled_w = hwcursor.W;
    int scaled_h = hwcursor.H;

    /*
    ** Pre-scale cursor *only* if we are not emulating a hw cursor.
    */
    if (Settings.Video.HardwareCursor) {
        scale_x = hwcursor.ScaleX;
        scale_y = hwcursor.ScaleY;
        scaled_w *= scale_x;
        scaled_h *= scale_y;
    }

    /*
    ** Allocate or reallocate surface if it has the wrong size.
    */
    if (hwcursor.Surface == nullptr || hwcursor.Surface->w != scaled_w || hwcursor.Surface->h != scaled_h) {
        if (hwcursor.Surface) {
            SDL_FreeSurface(hwcursor.Surface);
        }

        /*
        ** Real HW cursor needs to be scaled up. Emulated can use original cursor data.
        */
        if (Settings.Video.HardwareCursor) {
            hwcursor.Surface = SDL_CreateRGBSurfaceWithFormat(0, scaled_w, scaled_h, 8, SDL_PIXELFORMAT_INDEX8);
        } else {
            hwcursor.Surface =
                SDL_CreateRGBSurfaceFrom(hwcursor.Raw, hwcursor.W, hwcursor.H, 8, hwcursor.W, 0, 0, 0, 0);
        }

        SDL_SetSurfacePalette(hwcursor.Surface, palette);
        SDL_SetColorKey(hwcursor.Surface, SDL_TRUE, 0);
    }

    /*
    ** Prepare HW cursor by scaling up and creating the SDL version.
    */
    if (Settings.Video.HardwareCursor) {
        uint8_t* src = (uint8_t*)hwcursor.Raw;
        uint8_t* dst = (uint8_t*)hwcursor.Surface->pixels;
        int src_pitch = hwcursor.W;
        int dst_pitch = hwcursor.Surface->pitch;

        for (int y = 0; y < scaled_h; y++) {
            for (int x = 0; x < scaled_w; x++) {
                dst[dst_pitch * y + x] = src[src_pitch * (int)(y / scale_y) + (int)(x / scale_x)];
            }
        }

        if (hwcursor.Pending) {
            SDL_FreeCursor(hwcursor.Pending);
        }

        /*
        ** Queue new cursor to be set during frame flip.
        */
        hwcursor.Pending =
            SDL_CreateColorCursor(hwcursor.Surface, hwcursor.HotX * hwcursor.ScaleX, hwcursor.HotY * hwcursor.ScaleY);
    }
}

void Set_Video_Cursor(void* cursor, int w, int h, int hotx, int hoty)
{
    hwcursor.Raw = cursor;
    hwcursor.W = w;
    hwcursor.H = h;
    hwcursor.HotX = hotx;
    hwcursor.HotY = hoty;

#ifdef IPADOS_PORT
    ++cursor_generation;
    force_present = true;
#endif

    Update_HWCursor();
}

/***********************************************************************************************
 * Get_Free_Video_Memory -- returns amount of free video memory                                *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   bytes of available video RAM                                                      *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    11/29/95 12:52PM ST : Created                                                            *
 *=============================================================================================*/
unsigned int Get_Free_Video_Memory(void)
{
    return 1000000000;
}

/***********************************************************************************************
 * Get_Video_Hardware_Caps -- returns bitmask of direct draw video hardware support            *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   hardware flags                                                                    *
 *                                                                                             *
 * WARNINGS: Must call Set_Video_Mode 1st to create the direct draw object                     *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    1/12/96 9:14AM ST : Created                                                              *
 *=============================================================================================*/
unsigned Get_Video_Hardware_Capabilities(void)
{
    return VIDEO_BLITTER;
}

/***********************************************************************************************
 * Wait_Vert_Blank -- Waits for the start (leading edge) of a vertical blank                   *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *=============================================================================================*/
void Wait_Vert_Blank(void)
{
}

/***********************************************************************************************
 * Set_Palette -- set a direct draw palette                                                    *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    ptr to 768 rgb palette bytes                                                      *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    10/11/95 3:33PM ST : Created                                                             *
 *=============================================================================================*/
void Set_DD_Palette(void* rpalette)
{
#ifdef IPADOS_PORT
    if (has_previous_palette && std::memcmp(previous_palette, rpalette, sizeof(previous_palette)) == 0) {
        return;
    }
    std::memcpy(previous_palette, rpalette, sizeof(previous_palette));
    has_previous_palette = true;
#endif
    SDL_Color colors[256];

    unsigned char* rcolors = (unsigned char*)rpalette;
    for (int i = 0; i < 256; i++) {
        colors[i].r = (unsigned char)rcolors[i * 3] << 2;
        colors[i].g = (unsigned char)rcolors[i * 3 + 1] << 2;
        colors[i].b = (unsigned char)rcolors[i * 3 + 2] << 2;
        colors[i].a = 0xFF;
    }

    /*
    ** First color is transparent. This needs to be set so that hardware cursor has transparent
    ** surroundings when converting from 8-bit to 32-bit.
    */
    colors[0].a = 0;

    SDL_SetPaletteColors(palette, colors, 0, 256);

#ifdef IPADOS_PORT
    ++palette_generation;
    force_present = true;
#endif

    /*
    ** Cursor needs to be updated when palette changes.
    */
    Update_HWCursor();
}

/***********************************************************************************************
 * Wait_Blit -- waits for the DirectDraw blitter to become idle                                *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07-25-95 03:53pm ST : Created                                                             *
 *=============================================================================================*/

void Wait_Blit(void)
{
}

/***********************************************************************************************
 * SMC::SurfaceMonitorClass -- constructor for surface monitor class                           *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    11/3/95 3:23PM ST : Created                                                              *
 *=============================================================================================*/

SurfaceMonitorClass::SurfaceMonitorClass()
{
    SurfacesRestored = false;
}

/*
** VideoSurfaceDDraw
*/
class VideoSurfaceSDL2;
static VideoSurfaceSDL2* frontSurface = nullptr;

class VideoSurfaceSDL2 : public VideoSurface
{
public:
    VideoSurfaceSDL2(int w, int h, GBC_Enum flags)
        : flags(flags)
        , windowSurface(nullptr)
        , texture(nullptr)
#ifdef IPADOS_PORT
        , lastPaletteGeneration(0)
        , lastCursorGeneration(0)
        , lastTouchFeedbackGeneration(0)
        , lastCursorX(-1)
        , lastCursorY(-1)
        , hadSoftwareCursor(false)
        , hasLastFrame(false)
#endif
    {
        surface = SDL_CreateRGBSurface(0, w, h, 8, 0, 0, 0, 0);
        SDL_SetSurfacePalette(surface, palette);

        if (flags & GBC_VISIBLE) {
            windowSurface = SDL_CreateRGBSurfaceWithFormat(0, w, h, SDL_BITSPERPIXEL(pixel_format), pixel_format);
            texture = SDL_CreateTexture(renderer, windowSurface->format->format, SDL_TEXTUREACCESS_STREAMING, w, h);
            frontSurface = this;
        }
    }

    virtual ~VideoSurfaceSDL2()
    {
        if (frontSurface == this) {
            frontSurface = nullptr;
        }

        SDL_FreeSurface(surface);

        if (texture) {
            SDL_DestroyTexture(texture);
        }

        if (windowSurface) {
            SDL_FreeSurface(windowSurface);
        }
    }

    virtual void* GetData() const
    {
        return surface->pixels;
    }
    virtual int GetPitch() const
    {
        return surface->pitch;
    }
    virtual bool IsAllocated() const
    {
        return false;
    }

    virtual void AddAttachedSurface(VideoSurface* surface)
    {
    }

    virtual bool IsReadyToBlit()
    {
        return true;
    }

    virtual bool LockWait()
    {
        return (SDL_LockSurface(surface) == 0);
    }

    virtual bool Unlock()
    {
        SDL_UnlockSurface(surface);
        return true;
    }

    virtual void Blt(const Rect& destRect, VideoSurface* src, const Rect& srcRect, bool mask)
    {
        SDL_BlitSurface(((VideoSurfaceSDL2*)src)->surface, (SDL_Rect*)(&srcRect), surface, (SDL_Rect*)&destRect);
    }

    virtual void FillRect(const Rect& rect, unsigned char color)
    {
        SDL_Rect rectSDL = {rect.X, rect.Y, rect.Width + 1, rect.Height + 1};
        SDL_FillRect(surface, &rectSDL, color);
    }

    bool RenderSurface()
    {
        void* pixels;
        int pitch;

#ifdef IPADOS_PORT
        ++telemetry.attempts;
        Update_Power_State();
        if (touch_feedback.active
            && static_cast<Sint32>(SDL_GetTicks() - touch_feedback.expires_at) >= 0) {
            touch_feedback.active = false;
            ++touch_feedback.generation;
        }

        const Uint32 window_flags = window ? SDL_GetWindowFlags(window) : 0;
        const bool drawable = render_active && Is_App_In_Foreground() && window != nullptr && renderer != nullptr
                              && (window_flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED)) == 0;
        if (!drawable) {
            ++telemetry.inactive_skips;
            last_frame_idle = true;
            Report_Telemetry_If_Due();
            return false;
        }

        int cursor_x = -1;
        int cursor_y = -1;
        const bool software_cursor = !Settings.Video.HardwareCursor && !Get_Mouse_State() && hwcursor.Surface != nullptr;
        if (software_cursor) {
            Get_Video_Mouse(cursor_x, cursor_y);
        }

        bool indexed_pixels_changed = !hasLastFrame;
        if (!indexed_pixels_changed) {
            for (int y = 0; y < surface->h; ++y) {
                const Uint8* row = static_cast<const Uint8*>(surface->pixels) + y * surface->pitch;
                if (std::memcmp(row, &lastIndexedPixels[y * surface->w], surface->w) != 0) {
                    indexed_pixels_changed = true;
                    break;
                }
            }
        }

        const bool visible_change = force_present || indexed_pixels_changed
                                    || lastPaletteGeneration != palette_generation
                                    || lastCursorGeneration != cursor_generation
                                    || lastTouchFeedbackGeneration != touch_feedback.generation
                                    || hadSoftwareCursor != software_cursor || lastCursorX != cursor_x
                                    || lastCursorY != cursor_y;
        if (!visible_change) {
            ++telemetry.unchanged_skips;
            last_frame_idle = true;
            Report_Telemetry_If_Due();
            return false;
        }

        if (indexed_pixels_changed) {
            lastIndexedPixels.resize(surface->w * surface->h);
            for (int y = 0; y < surface->h; ++y) {
                const Uint8* row = static_cast<const Uint8*>(surface->pixels) + y * surface->pitch;
                std::memcpy(&lastIndexedPixels[y * surface->w], row, surface->w);
            }
        }
#endif

        SDL_BlitSurface(surface, NULL, windowSurface, NULL);

        if (Settings.Video.HardwareCursor) {
            /*
            ** Swap cursor before a frame is drawn. This reduces flickering when it's done only once per frame.
            */
            if (hwcursor.Pending) {
                SDL_SetCursor(hwcursor.Pending);

                if (hwcursor.Current) {
                    SDL_FreeCursor(hwcursor.Current);
                }

                hwcursor.Current = hwcursor.Pending;
                hwcursor.Pending = nullptr;
            }

            /*
            ** Update hardware cursor visibility.
            */
            SDL_ShowCursor(!Get_Mouse_State());
        } else if (!Get_Mouse_State() && hwcursor.Surface != nullptr) {
            /*
            ** Draw software emulated cursor.
            */
            int x, y;
            SDL_Rect dst;

            Get_Video_Mouse(x, y);

            const float cursor_scale = Settings.Video.LargeCursor ? 1.5f : 1.0f;
            dst.x = x - static_cast<int>(hwcursor.HotX * cursor_scale);
            dst.y = y - static_cast<int>(hwcursor.HotY * cursor_scale);
            dst.w = static_cast<int>(hwcursor.Surface->w * cursor_scale);
            dst.h = static_cast<int>(hwcursor.Surface->h * cursor_scale);

            if (Settings.Video.LargeCursor) {
                SDL_BlitScaled(hwcursor.Surface, nullptr, windowSurface, &dst);
            } else {
                SDL_BlitSurface(hwcursor.Surface, nullptr, windowSurface, &dst);
            }
        }

        SDL_UpdateTexture(texture, NULL, windowSurface->pixels, windowSurface->pitch);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, &render_dst);
#ifdef IPADOS_PORT
        if (touch_feedback.active) {
            const float scale_x = render_dst.w / static_cast<float>(hwcursor.GameW);
            const float scale_y = render_dst.h / static_cast<float>(hwcursor.GameH);
            const int center_x = render_dst.x + static_cast<int>(touch_feedback.x * scale_x);
            const int center_y = render_dst.y + static_cast<int>(touch_feedback.y * scale_y);
            const int radius = (touch_feedback.pencil ? 10 : 16) * Settings.Video.TouchUIScale / 100;
            SDL_Point ring[33];
            for (int i = 0; i <= 32; ++i) {
                const float angle = static_cast<float>(i) * 6.28318530718f / 32.0f;
                ring[i].x = center_x + static_cast<int>(std::cos(angle) * radius);
                ring[i].y = center_y + static_cast<int>(std::sin(angle) * radius);
            }
            if (touch_feedback.pencil) {
                SDL_SetRenderDrawColor(renderer, 80, 210, 255, 220);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 220, 80, 205);
            }
            SDL_RenderDrawLines(renderer, ring, 33);
            if (Settings.Video.HighContrast) {
                for (int i = 0; i <= 32; ++i) {
                    const float angle = static_cast<float>(i) * 6.28318530718f / 32.0f;
                    ring[i].x = center_x + static_cast<int>(std::cos(angle) * (radius + 3));
                    ring[i].y = center_y + static_cast<int>(std::sin(angle) * (radius + 3));
                }
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 245);
                SDL_RenderDrawLines(renderer, ring, 33);
            }
        }
        const Uint64 present_started = SDL_GetPerformanceCounter();
#endif
        SDL_RenderPresent(renderer);
#ifdef IPADOS_PORT
        const Uint64 present_finished = SDL_GetPerformanceCounter();
        telemetry.present_ms += (present_finished - present_started) * 1000.0 / SDL_GetPerformanceFrequency();
        ++telemetry.presents;
        ++telemetry.uploads;

        const Uint32 input_timestamp = pending_input_timestamp.exchange(0, std::memory_order_acq_rel);
        if (input_timestamp != 0) {
            const double latency = static_cast<Uint32>(SDL_GetTicks() - input_timestamp);
            telemetry.input_latency_ms += latency;
            telemetry.max_input_latency_ms = std::max(telemetry.max_input_latency_ms, latency);
            ++telemetry.input_samples;
        }

        lastPaletteGeneration = palette_generation;
        lastCursorGeneration = cursor_generation;
        lastTouchFeedbackGeneration = touch_feedback.generation;
        lastCursorX = cursor_x;
        lastCursorY = cursor_y;
        hadSoftwareCursor = software_cursor;
        hasLastFrame = true;
        force_present = false;
        last_frame_idle = false;
        Report_Telemetry_If_Due();
#endif
        return true;
    }

private:
    SDL_Surface* surface;
    SDL_Surface* windowSurface;
    SDL_Texture* texture;
    GBC_Enum flags;
#ifdef IPADOS_PORT
    std::vector<Uint8> lastIndexedPixels;
    uint64_t lastPaletteGeneration;
    uint64_t lastCursorGeneration;
    uint64_t lastTouchFeedbackGeneration;
    int lastCursorX;
    int lastCursorY;
    bool hadSoftwareCursor;
    bool hasLastFrame;
#endif
};

#ifdef IPADOS_PORT
bool Video_Render_Frame()
{
    if (frontSurface) {
        return frontSurface->RenderSurface();
    }
    return false;
}

void Set_Video_Render_Active(bool active)
{
    render_active = active;
    if (active) {
        force_present = true;
        last_frame_idle = false;
    }
}

void Video_Record_Input_Timestamp(uint32_t timestamp_ms)
{
    pending_input_timestamp.store(timestamp_ms != 0 ? timestamp_ms : SDL_GetTicks(), std::memory_order_release);
    // An input event must immediately leave the 15 Hz idle cadence. This keeps
    // the first pointer/touch response as quick as subsequent ones.
    last_frame_idle = false;
}

void Video_Set_Touch_Feedback(int game_x, int game_y, bool pencil, bool released)
{
    touch_feedback.x = std::max(0, std::min(game_x, hwcursor.GameW - 1));
    touch_feedback.y = std::max(0, std::min(game_y, hwcursor.GameH - 1));
    touch_feedback.pencil = pencil;
    touch_feedback.active = true;
    touch_feedback.expires_at = SDL_GetTicks() + (released ? 180 : 1000);
    ++touch_feedback.generation;
    force_present = true;
    last_frame_idle = false;
}

void Set_IPadOS_Text_Input_Rect(int game_x, int game_y, int game_w, int game_h)
{
    if (!window || !renderer || hwcursor.GameW <= 0 || hwcursor.GameH <= 0) {
        return;
    }

    int window_w = 0;
    int window_h = 0;
    int output_w = 0;
    int output_h = 0;
    SDL_GetWindowSize(window, &window_w, &window_h);
    SDL_GetRendererOutputSize(renderer, &output_w, &output_h);
    if (window_w <= 0 || window_h <= 0 || output_w <= 0 || output_h <= 0) {
        return;
    }

    const float game_to_output_x = render_dst.w / static_cast<float>(hwcursor.GameW);
    const float game_to_output_y = render_dst.h / static_cast<float>(hwcursor.GameH);
    const float output_to_window_x = window_w / static_cast<float>(output_w);
    const float output_to_window_y = window_h / static_cast<float>(output_h);
    SDL_Rect rect = {
        static_cast<int>((render_dst.x + game_x * game_to_output_x) * output_to_window_x),
        static_cast<int>((render_dst.y + game_y * game_to_output_y) * output_to_window_y),
        std::max(1, static_cast<int>(game_w * game_to_output_x * output_to_window_x)),
        std::max(1, static_cast<int>(game_h * game_to_output_y * output_to_window_y))};
    SDL_SetTextInputRect(&rect);
}

int Video_Get_Effective_Frame_Limit()
{
    Update_Power_State();
    if (!render_active || !Is_App_In_Foreground()) {
        return 5;
    }

    int limit = Settings.Video.FrameLimit > 0 ? Settings.Video.FrameLimit : 60;
    limit = std::max(15, std::min(limit, 60));
    if (Settings.Video.BatterySaving || low_power_mode || thermal_state >= 2) {
        limit = std::min(limit, 30);
    }
    if (last_frame_idle) {
        limit = std::min(limit, 15);
    }
    return limit;
}
#else
void Video_Render_Frame()
{
    if (frontSurface) {
        frontSurface->RenderSurface();
    }
}
#endif

/*
** Video
*/

Video::Video()
{
}

Video::~Video()
{
}

Video& Video::Shared()
{
    static Video video;
    return video;
}

VideoSurface* Video::CreateSurface(int w, int h, GBC_Enum flags)
{
    return new VideoSurfaceSDL2(w, h, flags);
}
