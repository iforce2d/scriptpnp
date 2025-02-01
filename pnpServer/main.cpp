
#include <experimental/filesystem>
#include <thread>
#include <cstring>
#include <sys/mman.h> // mlockall
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <fstream>

#include "../common/scv/planner.h"
#include "weeny.h"
#include "RTThread.h"
#include "interThread.h"

#include "server.h"
#include "../common/commandlist.h"
#include "log.h"

#include "../common/machinelimits.h"
#include "../common/overrides.h"

#include "json/json.h"
#include "../common/config.h"

#include "motionGlobals.h"
#include "estop.h"
#include "homing.h"
#include "probing.h"
#include "loadcell.h"

//#include "zhelpers.h"

#define DO_ZMQ

#define MIN_SPI_TRANSFERS   1

#define INVALID_FLOAT 0xFFFFFFFF

using namespace std::chrono_literals;
using namespace scv;

//typedef struct PACKED {
//    float vel;
//    float acc;
//    float jerk;
//} motionLimits;

//motionLimits currentMoveLimits;
//motionLimits currentRotationLimits[NUM_ROTATION_AXES];

// scv::vec3 p = scv::vec3_zero;
// scv::vec3 v = scv::vec3_zero;
// scv::vec3 a = scv::vec3_zero;
// float rots[NUM_ROTATION_AXES];
// scv::traverseFeedback_t segmentFeedback;

volatile bool allowSPI = false;
unsigned long spiTransferCount = 0;
volatile bool estop = false;
volatile bool allowOverrideExecution = true;

enum jogPhase_e {
    JP_IDLE,
    JP_MOVING,
    JP_STOPPING
};

struct TMCDriverSetting {
    int microsteps; // 0, 2, 4, 8, 16, 32, 64, 128, 266
    int current;    // up to 2000 mA
    TMCDriverSetting() {
        microsteps = -1;
        current = -1;
    }
};

struct jogInfo {
    jogPhase_e phase;
    int lastDir;
    scv::planner planner;
    float vel;
};

int8_t jogDirs[3];
jogInfo jogInfos[3];

/*enum motionMode_e {
    MM_NONE = 0,
    MM_TRAJECTORY,
    MM_JOG,
    MM_HOMING
};*/









bool allSPIOk = true;



void applyChangedBits(uint16_t& val, uint16_t bits, uint16_t changed) {
    val = (val & ~changed) | (bits & changed);
}

bool anyNonZero(uint8_t* bytes, int num) {
    for (int i = 0; i < num; i++) {
        if ( bytes[i] != 0 )
            return true;
    }
    return false;
}

//void applyHomeOffsetToMove(move& m) {
//    m.src -= offsetAtHome;
//    m.dst -= offsetAtHome;
//}

scv::rotate estopRotate;



void checkJogFinish() {
    for (int i = 0; i < 3; i++) {
        if ( jogInfos[i].phase == JP_MOVING ) {
            int dir = rtCommand.jogDirs[i];
            //printf("dir: %d, lastDir: %d\n", dir, jogInfos[i].lastDir);
            if ( dir == 0 || (dir != jogInfos[i].lastDir) ) {
                float velMag = fabs(v[i]);
                if ( velMag > 0.00001 ) {
                    //printf("Creating estop move for jog axis %d: velmag: %f, dir: %d, lastDir:%d\n", i, velMag, dir, jogInfos[i].lastDir);
                    jogInfos[i].planner.clear();
                    jogInfos[i].planner.resetTraverse();

                    move m;
                    m.moveType = MT_ESTOP;
                    m.vel = velMag;
                    m.acc =  999999;
                    m.jerk = 999999;
                    m.src = p;
                    m.dst = p + 0.01 * v;

                    jogInfos[i].planner.appendMove(m);
                    jogInfos[i].planner.calculateMoves();
                    //jogInfos[i].planner.addOffsetToMoves(offsetAtHome);

                    jogInfos[i].phase = JP_STOPPING;
                }
            }
        }
    }
}

void checkJogStart() {
    for (int i = 0; i < 3; i++) {
        if ( jogInfos[i].phase == JP_MOVING || jogInfos[i].phase == JP_STOPPING )
            continue;
        int dir = rtCommand.jogDirs[i];
        if ( dir != 0 ) {
            //printf("Creating jog move for axis %d, dir:%d\n", i, dir);
            jogInfos[i].planner.clear();
            jogInfos[i].planner.resetTraverse();

            move m;
            m.moveType = MT_NORMAL;
            m.vel = jogInfos[i].vel;
            m.acc =  999999;
            m.jerk = 999999;
            m.src = p;
            m.dst = p;
            m.dst[i] += rtCommand.jogDirs[i] * 1000; // <-- max distance one jog keypress can do

            jogInfos[i].planner.appendMove(m);
            jogInfos[i].planner.calculateMoves();
            //jogInfos[i].planner.addOffsetToMoves(offsetAtHome);

            //jogInfos[i].planner.printMoves();

            jogInfos[i].phase = JP_MOVING;

            //printf("Setting mode to JOG\n");
            motionMode = MM_JOG;
        }
    }
}

void setFrequencyOutputs() {

    float advanceTime = 0.001 * rtCommand.speedScale;

    float error, command, feedback, deadband;
    float posGain, velFFgain;//, accFFgain;

    posGain = 220.0;
    velFFgain = 1;
    //accFFgain = 1;
    for (int i = 0; i < 3; i++) {
        deadband = fabs(0.75 / data.pos_scale[i]);

        command = p[i];
        feedback = data.pos_fb[i];

        error = command - feedback;

        if (error > deadband)
        {
            error -= deadband;
        }
        else if (error < -deadband)
        {
            error += deadband;
        }
        else
        {
            error = 0;
        }

        float velFF = v[i];// (i == 0) ? v.x   :   (i == 1) ? v.y  :  v.z;
        //float accFF = a[i];//(i == 0) ? a.x   :   (i == 1) ? a.y  :  a.z;

        velFF *= advanceTime;
        //accFF *= advanceTime * advanceTime;

        float vel_cmd = posGain * error +
                        velFFgain * velFF;// +
                        //accFFgain * accFF;

        vel_cmd = vel_cmd * data.pos_scale[i];

        data.freq[i] = vel_cmd;
    }

//    if ( data.outputs & 0x1 ) {
//        printf("p: %f %f %f\n", p[0], p[1], p[2]);
//    }

    // rotation
    //posGain = 10.0;
    int i = 3;
    {
        deadband = fabs(0.75 / data.pos_scale[i]);

        command = rots[0];
        feedback = data.pos_fb[i];

        error = command - feedback;

        if (error > deadband)
        {
            error -= deadband;
        }
        else if (error < -deadband)
        {
            error += deadband;
        }
        else
        {
            error = 0;
        }

        float velFF = 0;//v[i];// (i == 0) ? v.x   :   (i == 1) ? v.y  :  v.z;
        //float accFF = 0;//a[i];//(i == 0) ? a.x   :   (i == 1) ? a.y  :  a.z;

        velFF *= advanceTime;
        //accFF *= advanceTime * advanceTime;

        float vel_cmd = posGain * error +
                        velFFgain * velFF;// +
                        //accFFgain * accFF;

        vel_cmd = vel_cmd * data.pos_scale[i];

        data.freq[i] = vel_cmd;
    }
}

void processOverrides() {

    // skip this if main thread is updating the overrides config
    if ( ! allowOverrideExecution )
        return;

    for (int i = 0; i < (int)overrideConfigs.size(); i++) {
        OverrideConfig &config = overrideConfigs[i];

        bool conditionMet = false;

        if (config.condition.motionAxis) {
            int axis = getBitPosition(config.condition.motionAxis);
            if ( axis < 3 ) {
                if ( config.condition.comparison == OCC_LESS_THAN ) {
                    conditionMet = (p[axis] + offsetAtHome[axis]) < config.condition.val;
                }
                else if ( config.condition.comparison == OCC_MORE_THAN ) {
                    conditionMet = (p[axis] + offsetAtHome[axis]) > config.condition.val;
                }
            }
        }
        else if ( config.condition.digitalOutput ) {
            int axis = getBitPosition(config.condition.digitalOutput);
            if ( axis < 16 ) {
                uint16_t bitIsSet = data.outputs & config.condition.digitalOutput;
                if ( config.condition.comparison == OCC_LESS_THAN ) {
                    conditionMet = ! bitIsSet;
                }
                else if ( config.condition.comparison == OCC_MORE_THAN ) {
                    conditionMet = bitIsSet;
                }
            }
        }
        else if ( config.condition.pressure ) {
            int axis = getBitPosition(config.condition.pressure);
            if ( axis < 4 ) {
                float vac = ((float)data.pressure - 50000) / 500.0f;
                if ( config.condition.comparison == OCC_LESS_THAN ) {
                    conditionMet = vac < config.condition.val;
                }
                else if ( config.condition.comparison == OCC_MORE_THAN ) {
                    conditionMet = vac > config.condition.val;
                }
            }
        }
        else if ( config.condition.loadcell ) {
            int axis = getBitPosition(config.condition.loadcell);
            if ( axis < 4 ) {
                if ( config.condition.comparison == OCC_LESS_THAN ) {
                    conditionMet = data.loadcell < config.condition.val;
                }
                else if ( config.condition.comparison == OCC_MORE_THAN ) {
                    conditionMet = data.loadcell > config.condition.val;
                }
            }
        }

        std::vector<overrideAction_t> *actions = &config.failActions;
        if ( conditionMet )
            actions = &config.passActions;

        for (int k = 0; k < (int)actions->size(); k++) {
            overrideAction_t &action = (*actions)[k];
            if ( action.digitalOutput ) {
                int axis = getBitPosition(action.digitalOutput);
                if ( axis < 16 ) {
                    if ( action.val )
                        data.outputs |= action.digitalOutput;
                    else
                        data.outputs &= ~action.digitalOutput;
                }
            }
            else if ( action.pwmOutput ) {
                int axis = getBitPosition(action.pwmOutput);
                if ( axis < 4 ) {
                    data.spindleSpeed = (uint16_t)(action.val * 65535);
                }
            }
        }
    }
}

trajectoryResult_e traj_result = TR_NONE;

bool anyLimitSwitchTriggered(int &which) { //return false;
    for (int i = 0; i < 3; i++) {

        homingParams_t& params = homingParams[i];

        int pinState = (data.inputs & (1 << params.triggerPin)) ? 1 : 0;

        if ( pinState == params.triggerState ) {
            which = i;
            return true;
        }
    }
    return false;
}

void doTrajectoryUpdate() {

    bool stillRunning = false;

    float advanceTime = 0.001;

    if ( currentTraj ) {

        int which = 0;
        if ( anyLimitSwitchTriggered(which) ) {
            g_log.log(LL_INFO, "limit switch triggered during trajectory");
            // no estop trajectory, just slam to a stop
            currentTraj = NULL;
            traj_result = TR_FAIL_LIMIT_TRIGGERED;
            motionMode = MM_NONE;
            v = vec3_zero;
            return;
        }

        vec3 actualPos = vec3(data.pos_fb[0], data.pos_fb[1], data.pos_fb[2]);
        vec3 followError = p - actualPos;

        for (int i = 0; i < 3; i++) {
            float fe = fabsf( followError[i] );
            if ( fe > 10 ) {
                g_log.log(LL_INFO, "follow error during trajectory");
                // no estop trajectory, just slam to a stop
                currentTraj = NULL;
                traj_result = TR_FAIL_FOLLOWING_ERROR;
                motionMode = MM_NONE;
                v = vec3_zero;
                return;
            }
        }

        stillRunning = currentTraj->advanceTraverse( advanceTime, rtCommand.speedScale, &p, &v, rots, &segmentFeedback );

        v *= rtCommand.speedScale;

        if ( segmentFeedback.stillRunning )
            stillRunning = true;

        //if ( stillRunning )
        //    printf("at: %f %f %f, r = %f\n", p.x, p.y, p.z, rots[0]);

        if ( segmentFeedback.digitalOutputChanged ) {
            uint16_t bits = segmentFeedback.digitalOutputBits;
            uint16_t changed = segmentFeedback.digitalOutputChanged;
            //printf("digitalOutputChanged: %d %d\n", bits, changed);
            applyChangedBits( data.outputs, bits, changed );
        }

        if ( segmentFeedback.pwmOutput[0] != INVALID_FLOAT ) {
            //printf("pwm output changed: %f\n", segmentFeedback.pwmOutput[0]);
            data.spindleSpeed = segmentFeedback.pwmOutput[0] * 65535;
        }

        if ( ! stillRunning ) {
            currentTraj = NULL;
            traj_result = TR_SUCCESS;
            motionMode = MM_NONE;
            v = vec3_zero;
        }
    }
}

void doJogUpdate() {

    bool stillRunning = false;
    float advanceTime = 0.001;

    for (int i = 0; i < 3; i++) {
        vec3 jogP, jogV;
        float jogRots[NUM_ROTATION_AXES];
        bool thisAxisRunning = jogInfos[i].planner.advanceTraverse( advanceTime, rtCommand.jogSpeedScale, &jogP, &jogV, jogRots, &segmentFeedback );

        jogV *= rtCommand.jogSpeedScale;

        stillRunning |= thisAxisRunning;
        if ( thisAxisRunning ) {
            p[i] = jogP[i];
            v[i] = jogV[i];
            //a[i] = jogA[i];
        }
        else
            jogInfos[i].phase = JP_IDLE;
    }
    if ( ! stillRunning ) {
        motionMode = MM_NONE;
        v = vec3_zero;
    }
}

int lastMode = 0;

void doRTReport() {
    vec3 actualPos = vec3(data.pos_fb[0], data.pos_fb[1], data.pos_fb[2]);

    if ( motionMode != lastMode ) {
        printf("Entered mode: %d\n", motionMode);
        lastMode = motionMode;
    }

    motionStatus sts;
    sts.mode = motionMode;
    sts.targetPos = p + offsetAtHome;
    sts.actualPos = actualPos + offsetAtHome;
    sts.actualVel = v;
    sts.actualRots[0] = data.pos_fb[3];
    sts.actualRots[1] = 0;
    sts.actualRots[2] = 0;
    sts.actualRots[3] = 0;
    //sts.speedScale = rtCommand.speedScale;
    //sts.jogSpeedScale = rtCommand.jogSpeedScale;
    sts.spiOk = data.lastSPIPacketGood;

    sts.homedAxes = homing_homedAxes;
    sts.outputs = data.outputs;
    sts.inputs = data.inputs;
    sts.rotary = data.rotary;
    memcpy( sts.adc, data.adc, 2*sizeof(uint16_t) );
    sts.pressure = data.pressure;
    sts.loadcell = data.loadcell;
    sts.pwm = data.spindleSpeed;

    sts.homingResult = homing_result;
    sts.probingResult = probing_result;
    sts.trajectoryResult = traj_result;

    rtReport( &sts );

    homing_result = HR_NONE;
    probing_result = PR_NONE;
    traj_result = TR_NONE;
}

void updateMotion() {

    if ( estop ) {
        initEstop();
        estop = false;
    }
    else if (motionMode == MM_JOG ) {
        checkJogFinish();
    }

    for (int i = 0; i < 3; i++)
        jogInfos[i].lastDir = rtCommand.jogDirs[i];

    // begin a trajectory if there was one commanded
    if ( motionMode == MM_NONE ) {        
        if ( rtCommand.traj ) {
            currentTraj = rtCommand.traj;
            rtCommand.traj = NULL;
            motionMode = MM_TRAJECTORY;
            traj_result = TR_NONE; // trajectory will stop if this becomes TR_FAIL
        }
    }

    // begin homing if commanded to
    if ( motionMode == MM_NONE ) {
        if ( anyNonZero(rtCommand.homeAxes, sizeof(rtCommand.homeAxes)) ) {
            if ( checkHomingStartConditions() ) {
                //printf("Beginning home axes: %d %d %d %d\n", rtCommand.homeAxes[0], rtCommand.homeAxes[1], rtCommand.homeAxes[2], rtCommand.homeAxes[3]);
                homing_axesRemaining.clear();
                for (int i = 0; i < (int)sizeof(rtCommand.homeAxes); i++)
                    homing_axesRemaining.push_back( rtCommand.homeAxes[i] );
                homing_axesRemaining.push_back( 0 ); // this signifies no more axes remaining
                motionMode = MM_HOMING;
                homing_result = HR_NONE; // homing will stop early if this becomes HR_FAIL
                startHomeNextAxis();
            }
        }
    }

    // begin probing if commanded to
    if ( motionMode == MM_NONE ) {
        if ( rtCommand.probeType != 0 ) {
            if ( checkProbingStartConditions() ) {
                motionMode = MM_PROBING;
                probing_result = PR_NONE; // probing will stop early if this becomes PR_FAIL
                startProbe();
            }
        }
    }

    // begin a jog if there is one commanded
    if ( motionMode == MM_NONE || motionMode == MM_JOG ) {
        // Jog starts and stops are per axis, which is why we check for other starts while already jogging
        checkJogStart();
    }

    if ( motionMode == MM_TRAJECTORY ) {
        doTrajectoryUpdate();
    }
    else if ( motionMode == MM_JOG ) {
        doJogUpdate();
    }
    else if ( motionMode == MM_HOMING ) {
        doHomingUpdate();
    }
    else if ( motionMode == MM_PROBING ) {
        doProbingUpdate();
    }

    doRTReport();
    setFrequencyOutputs();
}

void updateOutputs() {
    applyChangedBits( data.outputs, rtCommand.outputBits, rtCommand.outputChanged );

    if ( rtCommand.pwm != INVALID_FLOAT )
        data.spindleSpeed = rtCommand.pwm * 65535;

    memcpy(data.rgb, rtCommand.rgb, 6);

    memcpy(data.microsteps, rtCommand.microsteps, sizeof(data.microsteps));
    memcpy(data.rmsCurrent, rtCommand.rmsCurrent, sizeof(data.rmsCurrent));
}

void rtLoop() {

    memset(rtCommand.homeAxes, 0, sizeof(rtCommand.homeAxes));
    rtCommand.probeType = 0;
    rtCommand.pwm = INVALID_FLOAT;

    bool gotCommand = ntCommandCheck( &rtCommand );

    if ( allowSPI ) {

        if ( spiTransferCount == 1 )
            p = vec3(data.pos_fb[0], data.pos_fb[1], data.pos_fb[2]);

        if ( spiTransferCount > 0 ) {
            updateMotion();
        }

        if ( gotCommand ) {
            updateOutputs();
        }

        processOverrides();

        hal_float_t yBefore = data.pos_fb[1];
        spi_transfer();
        hal_float_t yAfter = data.pos_fb[1];

        if ( fabsf(yBefore - yAfter) > 10 ) {
            printf( "****** %f --> %f\n", yBefore, yAfter );
        }

        allSPIOk &= data.lastSPIPacketGood;

        spiTransferCount++;
    }
}

bool LockMemory() {
    int ret = mlockall(MCL_CURRENT | MCL_FUTURE);
    if (ret) {
        //throw std::runtime_error{std::string("mlockall failed: ") + std::strerror(errno)};
        printf("mlockall failed: %s\n", strerror(errno));
        return false;
    }
    return true;
}


/*void setPlanDefaults(scv::planner& p) {
    p.setPositionLimits(0, 0, 0, 10, 10, 7);
    p.setVelocityLimits(1000, 1000, 1000);
    p.setAccelerationLimits(50000, 50000, 50000);
    p.setJerkLimits(100000, 100000, 100000);
    p.setRotationVAJLimits(3000,30000,500000);

    for (int i = 0; i < NUM_ROTATION_AXES; i++) {
        p.setRotationPositionLimits(i, -200, 200);
    }
}

void setMoveDefaults(scv::move& m) {
    m.vel = 300;
    m.acc = 20000;
    m.jerk = 40000;
    m.blendType = scv::CBT_MIN_JERK;
}

void setRotateDefaults(scv::rotate &r) {
    r.vel = 3000;
    r.acc = 30000;
    r.jerk = 500000;
}*/

void sigint_handler(int sig){
    pid_t threadId = syscall(__NR_gettid);
    printf("Thread %d caught signal %d\n", threadId, sig);
    sigInt = true;
}

float speedScale = 1;
float jogSpeedScale = 1;

void reportActualPosition(int numReports, motionStatus& s, bool force) {

    //if ( s.trajectoryResult != TR_NONE ) printf("tr: %d", s.trajectoryResult); fflush(stdout);
    //printf("spi: %d, rtq: %d, mode: %d, actualPos: %f %f %f, freq: %f\r", s.spiOk, numReports, s.mode, s.actualPos.x, s.actualPos.y, s.actualPos.z, s.freq);

#ifdef DO_ZMQ
    static std::chrono::steady_clock::time_point lastPublishTime = {};

    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    long long timeSinceLastPublish = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPublishTime).count();

    if ( force || timeSinceLastPublish > 12 ) {
        publishStatus( &s, currentMoveLimits, currentRotationLimits[0], speedScale, jogSpeedScale, getWeight(), probing_resultHeight );
        lastPublishTime = now;
    }

#endif
}

void startSPIComms() {
    allSPIOk = true;
    allowSPI = true;
}

bool waitForMotionIdle( motionStatus &s, int timeoutMs) {
    while ( true ) {
        int numReports = rtReportCheck(&s);
        reportActualPosition(numReports, s, false);

        if ( s.mode == MM_NONE )
            return true;
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

bool jogButtonDown() {
    bool doingJog = false;
    for (int i = 0; !doingJog && i < 3; i++)
        doingJog = jogDirs[i] != 0;
    return doingJog;
}

vec3 makeVec3WithIgnoredFields(vec3 initial, pose override) {
    vec3 ret = initial;
    if ( override.x != INVALID_FLOAT )
        ret.x = override.x;
    if ( override.y != INVALID_FLOAT )
        ret.y = override.y;
    if ( override.z != INVALID_FLOAT )
        ret.z = override.z;
//    if ( override.a != INVALID_FLOAT )
//        ret.a = override.a;
    return ret;
}

// resets all data that will come from the config file to invalid
void resetConfig() {

    machineLimits.setPositionLimits(0, 0, -10,   10, 10, 0);

    currentMoveLimits.vel = 0;
    currentMoveLimits.acc = 0;
    currentMoveLimits.jerk = 0;

    for (int i = 0; i < 4; i++) {
        data.pos_scale[i] = 0.04;
        stepsPerUnit[i] = 0.04;
    }

    for (int i = 0; i < NUM_ROTATION_AXES; i++) {
        currentRotationLimits[i].vel = 0;
        currentRotationLimits[i].acc = 0;
        currentRotationLimits[i].jerk = 0;
    }

    for (int i = 0; i < NUM_MOTION_AXES; i++) {
        tmcParams[i].microsteps = 16;
        tmcParams[i].current = 200;
    }

    for (int i = 0; i < NUM_HOMABLE_AXES; i++) {
        initHomingParams(homingParams[i]);
    }
    initProbingParams(probingParams);
}

bool isValidTMCMicrostepsSetting(int n) {
    switch( n ) {
    case 256:
    case 128:
    case  64:
    case  32:
    case  16:
    case   8:
    case   4:
    case   2:
    case   0:
        return true;
    default: break;
    }
    return false;
}

bool isValidTMCCurrentSetting(int n) {
    return n >= 100 && n <= 2000;
}

bool isValidDigitalInputPin(int n) {
    return n >= 0 && n < 16;
}

bool isValidDigitalInputState(int n) {
    return n >= 0 && n < 2;
}

bool isValidHomingDirection(int n) {
    return n >= 0 && n < 2;
}

bool isValidHomingOrder(char* s) {
    s[8] = 0; // ensure null termination
    const char* axes = "xyzw";
    int len = strlen(s);
    for (int i = 0; i < len; i++) {
        char c = s[i];
        bool found = false;
        for (int k = 0; ! found && k < 4; k++) {
            if ( c == axes[k] )
                found = true;
        }
        if ( ! found )
            return false;
    }
    return true;
}


planner plan;

void updateLimitsInPlans() {
    g_log.log(LL_DEBUG, "updateLimitsInPlans");
    for (int i = 0; i < 3; i++) {
        machineLimits.setLimitsInPlan( &jogInfos[i].planner );
    }
    machineLimits.setLimitsInPlan( &plan );
    machineLimits.setLimitsInPlan( &estopPlanner );
    machineLimits.setLimitsInPlan( &homingPlanner );
    machineLimits.setLimitsInPlan( &probingPlanner );
}

bool compareFiles(const std::string& p1, const std::string& p2) {
    std::ifstream f1(p1, std::ifstream::binary|std::ifstream::ate);
    std::ifstream f2(p2, std::ifstream::binary|std::ifstream::ate);

    if (f1.fail() || f2.fail()) {
        return false; //file problem
    }

    if (f1.tellg() != f2.tellg()) {
        return false; //size mismatch
    }

    //seek back to beginning and use std::equal to compare contents
    f1.seekg(0, std::ifstream::beg);
    f2.seekg(0, std::ifstream::beg);
    return std::equal(std::istreambuf_iterator<char>(f1.rdbuf()),
                      std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(f2.rdbuf()));
}

void backupConfig() {

    if ( ! std::experimental::filesystem::exists("config.json") )
        return;

    if ( ! std::experimental::filesystem::exists("bkup") ) {
        g_log.log(LL_INFO, "Creating backups folder");
        mkdir("bkup", 0755);
    }

    // find newest file, compare content
    for (int i = 999; i >= 1; i--) {
        char buf[64];
        sprintf(buf, "bkup/config.json-%d", i);
        if ( std::experimental::filesystem::exists(buf) ) {
            printf("Comparing %s\n", buf); fflush(stdout);
            if ( compareFiles("config.json", buf) ) {
                g_log.log(LL_INFO, "Skipping config backup (same as %s)", buf);
                return;
            }
            break;
        }
    }

    for (int i = 1; i <= 999; i++) {
        char buf[64];
        sprintf(buf, "bkup/config.json-%d", i);
        if ( ! std::experimental::filesystem::exists(buf) ) {
            g_log.log(LL_INFO, "Creating config backup: %s", buf);
            std::ifstream  src("config.json", std::ios::binary);
            std::ofstream  dst(buf,   std::ios::binary);
            dst << src.rdbuf();
            break;
        }
    }
}

motionStatus mStatus = {0};

int main() {

    g_log.log(LL_DEBUG, "clientReport_t: %d bytes", sizeof(clientReport_t));
    g_log.log(LL_DEBUG, "commandRequest_t: %d bytes", sizeof(commandRequest_t));

    g_log.log(LL_INFO, "Server started");

    backupConfig();

    LockMemory();

    resetConfig();
    readConfigFile();

    initWeenyData();
    initInterThread();
    //initDefaultOverrides();

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = sigint_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    //pid_t threadId = syscall(__NR_gettid);
    //printf("Main thread id = %d\n", threadId);

#ifdef DO_ZMQ
    if ( ! startServer() ) {
        printf("Could not start server!\n");
        return -1;
    }
#endif

    rtCommand.speedScale = 1;
    rtCommand.jogSpeedScale = 1;

    RTThread rt_thread(98, SCHED_FIFO, 1000000);
    rt_thread.Start();

    // Let realtime thread do about 10 iterations to settle the sleep time.
    // Otherwise, the interval between the first few iterations can be near
    // zero, and SPI transfers will run quicker than the slave can follow.
    std::this_thread::sleep_for(10ms);

    // Now the realtime thread sleep interval should be the expected 1ms.
    startSPIComms();

    //while ( spiTransferCount < 3 * MIN_SPI_TRANSFERS ) ;

    // Now sleep for a bit to let the first transfers fetch the starting status.
    std::this_thread::sleep_for(10ms);

    //motionStatus mStatus = {0};
    mStatus.mode = 0;
    mStatus.targetPos = vec3_zero;

    rtReportCheck(&mStatus);
    /*if ( ! mStatus.spiOk ) {
        printf("Could not start SPI\n");
        sigIntRT = true;
        rt_thread.Join();
        return -1;
    }*/

    //planner plan;

    motionCommand mCmd = {0};
    mCmd.speedScale = 1;
    mCmd.jogSpeedScale = 1;
    mCmd.traj = &plan;    
    mCmd.rgb[0] = 0b00010001;
    mCmd.rgb[1] = 0b00000001;
    mCmd.pwm = INVALID_FLOAT;
    for (int i = 0; i < JOINTS; i++) {
        mCmd.microsteps[i] = 16;
        mCmd.rmsCurrent[i] = 200;
    }
    memset(mCmd.homeAxes, 0, sizeof(mCmd.homeAxes));
    mCmd.probeType = 0;

    for (int i = 0; i < 3; i++) {
        jogInfos[i].phase = JP_IDLE;
        jogInfos[i].lastDir = 0;
        jogInfos[i].vel = joggingSpeeds[i];

        //setPlanDefaults(jogInfos[i].planner);
        machineLimits.setLimitsInPlan( &jogInfos[i].planner );

        mCmd.jogDirs[i] = 0;
    }

    //setPlanDefaults(plan);
    //setPlanDefaults(estopPlanner);
    //setPlanDefaults(homingPlanner);

    machineLimits.setLimitsInPlan( &plan );
    machineLimits.setLimitsInPlan( &estopPlanner );
    machineLimits.setLimitsInPlan( &homingPlanner );
    machineLimits.setLimitsInPlan( &probingPlanner );

    scv::move m1;
    //setMoveDefaults(m1);
    m1.vel = currentMoveLimits.vel;
    m1.acc = currentMoveLimits.acc;
    m1.jerk = currentMoveLimits.jerk;
    m1.blendType = scv::CBT_MIN_JERK;

    scv::rotate r1[NUM_ROTATION_AXES];
    for (int i = 0; i < NUM_ROTATION_AXES; i++) {
        r1[i].axis = i;
        //setRotateDefaults(r1[i]);
        r1[i].vel = currentRotationLimits[i].vel;
        r1[i].acc = currentRotationLimits[i].acc;
        r1[i].jerk = currentRotationLimits[i].jerk;
    }

    estopRotate.axis = 0;
    estopRotate.vel = currentRotationLimits[0].vel;
    estopRotate.acc = currentRotationLimits[0].acc;
    estopRotate.jerk = currentRotationLimits[0].jerk;
    //setRotateDefaults(estopRotate);

    // vec3 positions[3];
    // positions[0] = vec3(-100,40,-15);
    // positions[1] = vec3(20,40,0);
    // positions[2] = vec3(-100,0,0);

    // vec3 positions2[3];
    // positions2[0] = vec3(-100,0,-15);
    // positions2[1] = vec3(20,40,0);
    // positions2[2] = vec3(0,0,0);

    vec3 maxFollowError = vec3_zero;

    //int tposind = 0;

    int numReports = rtReportCheck(&mStatus); // must clear old reports!!
    reportActualPosition(numReports, mStatus, false);

    std::chrono::steady_clock::time_point lastJogRecvTime = {};
    std::chrono::steady_clock::time_point lastHomeRecvTime = {};

    //printf("After first reportActualPosition\n");

    //int reps = 0;

    uint8_t rejectedTrajectoryResult = TR_NONE;
    uint8_t rejectedProbingResult = PR_NONE;

    while ( ! sigInt ) {

        bool shouldSendRTCommand = false;

        int numReports = rtReportCheck(&mStatus);

        vec3 followError = abs( mStatus.targetPos - mStatus.actualPos );
        maxFollowError = scv::max( maxFollowError, followError );

        bool forceReport = false;

        if ( mStatus.homingResult != HR_NONE ) {
            g_log.log(LL_INFO, "Homing result: %s", getHomingResultName(mStatus.homingResult));
            if ( mStatus.homingResult )
                maxFollowError = vec3_zero;
            forceReport = true;
        }

        if ( rejectedProbingResult != PR_NONE ) {
            mStatus.probingResult = rejectedProbingResult;
            rejectedProbingResult = PR_NONE;
        }
        if ( mStatus.probingResult != PR_NONE ) {
            g_log.log(LL_INFO, "Probing result: %s", getProbingResultName(mStatus.probingResult));
            if ( mStatus.probingResult == PR_SUCCESS ) {
                g_log.log(LL_INFO, "Probed height: %f", probing_resultHeight);
            }
            forceReport = true;
        }

        if ( rejectedTrajectoryResult != TR_NONE ) {
            mStatus.trajectoryResult = rejectedTrajectoryResult;
            rejectedTrajectoryResult = TR_NONE;
        }
        if ( mStatus.trajectoryResult != TR_NONE ) {
            g_log.log(LL_INFO, "Trajectory result: %s\n", getTrajectoryResultName(mStatus.trajectoryResult));
            forceReport = true;
        }

        // else {
        //     for (int i = 0; i < 3; i++) {
        //         float fe = fabsf( followError[i] );
        //         if ( fe > 1 ) {
        //             g_log.log(LL_INFO, "Trajectory aborted: %s\n", getTrajectoryResultName(TR_FAIL_FOLLOWING_ERROR));
        //             mStatus.trajectoryResult = TR_FAIL_FOLLOWING_ERROR;
        //             estop = true;
        //             forceReport = true;
        //         }
        //     }
        // }

        reportActualPosition(numReports, mStatus, forceReport);

        updateLoadcell( mStatus.loadcell );

        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

        commandRequest_t req = {0};
        CommandList program(CBM_NONE);
        commandMessageType_e msgType = MT_NONE;
        int didRecv = checkCommandRequests(&msgType, &req, &program);
        bool ackOrNack = true; // true if message is recognized
        if ( didRecv ) {

            if ( msgType == MT_SET_PROGRAM ) {
                if ( homing_homedAxes < 0x07 ) {
                    g_log.log(LL_ERROR, "Ignoring program, not homed");
                    rejectedTrajectoryResult = TR_FAIL_NOT_HOMED;
                }
                else if ( mStatus.mode != MM_NONE ) {
                    g_log.log(LL_ERROR, "Ignoring program, not in idle state");
                }
                else {
                    dumpCommandList(program);

                    plan.clear();

                    plan.setCornerBlendMethod(program.cornerBlendMethod);
                    plan.setMaxCornerBlendOverlapFraction(program.cornerBlendMaxFraction);

                    //printf("Blending: method = %d, max fraction = %f\n", program.cornerBlendMethod, program.cornerBlendMaxFraction);

                    memcpy(plan.traversal_rots, mStatus.actualRots, sizeof(plan.traversal_rots));
                    memcpy(plan.startingRotations, mStatus.actualRots, sizeof(plan.startingRotations));


                    // ie. last position, just for convenience in loop below
                    m1.dst = mStatus.actualPos;
                    for (int i = 0; i < NUM_ROTATION_AXES; i++)
                        r1[i].dst = mStatus.actualRots[0];

                    // if any move would be outside machine work area, abandon entire program
                    bool programIsValid = true;

                    for (int i = 0; i < (int)program.commands.size(); i++) {
                        Command* cmd = program.commands[i];
                        if ( cmd->type == CT_MOVETO ) {
                            Command_moveTo* cmdMoveto = (Command_moveTo*)cmd;
                            m1.src = m1.dst;

                            m1.dst = m1.src; // for unchanged axes

                            vec3 newPos = m1.dst;

                            if ( cmdMoveto->dst.x != INVALID_FLOAT ) {
                                if ( cmdMoveto->dst.flags_x & MOVE_FLAG_RELATIVE ) {
                                    newPos.x += cmdMoveto->dst.x;
                                }
                                else if ( cmdMoveto->dst.flags_x & MOVE_FLAG_LESS_THAN ) {
                                    if ( cmdMoveto->dst.x < newPos.x )
                                        newPos.x = cmdMoveto->dst.x;
                                }
                                else if ( cmdMoveto->dst.flags_x & MOVE_FLAG_MORE_THAN ) {
                                    if ( cmdMoveto->dst.x > newPos.x )
                                        newPos.x = cmdMoveto->dst.x;
                                }
                                else
                                    newPos.x = cmdMoveto->dst.x;
                            }
                            if ( cmdMoveto->dst.y != INVALID_FLOAT ) {
                                if ( cmdMoveto->dst.flags_y & MOVE_FLAG_RELATIVE ) {
                                    newPos.y += cmdMoveto->dst.y;
                                }
                                else if ( cmdMoveto->dst.flags_y & MOVE_FLAG_LESS_THAN ) {
                                    if ( cmdMoveto->dst.y < newPos.y )
                                        newPos.y = cmdMoveto->dst.y;
                                }
                                else if ( cmdMoveto->dst.flags_y & MOVE_FLAG_MORE_THAN ) {
                                    if ( cmdMoveto->dst.y > newPos.y )
                                        newPos.y = cmdMoveto->dst.y;
                                }
                                else
                                    newPos.y = cmdMoveto->dst.y;
                            }
                            if ( cmdMoveto->dst.z != INVALID_FLOAT ) {
                                if ( cmdMoveto->dst.flags_z & MOVE_FLAG_RELATIVE ) {
                                    newPos.z += cmdMoveto->dst.z;
                                }
                                else if ( cmdMoveto->dst.flags_z & MOVE_FLAG_LESS_THAN ) {
                                    if ( cmdMoveto->dst.z < newPos.z )
                                        newPos.z = cmdMoveto->dst.z;
                                }
                                else if ( cmdMoveto->dst.flags_z & MOVE_FLAG_MORE_THAN ) {
                                    if ( cmdMoveto->dst.z > newPos.z )
                                        newPos.z = cmdMoveto->dst.z;
                                }
                                else
                                    newPos.z = cmdMoveto->dst.z;
                            }

                            if ( newPos.x < machineLimits.posLimitLower.x || newPos.x > machineLimits.posLimitUpper.x ) {
                                newPos.x = m1.dst.x;
                                programIsValid = false;
                            }
                            if ( newPos.y < machineLimits.posLimitLower.y || newPos.y > machineLimits.posLimitUpper.y ) {
                                newPos.y = m1.dst.y;
                                programIsValid = false;
                            }
                            if ( newPos.z < machineLimits.posLimitLower.z || newPos.z > machineLimits.posLimitUpper.z ) {
                                newPos.z = m1.dst.z;
                                programIsValid = false;
                            }

                            m1.dst = newPos;

                            //applyHomeOffsetToMove(m1);
                            plan.appendMove(m1);
                        }
                        else if ( cmd->type == CT_WAIT ) {
                            Command_wait* cmdWait = (Command_wait*)cmd;
                            plan.appendWait( m1.dst, cmdWait->duration );
                        }
                        else if ( cmd->type == CT_DIGITAL_OUTPUT ) {
                            Command_digitalOutput* cmdDigitalOutput = (Command_digitalOutput*)cmd;
                            plan.appendDigitalOutput(cmdDigitalOutput->bits, cmdDigitalOutput->changed, cmdDigitalOutput->delay);
                        }
                        else if ( cmd->type == CT_SET_MOVE_LIMITS ) {
                            Command_setMoveLimits* cmdSetLimits = (Command_setMoveLimits*)cmd;
                            if ( cmdSetLimits->limits.vel != INVALID_FLOAT ) {
                                m1.vel = cmdSetLimits->limits.vel;
                                currentMoveLimits.vel = m1.vel;
                            }
                            if ( cmdSetLimits->limits.acc != INVALID_FLOAT ) {
                                m1.acc = cmdSetLimits->limits.acc;
                                currentMoveLimits.acc = m1.acc;
                            }
                            if ( cmdSetLimits->limits.jerk != INVALID_FLOAT ) {
                                m1.jerk = cmdSetLimits->limits.jerk;
                                currentMoveLimits.jerk = m1.jerk;
                            }
                        }
                        else if ( cmd->type == CT_SET_ROTATE_LIMITS ) {
                            Command_setRotateLimits* cmdSetLimits = (Command_setRotateLimits*)cmd;
                            if ( cmdSetLimits->limits.vel != INVALID_FLOAT ) {
                                r1[cmdSetLimits->axis].vel = cmdSetLimits->limits.vel;
                                currentRotationLimits[cmdSetLimits->axis].vel = r1[cmdSetLimits->axis].vel;
                            }
                            if ( cmdSetLimits->limits.acc != INVALID_FLOAT ) {
                                r1[cmdSetLimits->axis].acc = cmdSetLimits->limits.acc;
                                currentRotationLimits[cmdSetLimits->axis].acc = r1[cmdSetLimits->axis].acc;
                            }
                            if ( cmdSetLimits->limits.jerk != INVALID_FLOAT ) {
                                r1[cmdSetLimits->axis].jerk = cmdSetLimits->limits.jerk;
                                currentRotationLimits[cmdSetLimits->axis].jerk = r1[cmdSetLimits->axis].jerk;
                            }
                        }
                        else if ( cmd->type == CT_PWM_OUTPUT ) {
                            Command_setPWM* cmdSetPWM = (Command_setPWM*)cmd;
                            plan.appendPWMOutput(cmdSetPWM->vals, cmdSetPWM->delay);
                        }
                        else if ( cmd->type == CT_ROTATETO ) {
                            Command_rotateTo* cmdRotateto = (Command_rotateTo*)cmd;
                            for (int i = 0; i < NUM_ROTATION_AXES; i++) {
                                if ( cmdRotateto->dst[i] != INVALID_FLOAT ) {
                                    //r1[i].axis = i;
                                    r1[i].src = r1[i].dst;
                                    r1[i].dst = cmdRotateto->dst[i];
                                    plan.appendRotate(r1[i], cmdRotateto->delay);
                                }
                            }
                        }
                        else if ( cmd->type == CT_SYNC ) {
                            plan.appendSync(m1.dst);
                        }
                        else {
                            printf("(main) Unhandled command type: %s\n", getCommandName(cmd->type));
                        }
                    }

                    if ( programIsValid ) {
                        plan.printConstraints();
                        plan.calculateMoves();
                        plan.addOffsetToMoves(offsetAtHome);
                        plan.printConstraints();
                        plan.printMoves();
                        //plan.printSegments();

                        plan.resetTraverse();

                        mCmd.traj = &plan;
                        shouldSendRTCommand = true;
                    }
                    else {
                        g_log.log(LL_ERROR, "Ignoring program, would move outside work area");
                        rejectedTrajectoryResult = TR_FAIL_OUTSIDE_BOUNDS;
                    }
                }
            }
            else if ( req.type == MT_SET_ESTOP ) {
                estop = true;
                //sigInt = true;
            }
            else if ( req.type == MT_SET_SPEED_SCALE ) {
                if ( req.setSpeedScale.scale >= 0.1 && req.setSpeedScale.scale <= 1 ) {
                    mCmd.speedScale = req.setSpeedScale.scale;
                    speedScale = mCmd.speedScale;
                    mCmd.traj = NULL;
                    shouldSendRTCommand = true;
                }
            }
            else if ( req.type == MT_SET_JOG_SPEED_SCALE ) {
                if ( req.setSpeedScale.scale >= 0.1 && req.setSpeedScale.scale <= 1 ) {
                    mCmd.jogSpeedScale = req.setSpeedScale.scale;
                    jogSpeedScale = mCmd.jogSpeedScale;
                    mCmd.traj = NULL;
                    shouldSendRTCommand = true;
                }
            }
            else if ( req.type == MT_SET_JOG_STATUS ) {
                if ( mStatus.mode == MM_NONE || mStatus.mode == MM_JOG ) {
                    memcpy(jogDirs, req.setJogStatus.jogDirs, sizeof(mCmd.jogDirs));
                    memcpy(mCmd.jogDirs, req.setJogStatus.jogDirs, sizeof(mCmd.jogDirs));
                    mCmd.traj = NULL;
                    shouldSendRTCommand = true;
                    lastJogRecvTime = now;
                }
                else {
                    g_log.log(LL_DEBUG, "Ignoring jog because mode = %s", getModeName(mStatus.mode));
                }
            }
            // else if ( req.type == MT_SET_MOVETO ) {
            //     if ( mStatus.mode == MM_NONE ) {
            //         plan.clear();
            //         plan.resetTraverse();

            //         if ( req.setMoveto.dst.x == INVALID_FLOAT )
            //             req.setMoveto.dst.x = mStatus.actualPos.x;
            //         if ( req.setMoveto.dst.y == INVALID_FLOAT )
            //             req.setMoveto.dst.y = mStatus.actualPos.y;
            //         if ( req.setMoveto.dst.z == INVALID_FLOAT )
            //             req.setMoveto.dst.z = mStatus.actualPos.z;

            //         m1.src = mStatus.actualPos;
            //         m1.dst = vec3(req.setMoveto.dst.x, req.setMoveto.dst.y, req.setMoveto.dst.z);
            //         //m1.vel = req.setMoveto.speed;
            //         //applyHomeOffsetToMove(m1);
            //         plan.appendMove(m1);
            //         plan.calculateMoves();
            //         plan.addOffsetToMoves(offsetAtHome);

            //         plan.printMoves();

            //         mCmd.traj = &plan;
            //         shouldSendRTCommand = true;
            //     }
            // }
            else if ( req.type == MT_SET_DIGITAL_OUTPUTS ) {
                mCmd.outputBits = req.setDigitalOutputs.bits;
                mCmd.outputChanged = req.setDigitalOutputs.changed;
                mCmd.traj = NULL;
                shouldSendRTCommand = true;

                g_log.log(LL_DEBUG,"motionMode = %s", getModeName(motionMode));
                g_log.log(LL_DEBUG,"home offset: %f, %f, %f", offsetAtHome.x, offsetAtHome.y, offsetAtHome.z);
                //g_log.log(LL_DEBUG,"acc: %f, %f, %f", a.x, a.y, a.z);
            }
            else if ( req.type == MT_SET_PWM_OUTPUT ) {
                mCmd.pwm = req.setPWMOutput.val / 65535.0f;
                //printf("pwm: %d\n", mCmd.pwm);
                mCmd.traj = NULL;
                shouldSendRTCommand = true;
            }
            else if ( req.type == MT_SET_RGB_OUTPUT ) {
                memcpy(mCmd.rgb, req.setRGBOutput.rgb, 6);
                mCmd.traj = NULL;
                shouldSendRTCommand = true;
            }
            else if ( req.type == MT_SET_TMC_PARAMS ) {
                memcpy(mCmd.microsteps, req.setTMCParams.microsteps, JOINTS*sizeof(uint16_t));
                memcpy(mCmd.rmsCurrent, req.setTMCParams.rmsCurrent, JOINTS*sizeof(uint16_t));
                mCmd.traj = NULL;
                shouldSendRTCommand = true;
            }
            else if ( req.type == MT_HOME_AXES ) {
                if ( mStatus.mode != MM_NONE ) {
                    g_log.log(LL_WARN, "Ignoring homing request, not in idle state.");
                }
                else {
                    memcpy(mCmd.homeAxes, req.homeAxes.ordering, sizeof(mCmd.homeAxes));
                    resetAxesAboutToHome( mCmd.homeAxes );
                    mCmd.traj = NULL;
                    shouldSendRTCommand = true;
                    lastHomeRecvTime = now;
                    g_log.log(LL_INFO, "Homing requested.");
                }
            }
            else if ( req.type == MT_HOME_ALL ) {
                if ( mStatus.mode != MM_NONE ) {
                    g_log.log(LL_WARN, "Ignoring homing request, not in idle state.");
                }
                else {
                    memset(mCmd.homeAxes, 0, sizeof(mCmd.homeAxes));
                    int numAxes = strlen(homingOrder);
                    for (int i = 0; i < numAxes; i++) {
                        char c = homingOrder[i];
                        switch (c) {
                        case 'x': mCmd.homeAxes[i] = 1; break;
                        case 'y': mCmd.homeAxes[i] = 2; break;
                        case 'z': mCmd.homeAxes[i] = 3; break;
                        case 'w': mCmd.homeAxes[i] = 4; break;
                        }
                    }
                    resetAxesAboutToHome(mCmd.homeAxes);

                    mCmd.traj = NULL;
                    shouldSendRTCommand = true;
                    lastHomeRecvTime = now;
                    g_log.log(LL_INFO, "Homing requested.");
                }
            }
            else if ( req.type == MT_RESET_LOADCELL ) {
                g_log.log(LL_INFO, "Reset load cell requested.");
                resetLoadcell();
            }
            else if ( req.type == MT_CONFIG_STEPS_FETCH ) {

            }
            else if ( req.type == MT_CONFIG_STEPS_SET ) {
                if ( motionMode == MM_NONE ) {
                    const char* axisNames = "XYZWABCD";
                    for (int i = 0; i < NUM_MOTION_AXES; i++) {
                        if ( fabs(req.configSteps.perUnit[i]) >= 0.05 ) {
                            // todo: change actual data.pos_scale
                            stepsPerUnit[i] = req.configSteps.perUnit[i];
                            data.pos_scale[i] = req.configSteps.perUnit[i];
                            g_log.log(LL_INFO, "Steps per unit %c set to %f", axisNames[i], req.configSteps.perUnit[i]);
                        }
                    }

                    saveConfigToFile();
                }
            }
            else if ( req.type == MT_CONFIG_WORKAREA_FETCH ) {

            }
            else if ( req.type == MT_CONFIG_WORKAREA_SET ) {
                if ( motionMode == MM_NONE ) {
                    vec3 workAreaDimensions = machineLimits.posLimitUpper - machineLimits.posLimitUpper;

                    // assume at least 1 unit size
                    if ( fabs(req.workingArea.x) >= 1 ) {
                        workAreaDimensions.x = req.workingArea.x;
                        g_log.log(LL_INFO, "Working area X set to %f", req.workingArea.x);
                    }
                    if ( fabs(req.workingArea.y) >= 1 ) {
                        workAreaDimensions.y = req.workingArea.y;
                        g_log.log(LL_INFO, "Working area Y set to %f", req.workingArea.y);
                    }
                    if ( fabs(req.workingArea.z) >= 1 ) {
                        workAreaDimensions.z = req.workingArea.z;
                        g_log.log(LL_INFO, "Working area Z set to %f", req.workingArea.z);
                    }

                    machineLimits.setPositionLimits(0, 0, -workAreaDimensions.z,     workAreaDimensions.x, workAreaDimensions.y, 0);

                    saveConfigToFile();
                }
            }
            else if ( req.type == MT_CONFIG_INITSPEEDS_FETCH ) {

            }
            else if ( req.type == MT_CONFIG_INITSPEEDS_SET ) {

                if ( req.motionLimits.velLimitX >= 0.04 ) {
                    machineLimits.velLimit.x = req.motionLimits.velLimitX;
                    g_log.log(LL_INFO, "Hard-limit vel X set to %f", req.motionLimits.velLimitX);
                }
                if ( req.motionLimits.velLimitY >= 0.04 ) {
                    machineLimits.velLimit.y = req.motionLimits.velLimitY;
                    g_log.log(LL_INFO, "Hard-limit vel Y set to %f", req.motionLimits.velLimitY);
                }
                if ( req.motionLimits.velLimitZ >= 0.04 ) {
                    machineLimits.velLimit.z = req.motionLimits.velLimitZ;
                    g_log.log(LL_INFO, "Hard-limit vel Z set to %f", req.motionLimits.velLimitZ);
                }

                if ( req.motionLimits.accLimitX >= 0.04 ) {
                    machineLimits.accLimit.x = req.motionLimits.accLimitX;
                    g_log.log(LL_INFO, "Hard-limit acc X set to %f", req.motionLimits.accLimitX);
                }
                if ( req.motionLimits.accLimitY >= 0.04 ) {
                    machineLimits.accLimit.y = req.motionLimits.accLimitY;
                    g_log.log(LL_INFO, "Hard-limit acc Y set to %f", req.motionLimits.accLimitY);
                }
                if ( req.motionLimits.accLimitZ >= 0.04 ) {
                    machineLimits.accLimit.z = req.motionLimits.accLimitZ;
                    g_log.log(LL_INFO, "Hard-limit acc Z set to %f", req.motionLimits.accLimitZ);
                }

                if ( req.motionLimits.jerkLimitX >= 0.04 ) {
                    machineLimits.jerkLimit.x = req.motionLimits.jerkLimitX;
                    g_log.log(LL_INFO, "Hard-limit jerk X set to %f", req.motionLimits.jerkLimitX);
                }
                if ( req.motionLimits.jerkLimitY >= 0.04 ) {
                    machineLimits.jerkLimit.y = req.motionLimits.jerkLimitY;
                    g_log.log(LL_INFO, "Hard-limit jerk Y set to %f", req.motionLimits.jerkLimitY);
                }
                if ( req.motionLimits.jerkLimitZ >= 0.04 ) {
                    machineLimits.jerkLimit.z = req.motionLimits.jerkLimitZ;
                    g_log.log(LL_INFO, "Hard-limit jerk Z set to %f", req.motionLimits.jerkLimitZ);
                }

                if ( req.motionLimits.rotLimitVel >= 0.04 ) {
                    machineLimits.grotationVelLimit = req.motionLimits.rotLimitVel;
                    g_log.log(LL_INFO, "Rotation vel limit set to %f", req.motionLimits.rotLimitVel);
                }
                if ( req.motionLimits.rotLimitAcc >= 0.04 ) {
                    machineLimits.grotationAccLimit = req.motionLimits.rotLimitAcc;
                    g_log.log(LL_INFO, "Rotation acc limit set to %f", req.motionLimits.rotLimitAcc);
                }
                if ( req.motionLimits.rotLimitJerk >= 0.04 ) {
                    machineLimits.grotationJerkLimit = req.motionLimits.rotLimitJerk;
                    g_log.log(LL_INFO, "Rotation jerk limit set to %f", req.motionLimits.rotLimitJerk);
                }

                if ( req.motionLimits.initialMoveVel >= 0.04 ) {
                    machineLimits.initialMoveLimitVel = req.motionLimits.initialMoveVel;
                    g_log.log(LL_INFO, "Initial move velocity set to %f", req.motionLimits.initialMoveVel);
                }
                if ( req.motionLimits.initialMoveAcc >= 0.04 ) {
                    machineLimits.initialMoveLimitAcc = req.motionLimits.initialMoveAcc;
                    g_log.log(LL_INFO, "Initial move acceleration set to %f", req.motionLimits.initialMoveAcc);
                }
                if ( req.motionLimits.initialMoveJerk >= 0.04 ) {
                    machineLimits.initialMoveLimitJerk = req.motionLimits.initialMoveJerk;
                    g_log.log(LL_INFO, "Initial move jerk set to %f", req.motionLimits.initialMoveJerk);
                }

                if ( req.motionLimits.initialRotateVel >= 0.04 ) {
                    machineLimits.initialRotationLimitVel = req.motionLimits.initialRotateVel;
                    g_log.log(LL_INFO, "Initial rotate velocity set to %f", req.motionLimits.initialRotateVel);
                }
                if ( req.motionLimits.initialRotateAcc >= 0.04 ) {
                    machineLimits.initialRotationLimitAcc = req.motionLimits.initialRotateAcc;
                    g_log.log(LL_INFO, "Initial rotate acceleration set to %f", req.motionLimits.initialRotateAcc);
                }
                if ( req.motionLimits.initialRotateJerk >= 0.04 ) {
                    machineLimits.initialRotationLimitJerk = req.motionLimits.initialRotateJerk;
                    g_log.log(LL_INFO, "Initial rotate jerk set to %f", req.motionLimits.initialRotateJerk);
                }

                if ( req.motionLimits.maxOverlapFraction >= 0 && req.motionLimits.maxOverlapFraction <= 0.95 ) {
                    machineLimits.maxOverlapFraction = req.motionLimits.maxOverlapFraction;
                    g_log.log(LL_INFO, "Initial corner blend fraction set to %f", req.motionLimits.maxOverlapFraction);
                }

                updateLimitsInPlans();

                saveConfigToFile();
            }
            else if ( req.type == MT_CONFIG_TMC_FETCH ) {

            }
            else if ( req.type == MT_CONFIG_TMC_SET ) {
                for (int i = 0; i < NUM_MOTION_AXES; i++) {
                    if ( isValidTMCMicrostepsSetting(req.tmcSettings.settings[i].microsteps) ) {
                        tmcParams[i].microsteps = req.tmcSettings.settings[i].microsteps;
                        data.microsteps[i] = req.tmcSettings.settings[i].microsteps;
                        g_log.log(LL_INFO, "TMC %d microsteps set to %d", i, tmcParams[i].microsteps);
                    }
                    if ( isValidTMCCurrentSetting(req.tmcSettings.settings[i].current) ) {
                        tmcParams[i].current = req.tmcSettings.settings[i].current;
                        data.rmsCurrent[i] = req.tmcSettings.settings[i].current;
                        g_log.log(LL_INFO, "TMC %d current set to %d", i, tmcParams[i].current);
                    }
                }

                saveConfigToFile();
            }
            else if ( req.type == MT_CONFIG_HOMING_FETCH ) {

            }
            else if ( req.type == MT_CONFIG_HOMING_SET ) {
                if ( motionMode != MM_HOMING ) {
                    const char* axisNames = "XYZW";
                    for (int i = 0; i < NUM_HOMABLE_AXES; i++) {
                        if ( isValidDigitalInputPin(req.homingParams.params[i].triggerPin) ) {
                            homingParams[i].triggerPin = req.homingParams.params[i].triggerPin;
                            g_log.log(LL_INFO, "Homing %c pin set to %d", axisNames[i], homingParams[i].triggerPin);
                        }
                        if ( isValidDigitalInputState(req.homingParams.params[i].triggerState) ) {
                            homingParams[i].triggerState = req.homingParams.params[i].triggerState;
                            g_log.log(LL_INFO, "Homing %c state set to %d", axisNames[i], homingParams[i].triggerState);
                        }
                        if ( isValidHomingDirection(req.homingParams.params[i].direction) ) {
                            homingParams[i].direction = req.homingParams.params[i].direction;
                            g_log.log(LL_INFO, "Homing %c direction set to %d", axisNames[i], homingParams[i].direction);
                        }

                        if ( req.homingParams.params[i].approachspeed1 > 0.01 ) {
                            homingParams[i].approachspeed1 = req.homingParams.params[i].approachspeed1;
                            g_log.log(LL_INFO, "Homing %c approach speed 1 set to %f", axisNames[i], homingParams[i].approachspeed1);
                        }
                        if ( req.homingParams.params[i].approachspeed2 > 0.01 ) {
                            homingParams[i].approachspeed2 = req.homingParams.params[i].approachspeed2;
                            g_log.log(LL_INFO, "Homing %c approach speed 2 set to %f", axisNames[i], homingParams[i].approachspeed2);
                        }

                        if ( req.homingParams.params[i].backoffDistance1 > 0.01 ) {
                            homingParams[i].backoffDistance1 = req.homingParams.params[i].backoffDistance1;
                            g_log.log(LL_INFO, "Homing %c backoff distance 1 set to %f", axisNames[i], homingParams[i].backoffDistance1);
                        }
                        if ( req.homingParams.params[i].backoffDistance2 > 0.01 ) {
                            homingParams[i].backoffDistance2 = req.homingParams.params[i].backoffDistance2;
                            g_log.log(LL_INFO, "Homing %c backoff distance 2 set to %f", axisNames[i], homingParams[i].backoffDistance2);
                        }

                        //if ( req.homingParams.params[i].homedPosition > 0.01 ) {
                            homingParams[i].homedPosition = req.homingParams.params[i].homedPosition;
                            g_log.log(LL_INFO, "Homing %c homed position 1 set to %f", axisNames[i], homingParams[i].homedPosition);
                        //}
                    }

                    if ( isValidHomingOrder(req.homingParams.order) ) {
                        memcpy(homingOrder, req.homingParams.order, min(sizeof(req.homingParams.order), sizeof(homingOrder)));
                        g_log.log(LL_INFO, "Homing order set to: %s", homingOrder);
                    }

                    saveConfigToFile();
                }
            }
            else if ( req.type == MT_CONFIG_JOGGING_FETCH ) {

            }
            else if ( req.type == MT_CONFIG_JOGGING_SET ) {
                if ( motionMode == MM_NONE ) {
                    const char* axisNames = "XYZWABCD";
                    for (int i = 0; i < NUM_MOTION_AXES; i++) {
                        if ( req.jogParams.speed[i] >= 0.05 ) {
                            joggingSpeeds[i] = req.jogParams.speed[i];
                            jogInfos[i].vel = joggingSpeeds[i];
                            g_log.log(LL_INFO, "Jog speed %c set to %f", axisNames[i], req.jogParams.speed[i]);
                        }
                    }

                    saveConfigToFile();
                }
            }
            else if ( req.type == MT_CONFIG_OVERRIDES_FETCH ) {

            }
            else if ( msgType == MT_CONFIG_OVERRIDES_SET ) {
                // already handled in unpack

                // to avoid altering the overrides list while the realtime thread
                // is using it, unset an 'allow' value and wait for two loops.
                allowOverrideExecution = false;

                std::this_thread::sleep_for( 2ms );

                overrideConfigSet = overrideConfigSet_incoming;

                allowOverrideExecution = true;

                printOverrides();

                saveConfigToFile();
            }
            else if ( req.type == MT_CONFIG_LOADCELL_CALIB_FETCH ) {

            }
            else if ( msgType == MT_CONFIG_LOADCELL_CALIB_SET ) {

                loadcellCalibrationRawOffset = getLoadCellMeasurement() - getLoadCellBaseline();
                loadcellCalibrationWeight = req.loadcellCalib.weight;

                g_log.log(LL_INFO, "Load cell calib raw offset set to %d", loadcellCalibrationRawOffset);
                g_log.log(LL_INFO, "Load cell calib weight set to %f", loadcellCalibrationWeight);

                saveConfigToFile();
            }
            else if ( req.type == MT_CONFIG_PROBING_FETCH ) {

            }
            else if ( req.type == MT_CONFIG_PROBING_SET ) {

                if ( req.probingParams.params.digitalTriggerPin >= 0 && req.probingParams.params.digitalTriggerPin <= 15 ) {
                    probingParams.digitalTriggerPin = req.probingParams.params.digitalTriggerPin;
                    g_log.log(LL_INFO, "Probing digital trigger pin set to %d", probingParams.digitalTriggerPin);
                }

                if ( req.probingParams.params.digitalTriggerState >= 0 && req.probingParams.params.digitalTriggerState <= 1 ) {
                    probingParams.digitalTriggerState = req.probingParams.params.digitalTriggerState;
                    g_log.log(LL_INFO, "Probing digital trigger state set to %d", probingParams.digitalTriggerState);
                }

                if ( req.probingParams.params.vacuumSniffPin >= 0 && req.probingParams.params.vacuumSniffPin <= 15 ) {
                    probingParams.vacuumSniffPin = req.probingParams.params.vacuumSniffPin;
                    g_log.log(LL_INFO, "Probing vacuum sniff pin set to %d", probingParams.vacuumSniffPin);
                }

                if ( req.probingParams.params.vacuumSniffState >= 0 && req.probingParams.params.vacuumSniffState <= 1 ) {
                    probingParams.vacuumSniffState = req.probingParams.params.vacuumSniffState;
                    g_log.log(LL_INFO, "Probing vacuum sniff state set to %d", probingParams.vacuumSniffState);
                }

                if ( req.probingParams.params.vacuumSniffTimeMs >= 10 && req.probingParams.params.vacuumSniffTimeMs <= 2000 ) {
                    probingParams.vacuumSniffTimeMs = req.probingParams.params.vacuumSniffTimeMs;
                    g_log.log(LL_INFO, "Probing vacuum sniff time set to %d ms", probingParams.vacuumSniffTimeMs);
                }

                if ( req.probingParams.params.vacuumReplenishTimeMs >= 0 && req.probingParams.params.vacuumReplenishTimeMs <= 3000 ) {
                    probingParams.vacuumReplenishTimeMs = req.probingParams.params.vacuumReplenishTimeMs;
                    g_log.log(LL_INFO, "Probing vacuum replenish time set to %d ms", probingParams.vacuumReplenishTimeMs);
                }

                if ( req.probingParams.params.vacuumStep > 0.01 ) {
                    probingParams.vacuumStep = req.probingParams.params.vacuumStep;
                    g_log.log(LL_INFO, "Probing vacuum step set to %f", probingParams.vacuumStep);
                }

                if ( req.probingParams.params.approachspeed1 > 0.01 ) {
                    probingParams.approachspeed1 = req.probingParams.params.approachspeed1;
                    g_log.log(LL_INFO, "Probing approach speed 1 set to %f", probingParams.approachspeed1);
                }
                if ( req.probingParams.params.approachspeed2 > 0.01 ) {
                    probingParams.approachspeed2 = req.probingParams.params.approachspeed2;
                    g_log.log(LL_INFO, "Probing approach speed 2 set to %f", probingParams.approachspeed2);
                }

                if ( req.probingParams.params.backoffDistance1 > 0.01 ) {
                    probingParams.backoffDistance1 = req.probingParams.params.backoffDistance1;
                    g_log.log(LL_INFO, "Probing backoff distance 1 set to %f", probingParams.backoffDistance1);
                }
                if ( req.probingParams.params.backoffDistance2 > 0.01 ) {
                    probingParams.backoffDistance2 = req.probingParams.params.backoffDistance2;
                    g_log.log(LL_INFO, "Probing backoff distance 2 set to %f", probingParams.backoffDistance2);
                }

                saveConfigToFile();
            }
            else if ( req.type == MT_PROBE ) {
                if ( homing_homedAxes < 0x07 ) {
                    g_log.log(LL_ERROR, "Ignoring probe request, not homed");
                    rejectedProbingResult = TR_FAIL_NOT_HOMED;
                }
                else if ( mStatus.mode != MM_NONE ) {
                    g_log.log(LL_WARN, "Ignoring probing request, not in idle state.");
                }
                else {
                    // this +1 is because the mCmd.probeType is a value like PT_DIGITAL but we want it to be 1-indexed
                    mCmd.probeType = req.probe.type + 1;
                    mCmd.probeZ = req.probe.z;
                    mCmd.probeWeight = req.probe.minWeight;

                    mCmd.traj = NULL;
                    shouldSendRTCommand = true;
                    g_log.log(LL_INFO, "Probing requested.");
                }
            }
            else  {
                printf("Ignoring command request type: %s\n", getMessageName(req.type));
                ackOrNack = false;
            }

            processCommandReply(&req, ackOrNack);
        }


        if ( jogButtonDown() ) {
            long long timeSinceLastJogRecv = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastJogRecvTime).count();
            if ( timeSinceLastJogRecv > 200 ) {
                printf("Canceling jog\n"); fflush(stdout);
                for (int i = 0; i < 3; i++)
                    jogDirs[i] = 0;
                memcpy(mCmd.jogDirs, jogDirs, sizeof(mCmd.jogDirs));
                mCmd.traj = NULL;
                shouldSendRTCommand = true;
            }
        }


        // vec3 followError = abs( mStatus.targetPos - mStatus.actualPos );
        // maxFollowError = scv::max( maxFollowError, followError );

//        printf("followError %f, %f, %f   %f, %f, %f\n", mStatus.targetPos.x, mStatus.targetPos.y, mStatus.targetPos.z
//               , mStatus.actualPos.x, mStatus.actualPos.y, mStatus.actualPos.z);

        if ( shouldSendRTCommand ) {
            ntCommand( &mCmd );
            mCmd.traj = NULL;
        }

        //mCmd.pwm = INVALID_FLOAT;

        // Some values in mCmd are not a persistent state, they are events.
        // Clear them so that the event doesn't keep happening every time
        memset(mCmd.homeAxes, 0, sizeof(mCmd.homeAxes));
        mCmd.probeType = 0;

        std::this_thread::sleep_for(5ms);
    }

    estop = true;

    printf("Waiting for current move to finish\n");
    waitForMotionIdle( mStatus, 10000 );


    printf("Wait a bit...\n");
    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(5ms);
        int numReports = rtReportCheck(&mStatus); // must clear old reports!!
        reportActualPosition( numReports, mStatus, false );
    }

/*
    printf("Setting up final move back to origin\n");
    {
        vec3 dest = vec3_zero;
        rtReportCheck(&mStatus);
        printf("Final move from %f, %f, %f to %f, %f, %f\n", mStatus.actualPos.x, mStatus.actualPos.y, mStatus.actualPos.z, dest.x, dest.y, dest.z);

        m1.vel = 150;

        plan.clear();
        plan.resetTraverse();
        m1.src = mStatus.actualPos;
        m1.dst = dest;
        //applyHomeOffsetToMove(m1);
        plan.appendMove(m1);
        plan.calculateMoves();
        plan.addOffsetToMoves(offsetAtHome);

        mCmd.traj = &plan;
        ntCommand( &mCmd );

        std::this_thread::sleep_for(5ms);
    }

    printf("Waiting for final move to finish\n");
    waitForMotionIdle( mStatus, 10000 );
*/

    sigIntRT = true;

    rt_thread.Join();

    rtReportCheck(&mStatus);
    printf("Exiting with position %f, %f, %f\n", mStatus.actualPos.x, mStatus.actualPos.y, mStatus.actualPos.z);
    printf("maxFollowError %f, %f, %f\n", maxFollowError.x, maxFollowError.y, maxFollowError.z);


#ifdef DO_ZMQ
    stopServer();
#endif

    return 0;
}
