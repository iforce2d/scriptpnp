#ifndef SERVER_VIEW_H
#define SERVER_VIEW_H

#include "../common/config.h"

#ifndef NO_EXTERNS_FROM_SERVER_VIEW_HEADER

extern char serverHostname[128];

extern float config_stepsPerUnit[NUM_MOTION_AXES];
extern float config_jogSpeed[NUM_MOTION_AXES];

extern float config_stepsPerUnitX;
extern float config_stepsPerUnitY;
extern float config_stepsPerUnitZ;
extern float config_stepsPerUnitA;

extern float config_workingAreaX;
extern float config_workingAreaY;
extern float config_workingAreaZ;

extern tmcSettings_t config_tmc[NUM_MOTION_AXES];
extern homingParams_t config_homing[NUM_HOMABLE_AXES];

extern probingParams_t config_probing;

extern int32_t config_loadcellCalibrationRawOffset;
extern float config_loadcellCalibrationWeight;

#endif

void initDefaultConfigs();
void fetchAllServerConfigs();

void showServerView(bool* p_open);

#endif // SERVER_VIEW_H
