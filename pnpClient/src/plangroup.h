#ifndef PLANGROUP_H
#define PLANGROUP_H

#include <vector>

#include "scv/planner.h"

class PlanGroup
{
    std::vector<scv::planner*> plans;
    int traversal_planIndex;
    float traversal_planTime;
public:
    PlanGroup();
    ~PlanGroup();

    void clear();

    scv::planner* addPlan();
    void calculateMovesForLastPlan();

    void resetTraverse();
    bool advanceTraverse(scv_float dt, scv_float speedScale, scv::vec3* p, scv::vec3 *v, float* rots, scv::traverseFeedback_t* feedback);
    float getTraverseTime();
};

#endif // PLANGROUP_H
