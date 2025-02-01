#ifndef OVERRIDES_H
#define OVERRIDES_H

#include <stdint.h>
#include <vector>

#include "packable.h"

inline int getBitPosition(uint16_t b) {
    for (int i = 0; i < 16; i++) {
        if ( b & (1 << i) )
            return i;
    }
    return -1;
}

inline uint16_t setBitPosition(int i) {
    return (1 << i);
}

enum operandNameIndex_e {
    ONI_MOTION_AXIS = 0,
    ONI_DIGITAL_OUTPUT,
    ONI_PWM_OUTPUT,
    ONI_DIGITAL_INPUT,
    ONI_VACUUM_SENSOR,
    ONI_LOADCELL_SENSOR,
    ONI_ADC_INPUT,

    ONI_MAX
};

enum overrideConditionComparison_e {
    OCC_NONE        = -1,
    OCC_LESS_THAN   = 0,
    OCC_MORE_THAN
};

struct overrideCondition_t {
    // one of these will be non-zero, bit position indicates which axis to compare
    uint16_t motionAxis;
    uint16_t digitalOutput;
    uint16_t pwmOutput;
    uint16_t digitalInput;
    uint16_t pressure;
    uint16_t adc;
    uint16_t loadcell;

    overrideConditionComparison_e comparison;
    float val;

    int ui_operandIndex;

    overrideCondition_t() {
        motionAxis = 0;
        digitalOutput = 0;
        pwmOutput = 0;
        digitalInput = 0;
        pressure = 0;
        loadcell = 0;
        adc = 0;
        comparison = OCC_NONE;
        val = 0;
        ui_operandIndex = -1;
    }

    void resetSelector() {
        motionAxis = 0;
        digitalOutput = 0;
        pwmOutput = 0;
        digitalInput = 0;
        pressure = 0;
        loadcell = 0;
        adc = 0;
    }

    bool isValid() {
        return (motionAxis ||
                digitalOutput ||
                pwmOutput ||
                digitalInput ||
                pressure ||
                loadcell ||
                adc) && comparison != OCC_NONE;
    }
};

struct overrideAction_t {
    // one of these will be non-zero, bit position indicates which axis to set
    uint16_t digitalOutput;
    uint16_t pwmOutput;

    float val;

    int ui_operandIndex;

    overrideAction_t() {
        digitalOutput = 0;
        pwmOutput = 0;
        val = 0;
        ui_operandIndex = -1;
    }

    void resetSelector() {
        digitalOutput = 0;
        pwmOutput = 0;
    }

    bool isValid() {
        return digitalOutput ||
                pwmOutput;
    }

    int getSize();
    int pack(uint8_t* data);
    int unpack(uint8_t* data);
};

class OverrideConfig {
public:
    overrideCondition_t condition;
    std::vector<overrideAction_t> passActions;
    std::vector<overrideAction_t> failActions;

    bool ui_editing;

    bool isValid() {
        if ( ! condition.isValid() )
            return false;
        if ( passActions.empty() && failActions.empty() )
            return false;
        for (int i = 0; i < (int)passActions.size(); i++)
            if ( ! passActions[i].isValid() )
                return false;
        for (int i = 0; i < (int)failActions.size(); i++)
            if ( ! failActions[i].isValid() )
                return false;
        return true;
    }

    OverrideConfig();
    int getSize();
    int pack(uint8_t* data);
    int unpack(uint8_t* data);
};

extern std::vector<OverrideConfig> overrideConfigs;

class OverrideConfigSet : public Packable {
public:
    int getSize();
    int pack(uint8_t* data);
    bool unpack(uint8_t* data);
};

extern OverrideConfigSet overrideConfigSet;
extern OverrideConfigSet overrideConfigSet_incoming; // to unpack from network message without setting real one

void initDefaultOverrides();
void updateUIIndexOfOverrideOptions();



#endif
