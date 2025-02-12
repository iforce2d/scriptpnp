
#include <thread>

#include "scv/planner.h"
#include "commandlist.h"
//#include "plangroup.h"
#include "run.h"
#include "log.h"

#include "commandlist_parse.h"
#include "machinelimits.h"
#include "net_requester.h"
#include "scriptexecution.h"
#include "script/engine.h"

using namespace std;
using namespace scv;

PlanGroup planGroup_run;

extern cornerBlendMethod_e blendMethod;
extern float cornerBlendMaxOverlap;

extern trajectoryResult_e lastTrajResult;

bool waitingForPreviousActualRun = false; // true when server is running a command list

bool doActualRun(vector<string> &lines)
{
    g_log.log(LL_DEBUG, "doActualRun");

    asIScriptContext *ctx = getCurrentScriptContext();

    while ( waitingForPreviousActualRun ) {

        if ( ! currentlyRunningScriptThread() ) {
            waitingForPreviousActualRun = false;
            return false; // script was aborted
        }

        if ( lastTrajResult != TR_NONE )
            waitingForPreviousActualRun = false;
        else
            this_thread::sleep_for( 10ms );
    }

    CommandList program(blendMethod);
    program.cornerBlendMaxFraction = cornerBlendMaxOverlap;

    program.posLimitLower = machineLimits.posLimitLower;
    program.posLimitUpper = machineLimits.posLimitUpper;
    for (int i = 0; i < NUM_ROTATION_AXES; i++)
        program.rotationPositionLimits[i] = machineLimits.rotationPositionLimits[i];

    if ( parseCommandList(lines, program) ) { // uses heap
        if ( sanityCheckCommandList(program) ) {
            lastTrajResult = TR_NONE;
            if ( sendPackable(MT_SET_PROGRAM, program) ) {
                waitingForPreviousActualRun = true;
                return true;
            }
            else
                g_log.log(LL_DEBUG, "doActualRun: sendPackable failed - is server running?");
        }
    }

    return false;
}

