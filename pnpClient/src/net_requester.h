
#ifndef PNP_NET_REQUESTER_H
#define PNP_NET_REQUESTER_H

#include "pnpMessages.h"
#include "../common/packable.h"

void startRequester();
void stopRequester();
void sendCommandRequestOfType(uint16_t msgType);
bool sendCommandRequest(commandRequest_t* req);
bool sendPackable(uint16_t messageType, Packable& packable);
void checkRequestsQueue();
bool checkReplies(commandReply_t* rep);
bool isRequestInProgress();

#endif
