#include <vector>
#include <string>
#include <regex>

#include "imgui.h"

#include "log.h"
#include "workspace.h"
#include "duplicatetableview.h"
#include "util.h"
#include "db.h"

using namespace std;

void fetchTableNames();
extern vector<string> tableNames;
void addTableToRefresh(string tableName);

#define DUPLICATETABLEVIEW_WINDOW_TITLE "Duplicate table"

void showDuplicateTableView(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(320, 320), ImGuiCond_FirstUseEver);

    string windowPrefix = DUPLICATETABLEVIEW_WINDOW_TITLE;

    doLayoutLoad(windowPrefix);

    static string tableFrom = "";
    static string tableTo = "";
    bool duplicateClicked = false;

    ImGui::Begin(windowPrefix.c_str(), p_open);
    {
        if ( ImGui::Button("Refresh") ) {
            fetchTableNames();
        }

        ImGui::Text("Source table:");
        ImGui::SameLine();

        if (ImGui::BeginCombo("##dups", tableFrom.c_str(), ImGuiComboFlags_None))
        {
            for (string tableName : tableNames) {
                if (ImGui::Selectable(tableName.c_str(), false)) {
                    tableFrom = tableName;
                }
            }
            ImGui::EndCombo();
        }


        ImGui::Text("New table:");
        ImGui::SameLine();

        char buf[128];
        snprintf(buf, sizeof(buf), "%s", tableTo.c_str());
        ImGui::InputText("##newtbl", buf, sizeof(buf));
        tableTo = buf;

        bool allow = ( ! tableTo.empty() && ! tableFrom.empty() );
        if ( ! allow )
            ImGui::BeginDisabled();

        if ( ImGui::Button("Duplicate") ) {
            duplicateClicked = true;
        }

        if ( ! allow )
            ImGui::EndDisabled();

        doLayoutSave(windowPrefix);

        ImGui::End();
    }

    bool doDuplication = false;

    if ( duplicateClicked ) {
        duplicateClicked = false;
        if ( stringVecContains(tableNames, tableTo) )
            ImGui::OpenPopup("Existing table");
        else
            doDuplication = true;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Existing table", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Destination table '%s' already exists\nand will be overwritten, continue?", tableTo.c_str());

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
        if ( ImGui::Button("Ok", ImVec2(120, 0))) {
            doDuplication = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::EndPopup();
    }

    if ( doDuplication ) {
        g_log.log(LL_DEBUG, ("Duplicating table: "+tableFrom+" -> "+tableTo).c_str());

        string errMsg;
        vector< vector<string> > tmp;
        executeDatabaseStatement_generic("SELECT sql FROM sqlite_master WHERE type='table' AND name='"+tableFrom+"'", &tmp, errMsg);

        if ( tmp.empty() ) {
            g_log.log(LL_ERROR, "Could not find table '%s' to duplicate!", tableFrom.c_str());
        }
        else {
            string createStr = regex_replace( tmp[1][0], std::regex(tableFrom), tableTo );
            executeDatabaseStatement_generic("DROP TABLE IF EXISTS '"+tableTo+"'", NULL, errMsg);
            executeDatabaseStatement_generic(createStr, NULL, errMsg);
            executeDatabaseStatement_generic("INSERT INTO '"+tableTo+"' SELECT * from '"+tableFrom+"'", NULL, errMsg);
            fetchTableNames();
            addTableToRefresh( tableTo );
        }
    }
}















