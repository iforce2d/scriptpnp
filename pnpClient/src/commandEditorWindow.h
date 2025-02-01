#ifndef COMMAND_EDITOR_H
#define COMMAND_EDITOR_H

#include "codeEditorWindow.h"

class CommandEditorWindow : public CodeEditorWindow {
public:
    CommandEditorWindow(std::vector<CodeEditorDocument*> *documents, std::string id, int index);
    std::string getDBFileType() { return "command list"; }
    std::string getWindowTitle() { return "Command list editor"; };
    void preRenderDoc(CodeEditorDocument* doc);
    void renderCustomSection();
    bool shouldShowOutputPane() { return true; }
    void showOutputPane();
    int pushStyleColors();
    bool runCommandList(bool previewOnly);
    std::string getOpenDialogTitle() { return "Open command list..."; }
};

#endif
