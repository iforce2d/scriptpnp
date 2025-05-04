
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

        map<string, vector<int> > groupToContentMap;
        for (int i = 0; i < (int)infos.size(); i++) {
            customButtonHookInfo_t& info = infos[i];
            if ( info.label[0] == 0 )
                continue;
            string groupKey = (info.tabGroup[0] == 0) ? "Default" : info.tabGroup;
            groupToContentMap[ groupKey ].push_back( i );
        }

        int count = 0;
        auto it = groupToContentMap.begin();
        while ( it != groupToContentMap.end() ) {

            if (ImGui::BeginTabBar("buttontabs", ImGuiTabBarFlags_None))
            {
                if (ImGui::BeginTabItem(it->first.c_str()))
                {
                    for ( int i : it->second ) {
                        customButtonHookInfo_t& info = infos[i];
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
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            it++;
        }
    }

    doLayoutSave(CUSTOM_WINDOW_TITLE);

    ImGui::End();
}
