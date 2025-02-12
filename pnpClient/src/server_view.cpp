
#include "imgui.h"
#include "custompanel.h"
#include "eventhooks.h"
#include "scriptexecution.h"
#include "workspace.h"
#include "net_requester.h"
#include "net_subscriber.h"
#include "log.h"
#include "overrides.h"
#include "overrides_view.h"
#include "../common/pnpMessages.h"
#include "../common/machinelimits.h"
//#include "loadcell.h"

#define NO_EXTERNS_FROM_SERVER_VIEW_HEADER

#include "server_view.h"

using namespace std;

char serverHostname[256];
char homingOrder[9];

float config_stepsPerUnit[NUM_MOTION_AXES];
float config_jogSpeed[NUM_MOTION_AXES];

uint16_t config_estopDigitalOutState = 0;
uint16_t config_estopDigitalOutUsed = 0;
float config_estopPWMState[NUM_PWM_VALS] = {0};
uint8_t config_estopPWMUsed = 0;

float config_workingAreaX;
float config_workingAreaY;
float config_workingAreaZ;

tmcSettings_t config_tmc[NUM_MOTION_AXES];
homingParams_t config_homing[NUM_HOMABLE_AXES];

probingParams_t config_probing;

int32_t config_loadcellCalibrationRawOffset;
float config_loadcellCalibrationWeight;

#define SERVER_WINDOW_TITLE "Server"

void fetchAllServerConfigs() {
    sendCommandRequestOfType(MT_CONFIG_STEPS_FETCH);
    sendCommandRequestOfType(MT_CONFIG_WORKAREA_FETCH);
    sendCommandRequestOfType(MT_CONFIG_INITSPEEDS_FETCH);
    sendCommandRequestOfType(MT_CONFIG_TMC_FETCH);
    sendCommandRequestOfType(MT_CONFIG_HOMING_FETCH);
    sendCommandRequestOfType(MT_CONFIG_JOGGING_FETCH);
    sendCommandRequestOfType(MT_CONFIG_OVERRIDES_FETCH);
    sendCommandRequestOfType(MT_CONFIG_LOADCELL_CALIB_FETCH);
    sendCommandRequestOfType(MT_CONFIG_PROBING_FETCH);
    sendCommandRequestOfType(MT_CONFIG_ESTOP_FETCH);
}

void showRequestInProgress() {
    if ( isRequestInProgress() ) {
        ImGui::SameLine();
        ImGui::Text("Communicating with server...");
    }
}

void HelpMarker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void showHostnameSetup()
{
    ImGui::Text("Hostname:");
    ImGui::SameLine();
    ImGui::PushItemWidth(-75);
    ImGui::InputText("##serverhost", serverHostname, 255);

    ImGui::SameLine();
    if ( ImGui::Button("Connect") ) {

        g_log.log(LL_DEBUG, "Attempting connect to server: %s", serverHostname);

        stopRequester();
        stopSubscriber();

        startSubscriber();
        startRequester();

        fetchAllServerConfigs();
    }

    showRequestInProgress();
}

void showStepsPerUnitSetup()
{
    ImGui::TextWrapped("The number of motor steps required to move one unit.\n\n'Unit' is your choice of measurement (eg. millimeter, inches), just make sure to use the same units everywhere. For example if you use millimeters here, all speed values will be mm/s, acceleration will be mm/s/s etc.");

    const char* axisNames = "XYZWABCD";
    char buf[32];

    if (ImGui::BeginTable("stepsPerUnitTable", 2, ImGuiTableFlags_SizingFixedFit))
    {
        for (int i = 0; i < NUM_MOTION_AXES; i++) {
            sprintf(buf, "%c:", axisNames[i]);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            ImGui::Text(buf);

            ImGui::TableSetColumnIndex(1);

            ImGui::PushItemWidth(140);
            sprintf(buf, "##spu%c", axisNames[i]);
            const char* floatFmt = config_stepsPerUnit[i] == 0 ? "%.0f" : "%.6f";
            ImGui::InputFloat(buf, &config_stepsPerUnit[i], 0, 0, floatFmt);
        }

        ImGui::EndTable();
    }

    ImGui::NewLine();

    if ( ImGui::Button("Fetch") ) {
        sendCommandRequestOfType(MT_CONFIG_STEPS_FETCH);
    }
    ImGui::SameLine();
    if ( ImGui::Button("Save") ) {
        commandRequest_t req = createCommandRequest(MT_CONFIG_STEPS_SET);
        for (int i = 0; i < NUM_MOTION_AXES; i++) {
            req.configSteps.perUnit[i] = config_stepsPerUnit[i];
        }
        sendCommandRequest(&req);
    }

    showRequestInProgress();
}

void showWorkingAreaSetup()
{
    ImGui::TextWrapped("Working area (displayed as a green wireframe box in the 3D view). Movements will be restricted to stay within this box.\n\nThe front left top corner of the working area is the 'origin', or (0,0,0) location. The 'homed position' value in the homing settings for each axis is measured relative to the origin.\n\nUse the same units as in the 'Steps' tab!");

    if (ImGui::BeginTable("workingAreaTable", 2, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("X:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(140);
        const char* floatFmtx = config_workingAreaX == 0 ? "%.0f" : "%.3f";
        ImGui::InputFloat("##wax", &config_workingAreaX, 0, 0, floatFmtx);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Y:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(140);
        const char* floatFmty = config_workingAreaY == 0 ? "%.0f" : "%.3f";
        ImGui::InputFloat("##way", &config_workingAreaY, 0, 0, floatFmty);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Z:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(140);
        const char* floatFmtz = config_workingAreaZ == 0 ? "%.0f" : "%.3f";
        ImGui::InputFloat("##waz", &config_workingAreaZ, 0, 0, floatFmtz);

        ImGui::EndTable();
    }

    ImGui::NewLine();

    if ( ImGui::Button("Fetch") ) {
        sendCommandRequestOfType(MT_CONFIG_WORKAREA_FETCH);
    }
    ImGui::SameLine();
    if ( ImGui::Button("Save") ) {
        commandRequest_t req = createCommandRequest(MT_CONFIG_WORKAREA_SET);
        req.workingArea.x = config_workingAreaX;
        req.workingArea.y = config_workingAreaY;
        req.workingArea.z = config_workingAreaZ;
        sendCommandRequest(&req);
    }

    showRequestInProgress();
}

void showMotionLimitsSetup()
{
    ImGui::TextWrapped("Hard limits per individual axis to never be exceeded, even if commands or script call for higher values.");

    const char* axisNames = "XYZ"; // use Z for W as well
    char buf[32];

    for (int i = 0; i < 3; i++) {
        sprintf(buf, "Axis %c", axisNames[i]);

        if (ImGui::CollapsingHeader(buf))
        {
            if (ImGui::BeginTable("hardSpeedsTable", 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Velocity:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(140);
                const char* floatFmtv = machineLimits.velLimit[i] == 0 ? "%.0f" : "%.3f";
                sprintf(buf, "##hlv%d", i);
                ImGui::InputFloat(buf, &machineLimits.velLimit[i], 0, 0, floatFmtv);

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Acceleration:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(140);
                const char* floatFmta = machineLimits.accLimit[i] == 0 ? "%.0f" : "%.3f";
                sprintf(buf, "##hla%d", i);
                ImGui::InputFloat(buf, &machineLimits.accLimit[i], 0, 0, floatFmta);

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Jerk:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(140);
                const char* floatFmtj = machineLimits.jerkLimit[i] == 0 ? "%.0f" : "%.3f";
                sprintf(buf, "##hlj%d", i);
                ImGui::InputFloat(buf, &machineLimits.jerkLimit[i], 0, 0, floatFmtj);

                ImGui::EndTable();
            }
        }
    }

    if (ImGui::CollapsingHeader("Rotation"))
    {
        if (ImGui::BeginTable("hardSpeedsTableRotation", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Velocity:");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(140);
            const char* floatFmtv = machineLimits.grotationVelLimit == 0 ? "%.0f" : "%.3f";
            ImGui::InputFloat("##hlvr", &machineLimits.grotationVelLimit, 0, 0, floatFmtv);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Acceleration:");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(140);
            const char* floatFmta = machineLimits.grotationAccLimit == 0 ? "%.0f" : "%.3f";
            ImGui::InputFloat("##hlar", &machineLimits.grotationAccLimit, 0, 0, floatFmta);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Jerk:");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(140);
            const char* floatFmtj = machineLimits.grotationJerkLimit == 0 ? "%.0f" : "%.3f";
            ImGui::InputFloat("##hljr", &machineLimits.grotationJerkLimit, 0, 0, floatFmtj);

            ImGui::EndTable();
        }
    }

    ImGui::NewLine();

    ImGui::Separator();
    ImGui::TextWrapped("Initial settings to use after machine startup. These values remain in effect until modified by command lists, eg. with 'sml' and 'srl' commands.");

    ImGui::SeparatorText("Move");

    if (ImGui::BeginTable("initialSpeedsTableMove", 2, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Velocity:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(140);
        const char* floatFmtv = machineLimits.initialMoveLimitVel == 0 ? "%.0f" : "%.3f";
        ImGui::InputFloat("##msv", &machineLimits.initialMoveLimitVel, 0, 0, floatFmtv);

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Acceleration:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(140);
        const char* floatFmta = machineLimits.initialMoveLimitAcc == 0 ? "%.0f" : "%.3f";
        ImGui::InputFloat("##msa", &machineLimits.initialMoveLimitAcc, 0, 0, floatFmta);

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Jerk:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(140);
        const char* floatFmtj = machineLimits.initialMoveLimitJerk == 0 ? "%.0f" : "%.3f";
        ImGui::InputFloat("##msj", &machineLimits.initialMoveLimitJerk, 0, 0, floatFmtj);

        ImGui::EndTable();
    }

    ImGui::SeparatorText("Rotate");

    if (ImGui::BeginTable("initialSpeedsTableRotate", 2, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Velocity:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(140);
        const char* floatFmtv = machineLimits.initialRotationLimitVel == 0 ? "%.0f" : "%.3f";
        ImGui::InputFloat("##rsv", &machineLimits.initialRotationLimitVel, 0, 0, floatFmtv);

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Acceleration:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(140);
        const char* floatFmta = machineLimits.initialRotationLimitAcc == 0 ? "%.0f" : "%.3f";
        ImGui::InputFloat("##rsa", &machineLimits.initialRotationLimitAcc, 0, 0, floatFmta);

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Jerk:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(140);
        const char* floatFmtj = machineLimits.initialRotationLimitJerk == 0 ? "%.0f" : "%.3f";
        ImGui::InputFloat("##rsj", &machineLimits.initialRotationLimitJerk, 0, 0, floatFmtj);

        ImGui::EndTable();
    }

    ImGui::SeparatorText("Blending");

    if (ImGui::BeginTable("blendingTable", 2, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Max overlap:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(140);
        const char* floatFmtv = machineLimits.maxOverlapFraction == 0 ? "%.0f" : "%.3f";
        ImGui::InputFloat("##blend", &machineLimits.maxOverlapFraction, 0, 0, floatFmtv);
        ImGui::SameLine();
        HelpMarker("What fraction (0 - 0.95) of each straight section can be involved in a corner blend. This is a fraction of the time, not the distance.");

        ImGui::EndTable();
    }

    ImGui::NewLine();

    if ( ImGui::Button("Fetch") ) {
        sendCommandRequestOfType(MT_CONFIG_INITSPEEDS_FETCH);
    }

    ImGui::SameLine();
    if ( ImGui::Button("Save") ) {
        commandRequest_t req = createCommandRequest( MT_CONFIG_INITSPEEDS_SET );

        req.motionLimits.initialMoveVel =  machineLimits.initialMoveLimitVel;
        req.motionLimits.initialMoveAcc =  machineLimits.initialMoveLimitAcc;
        req.motionLimits.initialMoveJerk = machineLimits.initialMoveLimitJerk;
        req.motionLimits.initialRotateVel =  machineLimits.initialRotationLimitVel;
        req.motionLimits.initialRotateAcc =  machineLimits.initialRotationLimitAcc;
        req.motionLimits.initialRotateJerk = machineLimits.initialRotationLimitJerk;

        req.motionLimits.velLimitX = machineLimits.velLimit.x;
        req.motionLimits.velLimitY = machineLimits.velLimit.y;
        req.motionLimits.velLimitZ = machineLimits.velLimit.z;

        req.motionLimits.accLimitX = machineLimits.accLimit.x;
        req.motionLimits.accLimitY = machineLimits.accLimit.y;
        req.motionLimits.accLimitZ = machineLimits.accLimit.z;

        req.motionLimits.jerkLimitX = machineLimits.jerkLimit.x;
        req.motionLimits.jerkLimitY = machineLimits.jerkLimit.y;
        req.motionLimits.jerkLimitZ = machineLimits.jerkLimit.z;

        req.motionLimits.rotLimitVel =  machineLimits.grotationVelLimit;
        req.motionLimits.rotLimitAcc =  machineLimits.grotationAccLimit;
        req.motionLimits.rotLimitJerk = machineLimits.grotationJerkLimit;

        req.motionLimits.maxOverlapFraction = machineLimits.maxOverlapFraction;

        sendCommandRequest(&req);
    }

    showRequestInProgress();
}

void showTMCSetup()
{
    ImGui::Text("Trinamic TMC2209 driver settings (might work for TMC2208 too?)");
    ImGui::Text("Microsteps should be one of: 0, 2, 4, 8, 16, 32, 64, 128, 256");
    ImGui::Text("Current should be from 100 - 2000 mA");

    char buf[32];

    for (int i = 0; i < NUM_MOTION_AXES; i++)
    {
        sprintf(buf, "TMC driver %d", i);

        if (ImGui::CollapsingHeader(buf))
        {
            sprintf(buf, "tmcTable%d", i);
            if (ImGui::BeginTable(buf, 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                sprintf(buf, "##tmc%dm", i);
                ImGui::Text("Microstepping:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(100);
                int tmp = config_tmc[i].microsteps;
                ImGui::InputInt(buf, &tmp, 0);
                config_tmc[i].microsteps = tmp;

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                sprintf(buf, "##tmc%dc", i);
                ImGui::Text("Phase current:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(100);
                tmp = config_tmc[i].current;
                ImGui::InputInt(buf, &tmp, 0);
                ImGui::SameLine(); ImGui::Text("mA");
                config_tmc[i].current = tmp;

                ImGui::EndTable();
            }
        }
    }

    ImGui::NewLine();

    if ( ImGui::Button("Fetch") ) {
        sendCommandRequestOfType(MT_CONFIG_TMC_FETCH);
    }
    ImGui::SameLine();
    if ( ImGui::Button("Save") ) {
        commandRequest_t req = createCommandRequest( MT_CONFIG_TMC_SET );
        for (int i = 0; i < NUM_MOTION_AXES; i++) {
            req.tmcSettings.settings[i] = config_tmc[i];
        }
        sendCommandRequest(&req);
    }

    showRequestInProgress();
}

void showHomingSetup()
{
    ImGui::Text("Homing order:");
    ImGui::SameLine();
    ImGui::PushItemWidth(100);
    ImGui::InputText("##homingorder", homingOrder, 8);
    ImGui::SameLine();
    HelpMarker("Eg. zxy");

    ImGui::NewLine();

    ImGui::TextWrapped("Homing is performed by approaching the limit switch until it triggers, and then backing off. This is done twice, allowing different speeds and backoff distances.");

    const char* axisNames = "XYZW";
    char buf[32];

    for (int i = 0; i < NUM_HOMABLE_AXES; i++)
    {
        sprintf(buf, "Axis %c", axisNames[i]);

        if (ImGui::CollapsingHeader(buf))
        {
            sprintf(buf, "homingTable%d", i);
            if (ImGui::BeginTable(buf, 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                sprintf(buf, "##homing%dpin", i);
                ImGui::Text("Trigger pin:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(100);
                int tmp = config_homing[i].triggerPin;
                ImGui::InputInt(buf, &tmp, 0);
                config_homing[i].triggerPin = tmp;
                ImGui::SameLine();
                HelpMarker("Digital input pin to use for homing trigger");

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                sprintf(buf, "##homing%dstate", i);
                ImGui::Text("Trigger state:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(100);
                tmp = config_homing[i].triggerState;
                ImGui::InputInt(buf, &tmp, 0);
                config_homing[i].triggerState = tmp;
                ImGui::SameLine();
                HelpMarker("Pin state when limit switch is triggered (0 or 1)");

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                sprintf(buf, "##homing%ddir", i);
                ImGui::Text("Trigger direction:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(100);
                tmp = config_homing[i].direction;
                ImGui::InputInt(buf, &tmp, 0);
                config_homing[i].direction = tmp;
                ImGui::SameLine();
                HelpMarker("Direction to travel when homing (0 = descend, 1 = ascend)");


                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                sprintf(buf, "##homing%das1", i);
                ImGui::Text("Approach speed 1:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(100);
                float ftmp = config_homing[i].approachspeed1;
                ImGui::InputFloat(buf, &ftmp, 0);
                config_homing[i].approachspeed1 = ftmp;
                ImGui::SameLine();
                HelpMarker("Speed in units/second for initial approach. Typically this will be a faster speed to cover distance quicker.");

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                sprintf(buf, "##homing%dbo1", i);
                ImGui::Text("Backoff distance 1:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(100);
                ftmp = config_homing[i].backoffDistance1;
                ImGui::InputFloat(buf, &ftmp, 0);
                config_homing[i].backoffDistance1 = ftmp;
                ImGui::SameLine();
                HelpMarker("Distance to back off after initial approach. This should be far enough to reset the limit trigger.");


                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                sprintf(buf, "##homing%das2", i);
                ImGui::Text("Approach speed 2:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(100);
                ftmp = config_homing[i].approachspeed2;
                ImGui::InputFloat(buf, &ftmp, 0);
                config_homing[i].approachspeed2 = ftmp;
                ImGui::SameLine();
                HelpMarker("Speed in units/second for second approach. Typically this will be slower, with the aim of detecting the limit trigger position more precisely.");

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                sprintf(buf, "##homing%dbo2", i);
                ImGui::Text("Backoff distance 2:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(100);
                ftmp = config_homing[i].backoffDistance2;
                ImGui::InputFloat(buf, &ftmp, 0);
                config_homing[i].backoffDistance2 = ftmp;
                ImGui::SameLine();
                HelpMarker("Distance to back off after second approach. The final position after this backoff becomes the 'homed position' below.");

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                sprintf(buf, "##homing%dhp", i);
                ImGui::Text("Homed position:");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(100);
                ftmp = config_homing[i].homedPosition;
                ImGui::InputFloat(buf, &ftmp, 0);
                config_homing[i].homedPosition = ftmp;
                ImGui::SameLine();
                HelpMarker("Work area coordinate of final homed position (after finishing the second backoff)");

                ImGui::EndTable();
            }
        }
    }

    ImGui::NewLine();

    if ( ImGui::Button("Fetch") ) {
        sendCommandRequestOfType(MT_CONFIG_HOMING_FETCH);
    }
    ImGui::SameLine();
    if ( ImGui::Button("Save") ) {
        commandRequest_t req = createCommandRequest( MT_CONFIG_HOMING_SET );
        for (int i = 0; i < NUM_HOMABLE_AXES; i++) {
            req.homingParams.params[i] = config_homing[i];
        }
        memcpy(req.homingParams.order, homingOrder, min(sizeof(req.homingParams.order), sizeof(homingOrder)));
        sendCommandRequest(&req);
    }

    showRequestInProgress();
}

void showJogSpeedsSetup()
{
    ImGui::TextWrapped("Jog speeds, in units/second (or degrees/second for rotation)");

    const char* axisNames = "XYZWABCD";
    char buf[32];

    if (ImGui::BeginTable("jogSpeedsTable", 2, ImGuiTableFlags_SizingFixedFit))
    {
        for (int i = 0; i < NUM_MOTION_AXES; i++) {
            sprintf(buf, "%c:", axisNames[i]);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            ImGui::Text(buf);

            ImGui::TableSetColumnIndex(1);

            ImGui::PushItemWidth(140);
            sprintf(buf, "##jsp%c", axisNames[i]);
            const char* floatFmt = config_jogSpeed[i] == 0 ? "%.0f" : "%.3f";
            ImGui::InputFloat(buf, &config_jogSpeed[i], 0, 0, floatFmt);
        }
        ImGui::EndTable();
    }

    ImGui::NewLine();

    if ( ImGui::Button("Fetch") ) {
        sendCommandRequestOfType(MT_CONFIG_JOGGING_FETCH);
    }
    ImGui::SameLine();
    if ( ImGui::Button("Save") ) {
        commandRequest_t req = createCommandRequest( MT_CONFIG_JOGGING_SET );
        for (int i = 0; i < NUM_MOTION_AXES; i++) {
            req.jogParams.speed[i] = config_jogSpeed[i];
        }
        sendCommandRequest(&req);
    }

    showRequestInProgress();
}

void showEstopSetup()
{
    ImGui::TextWrapped("E-stop will immediately abort any ongoing scripts, which may leave some outputs in an undesirable state. Specify here how outputs should behave on e-stop.");

    char buf[32];
    char popupName[32];

    ImGui::SeparatorText("Digital outputs");

    if (ImGui::BeginTable("estopDigitalTable", 2, ImGuiTableFlags_SizingFixedFit))
    {
        for (int i = 0; i < 16; i++) {
            sprintf(buf, "%d:", i);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            ImGui::Text(buf);

            ImGui::TableSetColumnIndex(1);

            ImGui::PushItemWidth(140);

            if ( config_estopDigitalOutUsed & (1 << i) ) {
                if ( config_estopDigitalOutState & (1 << i) )
                    sprintf(buf, "high##estopdig%d", i);
                else
                    sprintf(buf, "low##estopdig%d", i);
            }
            else
                sprintf(buf, "no change##estopdig%d", i);

            sprintf(popupName, "estopdig%d", i);

            if (ImGui::Button(buf))
                ImGui::OpenPopup(popupName);
            if (ImGui::BeginPopup(popupName)) {
                if (ImGui::Selectable("no change")) {
                    config_estopDigitalOutState &= ~(1 << i);
                    config_estopDigitalOutUsed  &= ~(1 << i);
                }
                if (ImGui::Selectable("low")) {
                    config_estopDigitalOutState &= ~(1 << i);
                    config_estopDigitalOutUsed  |=  (1 << i);
                }
                if (ImGui::Selectable("high")) {
                    config_estopDigitalOutState |=  (1 << i);
                    config_estopDigitalOutUsed  |=  (1 << i);
                }
                ImGui::EndPopup();
            }
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("PWM outputs");

    if (ImGui::BeginTable("estopPWMTable", 2, ImGuiTableFlags_SizingFixedFit))
    {
        for (int i = 0; i < NUM_PWM_VALS; i++) {
            sprintf(buf, "PWM %d:", i);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            ImGui::Text(buf);

            ImGui::TableSetColumnIndex(1);

            ImGui::PushItemWidth(140);

            if ( config_estopPWMUsed & (1 << i) )
                sprintf(buf, "set to##estoppwm%d", i);
            else
                sprintf(buf, "no change##estoppwm%d", i);

            sprintf(popupName, "estoppwmused%d", i);

            if (ImGui::Button(buf))
                ImGui::OpenPopup(popupName);
            if (ImGui::BeginPopup(popupName)) {
                if (ImGui::Selectable("no change")) {
                    config_estopPWMUsed  &= ~(1 << i);
                }
                if (ImGui::Selectable("set to")) {
                    config_estopPWMUsed |=  (1 << i);
                }
                ImGui::EndPopup();
            }

            if ( config_estopPWMUsed & (1 << i) ) {
                ImGui::SameLine();

                ImGui::PushItemWidth(120);
                sprintf(buf, "##estoppwmval%d", i);
                float tmpf = config_estopPWMState[i];
                const char* floatFmt = tmpf == 0 ? "%.0f" : "%.3f";
                ImGui::InputFloat(buf, &tmpf, 0, 0, floatFmt);
                if ( tmpf < 0 ) tmpf = 0;
                if ( tmpf > 1 ) tmpf = 1;
                config_estopPWMState[i] = tmpf;
            }
        }
        ImGui::EndTable();
    }

    ImGui::NewLine();

    if ( ImGui::Button("Fetch") ) {
        sendCommandRequestOfType(MT_CONFIG_ESTOP_FETCH);
    }
    ImGui::SameLine();
    if ( ImGui::Button("Save") ) {
        commandRequest_t req = createCommandRequest( MT_CONFIG_ESTOP_SET );
        req.estopParams.outputs = config_estopDigitalOutState;
        req.estopParams.outputsUsed = config_estopDigitalOutUsed;
        for (int i = 0; i < NUM_MOTION_AXES; i++) {
            req.estopParams.pwmVal[i] = config_estopPWMState[i];
        }
        req.estopParams.pwmUsed = config_estopPWMUsed;
        sendCommandRequest(&req);
    }

    showRequestInProgress();
}

void showOutputOverrides() {

    showOverridesView_content();

    ImGui::NewLine();

    if ( ImGui::Button("Fetch") ) {
        sendCommandRequestOfType(MT_CONFIG_OVERRIDES_FETCH);
    }
    ImGui::SameLine();
    if ( ImGui::Button("Save") ) {
        sendPackable(MT_CONFIG_OVERRIDES_SET, overrideConfigSet);
    }

    showRequestInProgress();
}

extern clientReport_t lastStatusReport;
void showLoadcellCalib() {

    ImGui::TextWrapped("Calibrating the loadcell will let you see readings in real world units, eg. grams. Any non-zero load signifies a contact so this is not really necessary just for height probing.\n\nTo calibrate, first ensure there is no weight on the load cell, click 'establish baseline' and wait a few seconds. Then place a known weight on the load cell, enter it here and and click 'calibrate'.");

    ImGui::NewLine();

    if ( ImGui::Button("Establish baseline") ) {
        commandRequest_t req = createCommandRequest(MT_RESET_LOADCELL);
        sendCommandRequest(&req);
    }

    ImGui::NewLine();

    ImGui::Text("Known weight:");
    ImGui::SameLine();
    ImGui::PushItemWidth(100);
    ImGui::InputFloat("##loadcellweight", &config_loadcellCalibrationWeight );

    ImGui::SameLine();

    if ( ImGui::Button("Calibrate") ) {
        commandRequest_t req = createCommandRequest(MT_CONFIG_LOADCELL_CALIB_SET);
        //req.loadcellCalib.rawOffset = config_loadcellCalibrationRawOffset;
        req.loadcellCalib.weight = config_loadcellCalibrationWeight;
        sendCommandRequest(&req);
    }
    showRequestInProgress();

    ImGui::NewLine();

    ImGui::Text("Result weight: %f", lastStatusReport.weight);
}

extern probingResult_e lastProbingResult;
extern float lastProbedHeight;

void showProbeSetup() {
    // digital
    //     which pin
    //     which state of the pin triggers
    //         approach speed 1, backoff dist, approach speed 2
    // load
    //     approach speed 1, backoff dist, approach speed 2
    // vacuum
    // (measure leakage with open nozzle)
    // step height

    if (ImGui::CollapsingHeader("Digital input"))
    {
        if (ImGui::BeginTable("probedigital", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Trigger pin:");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(100);
            int tmp = config_probing.digitalTriggerPin;
            ImGui::InputInt("##probedigitaltriggerpin", &tmp, 0);
            config_probing.digitalTriggerPin = tmp;
            ImGui::SameLine();
            HelpMarker("Digital input pin to use for probing trigger");

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Trigger state:");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(100);
            tmp = config_probing.digitalTriggerState;
            ImGui::InputInt("##probedigitaltriggerstate", &tmp, 0);
            config_probing.digitalTriggerState = tmp;
            ImGui::SameLine();
            HelpMarker("Pin state when probe is triggered (0 or 1)");

            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Vacuum sensor"))
    {
        if (ImGui::BeginTable("probevacuum", 2, ImGuiTableFlags_SizingFixedFit))
        {

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Vacuum pin:");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(100);
            int tmp = config_probing.vacuumSniffPin;
            ImGui::InputInt("##probesniffpin", &tmp, 0);
            config_probing.vacuumSniffPin = tmp;
            ImGui::SameLine();
            HelpMarker("Digital output pin to open vacuum to nozzle");

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Vacuum pin state:");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(100);
            tmp = config_probing.vacuumSniffState;
            ImGui::InputInt("##probesniffstate", &tmp, 0);
            config_probing.vacuumSniffState = tmp;
            ImGui::SameLine();
            HelpMarker("Pin state when nozzle valve is open (0 or 1)");

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Sniff time (ms):");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(100);
            tmp = config_probing.vacuumSniffTimeMs;
            ImGui::InputInt("##probesnifftime", &tmp, 0);
            config_probing.vacuumSniffTimeMs = tmp;
            ImGui::SameLine();
            HelpMarker("Time to open nozzle and measure vacuum change");

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Replenish time (ms):");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(100);
            tmp = config_probing.vacuumReplenishTimeMs;
            ImGui::InputInt("##probereplenishtime", &tmp, 0);
            config_probing.vacuumReplenishTimeMs = tmp;
            ImGui::SameLine();
            HelpMarker("Wait time between sniffs (if necessary to allow vacuum level to recover)");

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Step distance:");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(100);
            float tmpf = config_probing.vacuumStep;
            ImGui::InputFloat("##probevacuumstep", &tmpf, 0);
            config_probing.vacuumStep = tmpf;
            ImGui::SameLine();
            HelpMarker("Distance to advance between sniffs");

            ImGui::EndTable();
        }
    }

    ImGui::NewLine();
    ImGui::SeparatorText("Speeds and distances");

    if (ImGui::BeginTable("probespeeds", 2, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Approach speed 1:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(100);
        float ftmp = config_probing.approachspeed1;
        ImGui::InputFloat("##probingapproach1", &ftmp, 0);
        config_probing.approachspeed1 = ftmp;
        ImGui::SameLine();
        HelpMarker("Speed in units/second for initial approach. Typically this will be a faster speed to cover distance quicker.");

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Backoff distance 1:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(100);
        ftmp = config_probing.backoffDistance1;
        ImGui::InputFloat("##probingbackoff1", &ftmp, 0);
        config_probing.backoffDistance1 = ftmp;
        ImGui::SameLine();
        HelpMarker("Distance to back off after initial approach. This should be far enough to reset the limit trigger.");

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Approach speed 2:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(100);
        ftmp = config_probing.approachspeed2;
        ImGui::InputFloat("##probingapproach2", &ftmp, 0);
        config_probing.approachspeed2 = ftmp;
        ImGui::SameLine();
        HelpMarker("Speed in units/second for second approach. Typically this will be slower, with the aim of detecting the limit trigger position more precisely.");

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Backoff distance 2:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(100);
        ftmp = config_probing.backoffDistance2;
        ImGui::InputFloat("##probingbackoff2", &ftmp, 0);
        config_probing.backoffDistance2 = ftmp;
        ImGui::SameLine();
        HelpMarker("Distance to back off after second approach. This can be zero if to remain at the triggered position.");

        ImGui::EndTable();
    }

    ImGui::NewLine();

    if ( ImGui::Button("Fetch") ) {
        sendCommandRequestOfType(MT_CONFIG_PROBING_FETCH);
    }
    ImGui::SameLine();
    if ( ImGui::Button("Save") ) {
        commandRequest_t req = createCommandRequest( MT_CONFIG_PROBING_SET );
        req.probingParams.params = config_probing;
        sendCommandRequest(&req);
    }

    showRequestInProgress();

    ImGui::NewLine();
    ImGui::SeparatorText("Test probe");

    static float probeZ = -8;
    static float probeMinWeight = 0;

    ImGui::TextWrapped("You can use the buttons below to test each type of probe.");

    if (ImGui::BeginTable("probedigital", 2, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Lowest Z:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(100);
        ImGui::InputFloat("##testprobez", &probeZ, 0);
        ImGui::SameLine();
        HelpMarker("Destination Z extent of probing");

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Min weight:");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(100);
        ImGui::InputFloat("##testprobeminweight", &probeMinWeight, 0);
        ImGui::SameLine();
        HelpMarker("For load cell probing, contact will be detected when weight exceeds this value, eg. set zero for any contact");

        ImGui::EndTable();
    }

    int doProbeType = -1;

    if ( ImGui::Button("Digital input##asdf") ) {
        doProbeType = PT_DIGITAL;
    }
    ImGui::SameLine();
    if ( ImGui::Button("Load cell") ) {
        doProbeType = PT_LOADCELL;
    }
    ImGui::SameLine();
    if ( ImGui::Button("Vacuum") ) {
        doProbeType = PT_VACUUM;
    }

    if ( doProbeType != -1 ) {
        commandRequest_t req = createCommandRequest(MT_PROBE);
        req.probe.type = doProbeType;
        req.probe.z = probeZ;
        req.probe.minWeight = probeMinWeight;
        sendCommandRequest(&req);
        lastProbingResult = PR_NONE;
        lastProbedHeight = 0;
    }

    if ( lastProbingResult == PR_SUCCESS ) {
        ImGui::Text("Last probe result: %s, height = %f", getProbingResultName(lastProbingResult), lastProbedHeight);
    }
    else
        ImGui::Text("Last probe result: %s", getProbingResultName(lastProbingResult));

}

void showServerView(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(480, 480), ImGuiCond_FirstUseEver);

    doLayoutLoad(SERVER_WINDOW_TITLE);

    static int currentTabIndex = -1;

    ImGui::Begin(SERVER_WINDOW_TITLE, p_open);
    {
        if (ImGui::BeginTabBar("servertabs", ImGuiTabBarFlags_None))
        {
            int whichTab = 0;
            int oldTabIndex = currentTabIndex;
            if (ImGui::BeginTabItem("Hostname"))
            {
                currentTabIndex = whichTab;
                showHostnameSetup();
                ImGui::EndTabItem();                
            }
            whichTab++;
            if (ImGui::BeginTabItem("Steps"))
            {
                currentTabIndex = whichTab;
                if ( currentTabIndex != oldTabIndex )
                    sendCommandRequestOfType(MT_CONFIG_STEPS_FETCH);
                showStepsPerUnitSetup();
                ImGui::EndTabItem();
            }
            whichTab++;
            if (ImGui::BeginTabItem("Work area"))
            {
                currentTabIndex = whichTab;
                if ( currentTabIndex != oldTabIndex )
                    sendCommandRequestOfType(MT_CONFIG_WORKAREA_FETCH);
                showWorkingAreaSetup();
                ImGui::EndTabItem();
            }
            whichTab++;
            if (ImGui::BeginTabItem("TMC"))
            {
                currentTabIndex = whichTab;
                if ( currentTabIndex != oldTabIndex )
                    sendCommandRequestOfType(MT_CONFIG_TMC_FETCH);
                showTMCSetup();
                ImGui::EndTabItem();
            }
            whichTab++;
            if (ImGui::BeginTabItem("Motion limits"))
            {
                currentTabIndex = whichTab;
                if ( currentTabIndex != oldTabIndex )
                    sendCommandRequestOfType(MT_CONFIG_INITSPEEDS_FETCH);
                showMotionLimitsSetup();
                ImGui::EndTabItem();
            }
            whichTab++;
            if (ImGui::BeginTabItem("Homing"))
            {
                currentTabIndex = whichTab;
                if ( currentTabIndex != oldTabIndex )
                    sendCommandRequestOfType(MT_CONFIG_HOMING_FETCH);
                showHomingSetup();
                ImGui::EndTabItem();
            }
            whichTab++;
            if (ImGui::BeginTabItem("Jogging"))
            {
                currentTabIndex = whichTab;
                if ( currentTabIndex != oldTabIndex )
                    sendCommandRequestOfType(MT_CONFIG_JOGGING_FETCH);
                showJogSpeedsSetup();
                ImGui::EndTabItem();
            }
            whichTab++;
            if (ImGui::BeginTabItem("E-stop"))
            {
                currentTabIndex = whichTab;
                if ( currentTabIndex != oldTabIndex )
                    sendCommandRequestOfType(MT_CONFIG_ESTOP_FETCH);
                showEstopSetup();
                ImGui::EndTabItem();
            }
            whichTab++;
            if (ImGui::BeginTabItem("Overrides"))
            {
                currentTabIndex = whichTab;
                if ( currentTabIndex != oldTabIndex )
                    sendCommandRequestOfType(MT_CONFIG_OVERRIDES_FETCH);
                showOutputOverrides();
                ImGui::EndTabItem();
            }
            whichTab++;
            if (ImGui::BeginTabItem("Load cell"))
            {
                currentTabIndex = whichTab;
                if ( currentTabIndex != oldTabIndex )
                    sendCommandRequestOfType(MT_CONFIG_LOADCELL_CALIB_FETCH);
                showLoadcellCalib();
                ImGui::EndTabItem();
            }
            whichTab++;
            if (ImGui::BeginTabItem("Probing"))
            {
                currentTabIndex = whichTab;
                if ( currentTabIndex != oldTabIndex )
                    sendCommandRequestOfType(MT_CONFIG_PROBING_FETCH);
                showProbeSetup();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    doLayoutSave(SERVER_WINDOW_TITLE);

    ImGui::End();
}

void initDefaultConfigs()
{
    homingOrder[0] = 0;
}
