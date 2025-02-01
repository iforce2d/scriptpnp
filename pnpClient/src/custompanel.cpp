
#include "imgui.h"
#include "custompanel.h"
#include "eventhooks.h"
#include "scriptexecution.h"
#include "workspace.h"

using namespace std;

#define CUSTOM_WINDOW_TITLE "Custom buttons"

void showCustomView(bool* p_open) {
    ImGui::SetNextWindowSize(ImVec2(320, 480), ImGuiCond_FirstUseEver);

    doLayoutLoad(CUSTOM_WINDOW_TITLE);

    ImGui::Begin(CUSTOM_WINDOW_TITLE, p_open);
    {
        vector<customButtonHookInfo_t> &infos = getCustomButtonInfos();

        int count = 0;
        for ( customButtonHookInfo_t & info : infos ) {

            if ( info.label[0] == 0 )
                continue;

            char btnId[148];
            sprintf(btnId, "%s##id%d", info.label, count++);
            if ( ImGui::Button(btnId) ) {
                if ( info.entryFunction[0] ) {
                    beforeRunScript();
                    runScript( "customButtonModule", info.entryFunction, info.preview, NULL);
                    afterRunScript();
                }
            }
        }
    }

    doLayoutSave(CUSTOM_WINDOW_TITLE);

    ImGui::End();
}
