#ifndef NOTIFY_H
#define NOTIFY_H

#include <string>

// These are just to match ImGuiToastType_ without having to include "imgui_notify/imgui_notify.h"
enum notifyType_t {
    NT_NONE,
    NT_SUCCESS,
    NT_WARNING,
    NT_ERROR,
    NT_INFO
};

void notify(std::string msg, int type, int timeout );

#endif
