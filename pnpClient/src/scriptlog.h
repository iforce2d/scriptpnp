#ifndef SCRIPTLOG_H
#define SCRIPTLOG_H

#include <vector>

#include "log.h"
#include "scriptexecution.h"

class ScriptLog : public AppLog
{
public:
    ScriptLog();

    void setOwner(void* p) { owner = p; }
    bool shouldClickErrors() { return true; }
    void drawOptionsSection();
    void clear();
    void grayOutExistingText();
    void log(logLevel_e level, codeCompileErrorInfo* errorInfo, long long timeTaken, const char* fmt, ...) IM_FMTARGS(5);
};

#endif // SCRIPTLOG_H
