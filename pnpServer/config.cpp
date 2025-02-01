
#include <fstream>
#include <string>

#include "json/json.h"

#include "weeny.h"
#include "../common/config.h"
#include "../common/overrides.h"
#include "log.h"
#include "../common/machinelimits.h"
#include "../common/scv/vec3.h"

using namespace std;

motionLimits currentMoveLimits;
motionLimits currentRotationLimits[NUM_ROTATION_AXES];

float stepsPerUnit[NUM_MOTION_AXES];
float joggingSpeeds[NUM_MOTION_AXES];

tmcSettings_t tmcParams[NUM_MOTION_AXES];
homingParams_t homingParams[4];
probingParams_t probingParams;
char homingOrder[9];

#define CONFIG_STEPS_PER_UNIT           "stepsPerUnit"
#define CONFIG_WORK_AREA                "workArea"
#define CONFIG_AXIS_SPEED_LIMIT         "axisSpeedLimits"
#define CONFIG_CURRENT_MOVE_LIMIT       "currentMoveLimits"
#define CONFIG_CURRENT_ROTATE_LIMIT     "currentRotateLimits"
#define CONFIG_BLEND_FRACTION           "currentBlendFraction"
#define CONFIG_HOMING_PARAMS            "homingParams"
#define CONFIG_TMC_PARAMS               "tmcDrivers"
#define CONFIG_JOG_SPEEDS               "jogSpeeds"
#define CONFIG_OVERRIDES                "outputOverrides"
#define CONFIG_LOADCELL_CALIB           "loadcellCalib"
#define CONFIG_PROBING_PARAMS           "probingParams"

void initTMCSettings(tmcSettings_t& p) {
    p.microsteps = 4;
    p.current = 200;
}

void initHomingParams(homingParams_t& p) {
    // these must be actually set by user
    p.direction = -1;
    p.triggerPin = -1;
    p.triggerState = -1;

    // defaults that will probably be ok, intended for mm units
    p.approachspeed1 = 30;
    p.approachspeed2 = 5;
    p.backoffDistance1 = 5;
    p.backoffDistance2 = 10;
    p.homedPosition = 0;
}

void initProbingParams(probingParams_t& p) {
    p.digitalTriggerPin = -1;
    p.digitalTriggerState = -1;
    p.vacuumStep = 0.1;
    p.vacuumSniffPin = -1;
    p.vacuumSniffState = 1;
    p.vacuumSniffTimeMs = 500;
    p.vacuumReplenishTimeMs = 500;
    p.approachspeed1 = 2;
    p.approachspeed2 = 1;
    p.backoffDistance1 = 2;
    p.backoffDistance2 = 5;
}

void readHomingParams( Json::Value& value, homingParams_t& params )
{
    params.triggerPin = value["pin"].asInt();
    params.triggerState = value["state"].asInt();
    params.direction = value["direction"].asInt();
    params.approachspeed1 = value["approachspeed1"].asFloat();
    params.approachspeed2 = value["approachspeed2"].asFloat();
    params.backoffDistance1 = value["backoffDistance1"].asFloat();
    params.backoffDistance2 = value["backoffDistance2"].asFloat();
    params.homedPosition = value["homedPosition"].asFloat();
}

Json::Value writeHomingParams( homingParams_t& params )
{
    Json::Value value;
    value["pin"] = params.triggerPin;
    value["state"] = params.triggerState;
    value["direction"] = params.direction;
    value["approachspeed1"] = params.approachspeed1;
    value["approachspeed2"] = params.approachspeed2;
    value["backoffDistance1"] = params.backoffDistance1;
    value["backoffDistance2"] = params.backoffDistance2;
    value["homedPosition"] = params.homedPosition;
    return value;
}

void printHomingParams( homingParams_t& params, int n )
{
    g_log.log(LL_DEBUG, "Config: homingParams[%d] pin = %d", n, params.triggerPin);
    g_log.log(LL_DEBUG, "Config: homingParams[%d] state = %d", n, params.triggerState);
    g_log.log(LL_DEBUG, "Config: homingParams[%d] direction = %d", n, params.direction);
    g_log.log(LL_DEBUG, "Config: homingParams[%d] approach speed 1 = %f", n, params.approachspeed1);
    g_log.log(LL_DEBUG, "Config: homingParams[%d] approach speed 2 = %f", n, params.approachspeed2);
    g_log.log(LL_DEBUG, "Config: homingParams[%d] backoff distance 1 = %f", n, params.backoffDistance1);
    g_log.log(LL_DEBUG, "Config: homingParams[%d] backoff distance 2 = %f", n, params.backoffDistance2);
    g_log.log(LL_DEBUG, "Config: homingParams[%d] homed position = %f", n, params.homedPosition);
}

void readTMCParams( Json::Value& value, tmcSettings_t& params )
{
    params.microsteps = value.get("microsteps", 4).asInt();
    params.current = value.get("current", 200).asInt();
}

Json::Value writeTMCParams( tmcSettings_t& params )
{
    Json::Value value;
    value["microsteps"] = params.microsteps;
    value["current"] = params.current;
    return value;
}

void printTMCParams( tmcSettings_t& params, int n )
{
    g_log.log(LL_DEBUG, "Config: tmcParams[%d] microsteps = %d", n, params.microsteps);
    g_log.log(LL_DEBUG, "Config: tmcParams[%d] current = %d", n, params.current);
}

void readOverrideAction( Json::Value& actionValue, string passOrFail, OverrideConfig& overrideConfig )
{
    if ( ! actionValue.isMember("output") ) {
        g_log.log(LL_ERROR, "Config: output override action has no 'output' property!");
        return;
    }
    if ( ! actionValue.isMember("index") ) {
        g_log.log(LL_ERROR, "Config: output override action has no 'index' property!");
        return;
    }
    if ( ! actionValue.isMember("value") ) {
        g_log.log(LL_ERROR, "Config: output override action has no 'value' property!");
        return;
    }

    string output = actionValue.get("output","").asString();

    overrideAction_t action;

    int index = actionValue.get("index",-1).asInt();
    if ( output == "digital" ) {
        if ( index < 0 || index > 15 ) {
            g_log.log(LL_ERROR, "Config: output override action index for digitalouput should be in range 0-15!");
            return;
        }
        action.digitalOutput = setBitPosition( index );
    }
    else if ( output == "pwm" ) {
        if ( index < 0 || index > 3 ) {
            g_log.log(LL_ERROR, "Config: output override action index for pwmouput should be in range 0-3!");
            return;
        }
        action.pwmOutput = setBitPosition( index );
    }

    action.val = actionValue.get("value",0).asFloat();

    if ( output == "digitaloutput" ) {
        if ( action.val )
            action.val = 1;
    }

    if ( passOrFail == "pass" )
        overrideConfig.passActions.push_back(action);
    else
        overrideConfig.failActions.push_back(action);
}

Json::Value writeOverrideAction( overrideAction_t& action ) {
    Json::Value value;

    int index = 0;
    if ( action.digitalOutput ) {
        value["output"] = "digital";
        index = getBitPosition(action.digitalOutput);
    }
    else if ( action.pwmOutput ) {
        value["output"] = "pwm";
        index = getBitPosition(action.pwmOutput);
    }

    value["index"] = index;

    value["value"] = action.val;

    return value;
}

void readOverride( Json::Value& value, OverrideConfig& overrideConfig )
{
    if ( ! value.isMember("condition") ) {
        g_log.log(LL_ERROR, "Config: output override entry has no 'condition' property!");
        return;
    }
    Json::Value conditionValue = value["condition"];
    string trigger = conditionValue.get("trigger","").asString();
    int index = conditionValue.get("index",-1).asInt();
    if ( index < 0 || index > 15 ) {
        g_log.log(LL_ERROR, "Config: output override index should be in range 0-15!");
        return;
    }
    uint16_t bitPosition = setBitPosition( index );

    if ( trigger == "motionaxis" ) {
        overrideConfig.condition.motionAxis = bitPosition;
        if ( index > 7 ) {
            g_log.log(LL_ERROR, "Config: output override condition motionaxis index should be in range 0-7!");
            return;
        }
    }
    else if ( trigger == "digitaloutput" ) {
        overrideConfig.condition.digitalOutput = bitPosition;
    }
    else if ( trigger == "pwmoutput" ) {
        overrideConfig.condition.pwmOutput = bitPosition;
        if ( index > 3 ) {
            g_log.log(LL_ERROR, "Config: output override condition pwmoutput index should be in range 0-3!");
            return;
        }
    }
    else if ( trigger == "digitalinput" ) {
        overrideConfig.condition.digitalInput = bitPosition;
    }
    else if ( trigger == "vacuumsensor" ) {
        overrideConfig.condition.pressure = bitPosition;
    }
    else if ( trigger == "loadcellsensor" ) {
        overrideConfig.condition.loadcell = bitPosition;
    }
    else if ( trigger == "adcsensor" ) {
        overrideConfig.condition.adc = bitPosition;
    }

    string test = conditionValue.get("test","").asString();
    if ( test.empty() ) {
        g_log.log(LL_ERROR, "Config: output override (trigger '%s') has no 'test' property!", trigger.c_str());
        return;
    }

    if ( test == "lessthan" || test == "low" )
        overrideConfig.condition.comparison = OCC_LESS_THAN;
    else if ( test == "morethan" || test == "high" )
        overrideConfig.condition.comparison = OCC_MORE_THAN;

    if ( trigger == "motionaxis" ||
        trigger == "pwmoutput" ||
        trigger == "vacuumsensor" ||
        trigger == "loadcellsensor" ||
        trigger == "adcsensor" ) {
        // float/analog states, two values to define comparison
        overrideConfig.condition.val = conditionValue.get("value",-1).asFloat();
    }

    // at this point the condition has been read ok

    string actionNames[] = { "pass", "fail" };

    for (int i = 0; i < 2; i++) {
        string actionName = actionNames[i];
        Json::Value actionListValue = value[actionName];
        if ( actionListValue.isArray() ) {
            int numPassActions = actionListValue.size();
            //g_log.log(LL_ERROR, "num %s actions %d", actionName.c_str(), numPassActions);
            for (int k = 0; k < numPassActions; k++) {
                Json::Value passValue = actionListValue[k];
                readOverrideAction( passValue, actionName, overrideConfig );
            }
        }
    }

    if ( overrideConfig.passActions.empty() && overrideConfig.failActions.empty() ) {
        g_log.log(LL_ERROR, "Config: output override has no pass/fail actions!");
        return;
    }

    overrideConfigs.push_back( overrideConfig );
}

Json::Value writeOverride( OverrideConfig& overrideConfig ) {
    Json::Value value;

    Json::Value conditionValue;

    int index = 0;
    if ( overrideConfig.condition.motionAxis ) {
        conditionValue["trigger"] = "motionaxis";
        index = getBitPosition(overrideConfig.condition.motionAxis);
    }
    else if ( overrideConfig.condition.digitalOutput ) {
        conditionValue["trigger"] = "digitaloutput";
        index = getBitPosition(overrideConfig.condition.digitalOutput);
    }
    else if ( overrideConfig.condition.pwmOutput ) {
        conditionValue["trigger"] = "pwmoutput";
        index = getBitPosition(overrideConfig.condition.pwmOutput);
    }
    else if ( overrideConfig.condition.digitalInput ) {
        conditionValue["trigger"] = "digitalinput";
        index = getBitPosition(overrideConfig.condition.digitalInput);
    }
    else if ( overrideConfig.condition.pressure ) {
        conditionValue["trigger"] = "vacuumsensor";
        index = getBitPosition(overrideConfig.condition.pressure);
    }
    else if ( overrideConfig.condition.loadcell ) {
        conditionValue["trigger"] = "loadcellsensor";
        index = getBitPosition(overrideConfig.condition.loadcell);
    }
    else if ( overrideConfig.condition.adc ) {
        conditionValue["trigger"] = "adcsensor";
        index = getBitPosition(overrideConfig.condition.adc);
    }

    conditionValue["index"] = index;

    if ( overrideConfig.condition.motionAxis ||
        overrideConfig.condition.pwmOutput ||
         overrideConfig.condition.pressure ||
         overrideConfig.condition.loadcell ||
        overrideConfig.condition.adc ) {
        conditionValue["test"] = overrideConfig.condition.comparison == OCC_LESS_THAN ? "lessthan" : "morethan";
        conditionValue["value"] = overrideConfig.condition.val;
    }
    else
        conditionValue["test"] = overrideConfig.condition.comparison == OCC_LESS_THAN ? "low" : "high";

    value["condition"] = conditionValue;

    if ( ! overrideConfig.passActions.empty() ) {
        Json::Value passActionsValue = Json::Value(Json::arrayValue);
        for (int i = 0; i < (int)overrideConfig.passActions.size(); i++) {
            overrideAction_t& action = overrideConfig.passActions[i];
            Json::Value actionValue = writeOverrideAction( action );
            passActionsValue[i] = actionValue;
        }
        value["pass"] = passActionsValue;
    }

    if ( ! overrideConfig.failActions.empty() ) {
        Json::Value failActionsValue = Json::Value(Json::arrayValue);
        for (int i = 0; i < (int)overrideConfig.failActions.size(); i++) {
            overrideAction_t& action = overrideConfig.failActions[i];
            Json::Value actionValue = writeOverrideAction( action );
            failActionsValue[i] = actionValue;
        }
        value["fail"] = failActionsValue;
    }

    return value;
}

string getOverrideActionString(overrideAction_t& action)
{
    string s = "      set ";

    if ( action.digitalOutput ) {
        int index = getBitPosition( action.digitalOutput );
        s += "digital output " + to_string(index) + " to ";
        s += action.val > 0 ? "high" : "low";
    }
    else if ( action.pwmOutput ) {
        int index = getBitPosition( action.pwmOutput );
        s += "PWM output " + to_string(index) + " to ";
        s += to_string( action.val );
    }

    return s;
}

void printOverride( OverrideConfig& overrideConfig, int priority )
{
    g_log.log(LL_INFO, "Override config, priority %d", priority);
    string s = "  When ";

    const char* axisNames = "XYZWABCD";

    if ( overrideConfig.condition.motionAxis ) {
        int index = getBitPosition( overrideConfig.condition.motionAxis );
        s += "motion axis " + string(1,axisNames[index]) + " is ";
    }
    else if ( overrideConfig.condition.digitalOutput ) {
        int index = getBitPosition( overrideConfig.condition.digitalOutput );
        s += "digital output " + to_string(index) + " is ";
    }
    else if ( overrideConfig.condition.pwmOutput ) {
        int index = getBitPosition( overrideConfig.condition.pwmOutput );
        s += "PWM output " + to_string(index) + " is ";
    }
    else if ( overrideConfig.condition.digitalInput ) {
        int index = getBitPosition( overrideConfig.condition.digitalInput );
        s += "digital input " + to_string(index) + " is ";
    }
    else if ( overrideConfig.condition.pressure ) {
        int index = getBitPosition( overrideConfig.condition.pressure );
        s += "vacuum sensor " + to_string(index) + " is ";
    }
    else if ( overrideConfig.condition.loadcell ) {
        int index = getBitPosition( overrideConfig.condition.loadcell );
        s += "loadcell sensor " + to_string(index) + " is ";
    }
    else if ( overrideConfig.condition.adc ) {
        int index = getBitPosition( overrideConfig.condition.adc );
        s += "ADC sensor " + to_string(index) + " is ";
    }

    if ( overrideConfig.condition.motionAxis ||
        overrideConfig.condition.pwmOutput ||
        overrideConfig.condition.pressure ||
        overrideConfig.condition.loadcell ||
        overrideConfig.condition.adc ) {
        if ( overrideConfig.condition.comparison == OCC_LESS_THAN )
            s += "less than " + to_string(overrideConfig.condition.val);
        else if ( overrideConfig.condition.comparison == OCC_MORE_THAN )
            s += "more than " + to_string(overrideConfig.condition.val);
    }
    else {
        if ( overrideConfig.condition.comparison == OCC_LESS_THAN )
            s += "low";
        else if ( overrideConfig.condition.comparison == OCC_MORE_THAN )
            s += "high";
    }

    g_log.log(LL_INFO, "%s", s.c_str());

    if ( ! overrideConfig.passActions.empty() ) {
        g_log.log(LL_INFO, "    Pass actions:");
        for (int i = 0; i < (int)overrideConfig.passActions.size(); i++) {
            s = getOverrideActionString( overrideConfig.passActions[i] );
            g_log.log(LL_INFO, "%s", s.c_str());
        }
    }

    if ( ! overrideConfig.failActions.empty() ) {
        g_log.log(LL_INFO, "    Fail actions:");
        for (int i = 0; i < (int)overrideConfig.failActions.size(); i++) {
            s = getOverrideActionString( overrideConfig.failActions[i] );
            g_log.log(LL_INFO, "%s", s.c_str());
        }
    }
}

void printOverrides() {
    for (int i = 0; i < (int)overrideConfigs.size(); i++) {
        printOverride( overrideConfigs[i], i );
    }
}




const char* configFilename = "config.json";

bool readConfigFile()
{    
    std::ifstream ifs;
    ifs.open(configFilename, std::ios::in);
    if ( ! ifs ) {
        g_log.log(LL_ERROR, "Could not open config file for reading: %s", configFilename);
        return false;
    }

    Json::Value fileValue;
    Json::Reader reader;
    bool parsed = reader.parse(ifs, fileValue);
    ifs.close();

    if ( ! parsed ) {
        g_log.log(LL_ERROR, "Could not parse config file: %s", configFilename);
        return false;
    }

    const char* axisNames = "xyzwabcd";
    char buf[32];

    if ( fileValue.isMember(CONFIG_STEPS_PER_UNIT) ) {
        Json::Value stepsPerUnitValue = fileValue[CONFIG_STEPS_PER_UNIT];
        // todo set actual data.pos_scale value here?
        for (int i = 0; i < NUM_MOTION_AXES; i++ ) {
            sprintf(buf, "%c", axisNames[i]);
            stepsPerUnit[i] = stepsPerUnitValue.get(buf, 0).asFloat();
            data.pos_scale[i] = stepsPerUnit[i];
            g_log.log(LL_DEBUG, "Config: steps per unit %s = %f", buf, stepsPerUnit[i]);
        }
    }

    if ( fileValue.isMember(CONFIG_WORK_AREA) ) {
        Json::Value workAreaValue = fileValue[CONFIG_WORK_AREA];
        float workArea[3];
        for (int i = 0; i < 3; i++ ) {
            sprintf(buf, "%c", axisNames[i]);
            workArea[i] = workAreaValue.get(buf, 0).asFloat();
            g_log.log(LL_DEBUG, "Config: work area %s = %f", buf, workArea[i]);
        }
        machineLimits.setPositionLimits(0, 0, -workArea[2],     workArea[0], workArea[1], 0);
    }

    if ( fileValue.isMember(CONFIG_AXIS_SPEED_LIMIT) ) {
        Json::Value axisSpeedLimitsValue = fileValue[CONFIG_AXIS_SPEED_LIMIT];
        for (int i = 0; i < 3; i++ ) {
            sprintf(buf, "%c", axisNames[i]);
            if ( axisSpeedLimitsValue.isMember(buf) ) {
                Json::Value speedLimitsValue = axisSpeedLimitsValue[buf];
                machineLimits.velLimit[i]  = speedLimitsValue.get("vel",0).asFloat();
                machineLimits.accLimit[i]  = speedLimitsValue.get("acc",0).asFloat();
                machineLimits.jerkLimit[i] = speedLimitsValue.get("jerk",0).asFloat();

                g_log.log(LL_DEBUG, "Config: machineLimits vel %s = %f", buf, machineLimits.velLimit[i]);
                g_log.log(LL_DEBUG, "Config: machineLimits acc %s = %f", buf, machineLimits.accLimit[i]);
                g_log.log(LL_DEBUG, "Config: machineLimits jerk %s = %f", buf, machineLimits.jerkLimit[i]);
            }
        }

        if ( axisSpeedLimitsValue.isMember("r") ) {
            Json::Value rotateLimitsValue = axisSpeedLimitsValue["r"];
            machineLimits.grotationVelLimit  = rotateLimitsValue.get("vel",0).asFloat();
            machineLimits.grotationAccLimit  = rotateLimitsValue.get("acc",0).asFloat();
            machineLimits.grotationJerkLimit = rotateLimitsValue.get("jerk",0).asFloat();

            g_log.log(LL_DEBUG, "Config: machineLimits rotate vel = %f", machineLimits.grotationVelLimit);
            g_log.log(LL_DEBUG, "Config: machineLimits rotate acc = %f", machineLimits.grotationAccLimit);
            g_log.log(LL_DEBUG, "Config: machineLimits rotate jerk = %f", machineLimits.grotationJerkLimit);
        }
    }

    if ( fileValue.isMember(CONFIG_CURRENT_MOVE_LIMIT) ) {
        Json::Value currentMoveLimitsValue = fileValue[CONFIG_CURRENT_MOVE_LIMIT];
        machineLimits.initialMoveLimitVel  = currentMoveLimitsValue.get("vel",0).asFloat();
        machineLimits.initialMoveLimitAcc  = currentMoveLimitsValue.get("acc",0).asFloat();
        machineLimits.initialMoveLimitJerk = currentMoveLimitsValue.get("jerk",0).asFloat();

        g_log.log(LL_DEBUG, "Config: initialMoveLimitVel  = %f", machineLimits.initialMoveLimitVel);
        g_log.log(LL_DEBUG, "Config: initialMoveLimitAcc  = %f", machineLimits.initialMoveLimitAcc);
        g_log.log(LL_DEBUG, "Config: initialMoveLimitJerk = %f", machineLimits.initialMoveLimitJerk);

        currentMoveLimits.vel  = machineLimits.initialMoveLimitVel;
        currentMoveLimits.acc  = machineLimits.initialMoveLimitAcc;
        currentMoveLimits.jerk = machineLimits.initialMoveLimitJerk;
    }

    if ( fileValue.isMember(CONFIG_CURRENT_ROTATE_LIMIT) ) {
        Json::Value currentRotateLimitsValue = fileValue[CONFIG_CURRENT_ROTATE_LIMIT];
        machineLimits.initialRotationLimitVel  = currentRotateLimitsValue.get("vel",0).asFloat();
        machineLimits.initialRotationLimitAcc  = currentRotateLimitsValue.get("acc",0).asFloat();
        machineLimits.initialRotationLimitJerk = currentRotateLimitsValue.get("jerk",0).asFloat();

        g_log.log(LL_DEBUG, "Config: initialRotationLimitVel  = %f", machineLimits.initialRotationLimitVel);
        g_log.log(LL_DEBUG, "Config: initialRotationLimitAcc  = %f", machineLimits.initialRotationLimitAcc);
        g_log.log(LL_DEBUG, "Config: initialRotationLimitJerk = %f", machineLimits.initialRotationLimitJerk);

        currentRotationLimits[0].vel  = machineLimits.initialRotationLimitVel;
        currentRotationLimits[0].acc  = machineLimits.initialRotationLimitAcc;
        currentRotationLimits[0].jerk = machineLimits.initialRotationLimitJerk;

        for (int i = 1; i < NUM_ROTATION_AXES; i++) {
            currentRotationLimits[i] = currentRotationLimits[0];
        }
    }

    if ( fileValue.isMember(CONFIG_BLEND_FRACTION) ) {
        machineLimits.maxOverlapFraction = fileValue.get(CONFIG_BLEND_FRACTION,0).asFloat();
        g_log.log(LL_DEBUG, "Config: corner blend fraction = %f", machineLimits.maxOverlapFraction);
    }

    if ( fileValue.isMember(CONFIG_HOMING_PARAMS) ) {
        Json::Value homingParamsValue = fileValue[CONFIG_HOMING_PARAMS];
        readHomingParams( homingParamsValue["x"], homingParams[0] );
        readHomingParams( homingParamsValue["y"], homingParams[1] );
        readHomingParams( homingParamsValue["z"], homingParams[2] );
        readHomingParams( homingParamsValue["w"], homingParams[3] );

        if ( homingParamsValue.isMember("order") ) {
            string s = homingParamsValue.get("order","").asString();
            memcpy(homingOrder, s.c_str(), min(s.size(), sizeof(homingOrder)));
        }

        g_log.log(LL_DEBUG, "Config: homing order = %s", homingOrder);

        for (int i = 0; i < 3; i++)
            printHomingParams( homingParams[i], i );
    }

    if ( fileValue.isMember(CONFIG_TMC_PARAMS) ) {
        Json::Value tmcParamsValue = fileValue[CONFIG_TMC_PARAMS];
        if ( tmcParamsValue.isArray() ) {
            for (int i = 0; i < NUM_MOTION_AXES; i++)
                readTMCParams( tmcParamsValue[i], tmcParams[i] );
            for (int i = 0; i < NUM_MOTION_AXES; i++)
                printTMCParams( tmcParams[i], i );
        }
    }

    if ( fileValue.isMember(CONFIG_JOG_SPEEDS) ) {
        Json::Value jogSpeedsValue = fileValue[CONFIG_JOG_SPEEDS];
        for (int i = 0; i < NUM_MOTION_AXES; i++ ) {
            sprintf(buf, "%c", axisNames[i]);
            joggingSpeeds[i] = jogSpeedsValue.get(buf, 0).asFloat();
            g_log.log(LL_DEBUG, "Config: joggingSpeed %s = %f", buf, joggingSpeeds[i]);
        }
    }

    if ( fileValue.isMember(CONFIG_OVERRIDES) ) {
        Json::Value overridesValue = fileValue[CONFIG_OVERRIDES];
        if ( overridesValue.isArray() ) {
            int numOverrides = overridesValue.size();
            for (int i = 0; i < numOverrides; i++) {
                Json::Value overrideValue = overridesValue[i];
                OverrideConfig overrideConfig;
                readOverride( overrideValue, overrideConfig );
            }
            g_log.log(LL_DEBUG, "Config: %d overrides are in effect:", numOverrides);
            printOverrides();
        }
    }

    if ( fileValue.isMember(CONFIG_LOADCELL_CALIB) ) {
        Json::Value loadCellCalibValue = fileValue[CONFIG_LOADCELL_CALIB];
        loadcellCalibrationRawOffset = loadCellCalibValue.get("rawOffset", 1000).asInt();
        loadcellCalibrationWeight = loadCellCalibValue.get("weight", 100).asInt();
        g_log.log(LL_DEBUG, "Config: load cell calib raw offset = %d", loadcellCalibrationRawOffset);
        g_log.log(LL_DEBUG, "Config: load cell calib weight = %f", loadcellCalibrationWeight);
    }

    if ( fileValue.isMember(CONFIG_PROBING_PARAMS) ) {
        Json::Value probingParamsValue = fileValue[CONFIG_PROBING_PARAMS];
        probingParams.digitalTriggerPin = probingParamsValue.get("digitalTriggerPin", -1).asInt();
        probingParams.digitalTriggerState = probingParamsValue.get("digitalTriggerState", 1).asInt();
        probingParams.vacuumStep = probingParamsValue.get("vacuumStep", 0.5).asFloat();
        probingParams.approachspeed1 = probingParamsValue["approachspeed1"].asFloat();
        probingParams.approachspeed2 = probingParamsValue["approachspeed2"].asFloat();
        probingParams.backoffDistance1 = probingParamsValue["backoffDistance1"].asFloat();
        probingParams.backoffDistance2 = probingParamsValue["backoffDistance2"].asFloat();
        probingParams.vacuumSniffPin = probingParamsValue.get("vacuumSniffPin", -1).asInt();
        probingParams.vacuumSniffState = probingParamsValue.get("vacuumSniffState", 0).asInt();
        probingParams.vacuumSniffTimeMs = probingParamsValue.get("vacuumSniffTimeMs", 500).asInt();
        probingParams.vacuumReplenishTimeMs = probingParamsValue.get("vacuumReplenishTimeMs", 500).asInt();

        g_log.log(LL_DEBUG, "Config: probing digital trigger pin = %d", probingParams.digitalTriggerPin);
        g_log.log(LL_DEBUG, "Config: probing digital trigger state = %d", probingParams.digitalTriggerState);        
        g_log.log(LL_DEBUG, "Config: probing vacuum sniff pin = %d", probingParams.vacuumSniffPin);
        g_log.log(LL_DEBUG, "Config: probing vacuum sniff state = %d", probingParams.vacuumSniffState);
        g_log.log(LL_DEBUG, "Config: probing vacuum sniff time = %d", probingParams.vacuumSniffTimeMs);
        g_log.log(LL_DEBUG, "Config: probing vacuum replenish time = %d", probingParams.vacuumReplenishTimeMs);
        g_log.log(LL_DEBUG, "Config: probing vacuum step = %f", probingParams.vacuumStep);
        g_log.log(LL_DEBUG, "Config: probing approach speed 1 = %f", probingParams.approachspeed1);
        g_log.log(LL_DEBUG, "Config: probing approach speed 2 = %f", probingParams.approachspeed2);
        g_log.log(LL_DEBUG, "Config: probing backoff distance 1 = %f", probingParams.backoffDistance1);
        g_log.log(LL_DEBUG, "Config: probing backoff distance 2 = %f", probingParams.backoffDistance2);
    }

    return true;
}

bool saveConfigToFile()
{
    g_log.log(LL_DEBUG, "Saving config to file");

    std::ofstream ofs;
    ofs.open(configFilename, std::ios::out);
    if ( ! ofs) {
        g_log.log(LL_ERROR, "Could not open file '%s' for writing.", configFilename);
        return false;
    }

    Json::Value fileValue;

    const char* axisNames = "xyzwabcd";
    char buf[32];


    Json::Value stepsPerUnitValue;
    for (int i = 0; i < NUM_MOTION_AXES; i++ ) {
        sprintf(buf, "%c", axisNames[i]);
        stepsPerUnitValue[buf] = stepsPerUnit[i];
    }
    fileValue[CONFIG_STEPS_PER_UNIT] = stepsPerUnitValue;


    Json::Value workAreaValue;
    scv::vec3 workAreaDimensions = machineLimits.posLimitUpper - machineLimits.posLimitLower;
    for (int i = 0; i < 3; i++ ) {
        sprintf(buf, "%c", axisNames[i]);
        workAreaValue[buf] = workAreaDimensions[i];
    }
    fileValue[CONFIG_WORK_AREA] = workAreaValue;


    Json::Value axisSpeedLimitsValue;
    for (int i = 0; i < 3; i++ ) {
        sprintf(buf, "%c", axisNames[i]);
        Json::Value speedLimitsValue;
        speedLimitsValue["vel"] = machineLimits.velLimit[i];
        speedLimitsValue["acc"] = machineLimits.accLimit[i];
        speedLimitsValue["jerk"] = machineLimits.jerkLimit[i];
        axisSpeedLimitsValue[buf] = speedLimitsValue;
    }

    Json::Value rotateLimitsValue;
    rotateLimitsValue["vel"] = machineLimits.grotationVelLimit;
    rotateLimitsValue["acc"] = machineLimits.grotationAccLimit;
    rotateLimitsValue["jerk"] = machineLimits.grotationJerkLimit;
    axisSpeedLimitsValue["r"] = rotateLimitsValue;

    fileValue[CONFIG_AXIS_SPEED_LIMIT] = axisSpeedLimitsValue;


    Json::Value initialMoveLimitsValue;
    for (int i = 0; i < NUM_MOTION_AXES; i++ ) {
        initialMoveLimitsValue["vel"] = machineLimits.initialMoveLimitVel;
        initialMoveLimitsValue["acc"] = machineLimits.initialMoveLimitAcc;
        initialMoveLimitsValue["jerk"] = machineLimits.initialMoveLimitJerk;
    }
    fileValue[CONFIG_CURRENT_MOVE_LIMIT] = initialMoveLimitsValue;


    Json::Value initialRotationLimitsValue;
    for (int i = 0; i < NUM_ROTATION_AXES; i++ ) {
        initialRotationLimitsValue["vel"] = machineLimits.initialRotationLimitVel;
        initialRotationLimitsValue["acc"] = machineLimits.initialRotationLimitAcc;
        initialRotationLimitsValue["jerk"] = machineLimits.initialRotationLimitJerk;
    }
    fileValue[CONFIG_CURRENT_ROTATE_LIMIT] = initialRotationLimitsValue;


    fileValue[CONFIG_BLEND_FRACTION] = machineLimits.maxOverlapFraction;


    Json::Value homingParamsValue;
    homingParamsValue["x"] = writeHomingParams( homingParams[0] );
    homingParamsValue["y"] = writeHomingParams( homingParams[1] );
    homingParamsValue["z"] = writeHomingParams( homingParams[2] );
    homingParamsValue["w"] = writeHomingParams( homingParams[3] );
    homingParamsValue["order"] = homingOrder;
    fileValue[CONFIG_HOMING_PARAMS] = homingParamsValue;


    Json::Value tmcParamsValue;
    for (int i = 0; i < NUM_MOTION_AXES; i++ ) {
        tmcParamsValue[i] = writeTMCParams( tmcParams[i] );
    }
    fileValue[CONFIG_TMC_PARAMS] = tmcParamsValue;


    Json::Value jogSpeedsValue;
    for (int i = 0; i < NUM_MOTION_AXES; i++ ) {
        sprintf(buf, "%c", axisNames[i]);
        jogSpeedsValue[buf] = joggingSpeeds[i];
    }
    fileValue[CONFIG_JOG_SPEEDS] = jogSpeedsValue;


    Json::Value outputOverridesValue;
    for (int i = 0; i < (int)overrideConfigs.size(); i++) {
        OverrideConfig& config = overrideConfigs[i];
        outputOverridesValue[i] = writeOverride( config );
    }
    fileValue[CONFIG_OVERRIDES] = outputOverridesValue;


    Json::Value loadcellCalibValue;
    loadcellCalibValue["rawOffset"] = loadcellCalibrationRawOffset;
    loadcellCalibValue["weight"] = loadcellCalibrationWeight;
    fileValue[CONFIG_LOADCELL_CALIB] = loadcellCalibValue;


    Json::Value probingParamsValue;
    probingParamsValue["digitalTriggerPin"] = probingParams.digitalTriggerPin;
    probingParamsValue["digitalTriggerState"] = probingParams.digitalTriggerState;
    probingParamsValue["vacuumStep"] = probingParams.vacuumStep;
    probingParamsValue["approachspeed1"] = probingParams.approachspeed1;
    probingParamsValue["approachspeed2"] = probingParams.approachspeed2;
    probingParamsValue["backoffDistance1"] = probingParams.backoffDistance1;
    probingParamsValue["backoffDistance2"] = probingParams.backoffDistance2;    
    probingParamsValue["vacuumSniffPin"] = probingParams.vacuumSniffPin;
    probingParamsValue["vacuumSniffState"] = probingParams.vacuumSniffState;
    probingParamsValue["vacuumSniffTimeMs"] = probingParams.vacuumSniffTimeMs;
    probingParamsValue["vacuumReplenishTimeMs"] = probingParams.vacuumReplenishTimeMs;
    fileValue[CONFIG_PROBING_PARAMS] = probingParamsValue;


    Json::StyledStreamWriter writer("    ");
    writer.write( ofs, fileValue );

    ofs.close();

    return true;
}























