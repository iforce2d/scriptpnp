
#include "homing.h"
#include "log.h"
#include "motionGlobals.h"
#include "estop.h"
#include "weeny.h"

using namespace scv;

std::vector<int> homing_axesRemaining;
int homing_currentAxis;
homingResult_e homing_result = HR_NONE;
planner homingPlanner;
homingPhase_e homing_phase;
std::chrono::steady_clock::time_point homingStartTime = {};
long int homingTimeout = 60000;
uint8_t homing_homedAxes = 0;

void resetAxesAboutToHome(uint8_t* orderingOneIndexed) {
    for (int i = 0; i < NUM_HOMABLE_AXES; i++) {
        if ( orderingOneIndexed[i] != 0 ) {
            homing_homedAxes &= ~(1 << (orderingOneIndexed[i]-1));
        }
    }
}

void prepareHomingPlanner() {

    int axis = homing_currentAxis - 1; // zero index now

    homingParams_t& params = homingParams[axis];

    float speed = 0;
    switch (homing_phase) {
    case HP_APPROACH1: speed = params.approachspeed1; break;
    case HP_APPROACH2: speed = params.approachspeed2; break;
    case HP_BACKOFF1:
    case HP_BACKOFF2:
        speed = 999999;
        break;
    case HP_DONE: // shouldn't get here
        break;
    }

    float homingDistance = speed * homingTimeout * 0.001;

    int dir = params.direction ? 1 : -1;
    if ( homing_phase == HP_BACKOFF1 || homing_phase == HP_BACKOFF2 ) {
        dir *= -1;
        homingDistance = (homing_phase == HP_BACKOFF1) ? params.backoffDistance1 : params.backoffDistance2;
    }

    homingPlanner.clear();
    homingPlanner.resetTraverse();

    move m;
    m.moveType = MT_NORMAL;
    m.vel = speed;
    m.acc =  999999;
    m.jerk = 999999;
    m.src = p;
    m.dst = p;
    m.dst[axis] += dir * homingDistance;

    homingPlanner.appendMove(m);
    homingPlanner.calculateMoves();
    //homingPlanner.addOffsetToMoves(offsetAtHome);

    //printf("homing planner moves\n");
    //homingPlanner.printMoves();

    if ( homing_phase == HP_APPROACH1 || homing_phase == HP_APPROACH2 )
        homingStartTime = std::chrono::steady_clock::now();
}

void startHomeNextAxis() {

    //    printf("homeNextAxis: remaining: ");
    //    for (int i = 0; i < (int)homing_axesRemaining.size(); i++)
    //        printf("%d ", homing_axesRemaining[i] );
    //    printf("\n");

    homing_currentAxis = homing_axesRemaining.front();
    homing_axesRemaining.erase( homing_axesRemaining.begin() );

    if ( homing_currentAxis == 0 ) {
        //printf("homeNextAxis: finished ok\n");
        g_log.log(LL_INFO, "Homing offset: %f %f %f", offsetAtHome.x, offsetAtHome.y, offsetAtHome.z);
        homing_result = HR_SUCCESS;
        motionMode = MM_NONE; // successful completion
        v = vec3_zero;
        return;
    }

    if ( homing_currentAxis >= 5 ) {
        g_log.log(LL_ERROR, "homeNextAxis: invalid axis %d\n", homing_currentAxis);
        homing_result = HR_FAIL_CONFIG;
        motionMode = MM_NONE;
        v = vec3_zero;
        return;
    }

    //printf("homeNextAxis: homing axis %d\n", homing_currentAxis);

    homing_phase = HP_APPROACH1;
    prepareHomingPlanner();
}

void doHomingUpdate() {

    if ( homing_phase == HP_APPROACH1 || homing_phase == HP_APPROACH2 ) {

        // only check timeout during approaches, not backoffs
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        long long timeSinceHomingStart = std::chrono::duration_cast<std::chrono::milliseconds>(now - homingStartTime).count();
        if ( timeSinceHomingStart > homingTimeout ) {
            g_log.log(LL_INFO, "homing: axis %d timed out", homing_currentAxis);
            homing_result = HR_FAIL_TIMED_OUT;
            homing_phase = HP_DONE;
            initEstop();
            return;
        }

        int axis = homing_currentAxis - 1;
        homingParams_t& params = homingParams[axis];

        int pinState = (data.inputs & (1 << params.triggerPin)) ? 1 : 0;

        if ( pinState == params.triggerState ) {
            g_log.log(LL_INFO, "homing: axis %d triggered", homing_currentAxis);

            homing_phase = (homingPhase_e)(homing_phase + 1);
            prepareHomingPlanner();
        }
    }

    float advanceTime = 0.001;// * rtCommand.speedScale;

    vec3 homingP, homingV;
    float homingRots[NUM_ROTATION_AXES];
    bool stillRunning = homingPlanner.advanceTraverse( advanceTime, 1, &homingP, &homingV, homingRots, &segmentFeedback );
    if ( stillRunning ) {
        p = homingP;
        v = homingV;
    }

    if ( homing_phase == HP_BACKOFF1 || homing_phase == HP_BACKOFF2 ) {
        if ( ! stillRunning ) {
            homing_phase = (homingPhase_e)(homing_phase + 1);
            if ( homing_phase == HP_DONE ) {
                //printf("Homing complete for axis %d\n", homing_currentAxis);
                int axis = homing_currentAxis - 1;
                homingParams_t& params = homingParams[axis];

                offsetAtHome[axis] = params.homedPosition - p[axis];
                //printf("offsetAtHome[%d] = %f\n", axis, offsetAtHome[axis]);

                homing_homedAxes |= (1 << axis);

                startHomeNextAxis();
            }
            else {
                prepareHomingPlanner(); // next phase
            }
        }
    }
}

bool checkHomingStartConditions() {

    for (int i = 0; i < (int)sizeof(rtCommand.homeAxes); i++) {
        if ( rtCommand.homeAxes[i] ) {

            int axis = rtCommand.homeAxes[i] - 1;
            homingParams_t& params = homingParams[axis];
            //printf("%f %f %f %f\n", params.approachspeed1, params.approachspeed2, params.backoffDistance1, params.backoffDistance2);

            if ( params.approachspeed1 <= 0 ||
                params.approachspeed2 <= 0 ||
                params.backoffDistance1 <= 0 ||
                params.backoffDistance2 <= 0 ) {
                g_log.log(LL_INFO, "homing: Invalid approach speed or backoff distance (<=0) for axis %d", axis+1);
                homing_result = HR_FAIL_CONFIG;
                return false;
            }

            if ( params.triggerPin >= 16 ) {
                g_log.log(LL_INFO, "homing: Invalid trigger pin (>=16) for axis %d", axis+1);
                homing_result = HR_FAIL_CONFIG;
                return false;
            }

            int pinState = (data.inputs & (1 << params.triggerPin)) ? 1 : 0;
            //printf("pinState: %d\n", pinState);
            if ( pinState == params.triggerState ) {
                g_log.log(LL_INFO, "homing: pin already in trigger state for axis %d", axis+1);
                homing_result = HR_FAIL_LIMIT_ALREADY_TRIGGERED;
                return false;
            }
        }
    }
    return true;
}
