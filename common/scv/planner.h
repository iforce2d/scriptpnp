#ifndef SCV_PLANNER_H
#define SCV_PLANNER_H

#include <vector>
#include <stdint.h>
#include "vec3.h"

//#include "../commands.h"

#define NUM_ROTATION_AXES   4
#define NUM_PWM_VALS        4

void initInvalidFloats(float* v, int n);

enum cornerBlendMethod_e {
    CBM_NONE,
    CBM_CONSTANT_JERK_SEGMENTS,
    CBM_INTERPOLATED_MOVES
};

namespace scv {

    enum moveType_e {
        MT_NORMAL,          // stationary point to stationary point, 7 segment accelerate then decelerate
        MT_WAIT,            // just wait
        MT_ESTOP,           // decelerate segments only
        MT_SYNC,            // will potentially become a wait
        //MT_JOGSTART         // accelerate segments only
    };

    enum cornerBlendType_e {
        CBT_NONE,
        CBT_MIN_JERK,
        CBT_MAX_JERK
    };

    enum delayableEventType_e {
        DET_DIGITAL_OUTPUT,
        DET_PWM_OUTPUT,
        DET_ROTATION
    };

    // A constant jerk portion of the trajectory, defined by an initial pos/vel/acc, and a jerk with duration
    struct segment {

        moveType_e moveType;

        vec3 pos;
        vec3 vel;
        vec3 acc;
        vec3 jerk;
        float duration;

        bool toDelete;

        segment() {
            toDelete = false;
        }
    };

    struct rotateSegment {

        scv_float pos;
        scv_float vel;
        scv_float acc;
        scv_float jerk;
        float duration;

    };

    // A single point to point rotation (one nozzle)
    struct rotate {

        int axis;

        scv_float src;
        scv_float dst;

        scv_float vel;
        scv_float acc;
        scv_float jerk;

        std::vector<rotateSegment> rotation_segments;
        int rotation_segmentIndex;
        scv_float rotation_segmentTime;

        rotate() {
            src = 0;
            dst = 0;
            vel = 0;
            acc = 0;
            jerk = 0;
        }
    };

    struct delayableEvent {
        delayableEventType_e type;
        int moveIndex; // the index of the move this event will occur *before*

        uint16_t bits;
        uint16_t changed;
        float pwm[4];
        rotate rot;

        float delay;
        float triggerTime;

        int id; // used by sync, as a reference that will survive sorting

        delayableEvent() {
            type = DET_DIGITAL_OUTPUT;
            bits = 0;
            changed = 0;
            initInvalidFloats(pwm, NUM_PWM_VALS);
            delay = 0;
            triggerTime = -1;
        }
    };

    // A single point to point movement
    struct move {
        moveType_e moveType;

        vec3 src;
        vec3 dst;

        // for wait 'moves'
        float waitDuration;

        // for sync 'moves'
        std::vector<int> eventIndicesToSync; // index into delayableEvents of the plan

        // constraints on the movement
        scv_float vel;
        scv_float acc;
        scv_float jerk;
        cornerBlendType_e blendType;
        scv_float blendClearance;
        bool containsBlend;        

        std::vector<segment> segments;

        float duration;
        float scheduledTime;
        int traversal_segmentIndex;
        scv_float traversal_segmentTime;

        move() {
            moveType = MT_NORMAL;
            src = vec3_zero;
            dst = vec3_zero;
            waitDuration = 0;
            vel = 0;
            acc = 0;
            jerk = 0;
            blendType = CBT_MAX_JERK;
            blendClearance = -1; // none
            containsBlend = false;
            traversal_segmentIndex = 0;
            traversal_segmentTime = 0;
            duration = 0;
            scheduledTime = 0;
        }
    };

    struct traverseFeedback_t {
        moveType_e moveType;
        uint16_t digitalOutputBits;
        uint16_t digitalOutputChanged;
        float pwmOutput[NUM_PWM_VALS];
        float rotationStarts[NUM_ROTATION_AXES];
        bool stillRunning;

        traverseFeedback_t() {
            moveType = MT_NORMAL;
            digitalOutputBits = 0;
            digitalOutputChanged = 0;
            initInvalidFloats(rotationStarts, NUM_ROTATION_AXES);
            initInvalidFloats(pwmOutput, NUM_PWM_VALS);
            stillRunning = false;
        }
    };

    class planner
    {
    public: // Typically these would be private, they are public here for convenience in the visualizer
        cornerBlendMethod_e cornerBlendMethod;
        scv_float maxOverlapFraction;
        vec3 posLimitLower;
        vec3 posLimitUpper;
        vec3 velLimit;
        vec3 accLimit;
        vec3 jerkLimit;

        vec2 rotationPositionLimits[NUM_ROTATION_AXES];

        scv_float rotationVelLimit;
        scv_float rotationAccLimit;
        scv_float rotationJerkLimit;

        vec3 startingPosition;
        float startingRotations[NUM_ROTATION_AXES];

        std::vector<move> moves;
        std::vector<segment> segments;

        std::vector<delayableEvent> delayableEvents;
        std::vector<int> unsyncedDelayableEvents; // these are ids of a delayable event

        std::vector<rotate*> rotationsInProgress;

        scv_float traversal_totalTime;
        int traversal_delayableEventIndex;
        int traversal_segmentIndex;
        scv_float traversal_segmentTime;
        float traversal_rots[NUM_ROTATION_AXES];

        // for interpolated moves
        //vec3 traversal_pos;
        //vec3 traversal_vel;
        scv_float traversal_time; // same as traversal_totalTime ?

        void calculateMove(move& m);
        void calculateSchedules();
        void calculateRotation(rotate& r);
        void blendCorner(move& m0, move& m1, bool isFirst, bool isLast);
        void collateSegments();
        void getSegmentState(segment& s, scv_float t, vec3* pos, vec3* vel, vec3* acc, vec3* jerk );
        //void getSegmentPosition(segment& s, scv_float t, scv::vec3* pos);
        //void getSegmentPosVelAcc(segment& s, scv_float t, scv::vec3* pos, scv::vec3* vel, vec3 *acc);
        scv_float getRotateSegmentPos(rotateSegment &s, scv_float t);
        std::vector<segment>& getSegments();

        void getFinalRotations(float* rots);

    // The actual public part would normally start from here
    public:
        planner();

        void clear();

        void setCornerBlendMethod(cornerBlendMethod_e m);
        void setMaxCornerBlendOverlapFraction(scv_float f);
        void setPositionLimits(scv_float lx, scv_float ly, scv_float lz, scv_float ux, scv_float uy, scv_float uz);
        void setVelocityLimits(scv_float x, scv_float y, scv_float z);
        void setAccelerationLimits(scv_float x, scv_float y, scv_float z);
        void setJerkLimits(scv_float x, scv_float y, scv_float z);

        void setRotationPositionLimits(int axis, scv_float lower, scv_float upper);
        void setRotationVAJLimits(scv_float vel, scv_float acc, scv_float jerk);

        void appendMove( move& l );
        void appendWait(vec3 &where, float sec );
        void appendDigitalOutput(uint16_t bits, uint16_t changed, float delay);
        void appendPWMOutput(float *pwm, float delay);
        void appendRotate( rotate& r, float delay );
        void appendSync(vec3 &where);

        bool calculateMoves();
        void addOffsetToMoves(vec3 offset);

        scv_float getRotationDuration(rotate& r);
        scv_float getTraverseTime();
        //bool getTrajectoryState_constantJerkSegments(scv_float time, int *segmentIndex, vec3* pos, vec3* vel, vec3* acc, vec3* jerk );
        //bool getTrajectoryState_interpolatedMoves(scv_float time, int *segmentIndex, vec3* pos, vec3* vel, vec3* acc, vec3* jerk );

        void resetTraverse();
        bool advanceRotations(scv_float dt);
        bool advanceTraverse(scv_float dt, scv_float speedScale, vec3* p, vec3 *v, float* rots, traverseFeedback_t* feedback);

        void printConstraints();    // print global limits for each axis
        void printMoves();          // print input parameters for each point to point move
        void printSegments();       // print calculated parameters for each segment
    };

} // namespace

#endif
