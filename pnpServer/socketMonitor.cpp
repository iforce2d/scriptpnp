
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <pthread.h>

#include <zmq.h>

#include "log.h"

extern void *context;

pthread_t socketMonitorThread;
bool runMonitor = true;

static void *req_socket_monitor (void *ctx)
{
    void *monitor = zmq_socket (context, ZMQ_PAIR);
    int rc = 0;

    if ( rc != 0 ) {
        g_log.log(LL_ERROR, "zmq_socket failed for monitor: %d (%s)", errno, strerror(errno));
        return NULL;
    }

    rc = zmq_connect (monitor, "inproc://monitor.req");

    if ( rc != 0 ) {
        g_log.log(LL_ERROR, "zmq_connect failed for monitor: %d (%s)", errno, strerror(errno));
        return NULL;
    }

    while (runMonitor) {
        // First frame in message contains event number and value
         zmq_msg_t msg;
         zmq_msg_init (&msg);
         if (zmq_msg_recv (&msg, monitor, 0) == -1)
            break; // Interrupted, presumably

         assert (zmq_msg_more (&msg));

         uint8_t *data = (uint8_t *) zmq_msg_data(&msg);
         uint16_t event = *(uint16_t *) (data);
         //uint32_t value = *(uint32_t *) (data + 2);

         switch (event) {
         case ZMQ_EVENT_CONNECTED: g_log.log(LL_DEBUG, "ZMQ_EVENT_CONNECTED"); break;
         case ZMQ_EVENT_CONNECT_DELAYED: g_log.log(LL_DEBUG, "ZMQ_EVENT_CONNECT_DELAYED"); break;
         case ZMQ_EVENT_CONNECT_RETRIED: g_log.log(LL_DEBUG, "ZMQ_EVENT_CONNECT_RETRIED"); break;
         case ZMQ_EVENT_LISTENING: g_log.log(LL_DEBUG, "ZMQ_EVENT_LISTENING"); break;
         case ZMQ_EVENT_BIND_FAILED: g_log.log(LL_DEBUG, "ZMQ_EVENT_BIND_FAILED"); break;
         case ZMQ_EVENT_ACCEPTED: g_log.log(LL_DEBUG, "ZMQ_EVENT_ACCEPTED"); break;
         case ZMQ_EVENT_ACCEPT_FAILED: g_log.log(LL_DEBUG, "ZMQ_EVENT_ACCEPT_FAILED"); break;
         case ZMQ_EVENT_CLOSED: g_log.log(LL_DEBUG, "ZMQ_EVENT_CLOSED"); break;
         case ZMQ_EVENT_CLOSE_FAILED: g_log.log(LL_DEBUG, "ZMQ_EVENT_CLOSE_FAILED"); break;
         case ZMQ_EVENT_DISCONNECTED: g_log.log(LL_DEBUG, "ZMQ_EVENT_DISCONNECTED"); break;
         case ZMQ_EVENT_MONITOR_STOPPED: g_log.log(LL_DEBUG, "ZMQ_EVENT_MONITOR_STOPPED"); break;
         default:
             break;
         }

         //printf("monitor value: %d, event: %d\n", value, event);

         // Second frame in message contains event address
         zmq_msg_init (&msg);
         if (zmq_msg_recv (&msg, monitor, 0) == -1)
            break; // Interrupted, presumably

         assert ( ! zmq_msg_more (&msg) );

         char address[128];
         data = (uint8_t *) zmq_msg_data (&msg);
         size_t size = zmq_msg_size (&msg);
         memcpy (address, data, size);
         address[size] = 0;

         //printf("monitor address: %s\n", address);
    }

    zmq_close (monitor);

    return NULL;
}

bool startSocketMonitor(void* socketToMonitor)
{
    if ( ! context ) {
        g_log.log(LL_ERROR, "startSocketMonitor called when no context exists!");
        return false;
    }

    //printf("Starting socket monitor... ");

    int rc = zmq_socket_monitor (socketToMonitor, "inproc://monitor.req", ZMQ_EVENT_ALL);
    if ( rc != 0 ) {
        g_log.log(LL_ERROR, "zmq_socket_monitor failed: %d (%s)", errno, strerror(errno));
        return false;
    }

    int tret = pthread_create( &socketMonitorThread, NULL, req_socket_monitor, (void*)NULL);
    if ( tret != 0 ) {
        g_log.log(LL_ERROR, "startSocketMonitor: pthread_create failed %d (%s)", errno, strerror(errno));
        return false;
    }

    g_log.log(LL_INFO, "Started socket monitor");

    return true;
}

void stopSocketMonitor()
{
    //printf("Stopping socket monitor... ");

    runMonitor = false;
    pthread_join( socketMonitorThread, NULL);

    g_log.log(LL_INFO, "Stopped socket monitor");
}

