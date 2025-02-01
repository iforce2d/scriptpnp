
#include "imgui.h"
#include "tweakspanel.h"
#include "eventhooks.h"
#include "workspace.h"

using namespace std;

#define TWEAKS_WINDOW_TITLE "Tweaks"

void showTweaksView(bool* p_open) {

    ImGui::SetNextWindowSize(ImVec2(320, 480), ImGuiCond_FirstUseEver);

    doLayoutLoad(TWEAKS_WINDOW_TITLE);

    ImGui::Begin(TWEAKS_WINDOW_TITLE, p_open);
    {
        vector<tweakInfo_t> &infos = getTweakInfos();

        int count = 0;
        for ( tweakInfo_t & info : infos ) {

            if ( ! info.name[0] )
                continue;

            char buf[148];
            sprintf(buf, "%s##id%d", info.name, count++);

            ImGui::SliderFloat(buf, &info.floatval, info.minval, info.maxval);
        }
    }

    doLayoutSave(TWEAKS_WINDOW_TITLE);

    ImGui::End();
}
