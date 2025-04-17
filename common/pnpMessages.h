#ifndef PNP_MESSAGES_H
#define PNP_MESSAGES_H

#include <stdint.h>
#include "commands.h"

#include "../common/config.h"

#define MESSAGE_VERSION 4

#define NUM_ROTATION_AXES 4

#ifndef PACKED
#define PACKED __attribute__((__packed__))
#endif

#define JOINTS 4

#define COMMAND_MESSAGE_LIST \
    tmpMacro(MT_ACK)\
    tmpMacro(MT_NACK)\
    tmpMacro(MT_SET_ESTOP)\
    tmpMacro(MT_SET_SPEED_SCALE)\
    tmpMacro(MT_SET_JOG_SPEED_SCALE)\
    tmpMacro(MT_SET_JOG_STATUS)\
    tmpMacro(MT_SET_PROGRAM)\
    tmpMacro(MT_SET_MOVETO)\
    tmpMacro(MT_SET_DIGITAL_OUTPUTS)\
    tmpMacro(MT_SET_PWM_OUTPUT)\
    tmpMacro(MT_SET_RGB_OUTPUT)\
    tmpMacro(MT_SET_TMC_PARAMS)\
    tmpMacro(MT_HOME_AXES)\
    tmpMacro(MT_HOME_ALL)\
    tmpMacro(MT_RESET_LOADCELL)\
    tmpMacro(MT_PROBE)\
    tmpMacro(MT_CONFIG_STEPS_FETCH)\
    tmpMacro(MT_CONFIG_STEPS_SET)\
    tmpMacro(MT_CONFIG_WORKAREA_FETCH)\
    tmpMacro(MT_CONFIG_WORKAREA_SET)\
    tmpMacro(MT_CONFIG_INITSPEEDS_FETCH)\
    tmpMacro(MT_CONFIG_INITSPEEDS_SET)\
    tmpMacro(MT_CONFIG_TMC_FETCH)\
    tmpMacro(MT_CONFIG_TMC_SET)\
    tmpMacro(MT_CONFIG_HOMING_FETCH)\
    tmpMacro(MT_CONFIG_HOMING_SET)\
    tmpMacro(MT_CONFIG_JOGGING_FETCH)\
    tmpMacro(MT_CONFIG_JOGGING_SET)\
    tmpMacro(MT_CONFIG_OVERRIDES_FETCH)\
    tmpMacro(MT_CONFIG_OVERRIDES_SET)\
    tmpMacro(MT_CONFIG_LOADCELL_CALIB_SET)\
    tmpMacro(MT_CONFIG_LOADCELL_CALIB_FETCH)\
    tmpMacro(MT_CONFIG_PROBING_SET)\
    tmpMacro(MT_CONFIG_PROBING_FETCH)\
    tmpMacro(MT_CONFIG_ESTOP_SET)\
    tmpMacro(MT_CONFIG_ESTOP_FETCH)\
    tmpMacro(MT_MAX)


#define tmpMacro(blah) blah,

enum commandMessageType_e {
    MT_NONE = 0,
    COMMAND_MESSAGE_LIST
};

#undef tmpMacro




enum motionMode_e {
    MM_NONE = 0,
    MM_TRAJECTORY,
    MM_JOG,
    MM_HOMING,
    MM_PROBING
};

enum homingResult_e {
    HR_NONE, // user estop
    HR_SUCCESS,
    HR_FAIL_CONFIG,
    HR_FAIL_LIMIT_ALREADY_TRIGGERED,
    HR_FAIL_TIMED_OUT
};

enum probingResult_e {
    PR_NONE, // user estop
    PR_SUCCESS,
    PR_FAIL_CONFIG,
    PR_FAIL_NOT_HOMED,
    PR_FAIL_NOT_TRIGGERED,
    PR_FAIL_ALREADY_TRIGGERED
};

enum trajectoryResult_e {
    TR_NONE, // user estop
    TR_SUCCESS,
    TR_FAIL_CONFIG,
    TR_FAIL_NOT_HOMED,
    TR_FAIL_OUTSIDE_BOUNDS,
    TR_FAIL_FOLLOWING_ERROR,
    TR_FAIL_LIMIT_TRIGGERED,
};

enum probeType_t {
    PT_DIGITAL,
    PT_LOADCELL,
    PT_VACUUM
};

enum probePhase_t {
    PP_FAST_APPROACH = 0x1,
    PP_SLOW_APPROACH = 0x2
};

typedef struct PACKED {
    uint8_t messageVersion;
    bool spiOk;
    int mode;
    uint8_t homingResult;
    uint8_t probingResult;
    uint8_t trajResult;

    float actualPosX;
    float actualPosY;
    float actualPosZ;
    float actualVelX;
    float actualVelY;
    float actualVelZ;
    float actualRots[NUM_ROTATION_AXES];

    float limMoveVel;
    float limMoveAcc;
    float limMoveJerk;
    float limRotateVel;
    float limRotateAcc;
    float limRotateJerk;

    float speedScale;
    float jogSpeedScale;

    uint8_t homedAxes;
    uint16_t outputs;

    uint16_t inputs;
    int32_t rotary;
    uint16_t adc[2];
    uint16_t pressure;
    int32_t loadcell;
    uint16_t pwm;
    float weight;
    float probedHeight;
} clientReport_t;




typedef struct PACKED {
    uint8_t val;
} msg_setEstop;

typedef struct PACKED {
    float scale;
} msg_setSpeedScale;

typedef struct PACKED {
    int8_t jogDirs[3];
} msg_setJogStatus;

typedef struct PACKED {
    //float speed;
    pose dst;
} msg_setMoveto;

typedef struct PACKED {
    uint16_t bits;
    uint16_t changed;
} msg_setDigitalOutputs;

typedef struct PACKED {
    uint16_t val;
} msg_setPWMOutput;

typedef struct PACKED {
    uint8_t rgb[6];
} msg_setRGBOutput;

typedef struct PACKED {
    uint16_t microsteps[JOINTS];
    uint16_t rmsCurrent[JOINTS];
} msg_setTMCParams;

typedef struct PACKED {
    uint8_t ordering[4];
} msg_homeAxes;

typedef struct PACKED {
    uint8_t dummy;
} msg_resetLoadcell;

typedef struct PACKED {
    uint8_t type;
    uint8_t flags;
    float z;
} msg_probe;



typedef struct PACKED {
    float perUnit[NUM_MOTION_AXES];
} msg_configSteps;

typedef struct PACKED {
    float x;
    float y;
    float z;
    float a;
} msg_configWorkingArea;

typedef struct PACKED {

    float velLimitX;
    float velLimitY;
    float velLimitZ;

    float accLimitX;
    float accLimitY;
    float accLimitZ;

    float jerkLimitX;
    float jerkLimitY;
    float jerkLimitZ;

    float rotLimitVel;
    float rotLimitAcc;
    float rotLimitJerk;

    // initial limits
    float initialMoveVel;
    float initialMoveAcc;
    float initialMoveJerk;
    float initialRotateVel;
    float initialRotateAcc;
    float initialRotateJerk;

    float maxOverlapFraction;
} msg_configMotionLimits;

typedef struct PACKED {
    tmcSettings_t settings[NUM_MOTION_AXES];
} msg_configTMC;

typedef struct PACKED {
    homingParams_t params[NUM_HOMABLE_AXES];
    char order[9];
} msg_configHoming;

typedef struct PACKED {
    float speed[NUM_MOTION_AXES];
} msg_configJogging;

typedef struct PACKED {
    int32_t rawOffset;
    float weight;
} msg_configLoadCellCalib;

typedef struct PACKED {
    probingParams_t params;
} msg_configProbing;

typedef struct PACKED {
    uint16_t outputs;
    uint16_t outputsUsed;
    float pwmVal[NUM_PWM_VALS];
    uint8_t pwmUsed;
} msg_configEstop;



// Note that a commandRequest_t must be POD because it might be queued in requestsQueue

typedef struct PACKED {
    uint16_t version;
    uint16_t type;
    union {
        msg_setEstop setEstop;
        msg_setSpeedScale setSpeedScale;
        msg_setJogStatus setJogStatus;
        msg_setMoveto setMoveto;
        msg_setDigitalOutputs setDigitalOutputs;
        msg_setPWMOutput setPWMOutput;
        msg_setRGBOutput setRGBOutput;
        msg_setTMCParams setTMCParams;
        msg_homeAxes homeAxes;
        msg_probe probe;
        msg_resetLoadcell resetLoadcell;
        msg_configSteps configSteps;
        msg_configWorkingArea workingArea;
        msg_configMotionLimits motionLimits;
        msg_configTMC tmcSettings;
        msg_configHoming homingParams;
        msg_configJogging jogParams;
        msg_configLoadCellCalib loadcellCalib;
        msg_configProbing probingParams;
        msg_configEstop estopParams;
    } PACKED;
} commandRequest_t;







typedef struct PACKED {
    uint16_t version;
    uint16_t type;
    union {
        msg_configSteps configSteps;
        msg_configWorkingArea workingArea;
        msg_configMotionLimits motionLimits;
        msg_configTMC tmcSettings;
        msg_configHoming homingParams;
        msg_configJogging jogParams;
        msg_configLoadCellCalib loadcellCalib;
        msg_configProbing probingParams;
        msg_configEstop estopParams;
    } PACKED;
} commandReply_t;



const char* getMessageName(uint8_t type);
const char* getModeName(int mode);
const char* getHomingResultName(int mode);
const char* getProbingResultName(int mode);
const char* getTrajectoryResultName(int mode);

commandRequest_t createCommandRequest(uint16_t msgType);

#endif
