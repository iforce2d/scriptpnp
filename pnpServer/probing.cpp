#include <stdio.h>

#include "probing.h"
#include "log.h"
#include "motionGlobals.h"
#include "estop.h"
#include "weeny.h"
#include "../common/config.h"
#include "../common/machinelimits.h"
#include "movingAverage.h"

using namespace scv;

uint8_t probing_type = 0;
probingResult_e probing_result = PR_NONE;
planner probingPlanner;
probingPhase_e probing_phase = PP_DONE;
probingPhaseVacuumApproach_e probing_phase_vacuum = PPVA_SNIFFING;
int probing_phase_vacuum_sniffCount = 0;
float probing_resultHeight = 0; // final answer
float probing_minForce = 0;
float probing_targetZ = 0; // max depth / probe limit
int probing_flags = 0;

uint16_t pressureBeforeSniff = 0;
uint16_t probing_vac_baseline = 0;
bool probing_vac_contacted = false;
bool isLoadcellTriggered = false;

extern motionStatus mStatus;

bool isProbeTriggered()
{
    if ( probing_type == PT_DIGITAL ) {
        int pinState = (data.inputs & (1 << probingParams.digitalTriggerPin)) ? 1 : 0;
        if ( pinState == probingParams.digitalTriggerState )
            return true;
    }
    else if ( probing_type == PT_LOADCELL ) {
        //if ( probing_minForce == 0 )
            return isLoadcellTriggered;
        /*else if ( getWeight() > probing_minForce )
            return true;*/
    }
    return false;
}

void prepareProbingPlanner_vacuum() {

    //g_log.log(LL_DEBUG,"prepareProbingPlanner_vacuum, phase = %d, subphase = %d, count = %d", probing_phase, probing_phase_vacuum, probing_phase_vacuum_sniffCount);
    //printf("probe: %f, %f, %f\n", p.x, p.y, p.z); fflush(stdout);

    if ( probing_phase == PP_APPROACH1 ) {
        if ( probing_phase_vacuum == PPVA_SNIFFING ) {

            pressureBeforeSniff = mStatus.pressure;

            probingPlanner.clear();
            probingPlanner.resetTraverse();
            probingPlanner.appendDigitalOutput( 0x2, 0x2, 0 );
            probingPlanner.appendWait( p, probingParams.vacuumSniffTimeMs / 1000.0f );
            probingPlanner.appendSync(p);
            probingPlanner.calculateMoves();

            //g_log.log(LL_DEBUG,"sniff moves");
            //probingPlanner.printMoves();

            probing_phase_vacuum_sniffCount++;

            //printf("Pressure before sniff: %d\n", pressureBeforeSniff );

            return;
        }
        else if ( probing_phase_vacuum == PPVA_CLOSING ) {

            probingPlanner.clear();
            probingPlanner.resetTraverse();
            probingPlanner.appendDigitalOutput( 0x0, 0x2, 0 );
            probingPlanner.appendSync(p);
            probingPlanner.calculateMoves();

            //g_log.log(LL_DEBUG,"closing moves");
            //probingPlanner.printMoves();

            return;
        }
        else if ( probing_phase_vacuum == PPVA_REPOSITIONING ) {

            probingPlanner.clear();
            probingPlanner.resetTraverse();

            move m;
            m.moveType = MT_NORMAL;
            m.vel = probingParams.approachspeed1;
            m.acc =  999999;
            m.jerk = 999999;
            m.src = p;
            m.dst = p;
            m.dst[2] -= probingParams.vacuumStep;

            probingPlanner.appendMove(m);
            if ( probingParams.vacuumReplenishTimeMs > 0 )
                probingPlanner.appendWait( m.dst, probingParams.vacuumReplenishTimeMs / 1000.0f );
            probingPlanner.calculateMoves();

            //g_log.log(LL_DEBUG,"repositioning moves");
            //probingPlanner.printMoves();

            return;
        }
    }
    else if ( probing_phase == PP_BACKOFF2 ) {

    }

}

void prepareProbingPlanner() {

    if ( probing_type == PT_VACUUM && probing_phase == PP_APPROACH1 ) {
        prepareProbingPlanner_vacuum();
        return;
    }

    //g_log.log(LL_DEBUG,"prepareProbingPlanner, phase = %d", probing_phase);
    //printf("probe: %f, %f, %f\n", p.x, p.y, p.z);

    float speed = 0;
    switch (probing_phase) {
    case PP_APPROACH1: speed = probingParams.approachspeed1; break;
    case PP_APPROACH2: speed = probingParams.approachspeed2; break;
    case PP_BACKOFF1:
    case PP_BACKOFF2:
        speed = 999999;
        break;
    case PP_DONE: // shouldn't get here
        break;
    }

    float zDest = p.z;
    if ( probing_phase == PP_APPROACH1 || probing_phase == PP_APPROACH2 ) {
        zDest = probing_targetZ - offsetAtHome.z;
    }
    else if ( probing_phase == PP_BACKOFF1 ) {
        zDest = p.z + probingParams.backoffDistance1;
    }
    else if ( probing_phase == PP_BACKOFF2 ) {
        zDest = p.z + probingParams.backoffDistance2;
    }

    float maxZLimit = machineLimits.posLimitUpper.z - offsetAtHome.z;
    if ( zDest > maxZLimit ) {
        zDest = maxZLimit;
        g_log.log(LL_DEBUG,"prepareProbingPlanner, limiting backoff to workspace");
    }

    probingPlanner.clear();
    probingPlanner.resetTraverse();

    move m;
    m.moveType = MT_NORMAL;
    m.vel = speed;
    m.acc =  999999;
    m.jerk = 999999;
    m.src = p;
    m.dst = p;
    m.dst[2] = zDest;

    probingPlanner.appendMove(m);
    probingPlanner.calculateMoves();

    //g_log.log(LL_DEBUG,"probing planner moves");
    //probingPlanner.printMoves();
}

void startProbe() {

    g_log.log(LL_DEBUG,"startProbe");

    probing_phase_vacuum = PPVA_SNIFFING;
    probing_vac_contacted = false;
    probing_phase_vacuum_sniffCount = 0;
    probing_phase = PP_APPROACH1;

    if ( probing_type == PT_LOADCELL || probing_type == PT_DIGITAL ) {
        if ( (rtCommand.probeFlags & PP_FAST_APPROACH) == 0 ) // only do slow approach
            probing_phase = PP_APPROACH2;
    }

    prepareProbingPlanner();
}

void applyChangedBits(uint16_t& val, uint16_t bits, uint16_t changed);

void doProbingUpdate_vacuum() {

    // in here probing_phase should always be PP_APPROACH1

    /*g_log.log(LL_INFO, "probing: fake fail");
    probing_result = PR_FAIL_ALREADY_TRIGGERED;
    probing_phase = PP_DONE;
    motionMode = MM_NONE;
    return;*/

    float advanceTime = 0.001;

    vec3 probingP, probingV;
    float probingRots[NUM_ROTATION_AXES];
    bool stillRunning = probingPlanner.advanceTraverse( advanceTime, 1, &probingP, &probingV, probingRots, &segmentFeedback );
    if ( segmentFeedback.stillRunning )
        stillRunning = true;
    if ( stillRunning ) {
        p = probingP;
        v = probingV;
    }
    if ( segmentFeedback.digitalOutputChanged ) {
        uint16_t bits = segmentFeedback.digitalOutputBits;
        uint16_t changed = segmentFeedback.digitalOutputChanged;
        applyChangedBits( data.outputs, bits, changed );
    }

    if ( ! stillRunning ) {
        if ( probing_phase_vacuum == PPVA_SNIFFING ) {

            //printf("Pressure after sniff: %d\n", mStatus.pressure );
            uint16_t pressureDiff = mStatus.pressure - pressureBeforeSniff;

            if ( probing_phase_vacuum_sniffCount == 1 ) {
                probing_vac_baseline = pressureDiff;
                printf("probing_vac_baseline: %d\n", probing_vac_baseline);
            }
            else {
                printf("Pressure diff: %d\n", pressureDiff );
                if ( pressureDiff < (probing_vac_baseline * 0.45) ) {
                    printf("Contacted!\n");
                    probing_resultHeight = mStatus.actualPos.z; // this is quantized to steps
                    probing_vac_contacted = true;
                }
            }

            probing_phase_vacuum = PPVA_CLOSING;

            prepareProbingPlanner_vacuum();

            return;
        }
        else if ( probing_phase_vacuum == PPVA_CLOSING ) {

            //printf("Finished close\n");

            if ( probing_vac_contacted ) {
                probing_phase = PP_BACKOFF2;
                printf("Resuming normal backoff2\n");
                prepareProbingPlanner(); // <---- back to normal procedure
                return;
            }
            else {
                if ( p.z <= probing_targetZ ) {
                    //printf("probing: no contact detected\n");
                    probing_result = PR_FAIL_NOT_TRIGGERED;
                    probing_phase = PP_DONE;
                    motionMode = MM_NONE;
                    v = vec3_zero;
                }
                else
                    probing_phase_vacuum = PPVA_REPOSITIONING;
            }

            prepareProbingPlanner_vacuum();

            return;
        }
        else if ( probing_phase_vacuum == PPVA_REPOSITIONING ) {

            //printf("Finished reposition\n");

            probing_phase_vacuum = PPVA_SNIFFING;

            prepareProbingPlanner_vacuum();

            /*g_log.log(LL_INFO, "probing: fake fail");
            probing_result = PR_FAIL_ALREADY_TRIGGERED;
            probing_phase = PP_DONE;
            motionMode = MM_NONE;
            return;*/
        }
    }

}

void doProbingUpdate() {

    if ( probing_type == PT_VACUUM && probing_phase == PP_APPROACH1 ) {
        doProbingUpdate_vacuum();
        return;
    }

    /*g_log.log(LL_INFO, "probing: fake fail");
    probing_result = PR_FAIL_ALREADY_TRIGGERED;
    probing_phase = PP_DONE;
    motionMode = MM_NONE;
    //initEstop();
    return;*/

    if ( probing_phase == PP_APPROACH1 || probing_phase == PP_APPROACH2 ) {

        if ( isProbeTriggered() ) {
            printf("probing: triggered\n");
            if ( probing_phase == PP_APPROACH2 ) {
                probing_resultHeight = mStatus.actualPos.z; // this is quantized to steps
                printf("probing_resultHeight: %f\n", probing_resultHeight);
            }
            probing_phase = (probingPhase_e)(probing_phase + 1);
            prepareProbingPlanner();
        }
    }

    float advanceTime = 0.001;

    vec3 probingP, probingV;//, probingA;
    float probingRots[NUM_ROTATION_AXES];
    bool stillRunning = probingPlanner.advanceTraverse( advanceTime, 1, &probingP, &probingV, probingRots, &segmentFeedback );
    if ( stillRunning ) {
        p = probingP;
        v = probingV;
        //a = probingA;
    }

    if ( probing_phase == PP_APPROACH1 || probing_phase == PP_APPROACH2 ) {
        if ( ! stillRunning ) {
            printf("probing: no hit detected\n");
            probing_result = PR_FAIL_NOT_TRIGGERED;
            probing_phase = PP_DONE;
            motionMode = MM_NONE;
            v = vec3_zero;
        }
    }
    else if ( probing_phase == PP_BACKOFF1 || probing_phase == PP_BACKOFF2 ) {
        if ( ! stillRunning ) {
            // backoff just finished
            probing_phase = (probingPhase_e)(probing_phase + 1);
            if ( probing_phase == PP_DONE ) {
                printf("probing: completed\n");
                probing_result = PR_SUCCESS;
                probing_phase = PP_DONE;
                motionMode = MM_NONE;
                v = vec3_zero;
            }
            else {
                if ( probing_phase == PP_APPROACH2 ) {
                    // just entered second approach phase, check if already triggered
                    if ( isProbeTriggered() ) {
                        printf("probing: backoff too short?\n");
                        probing_result = PR_FAIL_ALREADY_TRIGGERED;
                        probing_phase = PP_DONE;
                        motionMode = MM_NONE;
                        v = vec3_zero;
                    }
                }
                prepareProbingPlanner(); // next phase
            }
        }
        /*else if ( probing_phase == PP_BACKOFF1 ) {
            // still backing off
            if ( ! isProbeTriggered() ) {
                // trigger has been cleared can start approach2 early
                printf("probing: trigger cleared\n");
                probing_phase = (probingPhase_e)(probing_phase + 1);
                prepareProbingPlanner();
            }
        }*/
    }
}

bool checkProbingStartConditions() {

    if ( probingParams.approachspeed1 <= 0 ||
        probingParams.approachspeed2 <= 0 ||
        probingParams.backoffDistance1 <= 0 ||
        probingParams.backoffDistance2 < 0 ) {
        g_log.log(LL_INFO, "probing: invalid approach speed or backoff distance (<=0)");
        probing_result = PR_FAIL_CONFIG;
        return false;
    }

    if ( probingParams.digitalTriggerPin >= 16 ) {
        g_log.log(LL_INFO, "probing: invalid trigger pin (>=16)");
        probing_result = PR_FAIL_CONFIG;
        return false;
    }

    if ( rtCommand.probeZ < machineLimits.posLimitLower.z ||
        rtCommand.probeZ > machineLimits.posLimitUpper.z ) {
        g_log.log(LL_INFO, "probing: invalid Z depth");
        probing_result = PR_FAIL_CONFIG;
        return false;
    }

    /*if ( rtCommand.probeType == (PT_LOADCELL+1) ) {
        if ( rtCommand.probeWeight < 0 ) {
            g_log.log(LL_INFO, "probing: invalid weight for loadcell probe");
            probing_result = PR_FAIL_CONFIG;
            return false;
        }
    }*/

    if ( rtCommand.probeType == (PT_VACUUM+1) ) {
        if ( probingParams.vacuumSniffPin >= 16 ) {
            g_log.log(LL_INFO, "probing: invalid vacuum pin for vacuum probe");
            probing_result = PR_FAIL_CONFIG;
            return false;
        }
        if ( probingParams.vacuumSniffTimeMs <= 0 ) {
            g_log.log(LL_INFO, "probing: invalid sniff time for vacuum probe");
            probing_result = PR_FAIL_CONFIG;
            return false;
        }
        if ( probingParams.vacuumReplenishTimeMs < 0 ) {
            g_log.log(LL_INFO, "probing: invalid replenish time for vacuum probe");
            probing_result = PR_FAIL_CONFIG;
            return false;
        }
        if ( probingParams.vacuumStep < 0.001 ) {
            g_log.log(LL_INFO, "probing: invalid step distance for vacuum probe");
            probing_result = PR_FAIL_CONFIG;
            return false;
        }

    }

    // this is used in isProbeTriggered, so set first
    //probing_minForce = rtCommand.probeWeight;

    // for vaccum sniff, how to know if it's already touching?

    if ( isProbeTriggered() ) {
        g_log.log(LL_INFO, "probing: already in triggered state");
        probing_result = PR_FAIL_ALREADY_TRIGGERED;
        return false;
    }

    probing_type = rtCommand.probeType - 1;
    probing_targetZ = rtCommand.probeZ;

    return true;
}












// ---------------- load cell stuff below ----------------

extern motionMode_e motionMode;

int32_t loadcellCalibrationRawOffset = 1000;
float loadcellCalibrationWeight = 100;

MovingAverage<int32_t, 200> baselineMA;    // 1 sec
//MovingAverage<int32_t,   2> measurementMA; // 0.01 sec

void resetLoadcell() {
    baselineMA.reset();
    //measurementMA.reset();
}

void updateLoadcell(int32_t loadCellRaw) {

    isLoadcellTriggered = false;

    int fullCount = baselineMA.getReadingCount();

    // always use for baseline if just started
    if ( baselineMA.getNumReadingsTaken() < fullCount ) {
        baselineMA.addReading(loadCellRaw);
        //measurementMA.reset();
        return;
    }

    /*if ( baselineMA.getNumReadingsTaken() == fullCount ) {
        //baselineRange = baselineMA.getRange();
        g_log.log(LL_INFO, "Load cell baselineRange: %f", baselineMA.getRange());
    }*/

    float avg = baselineMA.getAverage();
    float threshold = avg + probingParams.loadcellTriggerThreshold;

    isLoadcellTriggered =
        ( probingParams.loadcellTriggerThreshold > 0 && loadCellRaw > threshold ) ||
        ( probingParams.loadcellTriggerThreshold < 0 && loadCellRaw < threshold );

    // only update baseline while not probing, or during first approach which tends to be longer and baseline slowly moves
    bool allowBaselineAdjustment =
        (motionMode == MM_NONE) ||
        (motionMode == MM_PROBING && probing_phase == PP_APPROACH1);
    if ( allowBaselineAdjustment ) {
        //bool b =
        baselineMA.addReading(loadCellRaw);
        /*if ( b ) {
            // periodically output to log
            g_log.log(LL_INFO, "Adjusted load cell baseline: %f", avg);
        }*/
        //measurementMA.reset();
        //return;
    }

    //measurementMA.addReading(loadCellRaw);
}

float getLoadCellBaseline() {
    return baselineMA.getAverage();
}


