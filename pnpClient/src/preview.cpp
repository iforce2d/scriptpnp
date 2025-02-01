
#include <iomanip>
#include <sstream>

#include "preview.h"
#include "log.h"

#include "commandlist_parse.h"
#include "machinelimits.h"

using namespace std;
using namespace scv;

PlanGroup planGroup_preview;
float animSpeedScale = 1;
vec3 animLoc;
float animRots[4] = {0};
vec3 lastActualPos = vec3_zero;
vec3 lastActualVel = vec3_zero;
float lastActualRots[4] = {0};
cornerBlendMethod_e blendMethod = CBM_INTERPOLATED_MOVES;
float cornerBlendMaxOverlap = 0.8f;
float traverseMaxVel = 100;

vector<traverseEvent_t> traverseEvents; // temporary, used to gather events
vector<traversePoint_t> traversePoints; // used to draw lines with color-coded velocity
vector<traverseEventLabel_t> traverseLabels;

previewStyle_e previewStyle = PS_LINES;

void generateTraverseEventLabels() {

    //memcpy(animRots, lastActualRots, sizeof(animRots));
    //memcpy(plan.traversal_rots, lastActualRots, sizeof(plan.traversal_rots));

    traverseLabels.clear();

    const char* axisNames = "abcd";

    for (int i = 0; i < (int)traverseEvents.size(); i++) {

        traverseEvent_t& e = traverseEvents[i];

        std::stringstream stream;
        stream << std::fixed << std::setprecision(2) << e.t;

        string timestamp = "[" + stream.str() + "] ";

        string s;
        int linesAdded = 0;

        if ( e.fb.digitalOutputChanged ) {
            s += timestamp + "Digital out: ";
            bool haveDigital = false;
            for (int i = 0; i < 16; i++) {
                if ( e.fb.digitalOutputChanged & (1 << i) ) {
                    if (haveDigital)
                        s += ", ";
                    s += ( e.fb.digitalOutputBits & (1 << i) ) ? std::to_string(i+1) : std::to_string(-(i+1));
                    haveDigital = true;
                }
            }
            linesAdded++;
        }

        bool havePWM = false;
        for (int k = 0; k < 4; k++) {
            if ( e.fb.pwmOutput[k] != INVALID_FLOAT ) {
                if ( ! havePWM ) {
                    if ( linesAdded > 0 )
                        s += "\n";
                    s += timestamp + "PWM: ";
                }
                else
                    s += ", ";
                string name = string(1, axisNames[k]);
                s += name + ": " + std::to_string(e.fb.pwmOutput[k]);
                havePWM = true;
                linesAdded++;
            }
        }

        bool haveRotation = false;
        for (int k = 0; k < NUM_ROTATION_AXES; k++) {
            if ( e.fb.rotationStarts[k] != INVALID_FLOAT ) {
                if ( ! haveRotation ) {
                    if ( linesAdded > 0 )
                        s += "\n";
                    s += timestamp + "Rotate: ";
                }
                else
                    s += ", ";
                string name = string(1, axisNames[k]);
                s += name + ": " + std::to_string(e.fb.rotationStarts[k]);
                haveRotation = true;
                linesAdded++;
            }
        }

        if ( linesAdded > 0 ) {

            bool mergeLabel = false;
            for (int k = 0; k < (int)traverseLabels.size(); k++) {
                traverseEventLabel_t &l = traverseLabels[k];
                if ( (l.pos - e.pos).Length() < 0.01 ) {
                    l.text += "\n" + s;
                    mergeLabel = true;
                    continue;
                }
            }

            if ( !mergeLabel ) {
                traverseEventLabel_t el;
                el.pos = e.pos;
                el.text = s;
                traverseLabels.push_back( el );
            }
        }
    }

}

void resetTraversePointsAndEvents() {
    planGroup_preview.resetTraverse();
    traversePoints.clear();
    traverseEvents.clear();
    traverseLabels.clear();
}

void calculateTraversePointsAndEvents() {

    resetTraversePointsAndEvents();

    float previewDt = 0.01;

    vec3 p = lastActualPos;
    vec3 v = vec3_zero;
    float rots[NUM_ROTATION_AXES];

    //memcpy(rots, lastActualRots, sizeof(rots));

    bool stillRunning = true;
    float totalTime = 0;
    while ( stillRunning ) {
        traverseFeedback_t fb;

        for (int i = 0; i < NUM_ROTATION_AXES; i++)
            rots[i] = INVALID_FLOAT;

        stillRunning = planGroup_preview.advanceTraverse( previewDt, 1, &p, &v, rots, &fb );

        stillRunning |= fb.stillRunning;

        traversePoint_t tp;
        tp.pos = p;
        tp.vel = v;
        traversePoints.push_back( tp );

        bool anyPWMChanged = false;
        for (int i = 0; !anyPWMChanged && i < NUM_PWM_VALS; i++) {
            if ( fb.pwmOutput[i] != INVALID_FLOAT )
                anyPWMChanged = true;
        }

        bool anyRotChanged = false;
        for (int i = 0; !anyRotChanged && i < NUM_ROTATION_AXES; i++) {
            if ( fb.rotationStarts[i] != INVALID_FLOAT )
                anyRotChanged = true;
        }

        if ( fb.digitalOutputChanged || anyPWMChanged || anyRotChanged ) {
            traverseEvent_t event;
            event.t = totalTime;
            event.pos = p;
            event.fb = fb;
            traverseEvents.push_back( event );
        }

        if ( stillRunning )
            totalTime += previewDt;
    }

    // memcpy(animRots, lastActualRots, sizeof(animRots));
    // memcpy(plan.traversal_rots, lastActualRots, sizeof(plan.traversal_rots));

    generateTraverseEventLabels();

    traverseEvents.clear();
    planGroup_preview.resetTraverse();
}

vec3 getPreviewColorFromSpeed(vec3 v) {
    float mag = v.Length();
    if ( mag > traverseMaxVel )
        mag = traverseMaxVel;
    float red = mag/traverseMaxVel;
    float green = 0;
    float blue = 1 - mag/traverseMaxVel;
    return vec3(red, green, blue);
}

extern cornerBlendMethod_e lastActualCornerBlendMethod;
extern motionLimits lastActualMoveLimits;
extern motionLimits lastActualRotateLimits;
extern float lastActualSpeedScale;

// These are initialized at the start of a preview, so that they can
// carry across multiple preview plans, simulating what the server would do
motionLimits previewMoveLimits;
motionLimits previewRotateLimits;

void setPreviewMoveLimitsFromCurrentActual() {
    previewMoveLimits = lastActualMoveLimits;
    previewRotateLimits = lastActualRotateLimits;
}

void loadCommandsPreview(CommandList& program, planner* plan) {

    plan->setCornerBlendMethod(lastActualCornerBlendMethod);
    //plan->setMaxCornerBlendOverlapFraction(cornerBlendMaxOverlap);

    scv::move m;
    m.vel = previewMoveLimits.vel;
    m.acc = previewMoveLimits.acc;
    m.jerk = previewMoveLimits.jerk;
    m.blendType = scv::CBT_MIN_JERK;
    //m.dst = lastActualPos; // ie. current position, just for convenience in first iteration of loop below
    m.dst = plan->startingPosition;

    scv::rotate r[NUM_ROTATION_AXES];
    for (int i = 0; i < NUM_ROTATION_AXES; i++) {
        r[i].vel = previewRotateLimits.vel;
        r[i].acc = previewRotateLimits.acc;
        r[i].jerk = previewRotateLimits.jerk;
        //r[i].dst = lastActualRots[i];
        r[i].dst = plan->startingRotations[i];
        r[i].axis = i;
    }

    for (int i = 0; i < (int)program.commands.size(); i++) {
        Command* cmd = program.commands[i];
        if ( cmd->type == CT_SET_CORNER_BLEND_OVERLAP ) {
            Command_setCornerBlendOverlap* cmdCornerBlendOverlap = (Command_setCornerBlendOverlap*)cmd;
            plan->setMaxCornerBlendOverlapFraction( cmdCornerBlendOverlap->overlap );
        }
        else if ( cmd->type == CT_MOVETO ) {
            Command_moveTo* cmdMoveto = (Command_moveTo*)cmd;
            m.src = m.dst;

            m.dst = m.src; // for unchanged axes

            if ( cmdMoveto->dst.x != INVALID_FLOAT ) {
                if ( cmdMoveto->dst.flags_x & MOVE_FLAG_RELATIVE ) {
                    m.dst.x += cmdMoveto->dst.x;
                }
                else if ( cmdMoveto->dst.flags_x & MOVE_FLAG_LESS_THAN ) {
                    if ( cmdMoveto->dst.x < m.dst.x )
                        m.dst.x = cmdMoveto->dst.x;
                }
                else if ( cmdMoveto->dst.flags_x & MOVE_FLAG_MORE_THAN ) {
                    if ( cmdMoveto->dst.x > m.dst.x )
                        m.dst.x = cmdMoveto->dst.x;
                }
                else
                    m.dst.x = cmdMoveto->dst.x;
            }
            if ( cmdMoveto->dst.y != INVALID_FLOAT ) {
                if ( cmdMoveto->dst.flags_y & MOVE_FLAG_RELATIVE ) {
                    m.dst.y += cmdMoveto->dst.y;
                }
                else if ( cmdMoveto->dst.flags_y & MOVE_FLAG_LESS_THAN ) {
                    if ( cmdMoveto->dst.y < m.dst.y )
                        m.dst.y = cmdMoveto->dst.y;
                }
                else if ( cmdMoveto->dst.flags_y & MOVE_FLAG_MORE_THAN ) {
                    if ( cmdMoveto->dst.y > m.dst.y )
                        m.dst.y = cmdMoveto->dst.y;
                }
                else
                    m.dst.y = cmdMoveto->dst.y;
            }
            if ( cmdMoveto->dst.z != INVALID_FLOAT ) {
                if ( cmdMoveto->dst.flags_z & MOVE_FLAG_RELATIVE ) {
                    m.dst.z += cmdMoveto->dst.z;
                }
                else if ( cmdMoveto->dst.flags_z & MOVE_FLAG_LESS_THAN ) {
                    if ( cmdMoveto->dst.z < m.dst.z )
                        m.dst.z = cmdMoveto->dst.z;
                }
                else if ( cmdMoveto->dst.flags_z & MOVE_FLAG_MORE_THAN ) {
                    if ( cmdMoveto->dst.z > m.dst.z )
                        m.dst.z = cmdMoveto->dst.z;
                }
                else
                    m.dst.z = cmdMoveto->dst.z;
            }

            plan->appendMove(m);
        }
        else if ( cmd->type == CT_WAIT ) {
            Command_wait* cmdWait = (Command_wait*)cmd;
            plan->appendWait( m.dst, cmdWait->duration );
        }
        else if ( cmd->type == CT_DIGITAL_OUTPUT ) {
            Command_digitalOutput* cmdDigitalOutput = (Command_digitalOutput*)cmd;
            plan->appendDigitalOutput(cmdDigitalOutput->bits, cmdDigitalOutput->changed, cmdDigitalOutput->delay);
        }
        else if ( cmd->type == CT_SET_MOVE_LIMITS ) {
            Command_setMoveLimits* cmdSetLimits = (Command_setMoveLimits*)cmd;
            if ( cmdSetLimits->limits.vel != INVALID_FLOAT ) {
                m.vel = cmdSetLimits->limits.vel;
                previewMoveLimits.vel = m.vel;
            }
            if ( cmdSetLimits->limits.acc != INVALID_FLOAT ) {
                m.acc = cmdSetLimits->limits.acc;
                previewMoveLimits.acc = m.acc;
            }
            if ( cmdSetLimits->limits.jerk != INVALID_FLOAT ) {
                m.jerk = cmdSetLimits->limits.jerk;
                previewMoveLimits.jerk = m.jerk;
            }
        }
        else if ( cmd->type == CT_SET_ROTATE_LIMITS ) {
            Command_setRotateLimits* cmdSetLimits = (Command_setRotateLimits*)cmd;
            if ( cmdSetLimits->limits.vel != INVALID_FLOAT ) {
                r[cmdSetLimits->axis].vel = cmdSetLimits->limits.vel;
                previewRotateLimits.vel = cmdSetLimits->limits.vel;
            }
            if ( cmdSetLimits->limits.acc != INVALID_FLOAT ) {
                r[cmdSetLimits->axis].acc = cmdSetLimits->limits.acc;
                previewRotateLimits.acc = cmdSetLimits->limits.acc;
            }
            if ( cmdSetLimits->limits.jerk != INVALID_FLOAT ) {
                r[cmdSetLimits->axis].jerk = cmdSetLimits->limits.jerk;
                previewRotateLimits.jerk = cmdSetLimits->limits.jerk;
            }
        }
        else if ( cmd->type == CT_PWM_OUTPUT ) {
            Command_setPWM* cmdSetPWM = (Command_setPWM*)cmd;
            plan->appendPWMOutput(cmdSetPWM->vals, cmdSetPWM->delay);
        }
        else if ( cmd->type == CT_ROTATETO ) {
            Command_rotateTo* cmdRotateto = (Command_rotateTo*)cmd;
            //printf("got rotate %f %f %f %f %f\n", cmdRotateto->a, cmdRotateto->b, cmdRotateto->c, cmdRotateto->d, cmdRotateto->delay);

            for (int i = 0; i < NUM_ROTATION_AXES; i++) {
                if ( cmdRotateto->dst[i] != INVALID_FLOAT ) {
                    r[i].src = r[i].dst;
                    r[i].dst = cmdRotateto->dst[i];
                    plan->appendRotate(r[i], cmdRotateto->delay);
                }
            }

        }
        else if ( cmd->type == CT_SYNC ) {
            plan->appendSync( m.dst );
        }
        else {
            g_log.log(LL_WARN, "(main) Unhandled command type: %s\n", getCommandName(cmd->type));
        }
    }

    // memcpy(animRots, lastActualRots, sizeof(animRots));
    // memcpy(plan->traversal_rots, lastActualRots, sizeof(plan->traversal_rots));

    // plan->calculateMoves();
    // plan->resetTraverse();

    //plan->printMoves(); fflush(stdout);
    //plan->printSegments(); fflush(stdout);

    //calculateTraversePointsAndEvents();
}

bool doPreview(vector<string> &lines)
{
    g_log.log(LL_DEBUG, "doPreview");

    CommandList program(blendMethod);
    program.cornerBlendMaxFraction = cornerBlendMaxOverlap;

    program.posLimitLower = machineLimits.posLimitLower;
    program.posLimitUpper = machineLimits.posLimitUpper;
    for (int i = 0; i < NUM_ROTATION_AXES; i++)
        program.rotationPositionLimits[i] = machineLimits.rotationPositionLimits[i];

    if ( parseCommandList(lines, program) ) { // uses heap
        planner* plan = planGroup_preview.addPlan();
        loadCommandsPreview(program, plan);
        planGroup_preview.calculateMovesForLastPlan();
    }

    return true;
}
