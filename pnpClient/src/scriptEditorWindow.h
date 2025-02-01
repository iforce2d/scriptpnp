#ifndef SCRIPT_EDITOR_WINDOW_H
#define SCRIPT_EDITOR_WINDOW_H

#include "codeEditorWindow.h"
#include "scriptlog.h"

class ScriptEditorWindow : public CodeEditorWindow {
    char entryFunction[128];
public:
    ScriptEditorWindow(std::vector<CodeEditorDocument*> *documents, std::string id, int index);
    std::string getDBFileType() { return "script"; }
    std::string getWindowTitle() { return "Script editor"; }
    void preRenderDoc(CodeEditorDocument *doc);
    void renderCustomSection();
    bool shouldShowOutputPane() { return true; }
    int pushStyleColors();
    void showOutputPane();
    std::string getOpenDialogTitle() { return "Open script..."; }

    //void gotoError(errorGotoInfo &info);

    bool runScript(bool previewOnly);
};

#endif // SCRIPT_EDITOR_WINDOW_H
