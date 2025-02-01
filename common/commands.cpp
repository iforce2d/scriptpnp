
#include <stdint.h>
#include <stdio.h>
#include "commands.h"


#define tmpMacro(theval) #theval,

char const *commandNames[] =
{
    "CT_NONE",
    COMMAND_TYPE_LIST
};

#undef tmpMacro


char gcnBuf[16];
const char* getCommandName(uint8_t type) {
    if ( type < CT_MAX ) {
        return commandNames[type];
    }
    else {
        sprintf(gcnBuf, "%d", type);
        return gcnBuf;
    }
}

void initInvalidFloats(float* v, int n) {
    for (int i = 0; i < n; i++)
        v[i] = INVALID_FLOAT;
}

Command* getCommandOfType(int t) {
    switch ( t ) {
    case CT_MOVETO:                     return new Command_moveTo();
    case CT_DIGITAL_OUTPUT:             return new Command_digitalOutput();
    case CT_WAIT:                       return new Command_wait();
    case CT_SET_MOVE_LIMITS:            return new Command_setMoveLimits();
    case CT_SET_ROTATE_LIMITS:          return new Command_setRotateLimits();
    case CT_PWM_OUTPUT:                 return new Command_setPWM();
    case CT_ROTATETO:                   return new Command_rotateTo();
    case CT_SYNC:                       return new Command_sync();
    case CT_SET_CORNER_BLEND_OVERLAP:   return new Command_setCornerBlendOverlap();
    }
    printf("(getCommandOfType) unhandled command type: %s\n", getCommandName(t));
    return NULL;
}

Command* Command::unpack(uint8_t *data, int& pos) {
    uint16_t type = 0;

    memcpy(&type, &data[pos], sizeof(type));

    Command* cmd = getCommandOfType(type);
    if ( ! cmd )
        return NULL;
    cmd->unpack(&data[pos]);
    pos += cmd->getSize();
    return cmd;
}
