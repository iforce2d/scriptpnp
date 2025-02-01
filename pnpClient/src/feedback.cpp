
#include "notify.h"
#include "pnpMessages.h"

void doNotifiesForHomingResult( int r )
{
    if ( r == HR_FAIL_TIMED_OUT )
        notify( "Timed out while homing", NT_ERROR, 5000 );
    else if ( r == HR_FAIL_CONFIG)
        notify( "Cannot run homing with invalid config", NT_ERROR, 5000 );
    else if ( r == HR_FAIL_LIMIT_ALREADY_TRIGGERED)
        notify( "Cannot start homing while limit is already triggered", NT_ERROR, 5000 );
}

void doNotifiesForTrajectoryResult( int r )
{
    if ( r == TR_FAIL_NOT_HOMED )
        notify( "Cannot run movement command while not homed", NT_ERROR, 5000 );
    else if ( r == TR_FAIL_CONFIG)
        notify( "Cannot run movement command with invalid config", NT_ERROR, 5000 );
    else if ( r == TR_FAIL_FOLLOWING_ERROR)
        notify( "Following error during movement", NT_ERROR, 5000 );
    else if ( r == TR_FAIL_LIMIT_TRIGGERED)
        notify( "Limit switch triggered during movement", NT_ERROR, 5000 );
    else if ( r == TR_FAIL_OUTSIDE_BOUNDS)
        notify( "Requested movement would be outside work area", NT_ERROR, 5000 );
}

void doNotifiesForProbingResult( int r )
{
    if ( r == PR_FAIL_CONFIG )
        notify( "Cannot run probing with invalid config", NT_ERROR, 5000 );
    else if ( r == PR_FAIL_NOT_HOMED )
        notify( "Cannot run probing while not homed", NT_ERROR, 5000 );
    else if ( r == PR_FAIL_NOT_TRIGGERED )
        notify( "Probing reached Z limit without contacting", NT_WARNING, 5000 );
    else if ( r == PR_FAIL_ALREADY_TRIGGERED )
        notify( "Cannot start probing while already triggered/contacting", NT_ERROR, 5000 );
}








