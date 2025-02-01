#include "net_requester.h"
#include "net_subscriber.h"
#include "script_probing.h"

int script_PT_DIGITAL = PT_DIGITAL;
int script_PT_LOADCELL = PT_LOADCELL;
int script_PT_VACUUM = PT_VACUUM;

// int script_PR_NONE = PR_NONE;
// int script_PR_SUCCESS = PR_SUCCESS;
// int script_PR_FAIL_NOT_TRIGGERED = PR_FAIL_NOT_TRIGGERED;
// int script_PR_FAIL_CONFIG = PR_FAIL_CONFIG;
// int script_PR_FAIL_ALREADY_TRIGGERED = PR_FAIL_ALREADY_TRIGGERED;

extern probingResult_e lastProbingResult;
extern float lastProbedHeight;

int script_getProbeResult() {
    return lastProbingResult;
}

float script_getProbedHeight() {
    return lastProbedHeight;
}

void script_probe(float depth, int type, float minForce)
{
    commandRequest_t req = createCommandRequest(MT_PROBE);
    req.probe.type = type;
    req.probe.z = depth;
    req.probe.minWeight = minForce;

    lastProbingResult = PR_NONE;
    lastProbedHeight = 0;

    sendCommandRequest(&req);
}
