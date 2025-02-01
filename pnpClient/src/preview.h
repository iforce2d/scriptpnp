#ifndef PREVIEW_H
#define PREVIEW_H

#include <string>

#include "scv/planner.h"
#include "commandlist.h"
#include "plangroup.h"

extern PlanGroup planGroup_preview;

struct traverseEvent_t
{
    float t;
    scv::vec3 pos;
    scv::traverseFeedback_t fb;
    std::string label;
};

struct traverseEventLabel_t
{
    scv::vec3 pos;
    std::string text;
};

struct traversePoint_t {
    scv::vec3 pos;
    scv::vec3 vel;
};

enum previewStyle_e {
    PS_POINTS,
    PS_LINES
};

extern float animSpeedScale;
extern scv::vec3 animLoc;
extern float animRots[4];
extern scv::vec3 lastActualPos;
extern scv::vec3 lastActualVel;
extern float lastActualRots[4];
extern PlanGroup plans;
extern cornerBlendMethod_e blendMethod;
extern float cornerBlendMaxOverlap;
extern previewStyle_e previewStyle;
extern float traverseMaxVel;

extern std::vector<traversePoint_t> traversePoints;
extern std::vector<traverseEventLabel_t> traverseLabels;

void loadCommandsPreview(CommandList& program, scv::planner* plan);

void setPreviewMoveLimitsFromCurrentActual();

void resetTraversePointsAndEvents();

bool doPreview(std::vector<std::string> &lines);
void calculateTraversePointsAndEvents();
scv::vec3 getPreviewColorFromSpeed(scv::vec3 v);

#endif // PREVIEW_H
