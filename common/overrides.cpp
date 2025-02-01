
#include <stdio.h>
#include <string>
#include <cstring>
#include "overrides.h"
#include "log.h"

using namespace std;

vector<OverrideConfig> overrideConfigs;
OverrideConfigSet overrideConfigSet;
OverrideConfigSet overrideConfigSet_incoming;


void initDefaultOverrides()
{
    {
        OverrideConfig oc;
        oc.condition.pressure = 0x1;
        oc.condition.comparison = OCC_MORE_THAN;
        oc.condition.val = -30;

        overrideAction_t oap;
        oap.digitalOutput = 0x1;
        oap.val = 1;
        oc.passActions.push_back(oap);

        overrideConfigs.push_back(oc);
    }
    {
        OverrideConfig oc;
        oc.condition.pressure = 0x1;
        oc.condition.comparison = OCC_LESS_THAN;
        oc.condition.val = -35;

        overrideAction_t oap;
        oap.digitalOutput = 0x1;
        oap.val = 0;
        oc.passActions.push_back(oap);

        overrideConfigs.push_back(oc);
    }

    {
        OverrideConfig oc;
        oc.condition.digitalOutput = 0x2;
        oc.condition.comparison = OCC_MORE_THAN;

        overrideAction_t oa;
        oa.digitalOutput = 0x1;
        oa.val = 0;
        oc.passActions.push_back(oa);

        overrideConfigs.push_back(oc);
    }

    {
        OverrideConfig oc;
        oc.condition.motionAxis = 0x1;
        oc.condition.comparison = OCC_MORE_THAN;
        oc.condition.val = 100;

        {
            overrideAction_t oap;
            oap.pwmOutput = 0x1;
            oap.val = 0.125;
            oc.passActions.push_back(oap);
        }

        {
            overrideAction_t oaf;
            oaf.pwmOutput = 0x1;
            oaf.val = 0;
            oc.failActions.push_back(oaf);
        }

        {
            overrideAction_t oap;
            oap.digitalOutput = 0x8;
            oap.val = 1;
            oc.passActions.push_back(oap);
        }

        {
            overrideAction_t oaf;
            oaf.digitalOutput = 0x8;
            oaf.val = 0;
            oc.failActions.push_back(oaf);
        }

        overrideConfigs.push_back(oc);
    }
}

void updateUIIndexOfOverrideOptions() {
    for (int i = 0; i < (int)overrideConfigs.size(); i++) {

        OverrideConfig &config = overrideConfigs[i];

        if ( config.condition.motionAxis )
            config.condition.ui_operandIndex = ONI_MOTION_AXIS;
        else if ( config.condition.digitalOutput )
            config.condition.ui_operandIndex = ONI_DIGITAL_OUTPUT;
        else if ( config.condition.pwmOutput )
            config.condition.ui_operandIndex = ONI_PWM_OUTPUT;
        else if ( config.condition.digitalInput )
            config.condition.ui_operandIndex = ONI_DIGITAL_INPUT;
        else if ( config.condition.pressure )
            config.condition.ui_operandIndex = ONI_VACUUM_SENSOR;
        else if ( config.condition.loadcell )
            config.condition.ui_operandIndex = ONI_LOADCELL_SENSOR;
        else if ( config.condition.adc )
            config.condition.ui_operandIndex = ONI_ADC_INPUT;

        for (int k = 0; k < (int)config.passActions.size(); k++) {
            overrideAction_t &action = config.passActions[k];
            if ( action.digitalOutput )
                action.ui_operandIndex = 0;
            else if ( action.pwmOutput )
                action.ui_operandIndex = 1;
        }

        for (int k = 0; k < (int)config.failActions.size(); k++) {
            overrideAction_t &action = config.failActions[k];
            if ( action.digitalOutput )
                action.ui_operandIndex = 0;
            else if ( action.pwmOutput )
                action.ui_operandIndex = 1;
        }
    }
}













#define PACK_AND_INCREMENT(what)\
    memcpy(&data[pos], &what, sizeof(what));\
    pos += sizeof(what);

#define UNPACK_AND_INCREMENT(what)\
    memcpy(&what, &data[pos], sizeof(what));\
    pos += sizeof(what);

int overrideAction_t::getSize()
{
    int s = 0;
    s += sizeof(digitalOutput);
    s += sizeof(pwmOutput);
    s += sizeof(val);
    return s;
}

int overrideAction_t::pack(uint8_t *data)
{
    int pos = 0;
    PACK_AND_INCREMENT( digitalOutput );
    PACK_AND_INCREMENT( pwmOutput );
    PACK_AND_INCREMENT( val );
    return pos;
}

int overrideAction_t::unpack(uint8_t* data)
{
    int pos = 0;
    UNPACK_AND_INCREMENT( digitalOutput );
    UNPACK_AND_INCREMENT( pwmOutput );
    UNPACK_AND_INCREMENT( val );
    return pos;
}






OverrideConfig::OverrideConfig() {
    ui_editing = false;
}

int OverrideConfig::getSize()
{
    int s = 0;
    s += sizeof(condition.motionAxis);
    s += sizeof(condition.digitalOutput);
    s += sizeof(condition.pwmOutput);
    s += sizeof(condition.digitalInput);
    s += sizeof(condition.pressure);
    s += sizeof(condition.loadcell);
    s += sizeof(condition.adc);
    s += sizeof(int8_t); // comparison
    s += sizeof(condition.val);  // val

    s += sizeof(uint8_t); // num pass actions
    s += sizeof(uint8_t); // num fail actions

    for (int i = 0; i < (int)passActions.size(); i++) {
        s += passActions[i].getSize();
    }

    for (int i = 0; i < (int)failActions.size(); i++) {
        s += passActions[i].getSize();
    }

    return s;
}

int OverrideConfig::pack(uint8_t* data) {

    int pos = 0;

    PACK_AND_INCREMENT( condition.motionAxis );
    PACK_AND_INCREMENT( condition.digitalOutput );
    PACK_AND_INCREMENT( condition.pwmOutput );
    PACK_AND_INCREMENT( condition.digitalInput );
    PACK_AND_INCREMENT( condition.pressure );
    PACK_AND_INCREMENT( condition.loadcell );
    PACK_AND_INCREMENT( condition.adc );

    int8_t comp = condition.comparison;
    PACK_AND_INCREMENT( comp );
    PACK_AND_INCREMENT( condition.val );

    uint8_t numPassActions = (uint8_t)passActions.size();
    uint8_t numFailActions = (uint8_t)failActions.size();

    PACK_AND_INCREMENT( numPassActions );
    PACK_AND_INCREMENT( numFailActions );

    for (int i = 0; i < numPassActions; i++) {
        pos += passActions[i].pack( &data[pos] );
    }
    for (int i = 0; i < numFailActions; i++) {
        pos += failActions[i].pack( &data[pos] );
    }

    return pos;
}

int OverrideConfig::unpack(uint8_t* data) {

    int pos = 0;

    UNPACK_AND_INCREMENT( condition.motionAxis );
    UNPACK_AND_INCREMENT( condition.digitalOutput );
    UNPACK_AND_INCREMENT( condition.pwmOutput );
    UNPACK_AND_INCREMENT( condition.digitalInput );
    UNPACK_AND_INCREMENT( condition.pressure );
    UNPACK_AND_INCREMENT( condition.loadcell );
    UNPACK_AND_INCREMENT( condition.adc );

    int8_t comp = 0;
    UNPACK_AND_INCREMENT( comp );
    condition.comparison = (overrideConditionComparison_e)comp;

    UNPACK_AND_INCREMENT( condition.val );

    uint8_t numPassActions = 0;
    uint8_t numFailActions = 0;

    UNPACK_AND_INCREMENT( numPassActions );
    UNPACK_AND_INCREMENT( numFailActions );

    for (int i = 0; i < numPassActions; i++) {
        overrideAction_t action;
        pos += action.unpack( &data[pos] );
        passActions.push_back( action );
    }
    for (int i = 0; i < numFailActions; i++) {
        overrideAction_t action;
        pos += action.unpack( &data[pos] );
        failActions.push_back( action );
    }

    return pos;
}



int OverrideConfigSet::getSize()
{
    int s = 0;
    s += sizeof(uint16_t); // number of overrides
    for (int i = 0; i < (int)overrideConfigs.size(); i++) {
        s += overrideConfigs[i].getSize();
    }
    return s;
}

int OverrideConfigSet::pack(uint8_t *data)
{
    int pos = 0;

    uint16_t numOverrides = overrideConfigs.size();
    PACK_AND_INCREMENT( numOverrides );

    for (int i = 0; i < (int)overrideConfigs.size(); i++) {
        pos += overrideConfigs[i].pack( &data[pos] );
    }
    return pos;
}

bool OverrideConfigSet::unpack(uint8_t* data)
{
    int pos = 0;

    uint16_t numOverrides = 0;
    UNPACK_AND_INCREMENT( numOverrides );

    for (int i = 0; i < numOverrides; i++) {
        OverrideConfig oc;
        pos += oc.unpack( &data[pos] );
        overrideConfigs.push_back( oc );
    }

    return true;
}













