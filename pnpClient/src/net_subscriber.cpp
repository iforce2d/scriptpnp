
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include <zmq.h>

#include "pnpMessages.h"
#include "log.h"
#include "server_view.h"

#include "imgui_notify/imgui_notify.h"

void* context = NULL;
void* subscriber = NULL;

pthread_t subscriberThread = 0;
void* dataRecvSocket = NULL;

bool alreadyReportedWrongSubscriberMessageVersion = false;

void* subscriberThreadFunc( void* ptr ) {

    if ( ! context )
        return NULL;

    void* signalStopSocket = zmq_socket(context, ZMQ_PAIR);
    if ( !signalStopSocket ) {
        g_log.log(LL_FATAL, "zmq_socket failed (signalStopSocket)");
    }
    else {
        int rc = zmq_bind(signalStopSocket, "inproc://stopSubscriber");
        if ( rc != 0 ) {
            g_log.log(LL_FATAL, "zmq_bind failed (signalStopSocket)");
        }
    }

    void* reportDataSocket = zmq_socket(context, ZMQ_PAIR);
    if ( ! reportDataSocket ) {
        g_log.log(LL_FATAL, "zmq_socket failed (reportDataSocket)");
    }
    else {
        int rc = zmq_connect(reportDataSocket, "inproc://subscriberData");
        if ( rc != 0 ) {
            g_log.log(LL_ERROR, "zmq_connect failed (reportDataSocket)");
        }
    }


    zmq_pollitem_t items[] = {
        { subscriber, 0, ZMQ_POLLIN, 0 },
        { signalStopSocket, 0, ZMQ_POLLIN, 0 }
    };

    while (1)
    {
        zmq_poll(items, 2, -1);
        if (items[1].revents & ZMQ_POLLIN)
        {
            //printf("Exiting subscriberThreadFunc\n"); fflush(stdout);
            break;
        }

        if (items[0].revents & ZMQ_POLLIN)
        {
            zmq_msg_t msgIn;
            if ( 0 != zmq_msg_init(&msgIn) ) {
                g_log.log(LL_FATAL, "zmq_msg_init failed");
            }
            else {
                int rc = zmq_msg_recv( &msgIn, subscriber, 0);
                if ( rc == -1 ) {
                    g_log.log(LL_ERROR, "zmq_msg_recv failed: %d (%s)", errno, strerror(errno));
                }
                else {
                    //clientReport_t* apr = (clientReport_t*)zmq_msg_data( &msg );
                    //printf("spi: %d, mode: %d, actualPos: %f %f %f\n", apr->spiOk, apr->mode, apr->x, apr->y, apr->z); fflush(stdout);

                    int msgSize = (int)zmq_msg_size(&msgIn);

                    zmq_msg_t msgOut;
                    if ( 0 != zmq_msg_init_size(&msgOut, msgSize)) {
                        g_log.log(LL_FATAL, "zmq_msg_init_size failed");
                    }
                    else {
                        memcpy( zmq_msg_data(&msgOut), zmq_msg_data(&msgIn), msgSize );
//printf("sending\n"); fflush(stdout);
                        if ( msgSize != zmq_msg_send( &msgOut, reportDataSocket, 0 ) ) {
                            g_log.log(LL_ERROR, "zmq_msg_send failed");
                        }

                        zmq_msg_close( &msgOut );
                    }
                    zmq_msg_close( &msgIn );
                }
            }
        }

    }

    zmq_close(signalStopSocket);
    zmq_close(reportDataSocket);

    return NULL;
}

void startSubscriber()
{
    if ( context )
        return;

    alreadyReportedWrongSubscriberMessageVersion = false;

    g_log.log(LL_DEBUG, "Starting subscriber...");

    context = zmq_ctx_new();
    //zmq_ctx_set( context, ZMQ_THREAD_PRIORITY, 99 );
    zmq_ctx_set( context, ZMQ_IO_THREADS, 2 );

    char url[256];
    sprintf(url, "tcp://%s:5561", serverHostname);

    subscriber = zmq_socket(context, ZMQ_SUB);
    zmq_connect(subscriber, url);
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);

    int trueValue = 1;
    zmq_setsockopt(subscriber, ZMQ_CONFLATE, &trueValue, sizeof(int)); // keep only most recent message

    //sleep(1);

    //printf("Subscriber listening...\n"); fflush(stdout);

    dataRecvSocket = zmq_socket(context, ZMQ_PAIR);
    if ( !dataRecvSocket ) {
        g_log.log(LL_ERROR, "zmq_socket failed (dataRecvSocket)");
    }
    else {
        int rc = zmq_bind(dataRecvSocket, "inproc://subscriberData");
        if ( rc != 0 ) {
            g_log.log(LL_FATAL, "zmq_bind failed (dataRecvSocket)");
        }
//        else {
//            zmq_setsockopt(dataRecvSocket, ZMQ_RCVTIMEO, 0, 6);
//        }
    }

    int rc = pthread_create( &subscriberThread, NULL, subscriberThreadFunc, NULL );
    if ( rc != 0 ) {
        g_log.log(LL_FATAL, "pthread_create failed (startSubscriber)");
    }
}

void stopSubscriber()
{
    if ( ! context )
        return;

    g_log.log(LL_DEBUG, "Stopping subscriber...");

    void* doSignalSocket = zmq_socket(context, ZMQ_PAIR);
    if ( ! doSignalSocket ) {
        g_log.log(LL_FATAL, "zmq_socket failed (doSignalSocket)");
    }
    else {
        int rc = zmq_connect(doSignalSocket, "inproc://stopSubscriber");
        if ( rc != 0 ) {
            g_log.log(LL_ERROR, "zmq_connect failed (doSignalSocket)");
        }
        else {
            rc = zmq_send(doSignalSocket, "0", 1, 0);
            if ( rc != 1 ) {
                g_log.log(LL_ERROR, "zmq_send failed (doSignalSocket)");
            }
        }
    }

    //printf("Waiting for subscriber thread to join... \n"); fflush(stdout);

    pthread_join( subscriberThread, NULL );

    zmq_close(doSignalSocket);
    zmq_close(dataRecvSocket);
    zmq_close(subscriber);

    zmq_ctx_destroy(context);

    context = NULL;
    subscriber = NULL;
    dataRecvSocket = NULL;

    //printf("done.\n"); fflush(stdout);
}

bool checkSubscriberMessages(clientReport_t* rep) {

    if ( ! dataRecvSocket )
        return false;

    zmq_pollitem_t items[] = {
        { dataRecvSocket, 0, ZMQ_POLLIN, 0 }
    };

    bool gotSomething = false;

    // This loop will fully read all available incoming messages. Some of these messages
    // will have a result for a homing/probing etc that will only be non-zero immediately
    // after that event completes. Need to catch with these variables and set before exiting.
    uint8_t homingResult = 0;
    uint8_t probingResult = 0;
    uint8_t trajResult = 0;

    while ( true ) {

        zmq_poll(items, 1, 0);

        if (items[0].revents & ZMQ_POLLIN)
        {
            //printf("Got data\n");

            zmq_msg_t msg;
            if ( 0 != zmq_msg_init(&msg) ) {
                g_log.log(LL_FATAL, "zmq_msg_init failed");
            }
            else {
                int rc = zmq_msg_recv( &msg, dataRecvSocket, 0);
                if ( rc == -1 ) {
                    g_log.log(LL_DEBUG, "zmq_msg_recv for dataRecvSocket failed: %d (%s)\n", errno, strerror(errno));
                }
                else {
                    //clientReport_t* apr = (clientReport_t*)zmq_msg_data( &msg );
                    //printf("spi: %d, mode: %d, actualPos: %f %f %f\n", apr->spiOk, apr->mode, apr->x, apr->y, apr->z); fflush(stdout);

                    memcpy(rep, zmq_msg_data(&msg), sizeof(clientReport_t));

                    if ( rep->messageVersion == MESSAGE_VERSION ) {

                        gotSomething = true;

                        if ( rep->homingResult != 0 ) {
                            //printf("hr = %d",rep->homingResult);
                            homingResult = rep->homingResult;
                        }
                        if ( rep->probingResult != 0 ) {
                            //printf("hr = %d",rep->probingResult);
                            probingResult = rep->probingResult;
                        }
                        if ( rep->trajResult != 0 ) {
                            //printf("tr = %d",rep->trajResult);
                            trajResult = rep->trajResult;
                        }
                    }
                    else {
                        if ( ! alreadyReportedWrongSubscriberMessageVersion ) {
                            char buf[128];
                            snprintf(buf, sizeof(buf), "Received update from server but message version is wrong (expected %d, got %d)", MESSAGE_VERSION, rep->messageVersion);
                            g_log.log(LL_WARN, buf);
                            ImGui::InsertNotification({ ImGuiToastType_Warning, 10000, buf });
                            alreadyReportedWrongSubscriberMessageVersion = true;
                        }
                    }

                    zmq_msg_close(&msg);
                }
            }
        }
        else {
            break;
        }
    }

    rep->homingResult = homingResult;
    rep->probingResult = probingResult;
    rep->trajResult = trajResult;

    return gotSomething;
}



