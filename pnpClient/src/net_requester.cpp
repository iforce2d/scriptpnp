
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <queue>
#include <chrono>

#include <zmq.h>

#include "pnpMessages.h"
#include "commandlist.h"
#include "log.h"
#include "server_view.h"
#include "overrides.h"
#include "notify.h"

extern void* context;
void* requester = NULL;

using namespace std;

void startRequester()
{
    if ( ! context )
        return;

    //g_log.log(LL_DEBUG, "Starting requester...");

    char url[256];
    sprintf(url, "tcp://%s:5562", serverHostname);

    requester = zmq_socket(context, ZMQ_REQ);
    int cr = zmq_connect(requester, url);
    if ( cr != 0 ) {
        g_log.log(LL_ERROR, "Requester connect failed");
    }

    int lingerTime = 1;
    zmq_setsockopt(requester, ZMQ_LINGER, &lingerTime, sizeof(int));

    zmq_setsockopt(requester, ZMQ_REQ_CORRELATE, &lingerTime, sizeof(int));
    zmq_setsockopt(requester, ZMQ_REQ_RELAXED, &lingerTime, sizeof(int));

//    int reconnectInterval = 1000;
//    zmq_setsockopt(requester, ZMQ_RECONNECT_IVL, &reconnectInterval, sizeof(int));

    //sleep(1);

    g_log.log(LL_DEBUG, "Requester listening...");

//    dataSendSocket = zmq_socket(context, ZMQ_PAIR);
//    if ( !dataSendSocket ) {
//        printf("zmq_socket failed (dataSendSocket)\n");
//    }
//    else {
//        int rc = zmq_bind(dataSendSocket, "inproc://data");
//        if ( rc != 0 ) {
//            printf("zmq_bind failed (dataSendSocket)\n");
//        }
//        else {
//            zmq_setsockopt(dataSendSocket, ZMQ_RCVTIMEO, 0, 6);
//        }
//    }

//    int rc = pthread_create( &requesterThread, NULL, requesterThreadFunc, NULL );
//    if ( rc != 0 ) {
//        printf("pthread_create failed (startSubscriber)\n");
//    }
}

void stopRequester()
{
    if ( ! context )
        return;

    g_log.log(LL_DEBUG, "Stopping requester...");

//    void* doSignalSocket = zmq_socket(context, ZMQ_PAIR);
//    if ( ! doSignalSocket ) {
//        printf("zmq_socket failed (doSignalSocket)\n");
//    }
//    else {
//        int rc = zmq_connect(doSignalSocket, "inproc://stopRequester");
//        if ( rc != 0 ) {
//            printf("zmq_connect failed (doSignalSocket)\n");
//        }
//        else {
//            rc = zmq_send(doSignalSocket, "0", 1, 0);
////            if ( rc != 0 ) {
////                printf("zmq_send failed (doSignalSocket)\n");
////            }
//        }
//    }

//    pthread_join( requesterThread, NULL );

//    zmq_close(doSignalSocket);
//    zmq_close(dataSendSocket);
    zmq_close(requester);
    //zmq_ctx_destroy(context);

    //context = NULL;
    requester = NULL;

    //printf("done.\n"); fflush(stdout);
}

static int numRequestsInProgress = 0;
std::chrono::steady_clock::time_point lastRequestSendTime = {};

struct requestQueueEntry_t {
    commandRequest_t request;
    uint8_t* packed;
    int packedSize;

    requestQueueEntry_t() {
        packed = NULL;
        packedSize = 0;
    }

    void clear() {
        if ( packed )
            delete[] packed;
    }
};

std::queue<requestQueueEntry_t> requestsQueue;

bool isRequestInProgress() {
    return numRequestsInProgress > 0;
}

bool sendCommandRequest(commandRequest_t* req) {

    if ( ! requester )
        return false;

    if ( isRequestInProgress() ) { // todo make this atomic?
        g_log.log(LL_DEBUG, "Too many requests in progress, queueing");
        requestQueueEntry_t entry;
        entry.request = *req;
        requestsQueue.push( entry );
        return false;
    }

    bool ret = false;

    int msgSize = (int)sizeof(commandRequest_t);

    zmq_msg_t msgOut;
    if ( 0 != zmq_msg_init_size(&msgOut, msgSize)) {
        g_log.log(LL_FATAL, "zmq_msg_init_size failed");
    }
    else {
        memcpy( zmq_msg_data(&msgOut), req, msgSize );

        if ( msgSize != zmq_msg_send( &msgOut, requester, ZMQ_DONTWAIT ) ) {
            g_log.log(LL_ERROR, "zmq_msg_send failed, requester: %d (%s)", errno, strerror(errno));
        }
        else {
            //printf("Sent request\n"); fflush(stdout);
            lastRequestSendTime = std::chrono::steady_clock::now();
            numRequestsInProgress++;
            ret = true;
        }

        zmq_msg_close( &msgOut );
    }

    return ret;
}

void sendCommandRequestOfType(uint16_t msgType)
{
    commandRequest_t req = createCommandRequest(msgType);
    sendCommandRequest(&req);
}

void packPackable(Packable& packable, uint16_t messageType, uint8_t* data)
{
    uint16_t version = MESSAGE_VERSION;

    int pos = 0;

    memcpy(&data[pos], &version, sizeof(version));
    pos += sizeof(version);

    memcpy(&data[pos], &messageType, sizeof(messageType));
    pos += sizeof(messageType);

    packable.pack( &data[pos] );
}

bool sendAlreadyPacked(uint8_t* packed, int msgSize)
{
    bool ret = false;

    g_log.log(LL_DEBUG, "sendAlreadyPacked, message size: %d", msgSize);

    zmq_msg_t msgOut;
    if ( 0 != zmq_msg_init_size(&msgOut, msgSize)) {
        g_log.log(LL_FATAL, "zmq_msg_init_size failed");
    }
    else {
        uint8_t* data = (uint8_t*)zmq_msg_data(&msgOut);

        memcpy(data, packed, msgSize);

        if ( msgSize != zmq_msg_send( &msgOut, requester, ZMQ_DONTWAIT ) ) {
            g_log.log(LL_ERROR, "zmq_msg_send failed, requester: %d (%s)", errno, strerror(errno));
        }
        else {
            //printf("Sent request\n"); fflush(stdout);
            lastRequestSendTime = std::chrono::steady_clock::now();
            numRequestsInProgress++;
            ret = true;
        }

        zmq_msg_close( &msgOut );
    }

    return ret;
}

bool sendPackable(uint16_t messageType, Packable& packable) {

    if ( ! requester )
        return false;

    uint16_t version = MESSAGE_VERSION;

    int msgSize = sizeof(version) + sizeof(messageType) + packable.getSize();

    if ( isRequestInProgress() ) { // todo make this atomic?
        g_log.log(LL_DEBUG, "Too many requests in progress, queueing");

        uint8_t* data = new uint8_t[msgSize];
        requestQueueEntry_t entry;
        packPackable(packable, messageType, data);
        entry.packed = data;
        entry.packedSize = msgSize;

        requestsQueue.push( entry );

        return false;
    }

    bool ret = false;    

    g_log.log(LL_DEBUG, "Sending packable, message size: %d", msgSize);

    zmq_msg_t msgOut;
    if ( 0 != zmq_msg_init_size(&msgOut, msgSize)) {
        g_log.log(LL_FATAL, "zmq_msg_init_size failed");
    }
    else {
        uint8_t* data = (uint8_t*)zmq_msg_data(&msgOut);

        packPackable(packable, messageType, data);

        if ( msgSize != zmq_msg_send( &msgOut, requester, ZMQ_DONTWAIT ) ) {
            g_log.log(LL_ERROR, "zmq_msg_send failed, requester: %d (%s)", errno, strerror(errno));
        }
        else {
            //printf("Sent request\n"); fflush(stdout);
            lastRequestSendTime = std::chrono::steady_clock::now();
            numRequestsInProgress++;
            ret = true;
        }

        zmq_msg_close( &msgOut );
    }

    return ret;
}

void checkRequestsQueue() {

    if ( numRequestsInProgress > 0 )
        return;

    if ( ! requestsQueue.empty() ) {
        requestQueueEntry_t entry = requestsQueue.front();
        requestsQueue.pop();
        if ( entry.packed ) {
            g_log.log(LL_DEBUG, "Sending previously deferred packable");
            sendAlreadyPacked( entry.packed, entry.packedSize );
            entry.clear();
        }
        else {
            g_log.log(LL_DEBUG, "Sending previously deferred request");
            sendCommandRequest(&entry.request);
        }
    }
}

void stopSubscriber();
void startSubscriber();

void clearRequestsQueue()
{
    std::queue<requestQueueEntry_t> empty;
    std::swap( requestsQueue, empty );
}

void checkRequestTimeout() {
    if ( numRequestsInProgress > 0 ) {
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        long long timeSinceLastRequest = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRequestSendTime).count();
        if ( timeSinceLastRequest > 1000 ) {
            g_log.log(LL_DEBUG, "Send request timeout, resetting connection");

            stopRequester();
            stopSubscriber();

            clearRequestsQueue();
            numRequestsInProgress = 0;

            startSubscriber();
            startRequester();
        }
    }
}

bool checkReplies(commandReply_t* rep) {

    if ( ! requester )
        return false;

    zmq_pollitem_t items[] = {
        { requester, 0, ZMQ_POLLIN, 0 }
    };

    bool gotSomething = false;

    //while ( true ) { // read everything available - NO! REQ/REP does not allow concurrent requests anyway

        zmq_poll(items, 1, 0);

        if (items[0].revents & ZMQ_POLLIN)
        {
            //printf("Got reply data\n");

            zmq_msg_t msg;
            if ( 0 != zmq_msg_init(&msg) ) {
                g_log.log(LL_FATAL, "zmq_msg_init failed");
            }
            else {
                int rc = zmq_msg_recv( &msg, requester, 0);
                if ( rc == -1 ) {
                    g_log.log(LL_ERROR, "zmq_msg_recv for requester failed: %d (%s)", errno, strerror(errno));
                }
                else {
                    size_t msgSize = zmq_msg_size(&msg);
                    uint8_t* data = (uint8_t*)zmq_msg_data(&msg);

                    int pos = 0;

                    uint16_t version = 0;
                    memcpy(&version, &data[pos], sizeof(version));
                    pos += sizeof(version);

                    if ( version != MESSAGE_VERSION ) {
                        g_log.log(LL_ERROR, "Message version mismatch: expected %d, received %d", MESSAGE_VERSION, version);
                        string msg = "Message version mismatch: expected " + to_string(MESSAGE_VERSION) + ", received " + to_string(version);
                        notify( msg, NT_ERROR, 5000 );
                        //ImGui::InsertNotification({ type, timeout, msg.c_str() });
                    }
                    else
                    {
                        uint16_t messageType = 0;
                        memcpy(&messageType, &data[pos], sizeof(messageType));
                        pos += sizeof(messageType);

                        if ( messageType == MT_CONFIG_OVERRIDES_FETCH ) {
                            g_log.log(LL_DEBUG, "Got reply %s", getMessageName(MT_CONFIG_OVERRIDES_FETCH));
                            overrideConfigs.clear();
                            if ( ! overrideConfigSet.unpack( &data[pos] ) ) {
                                g_log.log(LL_ERROR, "Overrides unpack failed");
                            }
                            updateUIIndexOfOverrideOptions();
                        }
                        else {
                            memcpy(rep, zmq_msg_data(&msg), msgSize);
                            if ( rep->version != MESSAGE_VERSION )
                                g_log.log(LL_ERROR, "Message version mismatch: expected %d, received %d", MESSAGE_VERSION, rep->version);
                            else
                                gotSomething = true;
                        }
                    }

                    zmq_msg_close(&msg);
                    numRequestsInProgress--;
                }
            }
        }
    //     else {
    //         break;
    //     }
    // }

    checkRequestTimeout();

    return gotSomething;
}





