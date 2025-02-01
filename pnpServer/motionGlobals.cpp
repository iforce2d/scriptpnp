
#include "motionGlobals.h"

using namespace scv;

scv::vec3 p = scv::vec3_zero;
scv::vec3 v = scv::vec3_zero;
//scv::vec3 a = scv::vec3_zero;
float rots[NUM_ROTATION_AXES];
scv::traverseFeedback_t segmentFeedback;

scv::vec3 offsetAtHome = scv::vec3_zero;
motionCommand rtCommand = {0};
motionMode_e motionMode = MM_NONE;

planner* currentTraj = NULL;
