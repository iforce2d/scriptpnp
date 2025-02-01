#ifndef HOMING_H
#define HOMING_H

#include <vector>
#include <chrono>

#include "../common/scv/planner.h"
#include "../common/config.h"
#include "../common/pnpMessages.h"

enum homingPhase_e {
    HP_APPROACH1, // ordering is important, the state will be incremented
    HP_BACKOFF1,
    HP_APPROACH2,
    HP_BACKOFF2,
    HP_DONE
};

extern std::vector<int> homing_axesRemaining;
extern int homing_currentAxis;
extern homingResult_e homing_result;
extern scv::planner homingPlanner;
extern homingPhase_e homing_phase;
extern std::chrono::steady_clock::time_point homingStartTime;
extern long int homingTimeout;
extern uint8_t homing_homedAxes;

void resetAxesAboutToHome(uint8_t* orderingOneIndexed);
void prepareHomingPlanner();
void startHomeNextAxis();
void doHomingUpdate();
bool checkHomingStartConditions();

#endif
