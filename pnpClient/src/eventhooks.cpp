
#include <vector>
#include <algorithm>

#include "imgui.h"
#include "eventhooks.h"
#include "scriptexecution.h"
#include "db.h"
#include "workspace.h"

using namespace std;

#define SCRIPT_HOOKS_WINDOW_TITLE "Script hooks"

functionKeyHookInfo_t functionKeyHookInfos[NUM_FUNCTION_KEY_HOOKS];
vector<customButtonHookInfo_t> customButtonHookInfos;
vector<tweakInfo_t> tweakInfos;

void saveEventHooksToDB()
{
    string errMsg;

    // The multiple insert/updates below can be very slow on mechanical disk drives.
    // Put them into one transaction to avoid doing writes between every statement.
    executeDatabaseStatement("BEGIN TRANSACTION", NULL, errMsg);

    for (int i = 0; i < NUM_FUNCTION_KEY_HOOKS; i++) {
        functionKeyHookInfo_t &info = functionKeyHookInfos[i];

        if ( ! info.dirty )
            continue;

        char label[32];
        sprintf(label, "F%d", i+1);

        string idStr = info.id == 0 ? "NULL" : to_string(info.id);

        string sql = "insert into internal_eventhook (id, type, label, entryfunction, preview) values ("+idStr+",'functionkey','"+string(label)+"','"+ info.entryFunction +"',"+to_string(info.preview)+")"
            + " on CONFLICT do UPDATE set label = '"+ label +"', entryfunction = '"+ info.entryFunction +"', preview = "+to_string(info.preview);
        executeDatabaseStatement(sql, NULL, errMsg);
        info.dirty = false;
    }

    int numRows = (int)customButtonHookInfos.size();
    for (int i = 0; i < numRows; i++) {
        customButtonHookInfo_t &info = customButtonHookInfos[i];

        if ( ! info.dirty )
            continue;

        string idStr = info.id == 0 ? "NULL" : to_string(info.id);

        string errMsg;
        vector< vector<string> > tmp;
        executeDatabaseStatement_generic("select id from internal_eventhook where id = "+to_string(info.id), &tmp, errMsg);
        if ( ! tmp.empty() ) {
            string sql = "UPDATE internal_eventhook SET label = '"+ string(info.label) +"', entryfunction = '"+ info.entryFunction +"', preview = "+to_string(info.preview) +", tab_group = '"+ info.tabGroup +"', display_order = "+to_string(info.displayOrder)+" where id = "+to_string(info.id);
            if ( executeDatabaseStatement(sql, NULL, errMsg) )
                info.dirty = false;
        }
        else {
            string sql = "INSERT INTO internal_eventhook (type, label, entryfunction, preview, tab_group, display_order) values ('button','"+string(info.label)+"','"+ info.entryFunction +"',"+to_string(info.preview)+",'"+ info.tabGroup +"',"+to_string(info.displayOrder)+")";
            if ( executeDatabaseStatement(sql, NULL, errMsg) ) {
                info.id = getLastInsertId();
                info.dirty = false;
            }
        }
    }

    numRows = (int)tweakInfos.size();
    for (int i = 0; i < numRows; i++) {
        tweakInfo_t &info = tweakInfos[i];

        if ( ! info.dirty )
            continue;

        string idStr = info.id == 0 ? "NULL" : to_string(info.id);

        string errMsg;
        vector< vector<string> > tmp;
        executeDatabaseStatement_generic("SELECT id FROM internal_tweak WHERE id = "+to_string(info.id), &tmp, errMsg);
        if ( ! tmp.empty() ) {
            string sql = "UPDATE internal_tweak SET name = '"+ string(info.name) +"', minval = "+to_string(info.minval) +", maxval = "+to_string(info.maxval) +", floatval = "+to_string(info.floatval) +", tab_group = '"+ info.tabGroup +"', display_order = "+to_string(info.displayOrder)+" where id = "+to_string(info.id);
            if ( executeDatabaseStatement(sql, NULL, errMsg) )
                info.dirty = false;
        }
        else {
            string sql = "INSERT INTO internal_tweak (name, minval, maxval, floatval, tab_group, display_order) VALUES ('"+string(info.name)+"',"+ to_string(info.minval) +","+to_string(info.maxval) +","+to_string(info.floatval)+",'"+ info.tabGroup +"',"+to_string(info.displayOrder)+")";
            if ( executeDatabaseStatement(sql, NULL, errMsg) ) {
                info.id = getLastInsertId();
                info.dirty = false;
            }
        }
    }

    executeDatabaseStatement("COMMIT", NULL, errMsg);
}

vector< vector<string> > loadEventHooksResult;

static int loadEventHooks_callback(void *NotUsed, int argc, char **argv, char **azColName) {

    vector<string> cols;

    for ( int i = 0; i < argc; i++ )
        cols.push_back( argv[i] ? argv[i] : "NULL" );

    loadEventHooksResult.push_back( cols );

    return 0;
}

void loadEventHooksFromDB()
{
    string errMsg;

    for (int i = 0; i < NUM_FUNCTION_KEY_HOOKS; i++) {
        functionKeyHookInfo_t &info = functionKeyHookInfos[i];
        char label[32];
        sprintf(label, "F%d", i+1);

        loadEventHooksResult.clear();

        string sql = "select id, entryfunction, preview from internal_eventhook where type = 'functionkey' and label = '"+string(label)+"'";
        if ( executeDatabaseStatement(sql, loadEventHooks_callback, errMsg) )
        {
            for (int r = 0; r < (int)loadEventHooksResult.size(); r++) {
                vector<string> &cols = loadEventHooksResult[r];
                info.id = atoi(cols[0].c_str());
                snprintf(info.entryFunction, sizeof(info.entryFunction), "%s", cols[1].c_str());
                info.dirty = false;
                info.preview = atoi(cols[2].c_str());
            }
        }

    }

    loadEventHooksResult.clear();

    string sql = "select id, label, entryfunction, preview, tab_group, display_order from internal_eventhook where type = 'button' order by tab_group, display_order, id";
    if ( executeDatabaseStatement(sql, loadEventHooks_callback, errMsg) )
    {
        for (int r = 0; r < (int)loadEventHooksResult.size(); r++) {
            vector<string> &cols = loadEventHooksResult[r];
            customButtonHookInfo_t info;
            info.id = atoi(cols[0].c_str());
            snprintf(info.label, sizeof(info.label), "%s", cols[1].c_str());
            snprintf(info.entryFunction, sizeof(info.entryFunction), "%s", cols[2].c_str());
            info.preview = atoi(cols[3].c_str());
            snprintf(info.tabGroup, sizeof(info.tabGroup), "%s", cols[4].c_str());
            info.displayOrder = atoi(cols[5].c_str());
            info.dirty = false;
            customButtonHookInfos.push_back(info);
        }
    }

    loadEventHooksResult.clear();

    sql = "select id, name, minval, maxval, floatval, tab_group, display_order from internal_tweak order by tab_group, display_order, id";
    if ( executeDatabaseStatement(sql, loadEventHooks_callback, errMsg) )
    {
        for (int r = 0; r < (int)loadEventHooksResult.size(); r++) {
            vector<string> &cols = loadEventHooksResult[r];
            tweakInfo_t info;
            info.id = atoi(cols[0].c_str());
            snprintf(info.name, sizeof(info.name), "%s", cols[1].c_str());
            info.minval = atof(cols[2].c_str());
            info.maxval = atof(cols[3].c_str());
            info.floatval = atof(cols[4].c_str());
            snprintf(info.tabGroup, sizeof(info.tabGroup), "%s", cols[5].c_str());
            info.displayOrder = atoi(cols[6].c_str());
            info.dirty = false;
            tweakInfos.push_back(info);
        }
    }

    loadEventHooksResult.clear();
}

#define EHST_TIMEOUT 5
float eventHooksNeedSaveTimer = 0;

bool showFunctionKeyHooks()
{
    bool somethingChanged = false;

    char buf[32];

    if (ImGui::BeginTable("fnKeyHooksTable", 3, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Key");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Entry function");

        for (int i = 0; i < NUM_FUNCTION_KEY_HOOKS; i++) {

            bool wasChanged = false;

            functionKeyHookInfo_t &info = functionKeyHookInfos[i];

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);            
            sprintf(buf, "F%d", i+1);
            ImGui::Text("%s", buf);

            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(160);
            sprintf(buf, "##fnkeyentry%d", i+1);
            ImGui::InputText(buf, info.entryFunction, MAX_ENTRY_FUNCTION_LEN-1, ImGuiInputTextFlags_CharsNoBlank);

            if ( ImGui::IsItemEdited() )
                wasChanged = true;

            ImGui::TableSetColumnIndex(2);
            sprintf(buf, "Preview##fnkeypv%d", i);
            ImGui::Checkbox(buf, &info.preview);

            if ( ImGui::IsItemEdited() )
                wasChanged = true;

            if ( wasChanged ) {
                info.dirty = true;
                somethingChanged = true;
            }

        }
        ImGui::EndTable();
    }

    return somethingChanged;
}

bool showCustomButtonHooks()
{
    bool somethingChanged = false;

    char buf[64];

    static bool addedButtonLastFrame = false;

    static int buttonIdToDelete = 0;

    vector<string> labelsInUse;

    int numRows = (int)customButtonHookInfos.size();
    ImVec2 outerSize = ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 24);
    if (ImGui::BeginTable("customButtonHooksTable", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY, outerSize))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Tab group");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Label");
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("Entry function");

        for (int i = 0; i < numRows; i++) {

            ImGui::PushID(i);

            bool wasChanged = false;

            customButtonHookInfo_t &info = customButtonHookInfos[i];

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);

            ImGui::PushItemWidth(80);
            sprintf(buf, "##btngrp%d", i);
            if ( addedButtonLastFrame && (i == numRows-1) ) {
                ImGui::SetKeyboardFocusHere();
                addedButtonLastFrame = false;
            }
            ImGui::InputText(buf, info.tabGroup, MAX_CUSTOM_BUTTON_GROUP_LEN-1, ImGuiInputTextFlags_None);

            if ( ImGui::IsItemEdited() )
                wasChanged = true;

            bool duplicateLabel = ( std::find(labelsInUse.begin(), labelsInUse.end(), info.label) != labelsInUse.end());

            if ( duplicateLabel ) {
                ImGui::PushStyleColor(ImGuiCol_Text,        ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBg,     ImVec4(0.2f, 0.0f, 0.0f, 1.0f));
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(160);
            sprintf(buf, "##btnlabel%d", i);
            ImGui::InputText(buf, info.label, MAX_CUSTOM_BUTTON_LABEL_LEN-1);

            if ( duplicateLabel ) {
                ImGui::PopStyleColor(2);
            }

            if ( ImGui::IsItemEdited() )
                wasChanged = true;

            labelsInUse.push_back( info.label );

            ImGui::TableSetColumnIndex(2);
            ImGui::PushItemWidth(160);
            sprintf(buf, "##btnfn%d", i);
            ImGui::InputText(buf, info.entryFunction, MAX_ENTRY_FUNCTION_LEN-1, ImGuiInputTextFlags_CharsNoBlank);

            if ( ImGui::IsItemEdited() )
                wasChanged = true;

            ImGui::TableSetColumnIndex(3);
            sprintf(buf, "Preview##btnpv%d", i);
            ImGui::Checkbox(buf, &info.preview);

            if ( ImGui::IsItemEdited() )
                wasChanged = true;


            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            ImGui::SameLine();
            sprintf(buf, "Delete##%d", i);
            if ( ImGui::Button(buf) ) {
                buttonIdToDelete = info.id;
            }
            ImGui::PopStyleColor(3);

            // other row to swap with
            int otherInfoIndex = -1;

            if ( i >= numRows-1 )
                ImGui::BeginDisabled();
            ImGui::SameLine();
            if ( ImGui::ArrowButton("##down", ImGuiDir_Down) ) {
                otherInfoIndex = i + 1;
            }
            if ( i >= numRows-1 )
                ImGui::EndDisabled();

            if ( i < 1 )
                ImGui::BeginDisabled();
            ImGui::SameLine();
            if ( ImGui::ArrowButton("##up", ImGuiDir_Up)) {
                otherInfoIndex = i - 1;
            }
            if ( i < 1 )
                ImGui::EndDisabled();

            if ( otherInfoIndex > -1 ) {
                customButtonHookInfo_t &otherInfo = customButtonHookInfos[otherInfoIndex];
                int tmpInt = info.displayOrder;
                info.displayOrder = otherInfo.displayOrder;
                otherInfo.displayOrder = tmpInt;

                customButtonHookInfo_t tmpEntry = info;
                info = otherInfo;
                otherInfo = tmpEntry;

                info.dirty = true;
                otherInfo.dirty = true;
                somethingChanged = true;
            }

            if ( duplicateLabel ) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text,        ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                ImGui::Text("Duplicate label, will be ignored!");
                ImGui::PopStyleColor(1);
            }

            if ( wasChanged ) {
                info.dirty = true;
                somethingChanged = true;
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if ( ImGui::Button("Add button") ) {
        customButtonHookInfo_t info;
        customButtonHookInfos.push_back(info);
        somethingChanged = true;
        addedButtonLastFrame = true;
    }

    if ( buttonIdToDelete > 0 ) {
        deleteCustomButton( buttonIdToDelete );
        buttonIdToDelete = 0;
    }

    return somethingChanged;
}

bool showTweakHooks()
{
    bool somethingChanged = false;

    char buf[64];

    static bool addedButtonLastFrame = false;

    int tweakIdToDelete = 0;

    int numRows = (int)tweakInfos.size();
    ImVec2 outerSize = ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 24);
    if (ImGui::BeginTable("tweaksTable", 4, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY, outerSize))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Tab group");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Name");
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("Min");
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("Max");

        vector<string> namesInUse;


        for (int i = 0; i < numRows; i++) {

            ImGui::PushID(i);

            bool wasChanged = false;

            tweakInfo_t &info = tweakInfos[i];

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);

            ImGui::PushItemWidth(80);
            sprintf(buf, "##btngrp%d", i);
            if ( addedButtonLastFrame && (i == numRows-1) ) {
                ImGui::SetKeyboardFocusHere();
                addedButtonLastFrame = false;
            }
            ImGui::InputText(buf, info.tabGroup, MAX_CUSTOM_BUTTON_GROUP_LEN-1, ImGuiInputTextFlags_None);

            if ( ImGui::IsItemEdited() )
                wasChanged = true;

            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(160);
            sprintf(buf, "##tweakname%d", i);

            bool duplicateName = ( std::find(namesInUse.begin(), namesInUse.end(), info.name) != namesInUse.end());

            if ( duplicateName ) {
                ImGui::PushStyleColor(ImGuiCol_Text,        ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBg,     ImVec4(0.2f, 0.0f, 0.0f, 1.0f));
            }

            if ( addedButtonLastFrame && (i == numRows-1) ) {
                ImGui::SetKeyboardFocusHere();
                addedButtonLastFrame = false;
            }

            ImGui::InputText(buf, info.name, MAX_TWEAK_LABEL_LEN-1);

            if ( duplicateName )
                ImGui::PopStyleColor(2);

            namesInUse.push_back( info.name );

            if ( ImGui::IsItemEdited() )
                wasChanged = true;

            ImGui::TableSetColumnIndex(2);
            ImGui::PushItemWidth(90);
            sprintf(buf, "##tweakmin%d", i);
            ImGui::InputFloat(buf, &info.minval, 0, 0, NULL, ImGuiInputTextFlags_CharsDecimal);

            if ( ImGui::IsItemEdited() )
                wasChanged = true;

            ImGui::TableSetColumnIndex(3);
            ImGui::PushItemWidth(90);
            sprintf(buf, "##tweakmax%d", i);
            ImGui::InputFloat(buf, &info.maxval, 0, 0, NULL, ImGuiInputTextFlags_CharsDecimal);            

            if ( ImGui::IsItemEdited() )
                wasChanged = true;


            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            ImGui::SameLine();
            sprintf(buf, "Delete##%d", i);
            if ( ImGui::Button(buf) ) {
                tweakIdToDelete = info.id;
            }
            ImGui::PopStyleColor(3);

            if ( info.floatval < info.minval )
                info.floatval = info.minval;
            if ( info.floatval > info.maxval )
                info.floatval = info.maxval;

            // other row to swap with
            int otherInfoIndex = -1;

            if ( i >= numRows-1 )
                ImGui::BeginDisabled();
            ImGui::SameLine();
            if ( ImGui::ArrowButton("##down", ImGuiDir_Down) ) {
                otherInfoIndex = i + 1;
            }
            if ( i >= numRows-1 )
                ImGui::EndDisabled();

            if ( i < 1 )
                ImGui::BeginDisabled();
            ImGui::SameLine();
            if ( ImGui::ArrowButton("##up", ImGuiDir_Up)) {
                otherInfoIndex = i - 1;
            }
            if ( i < 1 )
                ImGui::EndDisabled();

            if ( otherInfoIndex > -1 ) {
                tweakInfo_t &otherInfo = tweakInfos[otherInfoIndex];
                int tmpInt = info.displayOrder;
                info.displayOrder = otherInfo.displayOrder;
                otherInfo.displayOrder = tmpInt;

                tweakInfo_t tmpEntry = info;
                info = otherInfo;
                otherInfo = tmpEntry;

                info.dirty = true;
                otherInfo.dirty = true;
                somethingChanged = true;
            }

            if ( duplicateName ) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text,        ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                ImGui::Text("Duplicate name, will be ignored!");
                ImGui::PopStyleColor(1);
            }

            if ( wasChanged ) {
                info.dirty = true;
                somethingChanged = true;
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if ( ImGui::Button("Add tweak") ) {
        tweakInfo_t info;
        tweakInfos.push_back(info);
        somethingChanged = true;
        addedButtonLastFrame = true;
    }

    if ( tweakIdToDelete > 0 ) {
        deleteTweak( tweakIdToDelete );
        tweakIdToDelete = 0;
    }

    return somethingChanged;
}

void showHooksView(bool* p_open, float dt) {

    bool wasZero = eventHooksNeedSaveTimer == 0;

    eventHooksNeedSaveTimer -= dt;
    if ( eventHooksNeedSaveTimer < 0 )
        eventHooksNeedSaveTimer = 0;

    bool isZero = eventHooksNeedSaveTimer == 0;

    if ( ! wasZero && isZero ) {
        saveEventHooksToDB();
    }

    bool somethingChanged = false;

    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);

    doLayoutLoad(SCRIPT_HOOKS_WINDOW_TITLE);

    ImGui::Begin(SCRIPT_HOOKS_WINDOW_TITLE, p_open);
    {

        if (ImGui::BeginTabBar("hooktabs", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("Function keys"))
            {
                somethingChanged |= showFunctionKeyHooks();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("GUI buttons"))
            {
                somethingChanged |= showCustomButtonHooks();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tweaks"))
            {
                somethingChanged |= showTweakHooks();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    if ( somethingChanged )
        eventHooksNeedSaveTimer = EHST_TIMEOUT;

    doLayoutSave(SCRIPT_HOOKS_WINDOW_TITLE);

    ImGui::End();
}


void executeFunctionKeyHook(int index) // one-indexed
{
    index--;

    if ( index >= 0 && index < NUM_FUNCTION_KEY_HOOKS ) {
        functionKeyHookInfo_t &info = functionKeyHookInfos[index];
        if ( info.entryFunction[0] != 0 ) {
            runScript( "functionKeyModule", info.entryFunction, info.preview );
        }
    }
}

std::vector<customButtonHookInfo_t> & getCustomButtonInfos() {
    return customButtonHookInfos;
}

std::vector<tweakInfo_t> & getTweakInfos() {
    return tweakInfos;
}

tweakInfo_t *getTweakByName(std::string name)
{
    for (tweakInfo_t &t : tweakInfos) {
        if ( t.name == name )
            return &t;
    }
    return NULL;
}

void deleteCustomButton( int id ) {
    string errMsg;
    vector< vector<string> > tmp;
    if ( executeDatabaseStatement_generic("DELETE FROM internal_eventhook where type = 'button' and id = "+to_string(id), &tmp, errMsg) ) {
        for (int i = 0; i < (int)customButtonHookInfos.size(); i++) {
            customButtonHookInfo_t& info = customButtonHookInfos[i];
            if ( info.id == id ) {
                customButtonHookInfos.erase( customButtonHookInfos.begin() + i );
                return;
            }
        }
    }
}

void deleteTweak( int id ) {
    string errMsg;
    vector< vector<string> > tmp;
    if ( executeDatabaseStatement_generic("DELETE FROM internal_tweak where id = "+to_string(id), &tmp, errMsg) ) {
        for (int i = 0; i < (int)tweakInfos.size(); i++) {
            tweakInfo_t& info = tweakInfos[i];
            if ( info.id == id ) {
                tweakInfos.erase( tweakInfos.begin() + i );
                return;
            }
        }
    }
}








