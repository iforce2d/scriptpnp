
#include <map>

#include "imgui.h"
#include "tweakspanel.h"
#include "eventhooks.h"
#include "workspace.h"

using namespace std;

#define TWEAKS_WINDOW_TITLE "Tweaks"

#define TVST_TIMEOUT 5
float tweakValuesNeedSaveTimer = 0;

void showTweaksView(bool* p_open, float dt) {

    bool wasZero = tweakValuesNeedSaveTimer == 0;

    tweakValuesNeedSaveTimer -= dt;
    if ( tweakValuesNeedSaveTimer < 0 )
        tweakValuesNeedSaveTimer = 0;

    bool isZero = tweakValuesNeedSaveTimer == 0;

    if ( ! wasZero && isZero ) {
        saveEventHooksToDB();
    }

    ImGui::SetNextWindowSize(ImVec2(320, 480), ImGuiCond_FirstUseEver);

    doLayoutLoad(TWEAKS_WINDOW_TITLE);

    ImGui::Begin(TWEAKS_WINDOW_TITLE, p_open);
    {
        vector<tweakInfo_t> &infos = getTweakInfos();

        map<string, vector<int> > groupToContentMap;
        for (int i = 0; i < (int)infos.size(); i++) {
            tweakInfo_t& info = infos[i];
            if ( info.name[0] == 0 )
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
                        tweakInfo_t& info = infos[i];
                        char buf[148];
                        sprintf(buf, "%s##id%d", info.name, count++);

                        ImGui::SliderFloat(buf, &info.floatval, info.minval, info.maxval);

                        if ( ImGui::IsItemEdited() ) {
                            info.dirty = true;
                            tweakValuesNeedSaveTimer = TVST_TIMEOUT;
                        }
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            it++;
        }
    }

    doLayoutSave(TWEAKS_WINDOW_TITLE);

    ImGui::End();
}
