
#include <string.h>
#include "plangroup.h"
#include "machinelimits.h"

using namespace std;
using namespace scv;

PlanGroup::PlanGroup() {
    traversal_planIndex = 0;
    traversal_planTime = 0;
    scriptWaitTime = 0;
    type = -1;
}

PlanGroup::~PlanGroup()
{
    clear();
}

void PlanGroup::clear()
{
    for ( planner *p : plans ) {
        p->clear();
        delete p;
    }
    plans.clear();
    scriptWaitTime = 0;
}

void PlanGroup::setType(int t) {
    type = t;
}

int PlanGroup::getType() {
    return type;
}

extern vec3 lastActualPos;
extern float lastActualRots[4];

planner *PlanGroup::addPlan()
{
    planner* p = new planner();

    machineLimits.setLimitsInPlan(p);

    if ( ! plans.empty() ) {
        planner* prevPlan = plans.back();
        p->startingPosition = prevPlan->moves.back().dst;
        //memcpy(p->startingRotations, prevPlan->traversal_rots, sizeof(p->traversal_rots));
        prevPlan->getFinalRotations(p->startingRotations);
    }
    else {
        p->startingPosition = lastActualPos;
        memcpy(p->startingRotations, lastActualRots, sizeof(p->startingRotations));
    }

    plans.push_back(p);
    return p;
}

void PlanGroup::calculateMovesForLastPlan()
{
    planner *p = plans.back();

    /*if ( p->moves.empty() ) {
        plans.pop_back();
        delete p;
        return;
    }*/

    if ( plans.size() > 1 ) {
        planner *prevPlan = plans[plans.size()-2];
        if ( ! p->moves.empty() )
            p->moves[0].src = prevPlan->moves.back().dst;
        memcpy(p->traversal_rots, prevPlan->traversal_rots, sizeof(p->traversal_rots));
    }
    else {
        if ( ! p->moves.empty() )
            p->moves[0].src = lastActualPos;
        memcpy(p->traversal_rots, lastActualRots, sizeof(p->traversal_rots));
    }

    p->calculateMoves();
}

void PlanGroup::addWaitTime(int millis)
{
    scriptWaitTime += millis;
}

void PlanGroup::resetTraverse()
{
    traversal_planIndex = 0;
    traversal_planTime = 0;

    for ( planner *p : plans )
        p->resetTraverse();
}

bool PlanGroup::advanceTraverse(float dt, float speedScale, vec3 *p, vec3 *v, float *rots, traverseFeedback_t *feedback)
{
    if ( traversal_planIndex >= (int)plans.size() )
        return false;

    planner* plan = plans[traversal_planIndex];
    bool stillRunning = plan->advanceTraverse(dt, speedScale, p, v, rots, feedback);
    stillRunning |= feedback->stillRunning;

    if ( stillRunning )
        return true;

    traversal_planIndex++;
    traversal_planTime = 0;

    return traversal_planIndex < (int)plans.size();
}

float PlanGroup::getTraverseTime()
{
    float t = 0;
    for (planner *p : plans)
        t += p->getTraverseTime();
    t += scriptWaitTime * 0.001f;
    return t;
}
