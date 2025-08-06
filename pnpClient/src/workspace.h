#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <string>
#include "imgui.h"

struct workspaceWindowInfo_t {
    int id;
    std::string title;
    ImVec2 pos;
    ImVec2 size;
    workspaceWindowInfo_t() {
        id = 0;
        pos.x = pos.y = 0;
        size.x = size.y = 0;
    }
};

extern bool show_status_window;
extern bool show_full_status;
extern bool show_demo_window;
extern bool show_log_window;
extern bool show_usbcamera_window;
//extern bool show_usbcamera_view;
extern bool show_plot_view;
extern bool show_overrides_view;
extern bool show_hooks_view;
extern bool show_custom_view;
extern bool show_tweaks_view;
extern bool show_server_view;
extern bool show_serial_view;
extern bool show_table_views;
extern bool show_table_settings;
extern bool show_combobox_entries;
extern bool show_duplicate_table_view;
extern bool show_find_dialog;

extern std::string usbCameraFunctionComboboxEntries;
extern std::string tableButtonFunctionEntries;

bool haveCurrentWorkspaceLayout();
std::string getCurrentLayoutTitle();

void requestWorkspaceInfoSave(std::string layoutTitle);
void requestWorkspaceInfoResave();
std::string wasWorkspaceInfoSaveRequested();

void requestWorkspaceInfoLoad(std::string layoutTitle);
std::string wasWorkspaceInfoLoadRequested();

void beginSavingWorkspaceInfo();
bool isSavingWorkspaceInfo();
void endSavingWorkspaceInfo(std::string layoutTitle);

void beginLoadingWorkspaceInfo(std::string layoutTitle);
bool isLoadingWorkspaceInfo();
void endLoadingWorkspaceInfo();

void setWorkspaceWindowInfo(std::string title, ImVec2 pos, ImVec2 size);
workspaceWindowInfo_t getWorkspaceWindowInfo(std::string title);

void doLayoutSave(std::string title);
void doLayoutLoad(std::string title);

bool checkWorkspaceLayoutName(std::string layoutTitle, bool okayToOverwrite, std::string& errMsg);

void saveWorkspaceToDB_windowsOpen(std::string layoutTitle);
void saveWorkspaceToDB(std::string layoutTitle);

bool loadWorkspaceFromDB_windowsOpen(std::string layoutTitle);
bool loadWorkspaceFromDB(std::string title);

void openWorkspaceLayoutSaveAsDialogPopup();
void showWorkspaceLayoutSaveAsDialogPopup(bool openingNow);

void openWorkspaceLayoutOpenDialogPopup();
void showWorkspaceLayoutOpenDialogPopup(bool openingNow);

void showComboboxEntries(bool* p_open);

std::string& getTableButtonFunctionEntries();
std::string& getUSBCameraFunctionEntries();

#endif // WORKSPACE_H
