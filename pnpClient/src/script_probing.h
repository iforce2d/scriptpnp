#ifndef SCRIPT_PROBING_H
#define SCRIPT_PROBING_H

extern int script_PT_DIGITAL;
extern int script_PT_LOADCELL;
extern int script_PT_VACUUM;

// extern int script_PR_NONE;
// extern int script_PR_SUCCESS;
// extern int script_PR_FAIL_NOT_TRIGGERED;
// extern int script_PR_FAIL_CONFIG;
// extern int script_PR_FAIL_ALREADY_TRIGGERED;

int script_getProbeResult();
float script_getProbedHeight();
void script_probe(float depth, int type, bool twoPhase);

#endif // SCRIPT_PROBING_H
