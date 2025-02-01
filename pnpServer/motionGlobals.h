#ifndef MOTION_GLOBALS_H
#define MOTION_GLOBALS_H

#include "../common/scv/planner.h"
#include "interThread.h"
#include "../common/pnpMessages.h"

#define NUM_ROTATION_AXES 4

/*enum motionMode_e {
    MM_NONE = 0,
    MM_TRAJECTORY,
    MM_JOG,
    MM_HOMING
};

enum trajectoryResult_e {
    TR_NONE, // user estop
    TR_SUCCESS,
    TR_FAIL_LIMIT_TRIGGERED
};*/

extern scv::vec3 p;
extern scv::vec3 v;
//extern scv::vec3 a;
extern float rots[NUM_ROTATION_AXES];
extern scv::traverseFeedback_t segmentFeedback;

extern scv::vec3 offsetAtHome;
extern motionCommand rtCommand;
extern motionMode_e motionMode;

extern scv::planner* currentTraj;

#endif
