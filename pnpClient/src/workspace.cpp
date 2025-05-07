#include <map>
#include <regex>

#include "workspace.h"
#include "db.h"
#include "log.h"
#include "util.h"
#include "scriptexecution.h"
#include "tableView.h"
#include "script_globals.h"

using namespace std;

void HelpMarker(const char* desc);

struct workspaceLayoutInfo_t {
    //int id;
    string title;
    std::map<string, workspaceWindowInfo_t> workspaceWindowMap;
    /*bool show_status_window;
    bool show_demo_window;
    bool show_log_window;
    bool show_usbcamera_window;
    bool show_plot_view;
    bool show_overrides_view;
    bool show_hooks_view;
    bool show_custom_view;
    bool show_tweaks_view;
    bool show_server_view;
    bool show_serial_view;
    int show_script_editors;
    int show_command_editors;*/

    vector<string> internalWindows;
    vector<string> tableWindows;

    workspaceLayoutInfo_t() {
        //id = 0;
    }
};

bool show_status_window = true;
bool show_demo_window = false;
bool show_log_window = true;
bool show_usbcamera_window = false;
bool show_plot_view = false;
bool show_overrides_view = false;
bool show_hooks_view = false;
bool show_custom_view = false;
bool show_tweaks_view = false;
bool show_server_view = false;
bool show_serial_view = false;
bool show_table_views = false;
bool show_table_settings = false;
bool show_combobox_entries = false;
bool show_duplicate_table_view = false;

string workspaceInfoSaveRequestedTitle = "";
string workspaceInfoLoadRequestedTitle = "";
bool savingWorkspaceInfo = false;
bool loadingWorkspaceInfo = false;

workspaceLayoutInfo_t currentLayout; // undergoing save or restore


bool haveCurrentWorkspaceLayout()
{
    return currentLayout.title != "";
}

string getCurrentLayoutTitle() {
    return currentLayout.title;
}


void requestWorkspaceInfoSave(string layoutTitle)
{
    workspaceInfoSaveRequestedTitle = layoutTitle;
}

void requestWorkspaceInfoResave()
{
    if ( currentLayout.title == "" ) {
        g_log.log(LL_ERROR, "Can't resave workspace layout with no id or title");
        return;
    }
    requestWorkspaceInfoSave(currentLayout.title);
}

string wasWorkspaceInfoSaveRequested()
{
    string s = workspaceInfoSaveRequestedTitle;
    workspaceInfoSaveRequestedTitle = "";
    return s;
}



void requestWorkspaceInfoLoad(string layoutTitle)
{
    workspaceInfoLoadRequestedTitle = layoutTitle;
}

string wasWorkspaceInfoLoadRequested()
{
    string s = workspaceInfoLoadRequestedTitle;
    workspaceInfoLoadRequestedTitle = "";
    return s;
}



void beginSavingWorkspaceInfo() {
    savingWorkspaceInfo = true;
    currentLayout.workspaceWindowMap.clear();
}

bool isSavingWorkspaceInfo() {
    return savingWorkspaceInfo;
}

void endSavingWorkspaceInfo(string layoutTitle)
{
    savingWorkspaceInfo = false;

    // g_log.log(LL_DEBUG, "endSavingWorkspaceInfo, %d entries", (int)currentLayout.workspaceWindowMap.size());
    // for (pair<string, workspaceWindowInfo_t> infoPair : currentLayout.workspaceWindowMap) {
    //     string title = infoPair.first;
    //     workspaceWindowInfo_t &info = infoPair.second;
    //     g_log.log(LL_DEBUG, "%s: %d,%d %d,%d", title.c_str(), (int)info.pos.x, (int)info.pos.y, (int)info.size.x, (int)info.size.y);
    // }

    // currentLayout.show_status_window = show_status_window;
    // currentLayout.show_demo_window = show_demo_window;
    // currentLayout.show_log_window = show_log_window;
    // currentLayout.show_usbcamera_window = show_usbcamera_window;
    // currentLayout.show_plot_view = show_plot_view;
    // currentLayout.show_overrides_view = show_overrides_view;
    // currentLayout.show_hooks_view = show_hooks_view;
    // currentLayout.show_custom_view = show_custom_view;
    // currentLayout.show_tweaks_view = show_tweaks_view;
    // currentLayout.show_script_editors = getNumOpenScriptEditorWindows();
    // currentLayout.show_command_editors = getNumOpenCommandEditorWindows();

    saveWorkspaceToDB( layoutTitle );
}

void beginLoadingWorkspaceInfo(string layoutTitle) {
    loadWorkspaceFromDB(layoutTitle);
    loadingWorkspaceInfo = true;
}

bool isLoadingWorkspaceInfo() {
    return loadingWorkspaceInfo;
}

void endLoadingWorkspaceInfo()
{
    loadingWorkspaceInfo = false;
}

void setWorkspaceWindowInfo(string title, ImVec2 pos, ImVec2 size)
{
    workspaceWindowInfo_t info;
    info.pos = pos;
    info.size = size;
    currentLayout.workspaceWindowMap[title] = info;
}

workspaceWindowInfo_t getWorkspaceWindowInfo(std::string title)
{
    std::map<string, workspaceWindowInfo_t>::iterator it = currentLayout.workspaceWindowMap.find( title );
    if ( it != currentLayout.workspaceWindowMap.end() ) {
        pair<string, workspaceWindowInfo_t> infoPair = *it;
        workspaceWindowInfo_t info = infoPair.second;
        return info;
    }

    workspaceWindowInfo_t info;
    return info;
}

void doLayoutSave(string title)
{
    if ( isSavingWorkspaceInfo() ) {
        setWorkspaceWindowInfo(title, ImGui::GetWindowPos(), ImGui::GetWindowSize());
    }
}

void doLayoutLoad(string title)
{
    if ( isLoadingWorkspaceInfo() ) {
        workspaceWindowInfo_t info = getWorkspaceWindowInfo(title);
        if ( info.size.x > 0 ) {
            ImGui::SetNextWindowPos(info.pos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(info.size, ImGuiCond_Always);
        }
    }
}

vector< vector<string> > loadWorkspaceResult;

static int loadWorkspace_callback(void *NotUsed, int argc, char **argv, char **azColName) {

    vector<string> cols;

    for ( int i = 0; i < argc; i++ )
        cols.push_back( argv[i] ? argv[i] : "NULL" );

    loadWorkspaceResult.push_back( cols );

    return 0;
}

#define WW_STATUS           "status"
#define WW_DEMO             "demo"
#define WW_LOG              "log"
#define WW_USBCAMERA        "usbcamera"
#define WW_PLOT             "plot"
#define WW_OVERRIDES        "overrides"
#define WW_HOOKS            "hooks"
#define WW_CUSTOMBUTTONS    "custombuttons"
#define WW_TWEAKS           "tweaks"
#define WW_SCRIPTEDITOR     "scripteditor"
#define WW_COMMANDEDITOR    "commandeditor"
#define WW_SERVER           "server"
#define WW_SERIAL           "serial"
#define WW_DBTABLES         "dbtables"
#define WW_DBTABLES_AUTOGEN_SCRIPT      "dbtablesAutogenScript"
#define WW_COMBOBOX_ENTRIES             "comboboxentries"
#define WW_DUPLICATE_TABLE              "duplicatetable"

void saveWorkspaceToDB_windowsOpen(string layoutTitle)
{
    vector<string> internalsVec;
    if ( show_status_window ) internalsVec.push_back( WW_STATUS );
    if ( show_demo_window ) internalsVec.push_back( WW_DEMO );
    if ( show_log_window ) internalsVec.push_back( WW_LOG );
    if ( show_usbcamera_window ) internalsVec.push_back( WW_USBCAMERA );
    if ( show_plot_view ) internalsVec.push_back( WW_PLOT );
    if ( show_overrides_view ) internalsVec.push_back( WW_OVERRIDES );
    if ( show_hooks_view ) internalsVec.push_back( WW_HOOKS );
    if ( show_custom_view ) internalsVec.push_back( WW_CUSTOMBUTTONS );
    if ( show_tweaks_view ) internalsVec.push_back( WW_TWEAKS );
    if ( getNumOpenScriptEditorWindows() ) internalsVec.push_back( WW_SCRIPTEDITOR );
    if ( getNumOpenCommandEditorWindows() ) internalsVec.push_back( WW_COMMANDEDITOR );
    if ( show_server_view ) internalsVec.push_back( WW_SERVER );
    if ( show_serial_view ) internalsVec.push_back( WW_SERIAL );
    if ( show_table_views ) internalsVec.push_back( WW_DBTABLES );
    if ( show_table_settings ) internalsVec.push_back( WW_DBTABLES_AUTOGEN_SCRIPT );
    if ( show_combobox_entries ) internalsVec.push_back( WW_COMBOBOX_ENTRIES );
    if ( show_duplicate_table_view ) internalsVec.push_back( WW_DUPLICATE_TABLE );

    string internalsStr = joinStringVec(internalsVec, ",");
    string tablesStr = joinStringVec(getOpenTableNames(), ",");

    string errMsg;
    string sql = string("insert into internal_windowsopen (layouttitle, internal, tables) ") +
        " values ("
                 +"'"+layoutTitle+"',"
                 +"'"+internalsStr+"',"
                 +"'"+tablesStr+"'"
        +") ON CONFLICT DO UPDATE SET "
                 +" internal = '"+internalsStr+"',"
                 +" tables = '"+tablesStr+"'"
        ;

    executeDatabaseStatement(sql, NULL, errMsg);
}

void saveWorkspaceToDB_windows(string layoutTitle)
{
    string errMsg;

    for (pair<string, workspaceWindowInfo_t> infoPair : currentLayout.workspaceWindowMap) {
        string windowTitle = infoPair.first;
        workspaceWindowInfo_t &info = infoPair.second;

        string idStr = info.id == 0 ? "NULL" : to_string(info.id);

        string sql = "insert into internal_windowpos (id, layouttitle, windowtitle, x, y, w, h) values ("+
                     idStr +",'"+layoutTitle+"','"+windowTitle+"',"+to_string(info.pos.x)+","+to_string(info.pos.y)+","+to_string(info.size.x)+","+to_string(info.size.y)+")"
                     + " on CONFLICT do UPDATE set layouttitle = '"+ layoutTitle +"', windowtitle = '"+ windowTitle +"', x = "+to_string(info.pos.x) +", y = "+to_string(info.pos.y) +", w = "+to_string(info.size.x) +", h = "+to_string(info.size.y);
        executeDatabaseStatement(sql, NULL, errMsg);
    }
}

void saveWorkspaceToDB(string layoutTitle)
{
    saveWorkspaceToDB_windowsOpen(layoutTitle);
    saveWorkspaceToDB_windows(layoutTitle);
    currentLayout.title = layoutTitle;
}

bool loadWorkspaceFromDB_windowsOpen(string layoutTitle)
{
    loadWorkspaceResult.clear();

    string errMsg;

    string sql = "select ifnull(internal,''), ifnull(tables,'') from internal_windowsopen where layouttitle = '"+layoutTitle+"'";
    if ( executeDatabaseStatement(sql, loadWorkspace_callback, errMsg) ) {
        if ( ! loadWorkspaceResult.empty() ) {
            vector<string> &cols = loadWorkspaceResult[0];

            currentLayout.internalWindows.clear();
            currentLayout.tableWindows.clear();

            splitStringVec(currentLayout.internalWindows, cols[0], ',');

            show_status_window = stringVecContains( currentLayout.internalWindows, WW_STATUS );
            show_demo_window = stringVecContains( currentLayout.internalWindows, WW_DEMO );
            show_log_window = stringVecContains( currentLayout.internalWindows, WW_LOG );
            show_usbcamera_window = stringVecContains( currentLayout.internalWindows, WW_USBCAMERA );
            show_plot_view = stringVecContains( currentLayout.internalWindows, WW_PLOT );
            show_overrides_view = stringVecContains( currentLayout.internalWindows, WW_OVERRIDES );
            show_hooks_view = stringVecContains( currentLayout.internalWindows, WW_HOOKS );
            show_custom_view = stringVecContains( currentLayout.internalWindows, WW_CUSTOMBUTTONS );
            show_tweaks_view = stringVecContains( currentLayout.internalWindows, WW_TWEAKS );
            show_server_view = stringVecContains( currentLayout.internalWindows, WW_SERVER );
            show_serial_view = stringVecContains( currentLayout.internalWindows, WW_SERIAL );
            show_table_views = stringVecContains( currentLayout.internalWindows, WW_DBTABLES );
            show_table_settings = stringVecContains( currentLayout.internalWindows, WW_DBTABLES_AUTOGEN_SCRIPT );
            show_combobox_entries = stringVecContains( currentLayout.internalWindows, WW_COMBOBOX_ENTRIES );
            show_duplicate_table_view = stringVecContains( currentLayout.internalWindows, WW_DUPLICATE_TABLE );

            ensureNScriptEditorWindowsOpen( stringVecContains( currentLayout.internalWindows, WW_SCRIPTEDITOR ) ? 1 : 0 );
            ensureNCommandEditorWindowsOpen( stringVecContains( currentLayout.internalWindows, WW_COMMANDEDITOR ) ? 1 : 0 );

            splitStringVec(currentLayout.tableWindows, cols[1], ',');
            setDesiredOpenTables( currentLayout.tableWindows );
        }
        else {
            g_log.log(LL_ERROR, "No workspace layout with title '%s'", layoutTitle.c_str());
            return false;
        }
    }

    return true;
}

bool loadWorkspaceFromDB_windows(string layoutTitle)
{
    loadWorkspaceResult.clear();

    string errMsg;

    string sql = "select id, windowtitle, x, y, w, h from internal_windowpos where layouttitle = '"+layoutTitle+"'";
    if ( ! executeDatabaseStatement(sql, loadWorkspace_callback, errMsg) )
        return false;

    currentLayout.workspaceWindowMap.clear();

    for (int r = 0; r < (int)loadWorkspaceResult.size(); r++) {
        vector<string> &cols = loadWorkspaceResult[r];

        workspaceWindowInfo_t info;
        info.id = atoi(cols[0].c_str());
        info.title = cols[1];
        info.pos.x = atoi(cols[2].c_str());
        info.pos.y = atoi(cols[3].c_str());
        info.size.x = atoi(cols[4].c_str());
        info.size.y = atoi(cols[5].c_str());
        currentLayout.workspaceWindowMap[info.title] = info;
    }

    currentLayout.title = layoutTitle;

    return true;
}

bool loadWorkspaceFromDB(string layoutTitle)
{
    bool ok = true;
    ok &= loadWorkspaceFromDB_windowsOpen(layoutTitle);
    ok &= loadWorkspaceFromDB_windows(layoutTitle);
    return ok;
}

#define OPENLAYOUT_DIALOG_ID "Open layout..."
#define SAVELAYOUTAS_DIALOG_ID "Save layout as..."

string saveLayoutErrorMsg;
bool okayToOverwriteExistingLayout = false;
bool doRefocusOnInputAfterLayoutSaveAsFail = false;

bool checkWorkspaceLayoutName(string layoutTitle, bool okayToOverwrite, string& errMsg)
{
    if ( layoutTitle.empty() ) {
        errMsg = "Can't save workspace layout with empty title";
        //g_log.log(LL_ERROR, errMsg.c_str());
        return false;
    }

    loadWorkspaceResult.clear();

    string sql = "select layouttitle from internal_windowsopen where layouttitle = '"+layoutTitle+"'";
    if ( ! executeDatabaseStatement(sql, loadWorkspace_callback, errMsg) ) {
        errMsg = "DB error when checking if layout name exists";
        g_log.log(LL_ERROR, "%s", errMsg.c_str());
        return false;
    }

    if ( ! loadWorkspaceResult.empty() ) {
        if ( !okayToOverwrite ) {
            errMsg = "Workspace layout title already exists";
            return false;
        }
    }

    return true;
}

void openWorkspaceLayoutSaveAsDialogPopup()
{
    saveLayoutErrorMsg.clear();
    okayToOverwriteExistingLayout = false;
    ImGui::OpenPopup(SAVELAYOUTAS_DIALOG_ID);
}

void showWorkspaceLayoutSaveAsDialogPopup(bool openingNow)
{
    ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(SAVELAYOUTAS_DIALOG_ID, NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char path[256] = "";

        ImGui::Text("Title:");
        ImGui::SameLine();
        if ( openingNow || doRefocusOnInputAfterLayoutSaveAsFail )
            ImGui::SetKeyboardFocusHere();
        string textBefore = path;
        bool enterWasPressed = ImGui::InputText("##save", path, sizeof(path), ImGuiInputTextFlags_EnterReturnsTrue);
        bool textWasChanged = textBefore != path;

        if ( textWasChanged )
            saveLayoutErrorMsg = "";

        doRefocusOnInputAfterLayoutSaveAsFail = false;

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
        if ( enterWasPressed || ImGui::Button("OK", ImVec2(120, 0))) {
            if ( checkWorkspaceLayoutName(path, okayToOverwriteExistingLayout, saveLayoutErrorMsg) ) {
                requestWorkspaceInfoSave( path );
                ImGui::CloseCurrentPopup();
            }
            else
                doRefocusOnInputAfterLayoutSaveAsFail = true;
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();

        if ( ! saveLayoutErrorMsg.empty() ) {
            ImGui::TextColored( ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", saveLayoutErrorMsg.c_str());
            if ( strlen(path) > 0 )
                ImGui::Checkbox("Overwrite existing layout", &okayToOverwriteExistingLayout);
        }

        ImGui::EndPopup();
    }
}

string openLayoutErrorMsg;
static vector<string> openableLayoutNames;
static int selectedOpenLayoutIndex = -1;

void openWorkspaceLayoutOpenDialogPopup()
{
    openableLayoutNames.clear();
    loadWorkspaceResult.clear();

    string errMsg;
    string sql = "select layouttitle from internal_windowsopen";
    if ( ! executeDatabaseStatement(sql, loadWorkspace_callback, errMsg) ) {
        errMsg = "DB error when fetching layout names";
        g_log.log(LL_ERROR, "%s", errMsg.c_str());
        return;
    }

    for (int r = 0; r < (int)loadWorkspaceResult.size(); r++) {
        vector<string> &cols = loadWorkspaceResult[r];
        openableLayoutNames.push_back( cols[0] );
    }

    selectedOpenLayoutIndex = -1;
    ImGui::OpenPopup(OPENLAYOUT_DIALOG_ID);
}

void showWorkspaceLayoutOpenDialogPopup()
{
    ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(OPENLAYOUT_DIALOG_ID, NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        string pathToOpen;

        if (ImGui::BeginListBox("##openpath"))
        {
            for (int n = 0; n < (int)openableLayoutNames.size(); n++)
            {
                string thePath = openableLayoutNames[n];
                const bool isSelected = (selectedOpenLayoutIndex == n);

                if (ImGui::Selectable(thePath.c_str(), isSelected))
                    selectedOpenLayoutIndex = n;

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                    pathToOpen = thePath;
                }
            }
            ImGui::EndListBox();
        }

        if ( pathToOpen.empty() )
            ImGui::BeginDisabled();

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
        if ( ImGui::Button("OK", ImVec2(120, 0))) {
            requestWorkspaceInfoLoad(pathToOpen);
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();

        if ( pathToOpen.empty() )
            ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();

        if ( ! openLayoutErrorMsg.empty() ) {
            ImGui::TextColored( ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", openLayoutErrorMsg.c_str());
        }

        ImGui::EndPopup();
    }
}

#define COMBOBOX_ENTRIES_WINDOW_TITLE "Combobox entries"

string usbCameraFunctionComboboxEntries;
string tableButtonFunctionEntries;

void showComboboxEntries(bool* p_open) {

    ImGui::SetNextWindowSize(ImVec2(320, 480), ImGuiCond_FirstUseEver);

    doLayoutLoad(COMBOBOX_ENTRIES_WINDOW_TITLE);

    string oneEntry = " *[a-zA-Z]\\w*(-\\w+)? *";

    // comma separated alphanumeric string (starting with a letter), allow whitespace on either side
    string regexStr = "^("+oneEntry+")(,"+oneEntry+")*$";

    ImGui::Begin(COMBOBOX_ENTRIES_WINDOW_TITLE, p_open);
    {
        char buf[512];
        std::regex regexPattern( regexStr );

        {
            ImGui::Text( "USB camera functions" );

            ImGui::SameLine();
            HelpMarker("Script functions to show in USB camera view combobox, as a comma separated list.");

            bool regexFailed = false;
            std::smatch matches;
            if ( ! usbCameraFunctionComboboxEntries.empty() ) {
                if ( ! std::regex_search(usbCameraFunctionComboboxEntries, matches, regexPattern) ) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                    regexFailed = true;
                }
            }

            snprintf( buf, sizeof(buf), "%s", usbCameraFunctionComboboxEntries.c_str());
            ImGui::PushItemWidth(-40);
            ImGui::SameLine();
            ImGui::InputText( "##camfunc", buf, sizeof(buf) );
            usbCameraFunctionComboboxEntries = buf;

            if ( regexFailed ) {
                ImGui::PopStyleColor(1);
                ImGui::BeginDisabled();
            }

            ImGui::SameLine();
            if ( ImGui::Button("Set##usbcam") ) {
                script_setDBString( DBSTRING_USB_CAMERA_FUNCTIONS, usbCameraFunctionComboboxEntries );
            }

            if ( regexFailed ) {
                ImGui::EndDisabled();
            }
        }

        {
            ImGui::Text( "DB table button functions" );

            ImGui::SameLine();
            HelpMarker("Script functions to show in comboboxes for DB table buttons, as a comma separated list. To restrict display to a specific table, add that table as a hyphenated suffix, eg. \"gotoQR-feeder\" will only be shown in the 'feeder' table.");

            bool regexFailed = false;
            std::smatch matches;
            if ( ! tableButtonFunctionEntries.empty() ) {
                if ( ! std::regex_search(tableButtonFunctionEntries, matches, regexPattern) ) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                    regexFailed = true;
                }
            }

            snprintf( buf, sizeof(buf), "%s", tableButtonFunctionEntries.c_str());
            ImGui::PushItemWidth(-40);
            ImGui::SameLine();
            ImGui::InputText( "##tablebuttonfunc", buf, sizeof(buf) );
            tableButtonFunctionEntries = buf;

            if ( regexFailed ) {
                ImGui::PopStyleColor(1);
                ImGui::BeginDisabled();
            }

            ImGui::SameLine();
            if ( ImGui::Button("Set##tblbutton") ) {
                script_setDBString( DBSTRING_TABLE_BUTTON_FUNCTIONS, tableButtonFunctionEntries );
            }

            if ( regexFailed ) {
                ImGui::EndDisabled();
            }
        }
    }

    doLayoutSave(COMBOBOX_ENTRIES_WINDOW_TITLE);

    ImGui::End();
}
















