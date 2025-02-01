
#include <ctime>
#include <iomanip>

#include "imgui.h"
#include "imgui_internal.h"
#include "log.h"
#include "codeEditorWindow.h"
#include "workspace.h"
#include "scopelock.h"

using namespace std;

#define LOG_WINDOW_TITLE "Log"

extern ImFont* font_proggy;

ImVec4 logColor_old(0.7,0.8,0.7,0.3);
ImVec4 logColor_debug(0.7,0.8,0.7,1);
ImVec4 logColor_info(1,1,1,1);
ImVec4 logColor_warn(1,1,0.3,1);
ImVec4 logColor_error(1,0.3,0.3,1);
ImVec4 logColor_fatal(1,0.2,1,1);

ImVec4 logColorArray[] = {
    logColor_old,
    logColor_debug,
    logColor_info,
    logColor_warn,
    logColor_warn,
    logColor_error,
    logColor_error,
    logColor_fatal
};

const char* logPrefixArray[] = {
    "OLD", // shouldn't be used
    "DEBUG",
    "INFO ",
    "WARN ",
    "WARN ",
    "ERROR",
    "ERROR",
    "FATAL"
};

AppLog::AppLog()
{
#ifdef __APPLE__
    mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;
#else
    mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
#endif

    owner = NULL;
    AutoScroll = true;
    clear();
}

void AppLog::clear()
{
    ScopeLock lock(&mutex);
    Buf.clear();
    LineOffsets.clear();
    LineOffsets.push_back(0);
    levels.clear();
}

void AppLog::copy()
{
    ScopeLock lock(&mutex);
    ImGui::LogToClipboard();
    string s = Buf.c_str();
    if ( s.back() == '\n' )
        s.pop_back();
    ImGui::LogText("%s", s.c_str());
    ImGui::LogFinish();
}

void AppLog::doErrorGoto(int clickedLineNumber)
{
    ScopeLock lock(&mutex);
    if ( owner ) {
        map<int, errorGotoInfo>::iterator it = errorInfosMap.find( clickedLineNumber );
        if( it != errorInfosMap.end() ) {
            errorGotoInfo &info = it->second;
            //((CodeEditorWindow*)owner)->gotoError( info );
            gotoCodeCompileError( (CodeEditorWindow*)owner, info );
        }
    }
}

void AppLog::log(logLevel_e level, const char* fmt, ...)
{
    ScopeLock lock(&mutex);
    int sizeBefore = Buf.size();
    int pos = Buf.size();

    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    Buf.appendf("[%02d/%02d/%02d %02d:%02d:%02d] [%s] ", lt->tm_year%100, lt->tm_mon+1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec, logPrefixArray[level]);

    va_list args;
    va_start(args, fmt);
    Buf.appendfv(fmt, args);
    va_end(args);

    Buf.append("\n");

    levels.push_back( level );

    bool needLevelAdd = false;
    for (int new_size = Buf.size(); pos < new_size; pos++) {
        if (Buf[pos] == '\n') {
            LineOffsets.push_back(pos + 1);
            if ( needLevelAdd )
                levels.push_back( level );
            needLevelAdd = true;
        }
    }

    const char* buf = Buf.begin();
    printf("%s", buf + sizeBefore); fflush(stdout);
}

void AppLog::drawOptionsSection()
{
    ScopeLock lock(&mutex);
    if ( ImGui::BeginPopup("Options") )
    {
        ImGui::Checkbox("Auto-scroll", &AutoScroll);
        ImGui::EndPopup();
    }

    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");
    ImGui::SameLine();
    bool clearr = ImGui::Button("Clear");
    ImGui::SameLine();
    bool copy = ImGui::Button("Copy");
    ImGui::SameLine();
    ImGui::Text("Filter:");
    ImGui::SameLine();
    Filter.Draw("##filter");

    if (clearr)
        clear();

    if (copy) {
        ImGui::LogToClipboard();
        string s = Buf.c_str();
        if ( s.back() == '\n' )
            s.pop_back();
        ImGui::LogText("%s", s.c_str());
        ImGui::LogFinish();
    }
}

void AppLog::drawLogContent()
{
    ScopeLock lock(&mutex);
    drawOptionsSection();

    ImGui::Separator();

    ImGui::PushFont(font_proggy);

    if (ImGui::BeginChild("scrolling", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
    {

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        const char* buf = Buf.begin();
        const char* buf_end = Buf.end();
        if (Filter.IsActive())
        {
            // In this example we don't use the clipper when Filter is enabled.
            // This is because we don't have random access to the result of our filter.
            // A real application processing logs with ten of thousands of entries may want to store the result of
            // search/filter.. especially if the filtering function is not trivial (e.g. reg-exp).
            logLevel_e lastLevel = LL_INFO;
            for (int line_no = 0; line_no < LineOffsets.Size; line_no++)
            {
                if ( line_no < levels.size() )
                    lastLevel = levels[line_no];
                const char* line_start = buf + LineOffsets[line_no];
                const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                if (Filter.PassFilter(line_start, line_end)) {
                    ImGui::PushStyleColor(ImGuiCol_Text, logColorArray[lastLevel]);
                    ImGui::TextUnformatted(line_start, line_end);
                    ImGui::PopStyleColor();
                }
            }
        }
        else
        {
            // The simplest and easy way to display the entire buffer:
            //   ImGui::TextUnformatted(buf_begin, buf_end);
            // And it'll just work. TextUnformatted() has specialization for large blob of text and will fast-forward
            // to skip non-visible lines. Here we instead demonstrate using the clipper to only process lines that are
            // within the visible area.
            // If you have tens of thousands of items and their processing cost is non-negligible, coarse clipping them
            // on your side is recommended. Using ImGuiListClipper requires
            // - A) random access into your data
            // - B) items all being the  same height,
            // both of which we can handle since we have an array pointing to the beginning of each line of text.
            // When using the filter (in the block of code above) we don't have random access into the data to display
            // anymore, which is why we don't use the clipper. Storing or skimming through the search result would make
            // it possible (and would be recommended if you want to search through tens of thousands of entries).
            ImGuiListClipper clipper;
            clipper.Begin(LineOffsets.Size);
            logLevel_e lastLevel = LL_INFO;
            char lineNumBuf[64];
            while (clipper.Step())
            {
                for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
                {
                    snprintf(lineNumBuf, sizeof(lineNumBuf), "line%d", line_no);

                    if ( line_no < levels.size() )
                        lastLevel = levels[line_no];

                    const char* line_start = buf + LineOffsets[line_no];
                    const char* line_end = (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                    ImGui::PushStyleColor(ImGuiCol_Text, logColorArray[lastLevel]);

                    ImGui::TextUnformatted(line_start, line_end);

                    if ( shouldClickErrors() && (lastLevel == LL_SCRIPT_WARN || lastLevel == LL_SCRIPT_ERROR) ) {
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                            if ( ImGui::IsMouseClicked(0) ) {
                                doErrorGoto(line_no);
                            }
                        }
                    }

                    ImGui::PopStyleColor();
                }
            }
            clipper.End();
        }
        ImGui::PopStyleVar();

        // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
        // Using a scrollbar or mouse-wheel will take away from the bottom edge.
        if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::PopFont();
}

void AppLog::drawWindow(const char* title, bool* p_open)
{
    ScopeLock lock(&mutex);

    doLayoutLoad(LOG_WINDOW_TITLE);

    ImGui::Begin(title, p_open);
    {
        drawLogContent();
    }

    doLayoutSave(LOG_WINDOW_TITLE);

    ImGui::End();
}


AppLog g_log;

void showLogWindow(bool* p_open)
{

    // For the demo: add a debug button _BEFORE_ the normal log window contents
    // We take advantage of a rarely used feature: multiple calls to Begin()/End() are appending to the _same_ window.
    // Most of the contents of the window will be added by the log.Draw() call.
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    //    ImGui::Begin("Log", p_open);
    //    if (ImGui::SmallButton("[Debug] Add 5 entries"))
    //    {
    //        static int counter = 0;
    //        const char* categories[3] = { "info", "warn", "error" };
    //        const char* words[] = { "Bumfuzzled", "Cattywampus", "Snickersnee", "Abibliophobia", "Absquatulate", "Nincompoop", "Pauciloquent" };
    //        for (int n = 0; n < 5; n++)
    //        {
    //            const char* category = categories[counter % IM_ARRAYSIZE(categories)];
    //            const char* word = words[counter % IM_ARRAYSIZE(words)];
    //            g_log.log(LL_INFO, "[%05d] [%s] Hello, current time is %.1f, here's a word: '%s'\n",
    //                ImGui::GetFrameCount(), category, ImGui::GetTime(), word);
    //            counter++;
    //        }
    //    }
    //    ImGui::End();

    // Actually call in the regular Log helper (which will Begin() into the same window as we just did)
    g_log.drawWindow("Log", p_open);
}
