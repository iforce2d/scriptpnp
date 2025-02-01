
#ifndef THREADS_H
#define THREADS_H

#include <atomic>
#include "../common/scv/planner.h"

#define JOINTS 4
#define NUM_HOMABLE_AXES    4

/*enum homingResult_e {
    HR_NONE, // estop
    HR_SUCCESS,
    HR_FAIL
};

enum trajectoryResult_e {
    TR_NONE, // estop
    TR_SUCCESS,
    TR_FAIL
};*/

// from realtime to normal thread
typedef struct motionStatus {
    bool spiOk;
    int mode;
    scv::vec3 targetPos;
    scv::vec3 actualPos;
    scv::vec3 actualVel;
    float actualRots[NUM_ROTATION_AXES];
    //float speedScale;
    //float jogSpeedScale;
    //float freq;
    uint8_t homedAxes;
    uint16_t outputs;
    uint16_t inputs;
    int32_t rotary;
    uint16_t adc[2];
    uint16_t pressure;
    int32_t loadcell;
    uint16_t pwm;
    uint8_t homingResult;
    uint8_t probingResult;
    uint8_t trajectoryResult;
} motionStatus;

// from normal to realtime thread
typedef struct motionCommand {
    bool estop;
    float speedScale;
    float jogSpeedScale;
    int8_t jogDirs[3];
    scv::planner* traj;

    uint8_t homeAxes[NUM_HOMABLE_AXES]; // 1 indexed, so 0 = skip. Any non-zero will start homing

    uint8_t probeType; // 1 indexed, so 0 = skip. Non-zero will start probing
    float probeZ;
    float probeWeight;

    uint16_t outputBits;
    uint16_t outputChanged;

    float pwm;
    uint8_t rgb[6];
    uint16_t microsteps[JOINTS];
    uint16_t rmsCurrent[JOINTS];
} motionCommand;


struct message_fromRT_toNT {
    std::atomic<bool> ready;
    motionStatus mStatus;
};

struct message_fromNT_toRT {
    std::atomic<bool> ready;
    motionCommand mCmd;
};


void rtReport(motionStatus* sts);
int rtReportCheck(motionStatus *d);
int rtQueueLength();

void ntCommand(motionCommand *cmd);
int ntCommandCheck(motionCommand *cmd);
int ntQueueLength();

void initInterThread();

#endif
