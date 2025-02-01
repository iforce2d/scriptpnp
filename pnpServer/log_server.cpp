
#include <ctime>
#include <iomanip>
#include <stdarg.h>

#include "log.h"

const char* logPrefixArray[] = {
    "DEBUG",
    "INFO ",
    "WARN ",
    "ERROR",
    "FATAL"
};

AppLog g_log;

AppLog::AppLog()
{
}

char logbuf[2048];

void AppLog::log(logLevel_e level, const char* fmt, ...)
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    snprintf(logbuf, 2048, "[%02d/%02d/%02d %02d:%02d:%02d] [%s] ", lt->tm_year%100, lt->tm_mon+1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec, logPrefixArray[level]);

    printf(logbuf);

    va_list args;
    va_start(args, fmt);
    vsnprintf(logbuf, 2048, fmt, args);
    va_end(args);

    printf(logbuf);
    printf("\n");
}
