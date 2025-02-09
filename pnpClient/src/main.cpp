
// Visualization for scv planner using Dear ImGui, OpenGL2 implementation.

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl2.h"
#include "imgui_notify/imgui_notify.h"
#include <sqlite3.h>
#include <assimp/version.h>
#include <stdio.h>
#include <zmq.h>
#include <ZXing/Version.h>
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>

#include <assimp/cimport.h>

#include <thread>
#include <fstream>
#include <iomanip>

#include <chrono>
#include "implot.h"
#include "scv/planner.h"

#include "version.h"
#include "camera.h"

#include "pnpMessages.h"
#include "net_subscriber.h"
#include "net_requester.h"

#include "commandEditorWindow.h"
#include "scriptEditorWindow.h"
#include "commandlist.h"
#include "commandlist_parse.h"

#include "model.h"
#include "log.h"

#include "usbcamera.h"
#include "videoView.h"
#include "overrides_view.h"
#include "eventhooks.h"

#include "script/engine.h"
#include "db.h"

#include "codeEditorWindow.h"

//#include "ImGuiFileDialog.h"
#include "nfd.h"

#include "preview.h"
#include "machinelimits.h"

#include "overrides.h"

#include "scriptexecution.h"
#include "custompanel.h"
#include "tweakspanel.h"

#include "vision.h"

#include "workspace.h"
#include "server_view.h"

#include "image.h"

#include "serialPortInfo.h"
#include "serial_view.h"

#include "tableView.h"

#include "config.h"

#include "notify.h"
#include "feedback.h"

using namespace std;
using namespace scv;

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

#define NUM_ROTATION_AXES 4

GLFWwindow* window;

Camera camera;

ImFont* font_proggy = NULL;
ImFont* font_ubuntuMono = NULL;
ImFont* font_sourceCodePro = NULL;

#define STATUS_WINDOW_TITLE "Status"

// These are to cycle between red/green/blue when drawing segments to differentiate them
vec3 colors[] = {
    vec3(0.9f, 0.3f, 0.3f),
    vec3(0.3f, 0.9f, 0.3f),
    vec3(0.3f, 0.3f, 0.9f)
};

vec3 modelOffset = vec3(350,250, -26);
vec3 gantryOffset = vec3(-156.86,0,38);
vec3 xcarriageOffset = vec3(0,0,37.4);
vec3 zcarriageOffset = vec3(0,0,0);
vec3 tweak = vec3_zero;
scv::vec3 lightPos1 = vec3(-360, -700, 220);
scv::vec3 lightPos2 = vec3(471, 420, 730);
scv::vec3 lightPos3 = vec3(100, 680, 230);
bool enableLight1 = true;
bool enableLight2 = true;
bool enableLight3 = true;

// Draw 3 simple lines to show the world origin location
void drawAxes() {

    glLineWidth(4);
    glBegin(GL_LINES);

    glColor3f(1,0,0);
    glVertex3f(0,0,0);
    glVertex3f(10,0,0);

    glColor3f(0,1,0);
    glVertex3f(0,0,0);
    glVertex3f(0,10,0);

    glColor3f(0,0,1);
    glVertex3f(0,0,0);
    glVertex3f(0,0,10);

    glEnd();
}

// Draw a simple bounding box to show the position constraints of the planner
void drawPlannerBoundingBox() {
    glLineWidth(2);
    glColor3f(0,0.66,0);
    glBegin(GL_LINE_LOOP);
    glVertex3d( machineLimits.posLimitLower.x, machineLimits.posLimitLower.y, machineLimits.posLimitLower.z );
    glVertex3d( machineLimits.posLimitUpper.x, machineLimits.posLimitLower.y, machineLimits.posLimitLower.z );
    glVertex3d( machineLimits.posLimitUpper.x, machineLimits.posLimitUpper.y, machineLimits.posLimitLower.z );
    glVertex3d( machineLimits.posLimitLower.x, machineLimits.posLimitUpper.y, machineLimits.posLimitLower.z );
    glVertex3d( machineLimits.posLimitLower.x, machineLimits.posLimitUpper.y, machineLimits.posLimitUpper.z );
    glVertex3d( machineLimits.posLimitUpper.x, machineLimits.posLimitUpper.y, machineLimits.posLimitUpper.z );
    glVertex3d( machineLimits.posLimitUpper.x, machineLimits.posLimitLower.y, machineLimits.posLimitUpper.z );
    glVertex3d( machineLimits.posLimitLower.x, machineLimits.posLimitLower.y, machineLimits.posLimitUpper.z );
    glEnd();
    glBegin(GL_LINES);
    glVertex3d( machineLimits.posLimitUpper.x, machineLimits.posLimitLower.y, machineLimits.posLimitLower.z );
    glVertex3d( machineLimits.posLimitUpper.x, machineLimits.posLimitLower.y, machineLimits.posLimitUpper.z );
    glVertex3d( machineLimits.posLimitUpper.x, machineLimits.posLimitUpper.y, machineLimits.posLimitLower.z );
    glVertex3d( machineLimits.posLimitUpper.x, machineLimits.posLimitUpper.y, machineLimits.posLimitUpper.z );
    glVertex3d( machineLimits.posLimitLower.x, machineLimits.posLimitLower.y, machineLimits.posLimitLower.z );
    glVertex3d( machineLimits.posLimitLower.x, machineLimits.posLimitUpper.y, machineLimits.posLimitLower.z );
    glVertex3d( machineLimits.posLimitLower.x, machineLimits.posLimitLower.y, machineLimits.posLimitUpper.z );
    glVertex3d( machineLimits.posLimitLower.x, machineLimits.posLimitUpper.y, machineLimits.posLimitUpper.z );
    glEnd();
}

// These arrays will be used to draw the plots
#define MAXPLOTPOINTS 1000
float plotTime[MAXPLOTPOINTS]; // x axis of the plot, all others are y-axis values
float plotPosX[MAXPLOTPOINTS], plotPosY[MAXPLOTPOINTS], plotPosZ[MAXPLOTPOINTS];
float plotVac[MAXPLOTPOINTS];
float plotLoad[MAXPLOTPOINTS];
float plotWeight[MAXPLOTPOINTS];
float plotRot0[MAXPLOTPOINTS];
float plotVelX[MAXPLOTPOINTS], plotVelY[MAXPLOTPOINTS], plotVelZ[MAXPLOTPOINTS];
float plotAccX[MAXPLOTPOINTS], plotAccY[MAXPLOTPOINTS], plotAccZ[MAXPLOTPOINTS];
float plotVelMag[MAXPLOTPOINTS], plotAccMag[MAXPLOTPOINTS], plotJerkMag[MAXPLOTPOINTS];
int numPlotPoints = 0; // how many plot points are actually filled
/*
// These are used to get a moving average of the time taken to calculate the full trajectory
#define NUMCALCTIMES 64
float calcTimes[NUMCALCTIMES];
float calcTimeTotal = 0;
int calcTimeInd = 0;
float calcTime = 0; // final result to show in GUI
*/
float animAdvance = 0;  // used to animate a white dot moving along the path
//vec3 animLoc;
//float animRots[4] = {0};
bool showBoundingBox = true;
bool showControlPoints = true;
bool showPreviewEventsLabels = true;

bool haveViolation = false;

vec3 screenPos = vec3_zero;

// bool violation(vec3& p, vec3& j, vec3& dp, vec3& dv, vec3& da, vec3& dj) {
//     double margin = 1.0001;
//     if ( p.x < plan.posLimitLower.x * margin ) return true;
//     if ( p.y < plan.posLimitLower.y * margin ) return true;
//     if ( p.z < plan.posLimitLower.z * margin ) return true;
//     if ( p.x > plan.posLimitUpper.x * margin ) return true;
//     if ( p.y > plan.posLimitUpper.y * margin ) return true;
//     if ( p.z > plan.posLimitUpper.z * margin ) return true;
//     if ( fabs(dp.x) > plan.velLimit.x * margin ) return true;
//     if ( fabs(dp.y) > plan.velLimit.y * margin ) return true;
//     if ( fabs(dp.z) > plan.velLimit.z * margin ) return true;
//     if ( fabs(dv.x) > plan.accLimit.x * margin ) return true;
//     if ( fabs(dv.y) > plan.accLimit.y * margin ) return true;
//     if ( fabs(dv.z) > plan.accLimit.z * margin ) return true;
//     if ( fabs(da.x) > plan.jerkLimit.x * margin ) return true;
//     if ( fabs(da.y) > plan.jerkLimit.y * margin ) return true;
//     if ( fabs(da.z) > plan.jerkLimit.z * margin ) return true;
//     if ( fabs(j.x) > plan.jerkLimit.x * margin ) return true;
//     if ( fabs(j.y) > plan.jerkLimit.y * margin ) return true;
//     if ( fabs(j.z) > plan.jerkLimit.z * margin ) return true;
//     return false;
// }

// vec3 lastActualPos = vec3_zero;
// vec3 lastActualVel = vec3_zero;
// float lastActualRots[4] = {0};
motionLimits lastActualMoveLimits;
motionLimits lastActualRotateLimits;//[NUM_ROTATION_AXES];
float lastActualSpeedScale = 1;
float lastActualJogSpeedScale = 1;

cornerBlendMethod_e lastActualCornerBlendMethod = CBM_INTERPOLATED_MOVES;

//float animSpeedScale = 1;

int graphIndex = 0;

float fovy = 50;
float nearPlaneDist = 1;
float farPlaneDist = 2000;
float worldHeightOfViewplane = 2 * tan(fovy*0.5f*DEGTORAD) * nearPlaneDist;

int frameBufferWidth, frameBufferHeight;
float aspectRatio;

bool getScreenPos(vec3 worldPos, ImVec2& screenPos) {

    vec3 cameraToPoint = worldPos - camera.location;
    vec3 nearPlaneCenterInWorld = camera.location + nearPlaneDist * camera.forward;

    float dot1 = scv::dot(camera.forward, camera.location);
    float dot2 = scv::dot(camera.forward, nearPlaneCenterInWorld);
    float dot3 = scv::dot(camera.forward, worldPos);

    if ( (dot3 < dot2 && dot1 < dot2) || (dot3 > dot2 && dot1 > dot2)) {
        return false;
    }

    float fracToNearPlane = (dot2 - dot1) / (dot3 - dot1);

    vec3 pointOnNearPlane = camera.location + fracToNearPlane * cameraToPoint;
    vec3 projOnNearPlane = pointOnNearPlane - nearPlaneCenterInWorld;

    float rightDot = scv::dot(projOnNearPlane, camera.right);
    float upDot = scv::dot(projOnNearPlane, camera.up);

    float worldWidthOfViewplane = worldHeightOfViewplane * aspectRatio;

    screenPos.x = frameBufferWidth/2 + frameBufferWidth * rightDot / worldWidthOfViewplane;
    screenPos.y = frameBufferHeight/2 - frameBufferHeight * upDot / worldHeightOfViewplane;

    return true;
}

void showPointLabel( ImDrawList* bgdl, vec3 worldPos, string &text, ImColor pointColor, ImColor textColor ) {
    ImVec2 screenPos;
    if ( getScreenPos( worldPos, screenPos ) ) {
        bgdl->AddCircleFilled(screenPos, 5, pointColor);
        screenPos.x += 10;
        screenPos.y -= ImGui::GetFontSize() / 2;


        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        //float fontSize = ImGui::GetFontSize();

        float margin = 1;
        ImVec2 bgRectTL(screenPos.x - margin, screenPos.y - margin);
        ImVec2 bgRectBR(screenPos.x + textSize.x + margin, screenPos.y + textSize.y + margin);
        bgdl->AddRectFilled(bgRectTL, bgRectBR, 0x55000000);

        bgdl->AddText(screenPos, textColor, text.c_str());
    }
}



void showTraverseEventLabels(ImDrawList* bgdl)
{
    for (int i = 0; i < (int)traverseLabels.size(); i++) {
        traverseEventLabel_t& el = traverseLabels[i];
        showPointLabel( bgdl, el.pos, el.text, ImColor(192,255,0,255), ImColor(255,255,255,255) );
    }
}

void* mainModel = NULL;
void* gantryModel = NULL;
void* xcarriageModel = NULL;
void* zcarriageModel = NULL;
void* nozzleModel = NULL;
void* nozzleTipModel = NULL;

// On Ubuntu the background shows crazy flickering garbage during load unless
// it is actively cleared
void backgroundRenderCallback_loading(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// This function will be called during ImGui's rendering, while drawing the background.
// Draw all our stuff here so the GUI windows will then be over the top of it. Need to
// re-enable scissor test after we're done.
void backgroundRenderCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {

    haveViolation = false;

    // calculate the trajectory every frame, and measure the time taken
    //std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    //plan.calculateMoves();
    //std::chrono::steady_clock::time_point t1 =   std::chrono::steady_clock::now();
/*
    // update the moving average
    long long timeTaken = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    calcTimeTotal -= calcTimes[calcTimeInd];
    calcTimeTotal += timeTaken;
    calcTimes[calcTimeInd] = timeTaken;
    calcTimeInd = (calcTimeInd + 1) % NUMCALCTIMES;
    calcTime = 0;
    for (int i = 0; i < NUMCALCTIMES; i++) {
        calcTime += calcTimes[i];
    }
    calcTime /= NUMCALCTIMES;
*/
    // apply view transforms
    glfwGetFramebufferSize(window, &frameBufferWidth, &frameBufferHeight);
    aspectRatio = frameBufferWidth / (float)frameBufferHeight;

    glBindTexture(GL_TEXTURE_2D, 0); // webcam view binds texture, we want none

    glDisable(GL_SCISSOR_TEST);

    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluPerspective( fovy, aspectRatio, nearPlaneDist, farPlaneDist );

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    camera.gluLookAt();


    glEnable(GL_DEPTH_TEST);
/*
    if ( showBoundingBox ) {
        drawPlannerBoundingBox();
        glDepthFunc(GL_ALWAYS);
        drawAxes();
        glDepthFunc(GL_LESS);
    }
*/

    vec3 tmpV;
    traverseFeedback_t traverseFeedback;
    planGroup_preview.advanceTraverse( animAdvance, animSpeedScale, &animLoc, &tmpV, animRots, &traverseFeedback );

    if ( animRots[0] != 0 ) {
        int adf = 4;
        adf++;
    }

    enableModelRenderState();

    float scale = 1000;

    zcarriageOffset.x = lastActualPos.x + 17.2;
    zcarriageOffset.y = lastActualPos.y + 19;
    zcarriageOffset.z = lastActualPos.z + 48;

    xcarriageOffset.x = zcarriageOffset.x + 32;
    xcarriageOffset.y = zcarriageOffset.y + -10.6;

    gantryOffset.y = xcarriageOffset.y + -5.5;

    glPushMatrix();
    glTranslatef( modelOffset.x, modelOffset.y, modelOffset.z );
    glScalef(scale, scale, scale);
    renderModel(mainModel);
    glPopMatrix();

    glPushMatrix();
    glTranslatef( modelOffset.x + gantryOffset.x, gantryOffset.y, modelOffset.z + gantryOffset.z );
    glScalef(scale, scale, scale);
    renderModel(gantryModel);
    glPopMatrix();

    glPushMatrix();
    glTranslatef( xcarriageOffset.x, xcarriageOffset.y, modelOffset.z + xcarriageOffset.z );
    glScalef(scale, scale, scale);
    renderModel(xcarriageModel);
    glPopMatrix();

    glPushMatrix();
    glTranslatef( zcarriageOffset.x, zcarriageOffset.y, zcarriageOffset.z );
    glScalef(scale, scale, scale);
    renderModel(zcarriageModel);
    glPopMatrix();

    glPushMatrix();
    glTranslatef( lastActualPos.x, lastActualPos.y, lastActualPos.z );
    glScalef(scale, scale, scale);
    glRotatef(lastActualRots[0], 0, 0, 1);
    renderModel(nozzleModel);
    glPopMatrix();

    glPushMatrix();
    glTranslatef( animLoc.x, animLoc.y, animLoc.z );
    glScalef(scale, scale, scale);
    glRotatef(animRots[0], 0, 0, 1);
    renderModel(nozzleTipModel);
    glPopMatrix();

    // prep for imgui
    glDisable(GL_LIGHTING);
    glDisable(GL_SCISSOR_TEST);

    //glEnable(GL_TEXTURE_2D);





    // required for imgui
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);


    vec3 vp;

    numPlotPoints = 0;

    if ( previewStyle == PS_LINES ) {
        glLineWidth(3);
        glBegin(GL_LINES);
        for (int i = 1; i < (int)traversePoints.size(); i++) {
            traversePoint_t& tp0 = traversePoints[i-1];
            traversePoint_t& tp1 = traversePoints[i];
            vec3 c = getPreviewColorFromSpeed(tp1.vel);
            glColor3f( c.x, c.y, c.z );
            glVertex3f( tp0.pos.x, tp0.pos.y, tp0.pos.z );
            glVertex3f( tp1.pos.x, tp1.pos.y, tp1.pos.z );
        }
        glEnd();
    }
    else {
        glColor3f(0,0.5,0);
        glPointSize(4);
        glBegin(GL_POINTS);
        for (int i = 0; i < (int)traversePoints.size(); i++) {
            traversePoint_t& tp = traversePoints[i];
            glVertex3f( tp.pos.x, tp.pos.y, tp.pos.z );
        }
        glEnd();
    }

    glEnd();

    //numPlotPoints = scv::min(count, MAXPLOTPOINTS);

    //    if ( showControlPoints ) {
    //        glPointSize(8);
    //        glColor3f(1,0,1);
    //        glBegin(GL_POINTS);
    //        for (size_t i = 0; i < plan.moves.size(); i++) {
    //            scv::move& m = plan.moves[i];
    //            if ( i == 0 )
    //                glVertex3d( m.src.x, m.src.y, m.src.z );
    //            glVertex3d( m.dst.x, m.dst.y, m.dst.z );
    //        }
    //        glEnd();
    //    }

    //    glPointSize(12);
    //    if ( animRunning )
    //        glColor3f(1,1,1);
    //    else
    //        glColor3f(0.5,0.5,0.5);
    //    glBegin(GL_POINTS);
    //    glVertex3d( animLoc.x, animLoc.y, animLoc.z );
    //    glEnd();

    //    glColor3f(0,1,0);
    //    glBegin(GL_POINTS);
    //    glVertex3d( lastActualPos.x, lastActualPos.y, lastActualPos.z );
    //    glEnd();

    //    if ( haveViolation ) {
    //        glPointSize(12);
    //        glColor3f(1,0,0);
    //        glBegin(GL_POINTS);
    //        glVertex3d( vp.x, vp.y, vp.z );
    //        glEnd();
    //    }

    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glEnable(GL_SCISSOR_TEST);
}

// A convenience function to show vec3 components as individual inputs
void showVec3Editor(const char* label, vec3 *v) {
    char n[128];
    sprintf(n, "%s X", label);
    ImGui::InputFloat(n, &v->x, 0.1f, 1.0f);
    sprintf(n, "%s Y", label);
    ImGui::InputFloat(n, &v->y, 0.1f, 1.0f);
    sprintf(n, "%s Z", label);
    ImGui::InputFloat(n, &v->z, 0.1f, 1.0f);
}

void showVec3Slider(const char* label, vec3 *v) {
    char n[128];
    sprintf(n, "%s X", label);
    ImGui::SliderFloat(n, &v->x, -1000, 1000);
    sprintf(n, "%s Y", label);
    ImGui::SliderFloat(n, &v->y, -1000, 1000);
    sprintf(n, "%s Z", label);
    ImGui::SliderFloat(n, &v->z, -1000, 1000);
}

int shift = 250;

ImPlotPoint MyDataGetter(int idx, void* data) {
    float* vals = (float*)data;
    ImPlotPoint p;
    p.x = idx * 0.005;
    idx = (idx + graphIndex) % MAXPLOTPOINTS;
    p.y = vals[idx];
    return p;
}

/*
void randomizePoints() {

    scv::vec3 lastRandPos;

    for (size_t i = 0; i < plan.moves.size(); i++) {
        scv::move& m = plan.moves[i];

        if ( i > 0 )
            m.src = lastRandPos;

        scv::vec3 r;

        if ( i == 0 ) {
            r = scv::vec3(rand() / (float)RAND_MAX, rand() / (float)RAND_MAX, rand() / (float)RAND_MAX);
            r = r * plan.posLimitUpper;
            r += plan.posLimitLower;
            m.src = r;
        }

        r = scv::vec3(rand() / (float)RAND_MAX, rand() / (float)RAND_MAX, rand() / (float)RAND_MAX);
        r = r * plan.posLimitUpper;
        r += plan.posLimitLower;
        m.dst = r;

        lastRandPos = r;

    }
}
*/







static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}
/*
void loadTestCase_default() {
    plan.clear();
    plan.setPositionLimits(0, 0, 0, 10, 10, 7);

    scv::move m;
    m.blendClearance = 0;
    m.vel = 12;
    m.acc = 400;
    m.jerk = 800;
    m.blendType = CBT_MAX_JERK;
    m.src = vec3( 1, 1, 0);
    m.dst = vec3( 1, 1, 6);    plan.appendMove(m);
    m.dst = vec3( 1, 9, 6);    plan.appendMove(m);
    m.dst = vec3( 1, 9, 0);    plan.appendMove(m);
    m.dst = vec3( 5, 9, 0);    plan.appendMove(m);
    m.dst = vec3( 5, 5, 0);    plan.appendMove(m);
    m.dst = vec3( 5, 1, 6);    plan.appendMove(m);
    m.dst = vec3( 9, 1, 6);    plan.appendMove(m);
    m.dst = vec3( 9, 1, 0);    plan.appendMove(m);
    m.dst = vec3( 9, 9, 0);    plan.appendMove(m);
    m.dst = vec3( 9, 9, 6);    plan.appendMove(m);

    plan.calculateMoves();
    plan.resetTraverse();
}

// awkward things can happen when three points are colinear
void loadTestCase_straight() {
    plan.clear();
    plan.setPositionLimits(0, 0, 0, 10, 10, 10);

    scv::move m;
    m.vel = 6;
    m.acc = 200;
    m.jerk = 800;
    m.blendType = CBT_MIN_JERK;
    m.src = vec3( 1, 1, 0);
    m.dst = vec3( 5, 1, 0);               plan.appendMove(m);
    m.dst = vec3( 9, 1, 0);  m.vel = 12;  plan.appendMove(m);
    m.dst = vec3( 9, 1, 5);  m.vel = 12;  plan.appendMove(m);
    m.dst = vec3( 9, 1, 9);  m.vel = 6;   plan.appendMove(m);
    m.dst = vec3( 9, 5, 9);  m.vel = 3;   plan.appendMove(m);
    m.dst = vec3( 9, 9, 9);  m.vel = 12;  plan.appendMove(m);

    plan.calculateMoves();
    plan.resetTraverse();
}

// even more awkward things can happen when a move is exactly opposite to the previous move
void loadTestCase_retrace() {
    plan.clear();
    plan.setPositionLimits(0, 0, 0, 10, 10, 10);

    scv::move m;
    m.vel = 12;
    m.acc = 200;
    m.jerk = 800;
    m.blendType = CBT_MIN_JERK;
    m.src = vec3( 1, 1, 0);
    m.dst = vec3( 6, 1, 0);  m.vel = 6;   plan.appendMove(m);
    m.dst = vec3( 3, 1, 0);  m.vel = 12;  plan.appendMove(m);
    m.dst = vec3( 9, 1, 0);  m.vel = 8;   plan.appendMove(m);
    m.dst = vec3( 9, 1, 5);  m.vel = 12;  plan.appendMove(m);
    m.dst = vec3( 9, 1, 0);  m.vel = 9;   plan.appendMove(m);
    m.dst = vec3( 9, 1, 9);  m.vel = 6;   plan.appendMove(m);
    m.dst = vec3( 9, 5, 9);  m.vel = 2;   plan.appendMove(m);
    m.dst = vec3( 9,0.2,9);  m.vel = 3;   plan.appendMove(m);
    m.dst = vec3( 9, 9, 9);  m.vel = 10;  plan.appendMove(m);

    plan.calculateMoves();
    plan.resetTraverse();
}

void loadTestCase_malformed() {
    plan.clear();
    plan.setPositionLimits(0, 0, 0, 10, 10, 7);

    scv::move m;
    m.vel = 0;
    m.acc = 0;
    m.jerk = 0;
    m.blendType = CBT_MIN_JERK;
    m.src = vec3(0, 0, 0);
    m.dst = vec3(0, 0, 0);  plan.appendMove(m);
    m.dst = vec3(0, 0, 0);  plan.appendMove(m);
    m.dst = vec3(0, 0, 0);  plan.appendMove(m);
    m.jerk = 1;
    m.dst = vec3(1, 1, 0);  plan.appendMove(m);
    m.jerk = 0;
    m.dst = vec3(2, 0, 0);  plan.appendMove(m);
    m.jerk = 1;
    m.acc = 1;
    m.dst = vec3(3, 1, 0);  plan.appendMove(m);
    m.vel = 1;
    m.dst = vec3(3, 1, 0);  plan.appendMove(m);

    plan.calculateMoves();
    plan.resetTraverse();
}
*/

void showStatusIndicator(const char* title, bool on) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float x = p.x;
    float y = p.y;
    float sz = 12.0f;
    ImVec4 colRed = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    ImVec4 colGreen = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    ImU32 col = ImColor( on ? colGreen : colRed );
    int circle_segments = 0;
    draw_list->AddCircleFilled(ImVec2(x + 4 + sz * 0.5f, y + 4 + sz * 0.4f), sz * 0.5f, col, circle_segments);

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (float)20);
    ImGui::Text("%s", title);
}

void showStatusIndicator_16bits(const char* title, uint16_t bits, int num) {

    ImGui::Text("%s", title);
    ImGui::SameLine();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float x = p.x;
    float y = p.y;
    float sz = 12.0f;
    ImVec4 colRed = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    ImVec4 colGreen = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    for (int i = 0; i < num; i++) {
        ImU32 col = ImColor( (bits & (1<<i)) ? colGreen : colRed );
        int circle_segments = 0;
        draw_list->AddCircleFilled(ImVec2(x + 4 + sz * 0.5f, y + 4 + sz * 0.4f), sz * 0.5f, col, circle_segments);
        x += sz + 2;
    }

    ImGui::NewLine();
    //ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (float)20);
}



bool pausePlot = false;

#define PLOTS_WINDOW_TITLE "Plots"

void showPlotsView(bool* p_open) {

    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);

    doLayoutLoad(PLOTS_WINDOW_TITLE);

    ImGui::Begin(PLOTS_WINDOW_TITLE, p_open);
    {
        ImGui::Checkbox("Pause graph", &pausePlot);

        ImVec2 sz = ImGui::GetWindowSize();
        sz.x -= 16;
        sz.y -= 70;

        if (ImPlot::BeginPlot("Line Plots", sz)) {
            ImPlot::SetupAxes("t","");
            ImPlot::PlotLineG("Pos X", MyDataGetter, (void*)plotPosX, MAXPLOTPOINTS);
            ImPlot::PlotLineG("Pos Y", MyDataGetter, (void*)plotPosY, MAXPLOTPOINTS);
            ImPlot::PlotLineG("Pos Z", MyDataGetter, (void*)plotPosZ, MAXPLOTPOINTS);
            ImPlot::PlotLineG("Vel X", MyDataGetter, (void*)plotVelX, MAXPLOTPOINTS);
            ImPlot::PlotLineG("Vel Y", MyDataGetter, (void*)plotVelY, MAXPLOTPOINTS);
            ImPlot::PlotLineG("Vel Z", MyDataGetter, (void*)plotVelZ, MAXPLOTPOINTS);
            ImPlot::PlotLineG("Rot 0", MyDataGetter, (void*)plotRot0, MAXPLOTPOINTS);
            ImPlot::PlotLineG("Vac", MyDataGetter, (void*)plotVac, MAXPLOTPOINTS);
            ImPlot::PlotLineG("Load", MyDataGetter, (void*)plotLoad, MAXPLOTPOINTS);
            ImPlot::PlotLineG("Weight", MyDataGetter, (void*)plotWeight, MAXPLOTPOINTS);
            ImPlot::EndPlot();
        }
    }

    doLayoutSave(PLOTS_WINDOW_TITLE);

    ImGui::End();
}


ImFont* tryLoadFont(const char* fontFile, float size) {
    FILE* f = fopen(fontFile, "r");
    if ( f ) {
        fclose( f );

        ImGuiIO& io = ImGui::GetIO();
        ImFont* fontPtr = io.Fonts->AddFontFromFileTTF( fontFile, size );
        ImGui::MergeIconsWithLatestFont(size, false);
        return fontPtr;
    }
    g_log.log(LL_ERROR, "Could not load font file: %s", fontFile);
    return NULL;
}


// to be used in separate thread
int modelsLoaded = 0;
int modelsExpected = 6;
void* loadModels(void* ptr)
{
    mainModel = loadModel("base.obj"); modelsLoaded++; currentModelLoadPercent = 0;
    gantryModel = loadModel("gantry.obj"); modelsLoaded++; currentModelLoadPercent = 0;
    zcarriageModel = loadModel("zcarriage.obj"); modelsLoaded++; currentModelLoadPercent = 0;
    xcarriageModel = loadModel("xcarriage.obj"); modelsLoaded++; currentModelLoadPercent = 0;
    nozzleModel = loadModel("nozzle.obj"); modelsLoaded++; currentModelLoadPercent = 0;
    nozzleTipModel = loadModel("nozzleTip.obj"); modelsLoaded++; currentModelLoadPercent = 0;

    return NULL;
}





void MyUserData_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf)
{
    out_buf->append("[OpenCommandDocuments]\n");
    for (CodeEditorDocument* d : commandDocuments) {
        if ( d->hasOwnFile )
            out_buf->append((d->filename + "\n").c_str());
    }

    out_buf->append("[OpenScriptDocuments]\n");
    for (CodeEditorDocument* d : scriptDocuments) {
        if ( d->hasOwnFile )
            out_buf->append((d->filename + "\n").c_str());
    }

    out_buf->append("[ServerInfo]\n");
    out_buf->append(serverHostname);
    out_buf->append("\n");

    out_buf->append("\n");
}


int8_t jogDirs[3];

clientReport_t lastStatusReport = {0};
//homingResult_e lastHomeResult = HR_NONE;
volatile trajectoryResult_e lastTrajResult = TR_NONE;
volatile homingResult_e lastHomingResult = HR_NONE;
volatile probingResult_e lastProbingResult = PR_NONE;
float lastProbedHeight = 0;

bool jogButtonDown() {
    bool doingJog = false;
    for (int i = 0; !doingJog && i < 3; i++)
        doingJog = jogDirs[i] != 0;
    return doingJog;
}

bool escWasPressed = false;
int functionKeyWasPressed = 0;
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if ( action == GLFW_PRESS ) {
        if (key == GLFW_KEY_ESCAPE)
        {
            escWasPressed = true;
        }
        else if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F12) {
            functionKeyWasPressed = (key - GLFW_KEY_F1) + 1;
        }
    }
}

// void showClearNotificationButton() {
//     ImGui::SetNextWindowBgAlpha(opacity);
//     ImGui::SetNextWindowPos( ImVec2( vp_size.x - 15, 20), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
//     ImGui::Begin(window_name, NULL, NOTIFY_TOAST_FLAGS);

//     ImGui::End();
// }

bool closeWindowNow = false;
bool closeAttempted = false;
bool allDocsHaveOwnFile = false;

bool ctrlOJustPressed = false;
bool ctrlSJustPressed = false;

void window_close_callback(GLFWwindow* window)
{
    closeAttempted = true;
    glfwSetWindowShouldClose(window, GLFW_FALSE);
}

pthread_t mainThreadId;

int main(int, char**)
{
    g_log.log(LL_INFO, "ScriptPNP client v%d.%d.%d", SCRIPTPNP_CLIENT_VERSION_MAJOR, SCRIPTPNP_CLIENT_VERSION_MINOR, SCRIPTPNP_CLIENT_VERSION_PATCH);

    int major, minor, patch;

    g_log.log(LL_INFO, "Versions:");
    g_log.log(LL_INFO, "   glfw %s", glfwGetVersionString());
    g_log.log(LL_INFO, "   Dear Imgui %s", IMGUI_VERSION);
    g_log.log(LL_INFO, "   SQLite %s", sqlite3_libversion());
    zmq_version(&major, &minor, &patch);
    g_log.log(LL_INFO, "   ZeroMQ %d.%d.%d", major, minor, patch);
    g_log.log(LL_INFO, "   AngelScript %s", ANGELSCRIPT_VERSION_STRING);
    g_log.log(LL_INFO, "   libuvc %d.%d.%d", LIBUVC_VERSION_MAJOR, LIBUVC_VERSION_MINOR, LIBUVC_VERSION_PATCH);
    g_log.log(LL_INFO, "   libAssimp %d.%d.%d", aiGetVersionMajor(), aiGetVersionMinor(), aiGetVersionPatch());
    g_log.log(LL_INFO, "   libserialport %s", sp_get_package_version_string());
    g_log.log(LL_INFO, "   ZXing %s", ZXING_VERSION_STR);

    serverHostname[0] = 0;

    mainThreadId = pthread_self();
    g_log.log(LL_INFO, "Main thread id: %lu", mainThreadId);

    std::chrono::steady_clock::time_point appStartTime = std::chrono::steady_clock::now();

    pthread_t modelLoadThread;
    int ret = pthread_create(&modelLoadThread, NULL, loadModels, NULL);
    if ( ret ) {
        g_log.log(LL_FATAL, "Model load thread creation failed!");
        return 1;
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    window = glfwCreateWindow(1280, 960, "PNP controller", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    glfwSetKeyCallback(window, key_callback);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    tryLoadFont( "fonts/FreeSans.ttf", 16.0f );

    font_proggy =        tryLoadFont( "fonts/ProggyVector-Regular.ttf", 18.0f );
    font_sourceCodePro = tryLoadFont( "fonts/SourceCodePro-Regular.ttf", 18.0f );
    font_ubuntuMono =    tryLoadFont( "fonts/UbuntuMono-Regular.ttf", 18.0f);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4;
    style.FrameRounding = 4;
    style.PopupRounding = 4;
    style.ScrollbarRounding = 4;
    style.TabRounding = 4;
    style.ChildRounding = 4;

    style.Colors[ImGuiCol_TitleBg] = ImColor(28, 28, 28);

    string dbFile = "mypnp.db";
    if ( ! openDatabase(dbFile) ) {
        g_log.log(LL_ERROR, "Could not open database file: %s", dbFile.c_str());
    }
    else {
        g_log.log(LL_INFO, "Opened database file: %s", dbFile.c_str());
    }

    if ( ! setupScriptEngine() ) {
        g_log.log(LL_FATAL, "Could not set up script engine!");
    }

    generateScriptDocs();

    setupCommandParseMappings();

    camera.setLocation(200, -350, 200);
    camera.setDirection( 0, -25 );

    machineLimits.setPositionLimits(0, 0, -32,   400, 480, 0);

    //machineLimits.setVelocityLimits(1000, 1000, 1000);
    //machineLimits.setAccelerationLimits(50000, 50000, 50000);
    //machineLimits.setJerkLimits(100000, 100000, 100000);

    //machineLimits.setRotationVAJLimits(3000, 30000, 500000);

    lastActualMoveLimits.vel = machineLimits.initialMoveLimitVel;
    lastActualMoveLimits.acc = machineLimits.initialMoveLimitAcc;
    lastActualMoveLimits.jerk = machineLimits.initialMoveLimitJerk;

    lastActualRotateLimits.vel = machineLimits.initialRotationLimitVel;
    lastActualRotateLimits.acc = machineLimits.initialRotationLimitAcc;
    lastActualRotateLimits.jerk = machineLimits.initialRotationLimitJerk;

    for (int i = 0; i < NUM_ROTATION_AXES; i++) {
        machineLimits.setRotationPositionLimits(i, -200, 200);
    }

    loadWorkspaceFromDB_windowsOpen("autosave");
    loadEventHooksFromDB();

    fetchTableNames();

    // {
    //     TableData td;
    //     td.name = "global_string";
    //     tableDatas.push_back(td);
    // }
    // {
    //     TableData td;
    //     td.name = "tweak";
    //     tableDatas.push_back(td);
    // }

    //initDefaultOverrides();
    //loadWorkspaceFromDB("mylayout");

    //initVision();

    //loadTestCase_default();

    //plan.printConstraints();
    //plan.printMoves();
    //plan.printSegments();

    // bool show_status_window = true;
    // bool show_demo_window = false;
    // bool show_log_window = true;
    // bool show_usbcamera_window = true;
    // bool show_usbcamera_view = true;
    // bool show_plot_view = false;
    // bool show_overrides_view = false;
    // bool show_hooks_view = true;
    // bool show_custom_view = false;
    // bool show_tweaks_view = true;

    //bool stopOnViolation = false;
    //bool doRandomizePoints = false;


    float moveSpeedScale = 1;
    float jogSpeedScale = 1;
    //float lastMoveSpeedScale = moveSpeedScale;
    //float lastJogSpeedScale = jogSpeedScale;
    std::chrono::steady_clock::time_point lastPublishTime = std::chrono::steady_clock::now();

    std::chrono::steady_clock::time_point lastJogSendTime = std::chrono::steady_clock::now();




    for (int i = 0; i < 3; i++)
        jogDirs[i] = 0;

    //vec3 targetPos = vec3_zero;
    //float movetoSpeed = 75;


    //aiLogStream stream = aiGetPredefinedLogStream(aiDefaultLogStream_STDOUT,NULL);
    //aiAttachLogStream(&stream);






    vector<string> openCommandDocumentPaths;
    vector<string> openScriptDocumentPaths;

    string line;
    ifstream file("imgui.ini");
    if (file.is_open()) {
        int section = 0;
        while (getline(file, line)) {
            if ( line == "[OpenCommandDocuments]" ) {
                section = 1;
                continue;
            }
            if ( line == "[OpenScriptDocuments]" ) {
                section = 2;
                continue;
            }
            if ( line == "[ServerInfo]" ) {
                section = 3;
                continue;
            }
            if ( section == 1 ) {
                if ( ! line.empty() )
                    openCommandDocumentPaths.push_back( line );
            }
            if ( section == 2 ) {
                if ( ! line.empty() )
                    openScriptDocumentPaths.push_back( line );
            }
            if ( section == 3 ) {
                if ( ! line.empty() )
                    sprintf(serverHostname, "%s", line.c_str());
            }
        }
        file.close();
    }

    bool loadedCommandDoc = false;
    for (string path : openCommandDocumentPaths) {
        string errMsg;
        loadedCommandDoc |= loadDBFile(&commandDocuments, "command list", path, errMsg);
    }

    //bool loadedScriptDoc = false;
    for (string path : openScriptDocumentPaths) {
        string errMsg;
        bool actuallyLoaded = loadDBFile(&scriptDocuments, "script", path, errMsg);
        if ( actuallyLoaded ) {
            auto lang = TextEditor::LanguageDefinition::AngelScript();
            scriptDocuments.back()->editor.SetLanguageDefinition(lang);
            //loadedScriptDoc = true;
        }
    }

    /*if ( loadedCommandDoc ) {
        openCommandEditorWindow();
    }
    if ( loadedScriptDoc ) {
        openScriptEditorWindow();
    }*/

    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName = "UserData";
    ini_handler.TypeHash = ImHashStr("UserData");
    ini_handler.WriteAllFn = MyUserData_WriteAll;
    ImGui::AddSettingsHandler(&ini_handler);


    //uint16_t digOuts = 0;
    float pwmOut = 0;
    float lastPWMOut = 0;

    uint8_t rgbOuts[6];
    memset(rgbOuts, 0, 6);

    bool firstTimeSetDone = false;



    startSubscriber();
    startRequester();

    //fetchAllServerConfigs();

    updateSerialPortList();

    glfwSetWindowCloseCallback(window, window_close_callback);

    int modelLoadCompleteDwellIterations = 3; // remain showing the progress bar at 100% for 3 frames, ie. 0.05 seconds at 60fps
    while ( modelLoadCompleteDwellIterations > 0 )
    {
        float percentPerModel = 1 / (float)modelsExpected;
        float percent = modelsLoaded * percentPerModel;
        percent += currentModelLoadPercent * percentPerModel;

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImDrawList* bgdl = ImGui::GetBackgroundDrawList();
        bgdl->AddCallback(backgroundRenderCallback_loading, nullptr);

        ImGui::SetNextWindowSize(ImVec2(300, 86), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("PNP Client", NULL, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
        ImGui::Text("Loading models...");
        ImGui::ProgressBar( percent );
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);

        if ( modelsLoaded >= modelsExpected )
            modelLoadCompleteDwellIterations--;
    }

    pthread_join(modelLoadThread, NULL);




    std::chrono::steady_clock::time_point appStartCompleteTime = std::chrono::steady_clock::now();
    long long startupTime = std::chrono::duration_cast<std::chrono::milliseconds>(appStartCompleteTime - appStartTime).count();

    g_log.log(LL_DEBUG, "Startup time: %lld ms", startupTime);

    bool moveSpeedScaleChanged = false;
    bool jogSpeedScaleChanged = false;

    bool ctrlOPressedLastTime = false;
    bool ctrlSPressedLastTime = false;

    //while (!glfwWindowShouldClose(window))
    while ( ! closeWindowNow )
    {
        glfwPollEvents();

        // left shift + esc together
        if ( glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS &&
            glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
            break;

        ctrlOJustPressed = false;
        ctrlSJustPressed = false;

        bool anyCtrlPressed = ( glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ) ||
                              ( glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS );

        if ( anyCtrlPressed ) {
            if ( glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
                if ( ! ctrlOPressedLastTime )
                    ctrlOJustPressed = true;
                ctrlOPressedLastTime = true;
            }
            if ( glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                if ( ! ctrlSPressedLastTime )
                    ctrlSJustPressed = true;
                ctrlSPressedLastTime = true;
            }
        }
        else {
            ctrlOPressedLastTime = false;
            ctrlSPressedLastTime = false;
        }

        string workspaceLayoutTitleToSave = wasWorkspaceInfoSaveRequested();
        if ( workspaceLayoutTitleToSave != "" )
            beginSavingWorkspaceInfo();

        string workspaceLayoutTitleToLoad = wasWorkspaceInfoLoadRequested();
        if ( workspaceLayoutTitleToLoad != "" )
            beginLoadingWorkspaceInfo(workspaceLayoutTitleToLoad);

        if ( io.Framerate != 0 ) // framerate will be zero on first frame
            animAdvance = 1 / io.Framerate;

        //if ( stopOnViolation && haveViolation )
        //    doRandomizePoints = false;

        /*if ( doRandomizePoints ) {
            randomizePoints();
            plan.resetTraverse();
        }*/

        if ( ! io.WantCaptureMouse ) {
            if ( ImGui::IsMouseDown(1) ) {// hold right mouse button to pan view
                float yaw = camera.yaw + io.MouseDelta.x * 0.05;
                float pitch = camera.pitch + io.MouseDelta.y * -0.05;
                camera.setDirection(yaw, pitch);
            }
        }

        // these should be done regardless of where keyboard focus is
        //if ( glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS ) {
        if ( escWasPressed ) {
            abortScript();
            commandRequest_t req = createCommandRequest(MT_SET_ESTOP);
            req.setEstop.val = 0;
            sendCommandRequest(&req);
            escWasPressed = false;
        }

        if ( functionKeyWasPressed ) {
            executeFunctionKeyHook(functionKeyWasPressed);
            functionKeyWasPressed = 0;
        }

        if ( ! io.WantCaptureKeyboard ) {
            float camMoveSpeed = 2;
            float right = 0;
            float forward = 0;
            float up = 0;
            if ( ImGui::IsKeyDown(ImGuiKey_A) ) {
                right = -camMoveSpeed;
            }
            else if ( ImGui::IsKeyDown(ImGuiKey_D) ) {
                right = camMoveSpeed;
            }

            if ( ImGui::IsKeyDown(ImGuiKey_S) ) {
                forward = -camMoveSpeed;
            }
            else if ( ImGui::IsKeyDown(ImGuiKey_W) ) {
                forward = camMoveSpeed;
            }

            if ( ImGui::IsKeyDown(ImGuiKey_LeftCtrl) ) {
                up = -camMoveSpeed;
            }
            else if ( ImGui::IsKeyDown(ImGuiKey_LeftShift) ) {
                up = camMoveSpeed;
            }
            camera.translate(right, forward, up);

            if ( ImGui::IsKeyDown(ImGuiKey_T) ) {
                planGroup_preview.resetTraverse();
            }
        }

        if ( moveSpeedScaleChanged ) {
            commandRequest_t req = createCommandRequest(MT_SET_SPEED_SCALE);
            req.setSpeedScale.scale = moveSpeedScale;
            sendCommandRequest(&req);
        }

        if ( jogSpeedScaleChanged ) {
            commandRequest_t req = createCommandRequest(MT_SET_JOG_SPEED_SCALE);
            req.setSpeedScale.scale = jogSpeedScale;
            sendCommandRequest(&req);
        }

        if ( checkScriptRunThreadComplete() ) {
            g_log.log(LL_DEBUG, "checkScriptRunThreadComplete() true");
        }

        clientReport_t statRep = {0};
        if ( checkSubscriberMessages(&statRep) ) {
            lastActualPos = vec3(statRep.actualPosX, statRep.actualPosY, statRep.actualPosZ);
            lastActualVel = vec3(statRep.actualVelX, statRep.actualVelY, statRep.actualVelZ);
            memcpy(lastActualRots, statRep.actualRots, sizeof(lastActualRots));
            lastActualMoveLimits.vel = statRep.limMoveVel;
            lastActualMoveLimits.acc = statRep.limMoveAcc;
            lastActualMoveLimits.jerk = statRep.limMoveJerk;
            lastActualRotateLimits.vel = statRep.limRotateVel;
            lastActualRotateLimits.acc = statRep.limRotateAcc;
            lastActualRotateLimits.jerk = statRep.limRotateJerk;
            lastActualSpeedScale = statRep.speedScale;
            lastActualJogSpeedScale = statRep.jogSpeedScale;
            lastStatusReport = statRep;
            lastPublishTime = std::chrono::steady_clock::now();

            if ( lastStatusReport.trajResult != TR_NONE ) {
                lastTrajResult = (trajectoryResult_e)lastStatusReport.trajResult;
                doNotifiesForTrajectoryResult( lastTrajResult );
            }
            if ( lastStatusReport.homingResult != HR_NONE ) {
                lastHomingResult = (homingResult_e)lastStatusReport.homingResult;
                doNotifiesForHomingResult( lastHomingResult );
            }
            if ( lastStatusReport.probingResult != PR_NONE ) {
                lastProbingResult = (probingResult_e)lastStatusReport.probingResult;
                lastProbedHeight = lastStatusReport.probedHeight;
                doNotifiesForProbingResult( lastProbingResult );
            }

            //updateLoadcell( lastStatusReport );

            if ( ! firstTimeSetDone ) {
                fetchAllServerConfigs();
                moveSpeedScale = lastActualSpeedScale;
                jogSpeedScale = lastActualJogSpeedScale;
                firstTimeSetDone = true;
            }

            pwmOut = lastStatusReport.pwm / 65535.0f;
        }

        float vac = ((float)lastStatusReport.pressure - 50000) / 500.0f;
        float load = lastStatusReport.loadcell;//((float)lastStatusReport.loadcell) / 8388607.0f;

        if ( ! pausePlot ) {
            plotPosX[graphIndex] = lastActualPos.x;
            plotPosY[graphIndex] = lastActualPos.y;
            plotPosZ[graphIndex] = lastActualPos.z;
            plotVelX[graphIndex] = lastActualVel.x;
            plotVelY[graphIndex] = lastActualVel.y;
            plotVelZ[graphIndex] = lastActualVel.z;
            plotRot0[graphIndex] = lastActualRots[0];
            plotVac[graphIndex] = vac;
            plotLoad[graphIndex] = load;
            plotWeight[graphIndex] = lastStatusReport.weight;
            graphIndex = (graphIndex + 1) % MAXPLOTPOINTS;
        }

        checkRequestsQueue();

        commandReply_t rep;
        if ( checkReplies(&rep) ) {
            g_log.log(LL_DEBUG, "Got reply %s", getMessageName(rep.type));

            if ( rep.type == MT_CONFIG_STEPS_FETCH ) {
                for (int i = 0; i < NUM_MOTION_AXES; i++) {
                    config_stepsPerUnit[i] = rep.configSteps.perUnit[i];
                }
            }
            else if ( rep.type == MT_CONFIG_WORKAREA_FETCH ) {
                config_workingAreaX = rep.workingArea.x;
                config_workingAreaY = rep.workingArea.y;
                config_workingAreaZ = rep.workingArea.z;

                machineLimits.setPositionLimits(0, 0, -rep.workingArea.z,     rep.workingArea.x, rep.workingArea.y, 0);
            }
            else if ( rep.type == MT_CONFIG_INITSPEEDS_FETCH ) {

                machineLimits.initialMoveLimitVel =  rep.motionLimits.initialMoveVel;
                machineLimits.initialMoveLimitAcc =  rep.motionLimits.initialMoveAcc;
                machineLimits.initialMoveLimitJerk = rep.motionLimits.initialMoveJerk;

                machineLimits.initialRotationLimitVel =  rep.motionLimits.initialRotateVel;
                machineLimits.initialRotationLimitAcc =  rep.motionLimits.initialRotateAcc;
                machineLimits.initialRotationLimitJerk = rep.motionLimits.initialRotateJerk;

                machineLimits.velLimit.x = rep.motionLimits.velLimitX;
                machineLimits.velLimit.y = rep.motionLimits.velLimitY;
                machineLimits.velLimit.z = rep.motionLimits.velLimitZ;

                machineLimits.accLimit.x = rep.motionLimits.accLimitX;
                machineLimits.accLimit.y = rep.motionLimits.accLimitY;
                machineLimits.accLimit.z = rep.motionLimits.accLimitZ;

                machineLimits.jerkLimit.x = rep.motionLimits.jerkLimitX;
                machineLimits.jerkLimit.y = rep.motionLimits.jerkLimitY;
                machineLimits.jerkLimit.z = rep.motionLimits.jerkLimitZ;

                machineLimits.grotationVelLimit  = rep.motionLimits.rotLimitVel;
                machineLimits.grotationAccLimit  = rep.motionLimits.rotLimitAcc;
                machineLimits.grotationJerkLimit = rep.motionLimits.rotLimitJerk;

                machineLimits.maxOverlapFraction = rep.motionLimits.maxOverlapFraction;
            }
            else if ( rep.type == MT_CONFIG_TMC_FETCH ) {
                for (int i = 0; i < NUM_MOTION_AXES; i++) {
                    config_tmc[i] = rep.tmcSettings.settings[i];
                }
            }
            else if ( rep.type == MT_CONFIG_HOMING_FETCH ) {
                for (int i = 0; i < NUM_HOMABLE_AXES; i++) {
                    config_homing[i] = rep.homingParams.params[i];
                }
                memcpy(homingOrder, rep.homingParams.order, scv::min(sizeof(rep.homingParams.order), sizeof(homingOrder)));
            }
            else if ( rep.type == MT_CONFIG_JOGGING_FETCH ) {
                for (int i = 0; i < NUM_MOTION_AXES; i++) {
                    config_jogSpeed[i] = rep.jogParams.speed[i];
                }
            }
            else if ( rep.type == MT_CONFIG_OVERRIDES_FETCH ) {

            }
            else if ( rep.type == MT_CONFIG_LOADCELL_CALIB_FETCH ) {
                config_loadcellCalibrationRawOffset = rep.loadcellCalib.rawOffset;
                config_loadcellCalibrationWeight = rep.loadcellCalib.weight;
            }
            else if ( rep.type == MT_CONFIG_PROBING_FETCH ) {
                config_probing = rep.probingParams.params;
            }
            else {
                g_log.log(LL_DEBUG, "Ignoring command reply %s", getMessageName(rep.type));
            }

        }



        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImDrawList* bgdl = ImGui::GetBackgroundDrawList();
        bgdl->AddCallback(backgroundRenderCallback, nullptr);

        //showPointLabel( bgdl, vec3_zero, "Zero", ImColor(255,255,0,255), ImColor(255,255,255,255) );
        //showPointLabel( bgdl, vec3(50,50,0), "Fifty", ImColor(0,255,0,255), ImColor(192,192,192,255) );

        if ( showPreviewEventsLabels )
            showTraverseEventLabels( bgdl );

        if ( closeAttempted ) {
            closeAttempted = false;
            if ( currentlyRunningScriptThread() || currentlyPausingScript() ) {
                ImGuiToast toast(ImGuiToastType_Warning, 4000);
                toast.set_title("Ignoring app close attempt because script run is in progress");
                //toast.set_content("Lorem ipsum dolor sit amet");
                ImGui::InsertNotification(toast);
            }
            else {
                if ( ! isAnyDocumentDirty(allDocsHaveOwnFile) )
                    closeWindowNow = true;
                else
                    ImGui::OpenPopup("Unsaved documents");
            }
        }

        if (ImGui::BeginPopupModal("Unsaved documents", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("There are unsaved documents - exit anyway?");

            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
            if ( ImGui::Button("Exit", ImVec2(120, 0))) {
                closeWindowNow = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);

            if ( allDocsHaveOwnFile ) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
                if ( ImGui::Button("Save and exit", ImVec2(120, 0))) {
                    saveAllDocuments(commandDocuments);
                    saveAllDocuments(scriptDocuments);
                    closeWindowNow = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopStyleColor(3);
            }

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

        /*{
            // open Dialog Simple
            if (ImGui::Button("Open File Dialog")) {
                IGFD::FileDialogConfig config;
                config.path = ".";
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".cpp,.h,.hpp", config);
            }
            // display
            if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
                if (ImGuiFileDialog::Instance()->IsOk()) { // action if OK
                    std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                    std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
                    // action
                }

                // close
                ImGuiFileDialog::Instance()->Close();
            }
        }*/

        /*if (ImGui::Button("Open NFD Open Dialog"))
        {
            nfdchar_t *openPath = NULL;
            nfdresult_t result = NFD_OpenDialog( "png,jpg;pdf", NULL, &openPath );
            if ( result == NFD_OKAY )
            {
                puts("Success!");
                puts(openPath);
                free(openPath);
            }
            else if ( result == NFD_CANCEL )
            {
                puts("User pressed cancel.");
            }
            else
            {
                printf("Error: %s\n", NFD_GetError() );
            }
        }

        if (ImGui::Button("Open NFD Save Dialog"))
        {
            nfdchar_t *savePath = NULL;
            nfdresult_t result = NFD_SaveDialog( "png,jpg;pdf", NULL, &savePath );
            if ( result == NFD_OKAY )
            {
                puts("Success!");
                puts(savePath);
                free(savePath);
            }
            else if ( result == NFD_CANCEL )
            {
                puts("User pressed cancel.");
            }
            else
            {
                printf("Error: %s\n", NFD_GetError() );
            }
        }*/

        for (CodeEditorWindow* w : commandEditorWindows) {
            w->buttonFeedback.shouldPreview = false;
            w->buttonFeedback.shouldRun = false;
            w->render();
        }

        for (CodeEditorWindow* w : scriptEditorWindows) {
            w->buttonFeedback.shouldPreview = false;
            w->buttonFeedback.shouldRun = false;
            w->render();
        }

        for (int i = 0; i < (int)commandDocuments.size(); i++) {
            CodeEditorDocument* doc = commandDocuments[i];
            if ( doc->shouldClose ) {
                for (CodeEditorWindow* w : commandEditorWindows)
                    w->onDocumentClosed(doc);
                commandDocuments.erase(commandDocuments.begin() + i);
                delete doc;
                break;
            }
        }

        for (int i = 0; i < (int)scriptDocuments.size(); i++) {
            CodeEditorDocument* doc = scriptDocuments[i];
            if ( doc->shouldClose ) {
                for (CodeEditorWindow* w : scriptEditorWindows)
                    w->onDocumentClosed(doc);
                scriptDocuments.erase(scriptDocuments.begin() + i);
                delete doc;
                break;
            }
        }

        for (int i = 0; i < (int)commandEditorWindows.size(); i++) {
            CodeEditorWindow* w = commandEditorWindows[i];
            if ( ! w->shouldRemainOpen ) {
                commandEditorWindows.erase(commandEditorWindows.begin() + i);
                delete w;
                break;
            }
        }

        for (int i = 0; i < (int)scriptEditorWindows.size(); i++) {
            CodeEditorWindow* w = scriptEditorWindows[i];
            if ( ! w->shouldRemainOpen ) {
                removeActiveScriptLog( &w->log );
                scriptEditorWindows.erase(scriptEditorWindows.begin() + i);
                delete w;
                break;
            }
        }

        {
            bool doOpenLayout = false;
            bool doSaveLayout = false;
            bool doSaveLayoutAs = false;

            if (ImGui::BeginMainMenuBar())
            {
                //                if (ImGui::BeginMenu("File"))
                //                {
                //                    ShowExampleMenuFile();
                //                    ImGui::EndMenu();
                //                }
                //                if (ImGui::BeginMenu("Edit"))
                //                {
                //                    if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
                //                    if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
                //                    ImGui::Separator();
                //                    if (ImGui::MenuItem("Cut", "CTRL+X")) {}
                //                    if (ImGui::MenuItem("Copy", "CTRL+C")) {}
                //                    if (ImGui::MenuItem("Paste", "CTRL+V")) {}
                //                    ImGui::EndMenu();
                //                }

                if (ImGui::BeginMenu("Setup"))
                {
                    ImGui::MenuItem("Server", NULL, &show_server_view);
                    ImGui::MenuItem("Serial", NULL, &show_serial_view);
                    ImGui::MenuItem("Event hooks", NULL, &show_hooks_view);
                    ImGui::MenuItem("USB cameras", NULL, &show_usbcamera_window);

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Views"))
                {
                    ImGui::MenuItem("Status", NULL, &show_status_window);
                    ImGui::MenuItem("Log", NULL, &show_log_window);
                    ImGui::MenuItem("Plots", NULL, &show_plot_view);
                    ImGui::MenuItem("Custom buttons", NULL, &show_custom_view);
                    ImGui::MenuItem("Tweaks", NULL, &show_tweaks_view);

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Editors"))
                {
                    if (ImGui::MenuItem("Command list editor", NULL))
                        openCommandEditorWindow();
                    if (ImGui::MenuItem("Script editor", NULL))
                        openScriptEditorWindow();

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("DB"))
                {
                    ImGui::MenuItem("DB tables", NULL, &show_table_views);

                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Workspace"))
                {
                    if (ImGui::MenuItem("Load layout"))
                    {
                        doOpenLayout = true;
                    }

                    char wlobuf[128];
                    sprintf(wlobuf, "Save layout");
                    bool overwriteLayoutEnabled = haveCurrentWorkspaceLayout();
                    if ( overwriteLayoutEnabled )
                        snprintf(wlobuf, 128, "Save layout (%s)", getCurrentLayoutTitle().c_str());
                    if (ImGui::MenuItem(wlobuf, NULL, (bool*)NULL, overwriteLayoutEnabled))
                    {
                        doSaveLayout = true;
                    }

                    if (ImGui::MenuItem("Save layout as" ))
                    {
                        doSaveLayoutAs = true;
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }

            if ( doOpenLayout )
                openWorkspaceLayoutOpenDialogPopup();
            showWorkspaceLayoutOpenDialogPopup();

            if ( doSaveLayout )
                requestWorkspaceInfoResave();

            if ( doSaveLayoutAs )
                openWorkspaceLayoutSaveAsDialogPopup();
            showWorkspaceLayoutSaveAsDialogPopup(doSaveLayoutAs);

            if ( show_status_window )
            {
                doLayoutLoad(STATUS_WINDOW_TITLE);

                ImGui::Begin("Status", &show_status_window);

                ImGui::Checkbox("Demo Window", &show_demo_window);

                std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
                long long timeSinceLastPublish = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPublishTime).count();

                bool connected = timeSinceLastPublish < 200;
                if ( ! connected ) {
                    firstTimeSetDone = false;
                    lastStatusReport = {0};
                }

                showStatusIndicator("Server connection", connected );
                ImGui::SameLine();
                ImGui::Text(" (%lld ms)", timeSinceLastPublish);
                showStatusIndicator("SPI connection", lastStatusReport.spiOk );
                ImGui::Text("Mode: %s", getModeName(lastStatusReport.mode));
                ImGui::Text("Last home result: %s", getHomingResultName(lastHomingResult));
                ImGui::Text("Last traj result: %s", getTrajectoryResultName(lastTrajResult));
                if ( lastProbingResult == PR_SUCCESS ) {
                    ImGui::Text("Last probe result: %s, height = %f", getProbingResultName(lastProbingResult), lastProbedHeight);
                }
                else
                    ImGui::Text("Last probe result: %s", getProbingResultName(lastProbingResult));

                showStatusIndicator_16bits("Homed:", lastStatusReport.homedAxes, 4);
                showStatusIndicator_16bits("Outputs:", lastStatusReport.outputs, 16);
                showStatusIndicator_16bits("Inputs:", lastStatusReport.inputs, 16);

                ImGui::Text("Actual pos: %f, %f, %f", lastActualPos.x, lastActualPos.y, lastActualPos.z);
                ImGui::Text("Actual vel: %f, %f, %f", lastActualVel.x, lastActualVel.y, lastActualVel.z);
                ImGui::Text("Actual rot: %f", lastActualRots[0]);

                //ImGui::Text("Probed height: %f", lastStatusReport.probedHeight);

                ImGui::Text("Rotary enc: %d", lastStatusReport.rotary);
                ImGui::SameLine();
                ImGui::Text("ADCs: %.3f, %.3f", lastStatusReport.adc[0] / 4096.0f, lastStatusReport.adc[1] / 4096.0f);

                ImGui::Text("Vacuum: %0.2f kpa", vac);
                ImGui::SameLine();
                ImGui::Text("Load cell: %d", lastStatusReport.loadcell);
                ImGui::SameLine();
                ImGui::Text("Weight: %f", lastStatusReport.weight);

                ImGui::Text("Move limits: vel %.0f, acc %.0f, jerk %.0f", lastActualMoveLimits.vel, lastActualMoveLimits.acc, lastActualMoveLimits.jerk);
                ImGui::Text("Rotate limits: vel %.0f, acc %.0f, jerk %.0f", lastActualRotateLimits.vel, lastActualRotateLimits.acc, lastActualRotateLimits.jerk);
                moveSpeedScaleChanged = ImGui::SliderFloat("Speed scale", &moveSpeedScale, 0.1, 1);
                jogSpeedScaleChanged = ImGui::SliderFloat("Jog speed scale", &jogSpeedScale, 0.1, 1);

                if (ImGui::Button("EStop")) {
                    commandRequest_t req = createCommandRequest(MT_SET_ESTOP);
                    req.setEstop.val = 0;
                    sendCommandRequest(&req);
                }

                /*ImGui::SameLine();
                if (ImGui::Button("Zero")) {
                    if ( ! jogButtonDown() ) {
                        commandRequest_t req = createCommandRequest(MT_SET_MOVETO);
                        req.setMoveto.speed = movetoSpeed;
                        req.setMoveto.dst.x = targetPos.x;
                        req.setMoveto.dst.y = targetPos.y;
                        req.setMoveto.dst.z = targetPos.z;
                        sendCommandRequest(&req);
                        lastTrajResult = TR_NONE;
                    }
                }*/

                ImGui::SameLine();
                if ( ImGui::Button("RGB on") ) {

                    rgbOuts[0] |= 7;
                    rgbOuts[1] |= (1 << 3);
                    rgbOuts[2] |= (1 << 3);

                    commandRequest_t req = createCommandRequest(MT_SET_RGB_OUTPUT);
                    memcpy(req.setRGBOutput.rgb, rgbOuts, 6);
                    sendCommandRequest(&req);
                }

                ImGui::SameLine();
                if ( ImGui::Button("RGB off") ) {

                    rgbOuts[1] = (1 << 1);
                    rgbOuts[2] = (1 << 1);

                    commandRequest_t req = createCommandRequest(MT_SET_RGB_OUTPUT);
                    memcpy(req.setRGBOutput.rgb, rgbOuts, 6);
                    sendCommandRequest(&req);
                }

                ImGui::SameLine();
                if (ImGui::Button("TMC")) {
                    commandRequest_t req = createCommandRequest(MT_SET_TMC_PARAMS);
                    for (int i = 0; i < 4; i++) {
                        req.setTMCParams.microsteps[i] = 16; // 4x microstepping
                        req.setTMCParams.rmsCurrent[i] = 200;
                    }
                    sendCommandRequest(&req);
                }

                ImGui::SameLine();
                if (ImGui::Button("Test notifications")) {
                    notify("Test success", NT_SUCCESS, 5000 );
                    notify("Test warning", NT_WARNING, 5000 );
                    notify("Test error", NT_ERROR, 5000 );
                    notify("Test info", NT_INFO, 5000 );
                    //ImGui::InsertNotification({ ImGuiToastType_Success, 5000, "Hello World! This is a success! %s", "We can also format here:)" });
                    if ( ! ImGui::notifications.empty() )
                        ImGui::RemoveNotification(0);
                }

                if ( ImGui::Button("D0 on") ) {

                    //digOuts |= 0x1;

                    commandRequest_t req = createCommandRequest(MT_SET_DIGITAL_OUTPUTS);
                    req.setDigitalOutputs.bits = 0xff;
                    req.setDigitalOutputs.changed = 0x1;
                    sendCommandRequest(&req);
                }

                ImGui::SameLine();
                if ( ImGui::Button("D0 off") ) {

                    //digOuts &= ~0x1;

                    commandRequest_t req = createCommandRequest(MT_SET_DIGITAL_OUTPUTS);
                    req.setDigitalOutputs.bits = 0;
                    req.setDigitalOutputs.changed = 0x1;
                    sendCommandRequest(&req);
                }

                ImGui::SameLine();
                if ( ImGui::Button("D1 on") ) {

                    //digOuts |= 0x2;

                    commandRequest_t req = createCommandRequest(MT_SET_DIGITAL_OUTPUTS);
                    req.setDigitalOutputs.bits = 0xff;
                    req.setDigitalOutputs.changed = 0x2;
                    sendCommandRequest(&req);
                }

                ImGui::SameLine();
                if ( ImGui::Button("D1 off") ) {

                    //digOuts &= ~0x2;

                    commandRequest_t req = createCommandRequest(MT_SET_DIGITAL_OUTPUTS);
                    req.setDigitalOutputs.bits = 0;
                    req.setDigitalOutputs.changed = 0x2;
                    sendCommandRequest(&req);
                }

                ImGui::SameLine();
                if ( ImGui::Button("D2 on") ) {

                    //digOuts |= 0x4;

                    commandRequest_t req = createCommandRequest(MT_SET_DIGITAL_OUTPUTS);
                    req.setDigitalOutputs.bits = 0xff;
                    req.setDigitalOutputs.changed = 0x4;
                    sendCommandRequest(&req);
                }

                ImGui::SameLine();
                if ( ImGui::Button("D2 off") ) {

                    //digOuts &= ~0x4;

                    commandRequest_t req = createCommandRequest(MT_SET_DIGITAL_OUTPUTS);
                    req.setDigitalOutputs.bits = 0;
                    req.setDigitalOutputs.changed = 0x4;
                    sendCommandRequest(&req);
                }

                bool didDisable = false;
                if ( currentlyRunningScriptThread() || currentlyPausingScript() ) {
                    ImGui::BeginDisabled();
                    didDisable = true;
                }

                if ( ImGui::Button("Home X") ) {
                    commandRequest_t req = createCommandRequest(MT_HOME_AXES);
                    memset(&req.homeAxes, 0, sizeof(req.homeAxes));
                    req.homeAxes.ordering[0] = 1;
                    sendCommandRequest(&req);
                    lastHomingResult = HR_NONE;
                }

                ImGui::SameLine();
                if ( ImGui::Button("Home Y") ) {
                    commandRequest_t req = createCommandRequest(MT_HOME_AXES);
                    memset(&req.homeAxes, 0, sizeof(req.homeAxes));
                    req.homeAxes.ordering[0] = 2;
                    sendCommandRequest(&req);
                    lastHomingResult = HR_NONE;
                }

                ImGui::SameLine();
                if ( ImGui::Button("Home Z") ) {
                    commandRequest_t req = createCommandRequest(MT_HOME_AXES);
                    memset(&req.homeAxes, 0, sizeof(req.homeAxes));
                    req.homeAxes.ordering[0] = 3;
                    sendCommandRequest(&req);
                    lastHomingResult = HR_NONE;
                }

                ImGui::SameLine();
                if ( ImGui::Button("Home all") ) {
                    commandRequest_t req = createCommandRequest(MT_HOME_ALL);
                    memset(&req.homeAxes, 0, sizeof(req.homeAxes));
                    sendCommandRequest(&req);
                    lastHomingResult = HR_NONE;
                }

                if ( didDisable )
                    ImGui::EndDisabled();


                didDisable = false;
                if ( ! currentlyRunningScriptThread() || currentlyPausingScript() ) {
                    ImGui::BeginDisabled();
                    didDisable = true;
                }
                if ( ImGui::Button("Pause script") ) {
                    pauseScript();
                }
                if ( didDisable )
                    ImGui::EndDisabled();


                ImGui::SameLine();

                didDisable = false;
                if ( ! currentlyPausingScript() ) {
                    ImGui::BeginDisabled();
                    didDisable = true;
                }
                if ( ImGui::Button("Resume script") ) {
                    resumeScript();
                }
                if ( didDisable )
                    ImGui::EndDisabled();

                ImGui::SameLine();

                didDisable = false;
                if ( ! currentlyRunningScriptThread() ) {
                    ImGui::BeginDisabled();
                    didDisable = true;
                }
                if ( ImGui::Button("Abort script") ) {
                    abortScript();
                }
                if ( didDisable )
                    ImGui::EndDisabled();

                lastPWMOut = pwmOut;
                ImGui::SliderFloat("PWM out", &pwmOut, 0, 1);
                if ( pwmOut != lastPWMOut ) {

                    commandRequest_t req = createCommandRequest(MT_SET_PWM_OUTPUT);
                    req.setPWMOutput.val = 65535 * pwmOut;
                    sendCommandRequest(&req);

                    lastPWMOut = pwmOut;
                }

                const ImGuiKey jogKeys[] = {
                    ImGuiKey_LeftArrow, ImGuiKey_RightArrow,
                    ImGuiKey_DownArrow, ImGuiKey_UpArrow,
                    ImGuiKey_PageDown, ImGuiKey_PageUp
                };

                bool shouldSendJogCommand = false;
                if ( ! io.WantCaptureKeyboard ) {
                    for (int i = 0; i < 3; i++) {
                        bool jogDec = ImGui::IsKeyDown( jogKeys[2*i] );
                        bool jogInc = ImGui::IsKeyDown( jogKeys[2*i+1] );

                        int jog = 0;
                        if ( jogDec && ! jogInc )
                            jog = -1;
                        else if ( ! jogDec && jogInc )
                            jog = 1;

                        if ( jog != jogDirs[i] ) {
                            shouldSendJogCommand = true;
                        }

                        jogDirs[i] = jog;
                    }
                }

                if ( ! shouldSendJogCommand ) {
                    if ( jogButtonDown() ) {
                        long long timeSinceLastJogSend = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastJogSendTime).count();
                        if ( timeSinceLastJogSend > 100 ) {
                            shouldSendJogCommand = true;
                            lastJogSendTime = now;
                        }
                    }
                }

                if ( shouldSendJogCommand ) {
                    commandRequest_t jogReq = createCommandRequest(MT_SET_JOG_STATUS);
                    memcpy(jogReq.setJogStatus.jogDirs, jogDirs, sizeof(jogDirs));
                    sendCommandRequest(&jogReq);
                }


                //            ImGui::Text("WASD, left shift, left ctrl to move camera.\n"
                //                        "Hold right mouse button and drag to rotate camera.\n"
                //                        "T to restart animation");

                //char n[128];

                if (ImGui::CollapsingHeader("Display options"))
                {
                    // if (ImGui::BeginTable("split", 2))
                    // {
                    //     ImGui::TableNextColumn();
                        ImGui::Checkbox("Show bounding box", &showBoundingBox);
                        //ImGui::TableNextColumn(); ImGui::Checkbox("Show control points", &showControlPoints);
                    //     ImGui::EndTable();
                    // }

                    ImGui::Checkbox("Show preview event labels", &showPreviewEventsLabels);

                    int ps = previewStyle;
                    ImGui::RadioButton("Points", &ps, PS_POINTS); ImGui::SameLine();
                    ImGui::RadioButton("Lines", &ps, PS_LINES);
                    previewStyle = (previewStyle_e)ps;

                    ImGui::SliderFloat("Max vel", &traverseMaxVel, 10, 1000);

                    showVec3Editor("Model offset", &modelOffset);
                    //                showVec3Editor("Gantry offset", &gantryOffset);
                    //                showVec3Editor("X carriage offset", &xcarriageOffset);
                    //                showVec3Editor("Z carriage offset", &zcarriageOffset);
                    //                showVec3Editor("Tweak", &tweak);

                    ImGui::Checkbox("Enable light 1", &enableLight1);
                    if ( enableLight1 )
                        showVec3Slider("Light 1", &lightPos1);

                    ImGui::Checkbox("Enable light 2", &enableLight2);
                    if ( enableLight2 )
                        showVec3Slider("Light 2", &lightPos2);

                    ImGui::Checkbox("Enable light 3", &enableLight3);
                    if ( enableLight3 )
                        showVec3Slider("Light 3", &lightPos3);
                }

                //            if (ImGui::CollapsingHeader("Animation"))
                //            {
                //                ImGui::SliderFloat("Speed scale", &animSpeedScale, 0, 5);
                //                showVec3Editor("animLoc", &animLoc);
                //            }

                if ( false && ImGui::CollapsingHeader("Planner settings"))
                {
                    ImGui::SeparatorText("Position constraint");
                    showVec3Editor("Pos", &machineLimits.posLimitUpper);

                    ImGui::SeparatorText("Velocity constraint");
                    showVec3Editor("Vel", &machineLimits.velLimit);

                    ImGui::SeparatorText("Acceleration constraint");
                    showVec3Editor("Acc", &machineLimits.accLimit);

                    ImGui::SeparatorText("Jerk constraint");
                    showVec3Editor("Jerk", &machineLimits.jerkLimit);
                }

                /*if ( false && ImGui::CollapsingHeader("Control points"))
                {

                    if (ImGui::Button("All min jerk")) {
                        for (size_t i = 0; i < plan.moves.size(); i++) {
                            scv::move& m = plan.moves[i];
                            m.blendType = CBT_MIN_JERK;
                        }
                    } ImGui::SameLine();
                    if (ImGui::Button("All max jerk")) {
                        for (size_t i = 0; i < plan.moves.size(); i++) {
                            scv::move& m = plan.moves[i];
                            m.blendType = CBT_MAX_JERK;
                        }
                    }

                    ImGui::SeparatorText("Tests");

                    / *if (ImGui::Button("Default")) {
                        doRandomizePoints = false;
                        loadTestCase_default();
                    } ImGui::SameLine();
                    if (ImGui::Button("Straight")) {
                        doRandomizePoints = false;
                        loadTestCase_straight();
                    } ImGui::SameLine();
                    if (ImGui::Button("Retrace")) {
                        doRandomizePoints = false;
                        loadTestCase_retrace();
                    } ImGui::SameLine();
                    if (ImGui::Button("Malformed")) {
                        doRandomizePoints = false;
                        loadTestCase_malformed();
                    }* /

                    ImGui::Checkbox("Randomize points", &doRandomizePoints);
                    ImGui::Checkbox("Stop on violation", &stopOnViolation);

                    if (ImGui::TreeNode("Customize points"))
                    {
                        if (ImGui::Button("Add point")) {
                            vec3 p = vec3_zero;
                            if ( ! plan.moves.empty() ) {
                                scv::move& el = plan.moves[ plan.moves.size()-1 ];
                                p = el.dst;
                            }
                            scv::move m;
                            m.src = p;
                            m.dst = p + vec3( 2, 2, 2 );
                            m.vel = 10;
                            m.acc = 400;
                            m.jerk = 800;
                            plan.appendMove(m);
                        }

                        if ( ! plan.moves.empty() ) {
                            scv::move& m = plan.moves[0];
                            if (ImGui::TreeNode("Point 0")) {

                                ImGui::SeparatorText("Location");
                                showVec3Editor("Loc 0", &m.src);

                                ImGui::TreePop();
                            }
                        }

                        for (size_t i = 0; i < plan.moves.size(); i++) {
                            scv::move& m = plan.moves[i];
                            sprintf(n, "Point %d", (int)(i+1));
                            if (ImGui::TreeNode(n)) {

                                ImGui::SeparatorText("Location");
                                sprintf(n, "Loc %d", (int)(i+1));
                                showVec3Editor(n, &m.dst);

                                ImGui::SeparatorText("Constraints");
                                sprintf(n, "Vel %d", (int)(i+1));
                                ImGui::InputFloat(n, &m.vel, 0.1f, 1.0f);
                                sprintf(n, "Acc %d", (int)(i+1));
                                ImGui::InputFloat(n, &m.acc, 0.1f, 1.0f);
                                sprintf(n, "Jerk %d", (int)(i+1));
                                ImGui::InputFloat(n, &m.jerk, 0.1f, 1.0f);

                                if ( i > 0 ) {
                                    int e = m.blendType;
                                    ImGui::RadioButton("None", &e, CBT_NONE); ImGui::SameLine();
                                    ImGui::RadioButton("Min jerk", &e, CBT_MIN_JERK); ImGui::SameLine();
                                    ImGui::RadioButton("Max jerk", &e, CBT_MAX_JERK);
                                    m.blendType = (cornerBlendType_e)e;
                                }

                                ImGui::TreePop();
                            }
                        }

                        for (size_t i = 1; i < plan.moves.size(); i++) {
                            scv::move& m = plan.moves[i];
                            scv::move& prevMove = plan.moves[i-1];
                            m.src.x = prevMove.dst.x;
                            m.src.y = prevMove.dst.y;
                            m.src.z = prevMove.dst.z;
                        }

                        ImGui::TreePop();
                    }
                }*/

                // if ( false && ImGui::CollapsingHeader("Stats"))
                // {
                //     ImGui::Text("Calculation time %.1f us", calcTime);
                //     ImGui::Text("Violation: %s", haveViolation ? "yes":"no");
                //     ImGui::Text("Num segments: %d",(int)plan.getSegments().size());
                //     ImGui::Text("Traverse time: %.2f",plan.getTraverseTime());
                //     ImGui::Text("Average framerate: %.1f fps", io.Framerate);
                // }

                ImGui::Text("Average framerate: %.1f fps", io.Framerate);

                //ImGui::Checkbox("Pause graph", &pausePlot);
                //showPlots();


                doLayoutSave(STATUS_WINDOW_TITLE);

                ImGui::End();
            }
        }


        if (show_demo_window)
            //ImPlot::ShowDemoWindow(&show_demo_window);
            ImGui::ShowDemoWindow(&show_demo_window);

        if (show_server_view)
            showServerView(&show_server_view);

        if (show_serial_view)
            showSerialView(&show_serial_view);

        if (show_log_window)
            showLogWindow(&show_log_window);

        if (show_usbcamera_window)
            showUSBCameraControl(&show_usbcamera_window);

        showOpenUSBCameraViews();

        //if ( show_usbcamera_view )
        //    showUSBCameraView(&show_usbcamera_view);

        if ( show_plot_view )
            showPlotsView(&show_plot_view);

        // if ( show_overrides_view )
        //     showOverridesView(&show_overrides_view);

        if ( show_hooks_view )
            showHooksView(&show_hooks_view);

        if ( show_custom_view )
            showCustomView(&show_custom_view);

        if ( show_tweaks_view )
            showTweaksView(&show_tweaks_view);

        if ( show_table_views )
            showTableViewSelection(&show_table_views);

        showTableViews();


        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(43.f / 255.f, 43.f / 255.f, 43.f / 255.f, 100.f / 255.f));
        ImGui::RenderNotifications();
        ImGui::PopStyleVar(1);
        ImGui::PopStyleColor(1);



        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());





        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);


        if ( workspaceLayoutTitleToSave != "" )
            endSavingWorkspaceInfo( workspaceLayoutTitleToSave );
        if ( isLoadingWorkspaceInfo() )
            endLoadingWorkspaceInfo();

        if ( currentlyRunningScriptThread() )
        {

        }
        else
        {
            for (int i = 0; i < (int)commandEditorWindows.size(); i++) {
                CodeEditorWindow* w = commandEditorWindows[i];

                if ( w->buttonFeedback.shouldPreview ) {

                    for (CodeEditorDocument* cd : commandDocuments)
                        cd->clearErrorMarkers();

                    ((CommandEditorWindow*)w)->runCommandList(true);

                    /*CommandList program(blendMethod);
                    program.cornerBlendMaxFraction = cornerBlendMaxOverlap;

                    program.posLimitLower = machineLimits.posLimitLower;
                    program.posLimitUpper = machineLimits.posLimitUpper;
                    for (int i = 0; i < NUM_ROTATION_AXES; i++)
                        program.rotationPositionLimits[i] = machineLimits.rotationPositionLimits[i];

                    w->buttonFeedback.document->clearErrorMarkers();
                    vector<string> lines;
                    w->buttonFeedback.document->editor.GetTextLines(lines);
                    vector<commandListCompileErrorInfo> errorInfos;

                    setActiveScriptLog( ((CommandEditorWindow*)w)->log );

                    if ( parseCommandList(lines, errorInfos, program) ) { // uses heap

                        planGroup.clear();
                        planner* plan = planGroup.addPlan();
                        loadCommandsPreview(program, plan);
                        //memcpy(plan->traversal_rots, lastActualRots, sizeof(plan->traversal_rots));

                        planGroup.calculateMovesForLastPlan();
                        planGroup.resetTraverse();

                        calculateTraversePointsAndEvents();

                        //program.clear();
                    }
                    else {
                        for (commandListCompileErrorInfo &e : errorInfos) {
                            errorMarkerType_t markerType = e.type == LL_WARN ? EMT_WARNING : EMT_ERROR;
                            w->buttonFeedback.document->addErrorMarker(e.row, markerType, e.message);
                        }
                        w->buttonFeedback.document->showErrorMarkers();
                    }*/

                    animLoc = lastActualPos;
                    memcpy(animRots, lastActualRots, sizeof(animRots));
                }
                else if ( w->buttonFeedback.shouldRun ) {

                    for (CodeEditorDocument* cd : commandDocuments)
                        cd->clearErrorMarkers();

                    ((CommandEditorWindow*)w)->runCommandList(false);

                    animLoc = lastActualPos;
                    memcpy(animRots, lastActualRots, sizeof(animRots));

                    /*for (CodeEditorDocument* cd : commandDocuments)
                        cd->clearErrorMarkers();

                    CommandList program(blendMethod);
                    program.cornerBlendMaxFraction = cornerBlendMaxOverlap;

                    program.posLimitLower = machineLimits.posLimitLower;
                    program.posLimitUpper = machineLimits.posLimitUpper;
                    for (int i = 0; i < NUM_ROTATION_AXES; i++)
                        program.rotationPositionLimits[i] = machineLimits.rotationPositionLimits[i];

                    w->buttonFeedback.document->clearErrorMarkers();
                    vector<string> lines;
                    w->buttonFeedback.document->editor.GetTextLines(lines);
                    //vector<codeCompileErrorInfo> errorInfos;
                    if ( parseCommandList(lines, program) ) { // uses heap

    //                uint8_t* data = new uint8_t[program.getSize()];
    //                program.pack(data);
    //                delete[] data;

                        if ( sanityCheckCommandList(program) ) {
                            sendPackable(MT_SET_PROGRAM, program);
                            lastTrajResult = TR_NONE;
                        }
                        //program.clear();
                    }
                    else {
                        for (commandListCompileErrorInfo &e : errorInfos) {
                            errorMarkerType_t markerType = e.type == LL_WARN ? EMT_WARNING : EMT_ERROR;
                            w->buttonFeedback.document->addErrorMarker(e.row, markerType, e.message);
                        }
                        w->buttonFeedback.document->showErrorMarkers();
                    }*/

                }
            }

            for (CodeEditorWindow* w : scriptEditorWindows) {
                if ( w->buttonFeedback.shouldPreview ) {
                    saveAllDocuments(commandDocuments);
                    for (CodeEditorDocument* cd : commandDocuments)
                        cd->clearErrorMarkers();

                    animLoc = lastActualPos;
                    memcpy(animRots, lastActualRots, sizeof(animRots));

                    ((ScriptEditorWindow*)w)->runScript(true);
                }
                else if ( w->buttonFeedback.shouldRun ) {
                    saveAllDocuments(commandDocuments);
                    for (CodeEditorDocument* cd : commandDocuments)
                        cd->clearErrorMarkers();

                    ((ScriptEditorWindow*)w)->runScript(false);
                }
            }
        }
    }

    for (CodeEditorWindow* d : commandEditorWindows) {
        delete d;
    }
    for (CodeEditorWindow* d : scriptEditorWindows) {
        delete d;
    }

    closeAllPorts();

    stopRequester();
    stopSubscriber();

    closeAllUSBCameras();
    freeAsyncFrameBuffer();

    cleanupScriptEngine();

    saveEventHooksToDB();
    saveWorkspaceToDB_windowsOpen("autosave");

    closeDatabase();

    releaseModel(nozzleTipModel);
    releaseModel(nozzleModel);
    releaseModel(zcarriageModel);
    releaseModel(xcarriageModel);
    releaseModel(gantryModel);
    releaseModel(mainModel);
    cleanupAssimp();

    //cleanupVideoView();

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    // do these after destroying the ImGui context because the 'hasOwnFile' is needed
    for (CodeEditorDocument* d : commandDocuments) {
        delete d;
    }
    for (CodeEditorDocument* d : scriptDocuments) {
        delete d;
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

// void script_notify(string msg, int type, int timeout ) {
//     if ( type < 0 || type > ImGuiToastType_Info )
//         type = ImGuiToastType_None;
//     if ( timeout < 10 )
//         timeout = 10;
//     ImGui::InsertNotification({ type, timeout, msg.c_str() });
// }








