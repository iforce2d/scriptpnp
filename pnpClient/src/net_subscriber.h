
#ifndef PNP_NET_SUBSCRIBER_H
#define PNP_NET_SUBSCRIBER_H

#include "pnpMessages.h"

void startSubscriber();
void stopSubscriber();
bool checkSubscriberMessages(clientReport_t* rep);

#endif
