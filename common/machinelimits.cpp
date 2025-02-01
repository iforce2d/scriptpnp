#include "machinelimits.h"

using namespace std;
using namespace scv;

MachineLimits machineLimits;

MachineLimits::MachineLimits()
{
    // these are vel, acc, jerk
    velLimit.x = 100;
    velLimit.y = 100;
    velLimit.z = 100;

    accLimit.x = 5000;
    accLimit.y = 5000;
    accLimit.z = 5000;

    jerkLimit.x = 20000;
    jerkLimit.y = 20000;
    jerkLimit.z = 20000;

    grotationVelLimit =    3000;
    grotationAccLimit =   30000;
    grotationJerkLimit = 300000;

    initialMoveLimitVel =    100;
    initialMoveLimitAcc =   5000;
    initialMoveLimitJerk = 20000;
    initialRotationLimitVel =    3000;
    initialRotationLimitAcc =   30000;
    initialRotationLimitJerk = 300000;

    maxOverlapFraction = 0.8;
}

void MachineLimits::setPositionLimits(scv_float lx, scv_float ly, scv_float lz, scv_float ux, scv_float uy, scv_float uz)
{
    posLimitLower = vec3(lx,ly,lz);
    posLimitUpper = vec3(ux,uy,uz);
}

/*void MachineLimits::setVelocityLimits(scv_float x, scv_float y, scv_float z)
{
    velLimit = vec3(x,y,z);
}

void MachineLimits::setAccelerationLimits(scv_float x, scv_float y, scv_float z)
{
    accLimit = vec3(x,y,z);
}

void MachineLimits::setJerkLimits(scv_float x, scv_float y, scv_float z)
{
    jerkLimit = vec3(x,y,z);
}*/

void MachineLimits::setRotationPositionLimits(int axis, scv_float lower, scv_float upper)
{
    rotationPositionLimits[axis].x = lower;
    rotationPositionLimits[axis].y = upper;
}

void MachineLimits::setRotationVAJLimits(scv_float vel, scv_float acc, scv_float jerk)
{
    initialRotationLimitVel = vel;
    initialRotationLimitAcc = acc;
    initialRotationLimitJerk = jerk;
}

void MachineLimits::setLimitsInPlan(planner *p)
{
    p->setPositionLimits(machineLimits.posLimitLower.x, machineLimits.posLimitLower.y, machineLimits.posLimitLower.z,
                         machineLimits.posLimitUpper.x, machineLimits.posLimitUpper.y, machineLimits.posLimitUpper.z );
    p->setVelocityLimits(machineLimits.velLimit.x, machineLimits.velLimit.y, machineLimits.velLimit.z);
    p->setAccelerationLimits(machineLimits.accLimit.x, machineLimits.accLimit.y, machineLimits.accLimit.z);
    p->setJerkLimits(machineLimits.jerkLimit.x, machineLimits.jerkLimit.y, machineLimits.jerkLimit.z);

    for (int i = 0; i < NUM_ROTATION_AXES; i++)
        p->setRotationPositionLimits(i, machineLimits.rotationPositionLimits[i].x, machineLimits.rotationPositionLimits[i].y);
    p->setRotationVAJLimits(machineLimits.grotationVelLimit, machineLimits.grotationAccLimit, machineLimits.grotationJerkLimit);

    //p->setMaxCornerBlendOverlapFraction( maxOverlapFraction );
}
