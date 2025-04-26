
#include <chrono>

#include "scv/planner.h"
#include "scriptEditorWindow.h"

#include "script/engine.h"
#include "db.h"
#include "preview.h"

using namespace std;

extern string scriptPrintBuffer;

ScriptEditorWindow::ScriptEditorWindow(std::vector<CodeEditorDocument *> *docs, string id, int index) : CodeEditorWindow(docs, id, index)
{
    //log.log(LL_DEBUG, "Script output will appear here");
    memset(entryFunction, 0, sizeof(entryFunction));
}

void ScriptEditorWindow::preRenderDoc(CodeEditorDocument* doc)
{
    auto lang = TextEditor::LanguageDefinition::AngelScript();
    doc->editor.SetLanguageDefinition(lang);
}

void ScriptEditorWindow::renderCustomSection()
{
    //ImGui::Text("Total execution time: %.2f sec", 12.24);
}

int ScriptEditorWindow::pushStyleColors()
{
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,   IM_COL32(230, 115, 0, 255));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,         IM_COL32(70, 35, 0, 255));
    ImGui::PushStyleColor(ImGuiCol_Border,          IM_COL32(255, 128, 0, 192));

    ImGui::PushStyleColor(ImGuiCol_Tab,         IM_COL32(119, 84, 0, 255));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,  IM_COL32(231, 129, 0, 255));
    ImGui::PushStyleColor(ImGuiCol_TabActive,   IM_COL32(207, 112, 0, 255));

    ImGui::PushStyleColor(ImGuiCol_Button,         IM_COL32(119, 84, 0, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  IM_COL32(231, 129, 0, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   IM_COL32(207, 112, 0, 255));

    return 9;
}

void ScriptEditorWindow::showOutputPane()
{
    if ( ImGui::Button("Clear") )
        log.clear();

    ImGui::SameLine();
    if ( ImGui::Button("Copy") ) {
        log.copy();
    }

    ImGui::SameLine();
    ImGui::Text("Entry function:");
    ImGui::SameLine();
    ImGui::PushItemWidth(-119);
    ImGui::InputText("##entryFunction", entryFunction, sizeof(entryFunction), ImGuiInputTextFlags_CharsNoBlank);

    if ( currentlyRunningScriptThread() )
        ImGui::BeginDisabled();

    ImGui::SameLine();
    if ( ImGui::Button("Preview") )
        buttonFeedback.shouldPreview |= true;

    ImGui::SameLine();

    if ( ImGui::Button("Run") )
        buttonFeedback.shouldRun = true;

    if ( currentlyRunningScriptThread() )
        ImGui::EndDisabled();

    if ( planGroup_preview.getType() == 1 )
        ImGui::Text("Traverse time: %.2f s", planGroup_preview.getTraverseTime());

    log.drawLogContent();
}

bool ScriptEditorWindow::runScript(bool previewOnly)
{
    // string funcName = entryFunction;
    // if ( funcName.empty() )
    //     funcName = "main";
    // funcName = "void " +funcName +"()";

    if ( ! currentlyRunningScriptThread() )
        log.grayOutExistingText();

    return ::runScript("scriptEditorModule", entryFunction, previewOnly, this);
}



