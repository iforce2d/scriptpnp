#ifndef MACHINELIMITS_H
#define MACHINELIMITS_H

#include "scv/planner.h"

class MachineLimits
{
public:
    // Hard limits that the machine must never exceed
    scv::vec3 posLimitLower;
    scv::vec3 posLimitUpper;
    scv::vec3 velLimit;
    scv::vec3 accLimit;
    scv::vec3 jerkLimit;

    scv::vec2 rotationPositionLimits[NUM_ROTATION_AXES];
    scv_float grotationVelLimit;
    scv_float grotationAccLimit;
    scv_float grotationJerkLimit;

    // Initial limits that will be slower, just to avoid super
    // fast movements if user has not yet given specific speeds
    scv_float initialMoveLimitVel;
    scv_float initialMoveLimitAcc;
    scv_float initialMoveLimitJerk;
    scv_float initialRotationLimitVel;
    scv_float initialRotationLimitAcc;
    scv_float initialRotationLimitJerk;

    float maxOverlapFraction;

    MachineLimits();

    void setPositionLimits(scv_float lx, scv_float ly, scv_float lz, scv_float ux, scv_float uy, scv_float uz);
    //void setVelocityLimits(scv_float x, scv_float y, scv_float z);
    //void setAccelerationLimits(scv_float x, scv_float y, scv_float z);
    //void setJerkLimits(scv_float x, scv_float y, scv_float z);

    void setRotationPositionLimits(int axis, scv_float lower, scv_float upper);
    void setRotationVAJLimits(scv_float vel, scv_float acc, scv_float jerk);

    void setLimitsInPlan(scv::planner* plan);
};

extern MachineLimits machineLimits;

#endif // MACHINELIMITS_H
