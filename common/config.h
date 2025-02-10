#ifndef CONFIG_H
#define CONFIG_H

#include "../common/commands.h"

#ifndef PACKED
#define PACKED __attribute__((__packed__))
#endif

#define NUM_MOTION_AXES 8
#define NUM_HOMABLE_AXES 4

extern motionLimits currentMoveLimits;
extern motionLimits currentRotationLimits[NUM_ROTATION_AXES];

struct tmcSettings_t {
    int microsteps;
    int current;
} PACKED;

struct homingParams_t { // per axis
    int triggerPin; // 0 indexed, -1 = invalid
    int triggerState; // 0 = trigger when low, 1 = trigger when high
    int direction; // 0 = descend, 1 = ascend

    float approachspeed1;
    float approachspeed2;
    float backoffDistance1;
    float backoffDistance2;
    float homedPosition;
} PACKED;

struct probingParams_t {
    int digitalTriggerPin; // 0 indexed, -1 = invalid
    int digitalTriggerState; // 0 = trigger when low, 1 = trigger when high
    float vacuumStep;
    int vacuumSniffPin;
    int vacuumSniffState;
    int vacuumSniffTimeMs;
    int vacuumReplenishTimeMs;
    float approachspeed1;
    float approachspeed2;
    float backoffDistance1;
    float backoffDistance2;
} PACKED;

void initTMCSettings(tmcSettings_t& p);
void initHomingParams(homingParams_t& p);
void initProbingParams(probingParams_t& p);

extern float stepsPerUnit[NUM_MOTION_AXES];
extern float joggingSpeeds[NUM_MOTION_AXES];

extern uint16_t estopDigitalOutState;
extern uint16_t estopDigitalOutUsed;
extern float estopPWMState[NUM_PWM_VALS];
extern uint8_t estopPWMUsed;

extern tmcSettings_t tmcParams[NUM_MOTION_AXES];
extern homingParams_t homingParams[NUM_HOMABLE_AXES];

extern probingParams_t probingParams;

extern int32_t loadcellCalibrationRawOffset;
extern float loadcellCalibrationWeight;

extern char homingOrder[9];

bool readConfigFile();
bool saveConfigToFile();

void printOverrides();

#endif
