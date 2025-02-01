#ifndef LOG_H
#define LOG_H

#include <map>
#include <string>

// The 'SCRIPT' ones are expected to be clickable, and jump to the problem line in code
enum logLevel_e {
    LL_OLD,
    LL_DEBUG,
    LL_INFO,
    LL_WARN,
    LL_SCRIPT_WARN,
    LL_ERROR,
    LL_SCRIPT_ERROR,
    LL_FATAL
};

struct errorGotoInfo
{
    std::string fileType;
    std::string section;
    int         row;
    int         col;
};

extern const char* logPrefixArray[];

#ifdef CLIENT

    #define g_log g_logg

    #include "imgui.h"

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
#else
    #define IM_FMTARGS(FMT)
    #define IM_FMTLIST(FMT)
#endif

class AppLog
{
#ifdef CLIENT
protected:
    void* owner;
    std::map<int, errorGotoInfo> errorInfosMap;

    ImGuiTextBuffer     Buf;
    ImGuiTextFilter     Filter;
    ImVector<int>       LineOffsets; // Index to lines offset. We maintain this with AddLog() calls.
    ImVector<logLevel_e>levels;
    bool                AutoScroll;  // Keep scrolling if already at the bottom.

    pthread_mutex_t     mutex;
#endif

public:

    AppLog();
    void log(logLevel_e level, const char* fmt, ...) IM_FMTARGS(3);
#ifdef CLIENT
    virtual bool shouldClickErrors() { return false; }
    virtual void clear();
    void copy();
    virtual void doErrorGoto(int clickedLineNumber);
    void drawWindow(const char* title, bool* p_open = NULL);
    virtual void drawOptionsSection();
    void drawLogContent();
#endif
};

extern AppLog g_log;

void showLogWindow(bool* p_open);

#endif
