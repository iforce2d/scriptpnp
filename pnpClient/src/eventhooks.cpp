
#include <vector>

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
        char label[32];
        sprintf(label, "F%d", i+1);

        string idStr = info.id == 0 ? "NULL" : to_string(info.id);

        string sql = "insert into internal_eventhook (id, type, label, entryfunction, preview) values ("+idStr+",'functionkey','"+string(label)+"','"+ info.entryFunction +"',"+to_string(info.preview)+")"
            + " on CONFLICT do UPDATE set label = '"+ label +"', entryfunction = '"+ info.entryFunction +"', preview = "+to_string(info.preview);
        executeDatabaseStatement(sql, NULL, errMsg);
    }

    int numRows = (int)customButtonHookInfos.size();
    for (int i = 0; i < numRows; i++) {
        customButtonHookInfo_t &info = customButtonHookInfos[i];

        string idStr = info.id == 0 ? "NULL" : to_string(info.id);

        string sql = "insert into internal_eventhook (id, type, label, entryfunction, preview) values ("+idStr+",'button','"+string(info.label)+"','"+ info.entryFunction +"',"+to_string(info.preview)+")"
            + " on CONFLICT do UPDATE set label = '"+ info.label +"', entryfunction = '"+ info.entryFunction +"', preview = "+to_string(info.preview);
        executeDatabaseStatement(sql, NULL, errMsg);
    }

    numRows = (int)tweakInfos.size();
    for (int i = 0; i < numRows; i++) {
        tweakInfo_t &info = tweakInfos[i];

        string idStr = info.id == 0 ? "NULL" : to_string(info.id);

        string sql = "insert into internal_tweak (id, name, minval, maxval, floatval) values ("+idStr+",'"+string(info.name)+"',"+ to_string(info.minval) +","+to_string(info.maxval) +","+to_string(info.floatval)+")"
            + " on CONFLICT do UPDATE set name = '"+ string(info.name) +"', minval = "+to_string(info.minval) +", maxval = "+to_string(info.maxval) +", floatval = "+to_string(info.floatval);
        executeDatabaseStatement(sql, NULL, errMsg);
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
                info.preview = atoi(cols[2].c_str());
            }
        }
    }

    loadEventHooksResult.clear();

    string sql = "select id, label, entryfunction, preview from internal_eventhook where type = 'button' order by id";
    if ( executeDatabaseStatement(sql, loadEventHooks_callback, errMsg) )
    {
        for (int r = 0; r < (int)loadEventHooksResult.size(); r++) {
            vector<string> &cols = loadEventHooksResult[r];
            customButtonHookInfo_t info;
            info.id = atoi(cols[0].c_str());
            snprintf(info.label, sizeof(info.label), "%s", cols[1].c_str());
            snprintf(info.entryFunction, sizeof(info.entryFunction), "%s", cols[2].c_str());
            info.preview = atoi(cols[3].c_str());
            customButtonHookInfos.push_back(info);
        }
    }

    loadEventHooksResult.clear();

    sql = "select id, name, minval, maxval, floatval from internal_tweak order by id";
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
            tweakInfos.push_back(info);
        }

    }

    loadEventHooksResult.clear();
}

void showFunctionKeyHooks()
{
    char buf[32];

    if (ImGui::BeginTable("fnKeyHooksTable", 3, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Key");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Entry function");

        for (int i = 0; i < NUM_FUNCTION_KEY_HOOKS; i++) {

            functionKeyHookInfo_t &info = functionKeyHookInfos[i];

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);            
            sprintf(buf, "F%d", i+1);
            ImGui::Text("%s", buf);

            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(160);
            sprintf(buf, "##fnkeyentry%d", i+1);
            ImGui::InputText(buf, info.entryFunction, MAX_ENTRY_FUNCTION_LEN-1, ImGuiInputTextFlags_CharsNoBlank);

            ImGui::TableSetColumnIndex(2);
            sprintf(buf, "Preview##fnkeypv%d", i);
            ImGui::Checkbox(buf, &info.preview);

        }
        ImGui::EndTable();
    }
}

void showCustomButtonHooks()
{
    char buf[64];

    int numRows = (int)customButtonHookInfos.size();
    if (ImGui::BeginTable("customButtonHooksTable", 3, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Label");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Entry function");

        for (int i = 0; i < numRows; i++) {

            customButtonHookInfo_t &info = customButtonHookInfos[i];

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::PushItemWidth(160);
            sprintf(buf, "##btnlabel%d", i);
            ImGui::InputText(buf, info.label, MAX_CUSTOM_BUTTON_LABEL_LEN-1);

            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(160);
            sprintf(buf, "##btnfn%d", i);
            ImGui::InputText(buf, info.entryFunction, MAX_ENTRY_FUNCTION_LEN-1, ImGuiInputTextFlags_CharsNoBlank);

            ImGui::TableSetColumnIndex(2);
            sprintf(buf, "Preview##btnpv%d", i);
            ImGui::Checkbox(buf, &info.preview);
        }
        ImGui::EndTable();
    }

    if ( ImGui::Button("Add button") ) {
        customButtonHookInfo_t info;
        customButtonHookInfos.push_back(info);
    }
}

void showTweakHooks()
{
    char buf[64];

    int numRows = (int)tweakInfos.size();
    if (ImGui::BeginTable("tweaksTable", 3, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Name");
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("Min");
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("Max");

        for (int i = 0; i < numRows; i++) {

            tweakInfo_t &info = tweakInfos[i];

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::PushItemWidth(160);
            sprintf(buf, "##tweakname%d", i);
            ImGui::InputText(buf, info.name, MAX_TWEAK_LABEL_LEN-1);

            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(90);
            sprintf(buf, "##tweakmin%d", i);
            ImGui::InputFloat(buf, &info.minval, 0, 0, NULL, ImGuiInputTextFlags_CharsDecimal);

            ImGui::TableSetColumnIndex(2);
            ImGui::PushItemWidth(90);
            sprintf(buf, "##tweakmax%d", i);
            ImGui::InputFloat(buf, &info.maxval, 0, 0, NULL, ImGuiInputTextFlags_CharsDecimal);

            if ( info.floatval < info.minval )
                info.floatval = info.minval;
            if ( info.floatval > info.maxval )
                info.floatval = info.maxval;
        }
        ImGui::EndTable();
    }

    if ( ImGui::Button("Add tweak") ) {
        tweakInfo_t info;
        tweakInfos.push_back(info);
    }
}

void showHooksView(bool* p_open) {

    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);

    doLayoutLoad(SCRIPT_HOOKS_WINDOW_TITLE);

    ImGui::Begin(SCRIPT_HOOKS_WINDOW_TITLE, p_open);
    {

        if (ImGui::BeginTabBar("hooktabs", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("Function keys"))
            {
                showFunctionKeyHooks();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("GUI buttons"))
            {
                showCustomButtonHooks();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tweaks"))
            {
                showTweakHooks();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

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
