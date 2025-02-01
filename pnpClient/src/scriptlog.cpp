
#include <ctime>
#include <iomanip>

#include "scriptlog.h"
#include "scopelock.h"

using namespace std;

ScriptLog::ScriptLog() : AppLog() {}

void ScriptLog::drawOptionsSection()
{
}

void ScriptLog::clear()
{
    ScopeLock lock(&mutex);

    errorInfosMap.clear();
    AppLog::clear();
}

void ScriptLog::grayOutExistingText()
{
    ScopeLock lock(&mutex);

    for (int i = 0; i < (int)levels.size(); i++) {
        levels[i] = LL_OLD;
    }
}

// if errorInfo is not null, the text will be a single line
void ScriptLog::log(logLevel_e level, codeCompileErrorInfo* errorInfo, long long timeTaken, const char* fmt, ...)
{
    ScopeLock lock(&mutex);

    if ( errorInfo ) {

        int currentLine = levels.size();

        errorGotoInfo info;
        info.fileType = errorInfo->fileType == CT_SCRIPT ? "script" : "command list";
        info.section = errorInfo->section;
        info.col = errorInfo->col;
        info.row = errorInfo->row;
        errorInfosMap[currentLine] = info;
    }

    int old_size = Buf.size();

    if ( timeTaken > 0 ) {
        time_t t = time(NULL);
        struct tm *lt = localtime(&t);
        Buf.appendf("[%02d/%02d/%02d %02d:%02d:%02d] Script run took %lld us\n", lt->tm_year%100, lt->tm_mon+1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec, timeTaken);
        //levels.push_back( level );
    }

    va_list args;
    va_start(args, fmt);
    Buf.appendfv(fmt, args);
    va_end(args);

    Buf.append("\n");

    levels.push_back( level );

    bool needLevelAdd = false;
    for (int new_size = Buf.size(); old_size < new_size; old_size++) {
        if (Buf[old_size] == '\n') {
            LineOffsets.push_back(old_size + 1);
            if ( needLevelAdd )
                levels.push_back( level );
            needLevelAdd = true;
        }
    }
}
