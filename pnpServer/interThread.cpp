
#include <cstring>
#include <stdio.h>
#include "interThread.h"
#include "../common/pnpMessages.h"

using namespace scv;

#define NUM_RT_REPORT 20

message_fromRT_toNT datalist_RT2NT[NUM_RT_REPORT];

int writeInd_RT2NT = 0;
int readInd_RT2NT = 0;

// realtime thread places a message in buffer for NT thread to pick up
//void rtReport(int mode, scv::vec3 targetPos, scv::vec3 actualPos)
void rtReport(motionStatus* sts)
{
    message_fromRT_toNT* d = &datalist_RT2NT[writeInd_RT2NT];
    if ( ! d->ready.load(std::memory_order_acquire) ) {

        d->mStatus = *sts;

        d->ready.store(true, std::memory_order_release);
        writeInd_RT2NT = (writeInd_RT2NT + 1) % NUM_RT_REPORT;
    }
    else
        ;//printf("rtReport: no room\n");
}

// normal thread checks if realtime thread has provided anything
int rtReportCheck(motionStatus *mStatus)
{
    int ret = 0;

    // the result of a move/home/probe is only reported once,
    // but we might be clearing many messages in this loop, so
    // make sure any non-'none' messages are caught
    homingResult_e hr = HR_NONE;
    probingResult_e pr = PR_NONE;
    trajectoryResult_e tr = TR_NONE;

    message_fromRT_toNT* d = &datalist_RT2NT[readInd_RT2NT];
    while ( d->ready.load(std::memory_order_acquire) ) {

        *mStatus = d->mStatus;

        if ( d->mStatus.homingResult != HR_NONE )
            hr = (homingResult_e)d->mStatus.homingResult;
        if ( d->mStatus.probingResult != PR_NONE )
            pr = (probingResult_e)d->mStatus.probingResult;
        if ( d->mStatus.trajectoryResult != TR_NONE )
            tr = (trajectoryResult_e)d->mStatus.trajectoryResult;

        d->ready.store(false, std::memory_order_release);
        readInd_RT2NT = (readInd_RT2NT + 1) % NUM_RT_REPORT;
        d = &datalist_RT2NT[readInd_RT2NT];
        ret++;
    }

    mStatus->homingResult = hr;
    mStatus->probingResult = pr;
    mStatus->trajectoryResult = tr;

    return ret;
}

int rtQueueLength() {
    int n = 0;
    for (int i = 0; i < NUM_RT_REPORT; i++) {
        if ( datalist_RT2NT[i].ready )
            n++;
    }
    return n;
}


//---------------------------------------------------------------------------

#define NUM_NT_COMMAND 20

message_fromNT_toRT datalist_NT2RT[NUM_NT_COMMAND];

int writeInd_NT2RT = 0;
int readInd_NT2RT = 0;

// normal thread places a message in buffer for realtime thread to pick up
void ntCommand(motionCommand* cmd)
{
    message_fromNT_toRT* d = &datalist_NT2RT[writeInd_NT2RT];
    if ( ! d->ready.load(std::memory_order_acquire) ) {        

        d->mCmd = *cmd;

        d->ready.store(true, std::memory_order_release);
        writeInd_NT2RT = (writeInd_NT2RT + 1) % NUM_NT_COMMAND;
    }
    else
        printf("ntCommand: no room\n");
}

// realtime thread checks if normal thread has provided anything
int ntCommandCheck(motionCommand *cmd)
{
    int ret = 0;
    message_fromNT_toRT* d = &datalist_NT2RT[readInd_NT2RT];
    while ( d->ready.load(std::memory_order_acquire) ) {

        *cmd = d->mCmd;

        d->ready.store(false, std::memory_order_release);
        readInd_NT2RT = (readInd_NT2RT + 1) % NUM_NT_COMMAND;
        d = &datalist_NT2RT[readInd_NT2RT];
        ret++;
    }
    return ret;
}

int ntQueueLength() {
    int n = 0;
    for (int i = 0; i < NUM_NT_COMMAND; i++) {
        if ( datalist_NT2RT[i].ready )
            n++;
    }
    return n;
}





void initInterThread() {
    for (int i = 0; i < NUM_RT_REPORT; i++) {
        memset(datalist_RT2NT, 0, sizeof(datalist_RT2NT));
    }
    for (int i = 0; i < NUM_NT_COMMAND; i++) {
        memset(datalist_NT2RT, 0, sizeof(datalist_NT2RT));
    }
}
