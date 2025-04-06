#ifndef PROBING_H
#define PROBING_H

#include "../common/scv/planner.h"
#include "../common/config.h"
#include "../common/pnpMessages.h"

enum probingPhase_e {
    PP_APPROACH1, // ordering is important, the state will be incremented
    PP_BACKOFF1,
    PP_APPROACH2,
    PP_BACKOFF2,
    PP_DONE
};

enum probingPhaseVacuumApproach_e {
    PPVA_SNIFFING, // open nozzle and wait
    PPVA_CLOSING,  // close nozzle only
    PPVA_REPOSITIONING // moving and waiting (maybe)
};

extern uint8_t probing_type;
extern probingResult_e probing_result;
extern scv::planner probingPlanner;
extern probingPhase_e probing_phase;
//extern probingPhaseVacuumApproach_e probing_phase_vacuum;
extern float probing_resultHeight;

//void prepareProbingPlanner();
void startProbe();
void doProbingUpdate();
bool checkProbingStartConditions();

void resetLoadcell();
void updateLoadcell(int32_t loadCellRaw);
float getLoadCellBaseline();

#endif
