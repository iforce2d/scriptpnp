
#include <stdio.h>
#include <string>
#include "overrides.h"
#include "workspace.h"

#include "imgui.h"

using namespace std;

#define OVERRIDES_WINDOW_TITLE "Output overrides"

int buttonDifferentiatorIndex = 0;
char buttonDifferentiatorBuffer[128];

// ignore annoying GCC warning about not checking result of snprintf
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"

char* bd(const char* str) {
    snprintf(buttonDifferentiatorBuffer, sizeof(buttonDifferentiatorBuffer), "%s##%d", str, buttonDifferentiatorIndex++);
    return buttonDifferentiatorBuffer;
}

#pragma GCC diagnostic pop

const char* operandNames[] = {
    "motion axis",
    "digital output",
    "PWM output",
    "digital input",
    "vacuum sensor",
    "loadcell sensor",
    "ADC input"
};

const char* actionOperandNames[] = {
    "digital output",
    "PWM output"
};

const char* motionAxisNames[] = {
    "X",
    "Y",
    "Z",
    "W"
};

const char* floatComparisonNames[] = {
    "less than",
    "more than"
};

const char* digitalComparisonNames[] = {
    "low",
    "high"
};

void showOverrideEdit_condition(OverrideConfig &config, int index) {

    char operandPopup[32];
    char axisPopup[32];
    char comparisonPopup[32];
    sprintf(operandPopup, "op%d", index);
    sprintf(axisPopup, "axis%d", index);
    sprintf(comparisonPopup, "comparison%d", index);

    ImGui::Text("Condition: when");

    ImGui::SameLine();
    if (ImGui::Button(bd(config.condition.ui_operandIndex == -1 ? "<operand>" : operandNames[config.condition.ui_operandIndex])))
        ImGui::OpenPopup(operandPopup);
    if (ImGui::BeginPopup(operandPopup)) {
        for (int i = 0; i < IM_ARRAYSIZE(operandNames); i++)
            if (ImGui::Selectable(operandNames[i])) {

                config.condition.resetSelector();

                config.condition.ui_operandIndex = i;
            }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if ( config.condition.ui_operandIndex == -1 ) {
        ImGui::BeginDisabled();
        if (ImGui::Button(bd("<axis>")));
        ImGui::EndDisabled();
    }
    else if ( config.condition.ui_operandIndex == ONI_MOTION_AXIS ) { // motion axis
        int bitpos = getBitPosition(config.condition.motionAxis);
        if (ImGui::Button(bd(bitpos == -1 ? "<which>" : motionAxisNames[bitpos])))
            ImGui::OpenPopup(axisPopup);
        if (ImGui::BeginPopup(axisPopup)) {
            for (int i = 0; i < IM_ARRAYSIZE(motionAxisNames); i++)
                if (ImGui::Selectable(motionAxisNames[i])) {

                    config.condition.resetSelector();

                    config.condition.motionAxis = setBitPosition(i);
                }
            ImGui::EndPopup();
        }
    }
    else {
        char axisName[32];
        int bitpos = -1;
        int numAxes = 0;
        switch ( config.condition.ui_operandIndex ) {
        case ONI_DIGITAL_OUTPUT: bitpos = getBitPosition(config.condition.digitalOutput); numAxes = 16; break;
        case ONI_PWM_OUTPUT: bitpos = getBitPosition(config.condition.pwmOutput); numAxes = 4; break;
        case ONI_DIGITAL_INPUT: bitpos = getBitPosition(config.condition.digitalInput); numAxes = 16; break;
        case ONI_VACUUM_SENSOR: bitpos = getBitPosition(config.condition.pressure); numAxes = 4; break;
        case ONI_LOADCELL_SENSOR: bitpos = getBitPosition(config.condition.loadcell); numAxes = 4; break;
        case ONI_ADC_INPUT: bitpos = getBitPosition(config.condition.adc); numAxes = 4; break;
        default: break;
        }
        sprintf(axisName, "%d", bitpos);
        if (ImGui::Button(bd(bitpos == -1 ? "<which>" : axisName)))
            ImGui::OpenPopup(axisPopup);
        if (ImGui::BeginPopup(axisPopup)) {
            for (int i = 0; i < numAxes; i++) {
                sprintf(axisName, "%d", i);
                if (ImGui::Selectable(axisName)) {

                    config.condition.resetSelector();

                    switch ( config.condition.ui_operandIndex ) {
                    case ONI_DIGITAL_OUTPUT: config.condition.digitalOutput = setBitPosition(i); break;
                    case ONI_PWM_OUTPUT: config.condition.pwmOutput = setBitPosition(i); break;
                    case ONI_DIGITAL_INPUT: config.condition.digitalInput = setBitPosition(i); break;
                    case ONI_VACUUM_SENSOR: config.condition.pressure = setBitPosition(i); break;
                    case ONI_LOADCELL_SENSOR: config.condition.loadcell = setBitPosition(i); break;
                    case ONI_ADC_INPUT: config.condition.adc = setBitPosition(i); break;
                    }
                }
            }
            ImGui::EndPopup();
        }
    }

    ImGui::SameLine();
    ImGui::Text("is");

    ImGui::SameLine();
    if ( config.condition.ui_operandIndex == -1 ) {
        ImGui::BeginDisabled();
        if (ImGui::Button(bd("<comparison>")));
        ImGui::EndDisabled();
    }
    else {
        const char** names = floatComparisonNames;
        bool isDigital = false;
        switch ( config.condition.ui_operandIndex ) {
        case ONI_DIGITAL_INPUT:
        case ONI_DIGITAL_OUTPUT:
            names = digitalComparisonNames;
            isDigital = true;
        }
        if (ImGui::Button(bd(config.condition.comparison == -1 ? "<comparison>" : names[config.condition.comparison])))
            ImGui::OpenPopup(comparisonPopup);
        if (ImGui::BeginPopup(comparisonPopup)) {
            for (int i = 0; i < 2; i++)
                if (ImGui::Selectable(names[i]))
                    config.condition.comparison = (overrideConditionComparison_e)i;
            ImGui::EndPopup();
        }

        if ( ! isDigital ) {
            ImGui::SameLine();
            ImGui::PushItemWidth(140);
            ImGui::InputFloat(" ", &config.condition.val);
        }
    }

}

int deleteActionConfigIndex = -1;
int deleteActionListIndex = -1;
int deleteActionIndex = -1;

void showEditActions( vector<overrideAction_t> &actions, int configIndex, int actionsListIndex ) {
    for (int i = 0; i < (int)actions.size(); i++) {
        char buf[32];
        sprintf(buf, "Action %d", i+1);
        overrideAction_t &action = actions[i];

        char actionOperandPopup[32];
        sprintf(actionOperandPopup, "actionOp%d%d%d", configIndex, actionsListIndex, i);

        ImGui::Text("Action: set ");

        ImGui::SameLine();
        if (ImGui::Button(bd(action.ui_operandIndex == -1 ? "<operand>" : actionOperandNames[action.ui_operandIndex])))
            ImGui::OpenPopup(actionOperandPopup);
        if (ImGui::BeginPopup(actionOperandPopup)) {
            for (int i = 0; i < IM_ARRAYSIZE(actionOperandNames); i++)
                if (ImGui::Selectable(actionOperandNames[i]))
                    action.ui_operandIndex = i;
            ImGui::EndPopup();
        }

        char axisPopup[32];
        sprintf(axisPopup, "actionaxis%d%d%d", configIndex, actionsListIndex, i);

        ImGui::SameLine();
        if ( action.ui_operandIndex == -1 ) {
            ImGui::BeginDisabled();
            if (ImGui::Button(bd("<axis>")));
            ImGui::EndDisabled();
        }
        else {
            char axisName[32];
            int bitpos = -1;
            int numAxes = 0;
            bool isDigital = false;
            switch ( action.ui_operandIndex ) {
            case 0: bitpos = getBitPosition(action.digitalOutput); numAxes = 16; isDigital = true; break;
            case 1: bitpos = getBitPosition(action.pwmOutput); numAxes = 4; break;
            default: break;
            }
            sprintf(axisName, "%d", bitpos);
            if (ImGui::Button(bd(bitpos == -1 ? "<which>" : axisName)))
                ImGui::OpenPopup(axisPopup);
            if (ImGui::BeginPopup(axisPopup)) {
                for (int i = 0; i < numAxes; i++) {
                    sprintf(axisName, "%d", i);
                    if (ImGui::Selectable(axisName)) {

                        action.resetSelector();

                        switch ( action.ui_operandIndex ) {
                        case 0: action.digitalOutput = setBitPosition(i); break;
                        case 1: action.pwmOutput = setBitPosition(i); break;
                        }
                    }
                }
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            char lohiPopup[64];
            if ( isDigital ) {
                sprintf(lohiPopup, "actionlohi%d%d%d", configIndex, actionsListIndex, i);
                int ival = action.val;
                if (ImGui::Button(bd(digitalComparisonNames[ival])))
                    ImGui::OpenPopup(lohiPopup);
                if (ImGui::BeginPopup(lohiPopup)) {
                    for (int k = 0; k < 2; k++)
                        if (ImGui::Selectable(digitalComparisonNames[k]))
                            action.val = k;
                    ImGui::EndPopup();
                }
            }
            else {
                sprintf(lohiPopup, "##actionpwm%d%d%d", configIndex, actionsListIndex, i);
                ImGui::PushItemWidth(140);
                ImGui::InputFloat(lohiPopup, &action.val, 0, 0, "%.6f");
            }

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
            if ( ImGui::Button(bd("Delete")) ) {
                deleteActionConfigIndex = configIndex;
                deleteActionListIndex = actionsListIndex;
                deleteActionIndex = i;
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
        }
    }
}

void showOverrideEdit_actions(OverrideConfig &config, int configIndex) {

    if ( ImGui::Button(bd("Add new pass action")) ) {
        overrideAction_t oa;
        config.passActions.push_back(oa);
    }
    ImGui::SameLine();
    if ( ImGui::Button(bd("Add new fail action")) ) {
        overrideAction_t oa;
        config.failActions.push_back(oa);
    }

    ImGui::Text("Pass actions");
    ImGui::Indent(30);
    showEditActions( config.passActions, configIndex, 0 );
    ImGui::Unindent(30);
    ImGui::Text("Fail actions");
    ImGui::Indent(30);
    showEditActions( config.failActions, configIndex, 1 );
    ImGui::Unindent(30);

}

void showViewActions(vector<overrideAction_t> &actions) {
    for (int i = 0; i < (int)actions.size(); i++) {
        overrideAction_t &action = actions[i];
        string s = "Set ";
        s += actionOperandNames[action.ui_operandIndex];
        s += " ";

        char axisName[32];
        int bitpos = -1;
        bool isDigital = false;
        switch ( action.ui_operandIndex ) {
        case 0: bitpos = getBitPosition(action.digitalOutput); isDigital = true; break;
        case 1: bitpos = getBitPosition(action.pwmOutput); break;
        default: break;
        }
        sprintf(axisName, "%d", bitpos);
        s += axisName;
        s += " to ";

        if ( isDigital ) {
            s += action.val > 0.5 ? "high" : "low";
        }
        else {
            s += std::to_string(action.val);
        }

        ImGui::Text("%s", s.c_str());
    }
}

void showOverride(OverrideConfig &config, int index) {

    if ( config.isValid() ) {

        string s = "When ";

        s += operandNames[config.condition.ui_operandIndex];
        s += " ";

        if ( config.condition.ui_operandIndex == ONI_MOTION_AXIS ) {
            int bitpos = getBitPosition(config.condition.motionAxis);
            s += motionAxisNames[bitpos];
        }
        else {
            char axisName[32];
            int bitpos = -1;
            switch ( config.condition.ui_operandIndex ) {
            case ONI_DIGITAL_OUTPUT: bitpos = getBitPosition(config.condition.digitalOutput); break;
            case ONI_PWM_OUTPUT: bitpos = getBitPosition(config.condition.pwmOutput); break;
            case ONI_DIGITAL_INPUT: bitpos = getBitPosition(config.condition.digitalInput); break;
            case ONI_VACUUM_SENSOR: bitpos = getBitPosition(config.condition.pressure); break;
            case ONI_LOADCELL_SENSOR: bitpos = getBitPosition(config.condition.loadcell); break;
            case ONI_ADC_INPUT: bitpos = getBitPosition(config.condition.adc); break;
            default: break;
            }
            sprintf(axisName, "%d", bitpos);
            s += axisName;
        }

        s += " is ";

        const char** names = floatComparisonNames;
        bool isDigital = false;
        switch ( config.condition.ui_operandIndex ) {
        case ONI_DIGITAL_INPUT:
        case ONI_DIGITAL_OUTPUT:
            isDigital = true;
            names = digitalComparisonNames;
        }
        s += string(names[config.condition.comparison]);

        if ( ! isDigital ) {
            s += " ";
            s += std::to_string(config.condition.val);
        }

        ImGui::Text("%s", s.c_str());

        if ( !config.passActions.empty() ) {
            ImGui::Indent(30);
            showViewActions(config.passActions);
            ImGui::Unindent(30);
        }
        if ( !config.failActions.empty() ) {
            ImGui::Text("else");
            ImGui::Indent(30);
            showViewActions(config.failActions);
            ImGui::Unindent(30);
        }
    }
    else {
        ImGui::Text("(Invalid config - params not fully set, or no actions assigned)");
    }
}

void showOverridesView_content()
{
    buttonDifferentiatorIndex = 0;

    int moveUpIndex = -1;
    int moveDownIndex = -1;
    int deleteIndex = -1;

    deleteActionConfigIndex = -1;
    deleteActionListIndex = -1;
    deleteActionIndex = -1;

    for (int i = 0; i < (int)overrideConfigs.size(); i++) {
        char buf[32];
        sprintf(buf, "Override priority %d", i);
        ImGui::SeparatorText(buf);
        OverrideConfig &config = overrideConfigs[i];

        if ( config.ui_editing ) {
            showOverrideEdit_condition(config, i);
            ImGui::Indent(30);
            showOverrideEdit_actions(config, i);
            ImGui::Unindent(30);
            if ( ImGui::Button(bd("Done")) )
                config.ui_editing = false;
        }
        else {
            showOverride(config, i);
            if ( ImGui::Button(bd("Edit")) )
                config.ui_editing = true;
            if ( i < (int)overrideConfigs.size()-1 ) {
                ImGui::SameLine();
                if ( ImGui::Button(bd("Move down")) )
                    moveDownIndex = i;
            }
            if ( i > 0 ) {
                ImGui::SameLine();
                if ( ImGui::Button(bd("Move up")) )
                    moveUpIndex = i;
            }

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
            if ( ImGui::Button(bd("Delete")) )
                deleteIndex = i;
            ImGui::PopStyleColor();
            ImGui::PopStyleColor();
        }
        ImGui::NewLine();
    }

    if ( ImGui::Button("Add new override") ) {
        OverrideConfig oc;
        overrideConfigs.push_back(oc);
    }

    if ( moveDownIndex > -1 ) {
        OverrideConfig tmp = overrideConfigs[moveDownIndex];
        overrideConfigs[moveDownIndex] = overrideConfigs[moveDownIndex+1];
        overrideConfigs[moveDownIndex+1] = tmp;
    }

    if ( moveUpIndex > -1 ) {
        OverrideConfig tmp = overrideConfigs[moveUpIndex];
        overrideConfigs[moveUpIndex] = overrideConfigs[moveUpIndex-1];
        overrideConfigs[moveUpIndex-1] = tmp;
    }

    if ( deleteIndex > -1 ) {
        overrideConfigs.erase( overrideConfigs.begin() + deleteIndex );
    }

    if ( deleteActionConfigIndex > -1 && deleteActionListIndex > -1 && deleteActionIndex > -1 ) {
        vector<overrideAction_t> *actionVec;
        if ( deleteActionListIndex == 0 )
            actionVec = &overrideConfigs[deleteActionConfigIndex].passActions;
        else
            actionVec = &overrideConfigs[deleteActionConfigIndex].failActions;
        actionVec->erase( actionVec->begin() + deleteActionIndex );
    }
}

void showOverridesView(bool* p_open) {

    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);

    doLayoutLoad(OVERRIDES_WINDOW_TITLE);

    ImGui::Begin(OVERRIDES_WINDOW_TITLE, p_open);
    {
        showOverridesView_content();
    }

    doLayoutSave(OVERRIDES_WINDOW_TITLE);

    ImGui::End();
}










