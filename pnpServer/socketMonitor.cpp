
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <pthread.h>

#include <zmq.h>

extern void *context;

pthread_t socketMonitorThread;
bool runMonitor = true;

static void *req_socket_monitor (void *ctx)
{
    void *monitor = zmq_socket (context, ZMQ_PAIR);
    int rc = 0;

    if ( rc != 0 ) {
        printf("zmq_socket failed for monitor: %d (%s)\n", errno, strerror(errno));
        return NULL;
    }

    rc = zmq_connect (monitor, "inproc://monitor.req");

    if ( rc != 0 ) {
        printf("zmq_connect failed for monitor: %d (%s)\n", errno, strerror(errno));
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
         case ZMQ_EVENT_CONNECTED: printf("ZMQ_EVENT_CONNECTED\n"); break;
         case ZMQ_EVENT_CONNECT_DELAYED: printf("ZMQ_EVENT_CONNECT_DELAYED\n"); break;
         case ZMQ_EVENT_CONNECT_RETRIED: printf("ZMQ_EVENT_CONNECT_RETRIED\n"); break;
         case ZMQ_EVENT_LISTENING: printf("ZMQ_EVENT_LISTENING\n"); break;
         case ZMQ_EVENT_BIND_FAILED: printf("ZMQ_EVENT_BIND_FAILED\n"); break;
         case ZMQ_EVENT_ACCEPTED: printf("ZMQ_EVENT_ACCEPTED\n"); break;
         case ZMQ_EVENT_ACCEPT_FAILED: printf("ZMQ_EVENT_ACCEPT_FAILED\n"); break;
         case ZMQ_EVENT_CLOSED: printf("ZMQ_EVENT_CLOSED\n"); break;
         case ZMQ_EVENT_CLOSE_FAILED: printf("ZMQ_EVENT_CLOSE_FAILED\n"); break;
         case ZMQ_EVENT_DISCONNECTED: printf("ZMQ_EVENT_DISCONNECTED\n"); break;
         case ZMQ_EVENT_MONITOR_STOPPED: printf("ZMQ_EVENT_MONITOR_STOPPED\n"); break;
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
        printf("startSocketMonitor called when no context exists!\n");
        return false;
    }

    printf("Starting socket monitor... ");

    int rc = zmq_socket_monitor (socketToMonitor, "inproc://monitor.req", ZMQ_EVENT_ALL);
    if ( rc != 0 ) {
        printf("zmq_socket_monitor failed: %d (%s)\n", errno, strerror(errno));
        return false;
    }

    int tret = pthread_create( &socketMonitorThread, NULL, req_socket_monitor, (void*)NULL);
    if ( tret != 0 ) {
        printf("pthread_create failed: %d (%s)\n", errno, strerror(errno));
        return false;
    }

    printf("done\n");

    return true;
}

void stopSocketMonitor()
{
    printf("Stopping socket monitor... ");

    runMonitor = false;
    pthread_join( socketMonitorThread, NULL);

    printf("done\n");
}

