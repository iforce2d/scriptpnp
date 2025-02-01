#ifndef LOG_H
#define LOG_H

enum logLevel_e {
    LL_DEBUG,
    LL_INFO,
    LL_WARN,
    LL_ERROR,
    LL_FATAL
};

#ifdef CLIENT
#include "imgui.h"
#endif

#if !defined(IMGUI_USE_STB_SPRINTF) && defined(__MINGW32__) && !defined(__clang__)
#define IM_FMTARGS(FMT)             __attribute__((format(gnu_printf, FMT, FMT+1)))
#define IM_FMTLIST(FMT)             __attribute__((format(gnu_printf, FMT, 0)))
#elif !defined(IMGUI_USE_STB_SPRINTF) && (defined(__clang__) || defined(__GNUC__))
#define IM_FMTARGS(FMT)             __attribute__((format(printf, FMT, FMT+1)))
#define IM_FMTLIST(FMT)             __attribute__((format(printf, FMT, 0)))
#else
#define IM_FMTARGS(FMT)
#define IM_FMTLIST(FMT)
#endif

class AppLog
{
#ifdef CLIENT
    ImGuiTextBuffer     Buf;
    ImGuiTextFilter     Filter;
    ImVector<int>       LineOffsets; // Index to lines offset. We maintain this with AddLog() calls.
    ImVector<logLevel_e>levels;
    bool                AutoScroll;  // Keep scrolling if already at the bottom.
#endif

public:

    AppLog();
    void log(logLevel_e level, const char* fmt, ...) IM_FMTARGS(3);
#ifdef CLIENT
    void clear();
    void draw(const char* title, bool* p_open = NULL);
#endif
};

extern AppLog g_log;

void showLogWindow(bool* p_open);

#endif
