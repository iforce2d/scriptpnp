
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
#include "imgui_notify/imgui_notify.h"

using namespace std;

std::vector<ImGuiToast> ImGui::notifications;

void notify(string msg, int type, int timeout ) {
    if ( type < 0 || type > ImGuiToastType_Info )
        type = ImGuiToastType_None;
    if ( timeout < 10 )
        timeout = 10;
    ImGui::InsertNotification({ type, timeout, msg.c_str() });
}
