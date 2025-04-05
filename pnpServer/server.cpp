
#include <unistd.h>

#include <string.h>
#include <zmq.h>

#include "log.h"
#include "weeny.h"
#include "server.h"
#include "../common/config.h"
#include "socketMonitor.h"
#include "../common/machinelimits.h"
#include "../common/overrides.h"

using namespace scv;

void *context = NULL;
void *publisher = NULL;
void *replier = NULL;

bool startServer() {

    if ( context ) {
        g_log.log(LL_WARN, "startServer called when context already exists!\n");
        return false;
    }

    //g_log.log(LL_INFO, "Starting server... ");

    context = zmq_ctx_new();
    zmq_ctx_set( context, ZMQ_IO_THREADS, 2 );

    if ( ! context ) {
        g_log.log(LL_ERROR, "zmq_ctx_new failed: %d (%s)\n", errno, strerror(errno));
        return false;
    }

    publisher = zmq_socket(context, ZMQ_PUB);
    if ( ! publisher ) {
        g_log.log(LL_ERROR, "zmq_socket failed, publisher: %d (%s)\n", errno, strerror(errno));
        return false;
    }

    int rc = zmq_bind(publisher, "tcp://*:5561");
    if ( rc != 0 ) {
        g_log.log(LL_ERROR, "zmq_bind failed, publisher: %d (%s)\n", errno, strerror(errno));

        zmq_close(publisher);
        zmq_ctx_destroy(context);

        return false;
    }

    int trueValue = 1;
    zmq_setsockopt(publisher, ZMQ_CONFLATE, &trueValue, sizeof(int)); // keep only most recent message

    //int milliseconds = 1;
    //zmq_setsockopt(publisher, ZMQ_TCP_MAXRT, &milliseconds, sizeof(int)); // set retransmit timeout


    replier = zmq_socket(context, ZMQ_REP);
    if ( ! replier ) {
        g_log.log(LL_ERROR, "zmq_socket failed, replier: %d (%s)\n", errno, strerror(errno));

        zmq_close(publisher);
        zmq_ctx_destroy(context);

        return false;
    }

    rc = zmq_bind(replier, "tcp://*:5562");
    if ( rc != 0 ) {
        g_log.log(LL_ERROR, "zmq_bind failed, replier: %d (%s)\n", errno, strerror(errno));

        zmq_close(replier);
        zmq_close(publisher);
        zmq_ctx_destroy(context);

        return false;
    }

    g_log.log(LL_INFO, "Started server");

    startSocketMonitor(publisher);

    return true;
}

void stopServer() {

    //printf("Stopping server... ");

    zmq_close (replier);
    zmq_close (publisher);
    zmq_ctx_destroy (context);

    replier = NULL;
    publisher = NULL;
    context = NULL;

    // Destroying the context will have caused the blocking zmq_msg_recv in the monitor to ETERM
    // Still need to join the monitor thread
    stopSocketMonitor();

    g_log.log(LL_INFO, "Stopped server");
}

bool checkCommandRequests(commandMessageType_e* msgType, commandRequest_t* req, CommandList* program) {

    if ( ! replier )
        return false;

    zmq_pollitem_t items[] = {
        { replier, 0, ZMQ_POLLIN, 0 }
    };

    *msgType = MT_NONE;
    bool didRecv = false;

    zmq_poll(items, 1, 0);

    if (items[0].revents & ZMQ_POLLIN)
    {
        //printf("Got replier data\n");

        zmq_msg_t msg;
        if ( 0 != zmq_msg_init(&msg) ) {
            g_log.log(LL_ERROR, "zmq_msg_init failed\n");
        }
        else {
            int rc = zmq_msg_recv( &msg, replier, 0);
            if ( rc == -1 ) {
                g_log.log(LL_ERROR, "zmq_msg_recv for replier failed: %d (%s)\n", errno, strerror(errno));
            }
            else {
                didRecv = true;

                size_t msgSize = zmq_msg_size(&msg);
                uint8_t* data = (uint8_t*)zmq_msg_data(&msg);

                //printf("checkCommandRequests received %d bytes\n", msgSize);

                int pos = 0;

                uint16_t version = 0;
                memcpy(&version, &data[pos], sizeof(version));
                pos += sizeof(version);

                if ( version != MESSAGE_VERSION ) {
                    g_log.log(LL_ERROR, "Message version mismatch: expected %d, received %d", MESSAGE_VERSION, version);
                }
                else {
                    uint16_t messageType = 0;
                    memcpy(&messageType, &data[pos], sizeof(messageType));
                    pos += sizeof(messageType);

                    if ( messageType == MT_SET_PROGRAM ) {
                        g_log.log(LL_DEBUG, "Got packable %s", getMessageName(MT_SET_PROGRAM));
                        if ( program->unpack( &data[pos] ) ) {
                            *msgType = MT_SET_PROGRAM;
                            req->type = MT_SET_PROGRAM; // for processCommandReply
                        }
                        else {
                            g_log.log(LL_ERROR, "Program unpack failed");
                        }
                    }
                    else if ( messageType == MT_CONFIG_OVERRIDES_SET ) {
                        g_log.log(LL_DEBUG, "Got packable %s", getMessageName(MT_CONFIG_OVERRIDES_SET));
                        overrideConfigs.clear();

                        // printf("Incoming %d bytes\n", msgSize); fflush(stdout);
                        //  for (int i = 0; i < msgSize; i++) {
                        //      printf("%d ", data[i]); fflush(stdout);
                        //  }
                        //  printf("\n");

                         if ( overrideConfigSet_incoming.unpack( &data[pos] ) ) {
                            *msgType = MT_CONFIG_OVERRIDES_SET;
                             req->type = MT_CONFIG_OVERRIDES_SET; // for processCommandReply
                            //g_log.log(LL_ERROR, "Overrides unpack ok");
                         }
                        else
                            g_log.log(LL_ERROR, "Overrides unpack failed");
                        //updateUIIndexOfOverrideOptions();
                    }
                    else {
                        //printf("Got single command message: %s\n", getMessageName(messageType) );
                        memcpy(req, data, msgSize);
                        *msgType = (commandMessageType_e)messageType;
                    }
                }

                zmq_msg_close(&msg);
            }
        }
    }

    //if ( didRecv )
    //printf("checkCommandRequests returning didRecv = %d\n", didRecv); fflush(stdout);

    return didRecv;
}

bool sendPackable(uint16_t messageType, Packable& packable) {

    bool ret = false;

    uint16_t version = MESSAGE_VERSION;

    int msgSize = sizeof(version) + sizeof(messageType) + packable.getSize();

    //g_log.log(LL_DEBUG, "Sending packable, message size: %d", msgSize);

    zmq_msg_t msgOut;
    if ( 0 != zmq_msg_init_size(&msgOut, msgSize)) {
        g_log.log(LL_FATAL, "zmq_msg_init_size failed");
    }
    else {
        uint8_t* data = (uint8_t*)zmq_msg_data(&msgOut);

        int pos = 0;

        memcpy(&data[pos], &version, sizeof(version));
        pos += sizeof(version);

        memcpy(&data[pos], &messageType, sizeof(messageType));
        pos += sizeof(messageType);

        packable.pack( &data[pos] );

        if ( msgSize != zmq_msg_send( &msgOut, replier, ZMQ_DONTWAIT ) ) {
            g_log.log(LL_ERROR, "zmq_msg_send failed, replier: %d (%s)", errno, strerror(errno));
        }
        else {
            //printf("Sent request\n"); fflush(stdout);
            ret = true;
        }

        zmq_msg_close( &msgOut );
    }

    return ret;
}

void processCommandReply(commandRequest_t* req, bool ackOrNack) {

    g_log.log(LL_DEBUG, "Processing command request: %s", getMessageName(req->type) );

    commandReply_t rep;
    rep.version = MESSAGE_VERSION;
    rep.type = ackOrNack ? MT_ACK : MT_NACK;

    if ( req->type == MT_CONFIG_STEPS_FETCH || req->type == MT_CONFIG_STEPS_SET ) {
        rep.type = MT_CONFIG_STEPS_FETCH; // a 'set' will also reply with the same reply as 'fetch'
        for (int i = 0; i < NUM_MOTION_AXES; i++) {
            rep.configSteps.perUnit[i] = stepsPerUnit[i];
        }
    }
    else if ( req->type == MT_CONFIG_WORKAREA_FETCH || req->type == MT_CONFIG_WORKAREA_SET ) {
        rep.type = MT_CONFIG_WORKAREA_FETCH; // a 'set' will also reply with the same reply as 'fetch'
        vec3 workArea = machineLimits.posLimitUpper - machineLimits.posLimitLower;
        rep.workingArea.x = workArea.x;
        rep.workingArea.y = workArea.y;
        rep.workingArea.z = workArea.z;
    }
    else if ( req->type == MT_CONFIG_INITSPEEDS_FETCH || req->type == MT_CONFIG_INITSPEEDS_SET ) {
        rep.type = MT_CONFIG_INITSPEEDS_FETCH; // a 'set' will also reply with the same reply as 'fetch'

        rep.motionLimits.initialMoveVel =  machineLimits.initialMoveLimitVel;
        rep.motionLimits.initialMoveAcc =  machineLimits.initialMoveLimitAcc;
        rep.motionLimits.initialMoveJerk = machineLimits.initialMoveLimitJerk;
        rep.motionLimits.initialRotateVel =  machineLimits.initialRotationLimitVel;
        rep.motionLimits.initialRotateAcc =  machineLimits.initialRotationLimitAcc;
        rep.motionLimits.initialRotateJerk = machineLimits.initialRotationLimitJerk;

        rep.motionLimits.velLimitX = machineLimits.velLimit.x;
        rep.motionLimits.velLimitY = machineLimits.velLimit.y;
        rep.motionLimits.velLimitZ = machineLimits.velLimit.z;

        rep.motionLimits.accLimitX = machineLimits.accLimit.x;
        rep.motionLimits.accLimitY = machineLimits.accLimit.y;
        rep.motionLimits.accLimitZ = machineLimits.accLimit.z;

        rep.motionLimits.jerkLimitX = machineLimits.jerkLimit.x;
        rep.motionLimits.jerkLimitY = machineLimits.jerkLimit.y;
        rep.motionLimits.jerkLimitZ = machineLimits.jerkLimit.z;

        rep.motionLimits.rotLimitVel =  machineLimits.grotationVelLimit;
        rep.motionLimits.rotLimitAcc =  machineLimits.grotationAccLimit;
        rep.motionLimits.rotLimitJerk = machineLimits.grotationJerkLimit;

        rep.motionLimits.maxOverlapFraction = machineLimits.maxOverlapFraction;
    }
    else if ( req->type == MT_CONFIG_TMC_FETCH || req->type == MT_CONFIG_TMC_SET ) {
        rep.type = MT_CONFIG_TMC_FETCH; // a 'set' will also reply with the same reply as 'fetch'
        for (int i = 0; i < NUM_MOTION_AXES; i++) {
            rep.tmcSettings.settings[i] = tmcParams[i];
        }
    }
    else if ( req->type == MT_CONFIG_HOMING_FETCH || req->type == MT_CONFIG_HOMING_SET ) {
        rep.type = MT_CONFIG_HOMING_FETCH; // a 'set' will also reply with the same reply as 'fetch'
        for (int i = 0; i < NUM_HOMABLE_AXES; i++) {
            rep.homingParams.params[i] = homingParams[i];
        }
        memcpy(rep.homingParams.order, homingOrder, min(sizeof(rep.homingParams.order), sizeof(homingOrder)));
    }
    else if ( req->type == MT_CONFIG_JOGGING_FETCH || req->type == MT_CONFIG_JOGGING_SET ) {
        rep.type = MT_CONFIG_JOGGING_FETCH; // a 'set' will also reply with the same reply as 'fetch'
        for (int i = 0; i < NUM_MOTION_AXES; i++) {
            rep.jogParams.speed[i] = joggingSpeeds[i];
        }
    }
    else if ( req->type == MT_CONFIG_OVERRIDES_FETCH || req->type == MT_CONFIG_OVERRIDES_SET ) {
        sendPackable(MT_CONFIG_OVERRIDES_FETCH, overrideConfigSet);
        return; // send is already done
    }
    else if ( req->type == MT_CONFIG_LOADCELL_CALIB_FETCH || req->type == MT_CONFIG_LOADCELL_CALIB_SET ) {
        rep.type = MT_CONFIG_LOADCELL_CALIB_FETCH; // a 'set' will also reply with the same reply as 'fetch'
        rep.loadcellCalib.rawOffset = loadcellCalibrationRawOffset;
        rep.loadcellCalib.weight = loadcellCalibrationWeight;
    }
    else if ( req->type == MT_CONFIG_PROBING_FETCH || req->type == MT_CONFIG_PROBING_SET ) {
        rep.type = MT_CONFIG_PROBING_FETCH; // a 'set' will also reply with the same reply as 'fetch'
        rep.probingParams.params = probingParams;
    }
    else if ( req->type == MT_CONFIG_ESTOP_FETCH || req->type == MT_CONFIG_ESTOP_SET ) {
        rep.type = MT_CONFIG_ESTOP_FETCH; // a 'set' will also reply with the same reply as 'fetch'
        rep.estopParams.outputs = estopDigitalOutState;
        rep.estopParams.outputsUsed = estopDigitalOutUsed;
        for (int i = 0; i < NUM_PWM_VALS; i++) {
            rep.estopParams.pwmVal[i] = estopPWMState[i];
        }
        rep.estopParams.pwmUsed = estopPWMUsed;
    }


    size_t msgSize = sizeof(commandReply_t);

    // for (int i = 0; i < msgSize; i++) {
    //     printf("%d ", ((uint8_t*)&rep)[i]); fflush(stdout);
    // }
    // printf("\n");

    zmq_msg_t msg;
    zmq_msg_init_size(&msg, msgSize);
    memcpy(zmq_msg_data(&msg), &rep, msgSize);
    int ret = zmq_msg_send(&msg, replier, ZMQ_DONTWAIT);
    if ( ret < 0 ) {
        g_log.log(LL_ERROR, "zmq_msg_send failed: %d (%s)", errno, strerror(errno));
    }
    // else
    //     g_log.log(LL_DEBUG, "Replied %d bytes", ret);
    zmq_msg_close(&msg);
}

void publishStatus(motionStatus *s, motionLimits currentMoveLimits, motionLimits currentRotationLimits, float speedScale, float jogSpeedScale, float weight, float probingZ)
{
    clientReport_t apr;
    apr.spiOk = s->spiOk;
    apr.mode = s->mode;
    apr.homingResult = s->homingResult;
    apr.probingResult = s->probingResult;
    apr.trajResult = s->trajectoryResult;
    apr.homedAxes = s->homedAxes;
    apr.actualPosX = s->actualPos.x;
    apr.actualPosY = s->actualPos.y;
    apr.actualPosZ = s->actualPos.z;
    apr.actualVelX = s->actualVel.x;
    apr.actualVelY = s->actualVel.y;
    apr.actualVelZ = s->actualVel.z;
    memcpy(apr.actualRots, s->actualRots, sizeof(apr.actualRots));
    apr.outputs = s->outputs;
    apr.inputs = s->inputs;
    apr.rotary = s->rotary;
    memcpy(apr.adc, s->adc, 2*sizeof(uint16_t));
    apr.pressure = s->pressure;
    apr.loadcell = s->loadcell;
    apr.pwm = s->pwm;

    apr.speedScale = speedScale;
    apr.jogSpeedScale = jogSpeedScale;
    apr.weight = weight;

    apr.probedHeight = probingZ;

    apr.limMoveVel = currentMoveLimits.vel;
    apr.limMoveAcc = currentMoveLimits.acc;
    apr.limMoveJerk = currentMoveLimits.jerk;

    apr.limRotateVel = currentRotationLimits.vel;
    apr.limRotateAcc = currentRotationLimits.acc;
    apr.limRotateJerk = currentRotationLimits.jerk;

    // if ( apr.probingResult ) {
    //     g_log.log(LL_DEBUG, "publish: %d, %f", apr.probingResult, apr.probedHeight);
    // }

    zmq_msg_t msg;
    if ( 0 != zmq_msg_init_size(&msg, sizeof(apr))) {
         g_log.log(LL_ERROR, "zmq_msg_init_size failed\n");
         return;
    }

    memcpy( zmq_msg_data(&msg), &apr, sizeof(apr) );

    if ( sizeof(apr) != zmq_msg_send( &msg, publisher, 0 ) ) {
        g_log.log(LL_ERROR, "zmq_msg_send failed\n");
    }
}
