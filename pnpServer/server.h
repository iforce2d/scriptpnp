#ifndef SERVER_H
#define SERVER_H

#include "../common/scv/vec3.h"
#include "../common/pnpMessages.h"
#include "interThread.h"
#include "../common/commandlist.h"

bool startServer();
void stopServer();

void publishStatus(motionStatus* s, motionLimits currentMoveLimits, motionLimits currentRotationLimits, float speedScale, float jogSpeedScale, float weight, float probingZ);

bool checkCommandRequests(commandMessageType_e *msgType, commandRequest_t* req, CommandList* program);
void processCommandReply(commandRequest_t* rep, bool ackOrNack);

#endif
