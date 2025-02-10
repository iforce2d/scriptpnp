
#include <stdio.h>
#include <string.h>

#include "motionGlobals.h"
#include "estop.h"
#include "weeny.h"

using namespace scv;

planner estopPlanner;

void initEstop() {
    // just received estop signal
    estopPlanner.clear();
    estopPlanner.resetTraverse();

    float currentVelMag = v.Length();
    if ( currentVelMag > 0.00001 ) {
        printf("Creating estop move: velmag: %f\n", currentVelMag);

        //estopRotate.src = rots[0];
        //estopRotate.dst = rots[0];
        //estopPlanner.appendRotate( estopRotate, 0 );

        memcpy(estopPlanner.traversal_rots, rots, sizeof(estopPlanner.traversal_rots));

        move m;
        m.moveType = MT_ESTOP;
        m.vel = currentVelMag;
        m.acc =  999999;
        m.jerk = 999999;
        m.src = p;
        m.dst = p + 0.01 * v; // 0.01 seconds into future if going straight
        estopPlanner.appendMove(m);

        estopPlanner.calculateMoves();
        //estopPlanner.addOffsetToMoves(offsetAtHome);
        printf("estop move:\n");
        estopPlanner.printMoves();

        currentTraj = &estopPlanner;
        motionMode = MM_TRAJECTORY;
    }
    else {
        printf("estop: already stopped (currentVelMag = %f)\n", currentVelMag);
        currentTraj = NULL;
        motionMode = MM_NONE;
    }

    // reset any outputs as necesary
    for (int i = 0; i < 16; i++) {
        uint16_t theBit = (1 << i);
        if ( estopDigitalOutUsed & theBit ) {
            if ( estopDigitalOutState & theBit )
                data.outputs |= theBit;
            else
                data.outputs &= ~theBit;
        }
    }

    for (int i = 0; i < NUM_PWM_VALS; i++) {
        if ( estopPWMUsed & (1 << i) ) {
            data.spindleSpeed = (uint16_t)(estopPWMState[i] * 65535);
        }
    }
}

