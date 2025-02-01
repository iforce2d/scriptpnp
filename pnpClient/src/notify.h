#ifndef NOTIFY_H
#define NOTIFY_H

#include <string>

enum notifyType_t {
    NT_NONE,
    NT_SUCCESS,
    NT_WARNING,
    NT_ERROR,
    NT_INFO
};

void notify(std::string msg, int type, int timeout );

#endif
