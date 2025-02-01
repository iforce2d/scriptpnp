
#include <cstdio>
#include "pnpMessages.h"

#define tmpMacro(theval) #theval,

char const *messageNames[] =
{
    "MT_NONE",
    COMMAND_MESSAGE_LIST
};

#undef tmpMacro

char gmnBuf[64];
const char* getMessageName(uint8_t type) {
    if ( type < MT_MAX )
        return messageNames[type];
    else {
        sprintf(gmnBuf, "%d", type);
        return &gmnBuf[0];
    }
}

const char* getModeName(int mode) {
    switch ( mode ) {
    case MM_NONE: return "none";
    case MM_TRAJECTORY: return "traj";
    case MM_JOG: return "jog";
    case MM_HOMING: return "homing";
    case MM_PROBING: return "probing";
    }
    return "getModeName: unknown";
}

const char* getHomingResultName(int mode) {
    switch ( mode ) {
    case HR_NONE: return "none";
    case HR_SUCCESS: return "success";
    case HR_FAIL_CONFIG: return "fail (config)";
    case HR_FAIL_LIMIT_ALREADY_TRIGGERED: return "fail (limit already triggered)";
    case HR_FAIL_TIMED_OUT: return "fail (timed out)";
    }
    return "getHomingResultName: unknown";
}

const char* getProbingResultName(int mode) {
    switch ( mode ) {
    case PR_NONE: return "none";
    case PR_SUCCESS: return "success";
    case PR_FAIL_CONFIG: return "fail (config)";
    case PR_FAIL_NOT_HOMED: return "fail (not homed)";
    case PR_FAIL_NOT_TRIGGERED: return "fail (not triggered)";
    case PR_FAIL_ALREADY_TRIGGERED: return "fail (already triggered)";
    }
    return "getProbingResultName: unknown";
}

const char* getTrajectoryResultName(int mode) {
    switch ( mode ) {
    case TR_NONE: return "none";
    case TR_SUCCESS: return "success";
    case TR_FAIL_CONFIG: return "fail (invalid config)";
    case TR_FAIL_NOT_HOMED: return "fail (not homed)";
    case TR_FAIL_OUTSIDE_BOUNDS: return "fail (move outside bounds)";
    case TR_FAIL_FOLLOWING_ERROR: return "fail (following error)";
    case TR_FAIL_LIMIT_TRIGGERED: return "fail (limit triggered)";
    }
    return "getTrajectoryResultName: unknown";
}

commandRequest_t createCommandRequest(uint16_t msgType)
{
    commandRequest_t req;
    req.version = MESSAGE_VERSION;
    req.type = msgType;
    return req;
}
