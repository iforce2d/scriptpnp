
#include "scv/planner.h"
#include "commandEditorWindow.h"
#include "preview.h"
#include "machinelimits.h"
#include "commandlist_parse.h"
#include "script/engine.h"
#include "net_requester.h"

using namespace std;
using namespace scv;

extern float animSpeedScale;
extern vec3 animLoc;
extern cornerBlendMethod_e blendMethod;
extern float cornerBlendMaxOverlap;

CommandEditorWindow::CommandEditorWindow(std::vector<CodeEditorDocument *> *docs, string id, int index) : CodeEditorWindow(docs, id, index)
{
}

void CommandEditorWindow::preRenderDoc(CodeEditorDocument *doc)
{
    auto lang = TextEditor::LanguageDefinition::PNPCommand();
    doc->editor.SetLanguageDefinition(lang);
}

void CommandEditorWindow::renderCustomSection()
{
    // if (ImGui::CollapsingHeader("Animation"))
    // {
    //     ImGui::SliderFloat("Speed scale", &animSpeedScale, 0, 2);
    //     ImGui::Text("Actual pos: %f, %f, %f", animLoc.x, animLoc.y, animLoc.z);
    // }

    // int bm = blendMethod;
    // ImGui::RadioButton("None", &bm, CBM_NONE); ImGui::SameLine();
    // ImGui::RadioButton("Jerk limited", &bm, CBM_CONSTANT_JERK_SEGMENTS); ImGui::SameLine();
    // ImGui::RadioButton("Interpolate", &bm, CBM_INTERPOLATED_MOVES);
    // blendMethod = (cornerBlendMethod_e)bm;

    // if (blendMethod == CBM_INTERPOLATED_MOVES) {
    //     float overlapBefore = cornerBlendMaxOverlap;
    //     ImGui::SliderFloat("Corner blend max overlap", &cornerBlendMaxOverlap, 0, 1, "%.2f");
    //     if ( cornerBlendMaxOverlap != overlapBefore )
    //         buttonFeedback.shouldPreview |= true;
    // }

    // if ( ImGui::Button("Preview") )
    //     buttonFeedback.shouldPreview |= true;

    // ImGui::SameLine();

    // if ( ImGui::Button("Run") )
    //     buttonFeedback.shouldRun = true;

    // ImGui::Text("Total execution time: %.2f sec", planGroup.getTraverseTime());
}

void CommandEditorWindow::showOutputPane()
{
    //if (ImGui::CollapsingHeader("Animation"))
    {
        //ImGui::SliderFloat("Speed scale", &animSpeedScale, 0, 2);
        //ImGui::Text("Actual pos: %f, %f, %f", animLoc.x, animLoc.y, animLoc.z);
    }

    // int bm = blendMethod;
    // ImGui::RadioButton("None", &bm, CBM_NONE); ImGui::SameLine();
    // ImGui::RadioButton("Jerk limited", &bm, CBM_CONSTANT_JERK_SEGMENTS); ImGui::SameLine();
    // ImGui::RadioButton("Interpolate", &bm, CBM_INTERPOLATED_MOVES);
    // blendMethod = (cornerBlendMethod_e)bm;

    /*ImGui::Text("Blending:");
    ImGui::SameLine();

    const char* previewVal;
    if ( blendMethod == CBM_INTERPOLATED_MOVES )
        previewVal = "Interpolate";
    else if ( blendMethod == CBM_CONSTANT_JERK_SEGMENTS )
        previewVal = "Jerk limited";
    else
        previewVal = "None";

    if (ImGui::BeginCombo("##blnding", previewVal, ImGuiComboFlags_WidthFitPreview))
    {
        if (ImGui::Selectable("None", blendMethod == CBM_NONE))
            blendMethod = CBM_NONE;
        if (blendMethod == CBM_NONE)
            ImGui::SetItemDefaultFocus();

        if (ImGui::Selectable("Jerk limited", blendMethod == CBM_CONSTANT_JERK_SEGMENTS))
            blendMethod = CBM_CONSTANT_JERK_SEGMENTS;
        if (blendMethod == CBM_CONSTANT_JERK_SEGMENTS)
            ImGui::SetItemDefaultFocus();

        if (ImGui::Selectable("Interpolate", blendMethod == CBM_INTERPOLATED_MOVES))
            blendMethod = CBM_INTERPOLATED_MOVES;
        if (blendMethod == CBM_INTERPOLATED_MOVES)
            ImGui::SetItemDefaultFocus();

        ImGui::EndCombo();
    }*/


    /*if (blendMethod == CBM_INTERPOLATED_MOVES) {

        ImGui::Text("Corner blend max overlap:");
        ImGui::SameLine();
        ImGui::PushItemWidth(-1);

        float overlapBefore = cornerBlendMaxOverlap;
        ImGui::SliderFloat("##maxovrlap", &cornerBlendMaxOverlap, 0, 1, "%.2f");
        if ( cornerBlendMaxOverlap != overlapBefore )
            buttonFeedback.shouldPreview |= true;
    }*/

    // if ( ImGui::Button("Clear") )
    //     log.clear();

    // ImGui::SameLine();
    // if ( ImGui::Button("Copy") ) {
    //     log.copy();
    // }

    // ImGui::SameLine();
    // ImGui::Text("Entry function:");
    // ImGui::SameLine();
    // ImGui::PushItemWidth(-119);
    // ImGui::InputText("##entryFunction", entryFunction, sizeof(entryFunction), ImGuiInputTextFlags_CharsNoBlank);

    ImGui::Text("Preview speed:");
    ImGui::SameLine();
    ImGui::PushItemWidth(-114);
    ImGui::SliderFloat("##ass", &animSpeedScale, 0, 2);

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

    ImGui::Text("Traverse time: %.2f s", planGroup_preview.getTraverseTime());

    log.drawLogContent();
}

int CommandEditorWindow::pushStyleColors()
{
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,   IM_COL32(0, 180, 0, 255));
    ImGui::PushStyleColor(ImGuiCol_TitleBg,         IM_COL32(0, 50, 0, 255));
    ImGui::PushStyleColor(ImGuiCol_Border,          IM_COL32(0, 128, 0, 255));

    ImGui::PushStyleColor(ImGuiCol_Tab,         IM_COL32(43, 111, 0, 220));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,  IM_COL32(66, 213, 83, 220));
    ImGui::PushStyleColor(ImGuiCol_TabActive,   IM_COL32(51, 160, 0, 255));

    ImGui::PushStyleColor(ImGuiCol_Button,         IM_COL32(43, 111, 0, 220));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  IM_COL32(66, 213, 83, 220));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   IM_COL32(51, 160, 0, 255));

    return 9;
}

extern trajectoryResult_e lastTrajResult;

bool CommandEditorWindow::runCommandList(bool previewOnly)
{
    clearCompileErrorInfos();

    CommandList program(CBM_INTERPOLATED_MOVES);
    program.cornerBlendMaxFraction = cornerBlendMaxOverlap;

    program.posLimitLower = machineLimits.posLimitLower;
    program.posLimitUpper = machineLimits.posLimitUpper;
    for (int i = 0; i < NUM_ROTATION_AXES; i++)
        program.rotationPositionLimits[i] = machineLimits.rotationPositionLimits[i];

    currentDocument->clearErrorMarkers();
    vector<string> lines;
    currentDocument->editor.GetTextLines(lines);
   // vector<codeCompileErrorInfo> errorInfos;

    log.clear();

    setActiveScriptLog( &log );
    setActiveCommandListPath( currentDocument->filename );

    bool parsedOk = parseCommandList(lines, program); // uses heap

    setActiveScriptLog( NULL );
    setActiveCommandListPath("");

    if ( parsedOk ) {

        dumpCommandList(program);

        if ( previewOnly ) {
            setPreviewMoveLimitsFromCurrentActual();
            planGroup_preview.clear();
            planner* plan = planGroup_preview.addPlan();
            loadCommandsPreview(program, plan);

            planGroup_preview.calculateMovesForLastPlan();
            planGroup_preview.resetTraverse();

            calculateTraversePointsAndEvents();
        }
        else {
            if ( sanityCheckCommandList(program) ) {
                lastTrajResult = TR_NONE;
                sendPackable(MT_SET_PROGRAM, program);
            }
        }
    }
    else {
        showCompileErrorInfos();
        return false;
    }

    return true;
}
