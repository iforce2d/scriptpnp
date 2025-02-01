
#include <vector>
#include <string>
#include <algorithm>

#include "commandlist.h"
#include "pnpMessages.h"


using namespace std;

#define NUM_ROTATION_AXES 4

#define INVALID_FLOAT   0xFFFFFFFF

CommandList::CommandList(cornerBlendMethod_e blendMethod) {
    cornerBlendMethod = blendMethod;
    cornerBlendMaxFraction = 0.8f;
    posLimitLower = scv::vec3(0,0,0);
    posLimitUpper = scv::vec3(1,1,1);
    for (int i = 0 ; i < NUM_ROTATION_AXES; i++) {
        rotationPositionLimits[i].x = -180;
        rotationPositionLimits[i].y = 180;
    }
}

CommandList::~CommandList()
{
    clear();
}

int CommandList::getSize() {
    int s = sizeof(uint8_t) + // blend method
            sizeof(float) +   // blend fraction
            sizeof(uint16_t); // for numCommands
    for (int i = 0; i < (int)commands.size(); i++) {
        s += commands[i]->getSize();
    }
    return s;
}

int CommandList::pack(uint8_t* data) {

    uint8_t blend = cornerBlendMethod;
    memcpy(data, &blend, sizeof(blend));
    int pos = sizeof(blend);

    memcpy(&data[pos], &cornerBlendMaxFraction, sizeof(cornerBlendMaxFraction));
    pos += sizeof(cornerBlendMaxFraction);

    uint16_t numCommands = (uint16_t)commands.size();
    memcpy(&data[pos], &numCommands, sizeof(numCommands));
    pos += sizeof(numCommands);

    for (int i = 0; i < (int)commands.size(); i++) {
        pos += commands[i]->pack(&data[pos]);
    }

    return pos;
}

bool CommandList::unpack(uint8_t* data) {
    uint8_t blend = 0;
    uint16_t numCommands = 0;

    memcpy(&blend, data, sizeof(blend));
    int pos = sizeof(blend);
    cornerBlendMethod = (cornerBlendMethod_e)blend;

    memcpy(&cornerBlendMaxFraction, &data[pos], sizeof(cornerBlendMaxFraction));
    pos += sizeof(cornerBlendMaxFraction);

    memcpy(&numCommands, &data[pos], sizeof(numCommands));
    pos += sizeof(numCommands);

    for (int i = 0; i < numCommands; i++) {
        Command* cmd = Command::unpack(data, pos);
        if ( cmd )
            commands.push_back(cmd);
        else {
            clear();
            return false;
        }
    }

    return true;
}

void CommandList::clear() {
    for (int i = 0; i < (int)commands.size(); i++) {
        delete commands[i];
    }
    commands.clear();
}

bool CommandList::empty() {
    return commands.empty();
}




bool sanityCheckCommandList(CommandList& program) {

    for (int i = 0; i < (int)program.commands.size(); i++) {
        Command* cmd = program.commands[i];
        if ( cmd->type == CT_MOVETO ) {
            Command_moveTo* mt = (Command_moveTo*)cmd;
            if ( (mt->dst.x != INVALID_FLOAT && mt->dst.flags_x != MOVE_FLAG_RELATIVE && (mt->dst.x < program.posLimitLower.x || mt->dst.x > program.posLimitUpper.x)) ||
                 (mt->dst.y != INVALID_FLOAT && mt->dst.flags_y != MOVE_FLAG_RELATIVE && (mt->dst.y < program.posLimitLower.y || mt->dst.y > program.posLimitUpper.y)) ||
                 (mt->dst.z != INVALID_FLOAT && mt->dst.flags_z != MOVE_FLAG_RELATIVE && (mt->dst.z < program.posLimitLower.z || mt->dst.z > program.posLimitUpper.z)) )
                return false;
        }
        else if ( cmd->type == CT_ROTATETO ) {
            Command_rotateTo* rt = (Command_rotateTo*)cmd;
            for (int i = 0; i < NUM_ROTATION_AXES; i++) {
                if ( rt->dst[i] != INVALID_FLOAT && (rt->dst[i] < program.rotationPositionLimits[i].x || rt->dst[i] > program.rotationPositionLimits[i].y) )
                    return false;
            }
        }
    }
    return true;
}

void dumpCommandList(CommandList& program) {
    printf("Program numCommands: %d\n", (int)program.commands.size());
    for (int i = 0; i < (int)program.commands.size(); i++) {
        Command* cmd = program.commands[i];
        printf("  Command: %s\n", getCommandName(cmd->type));
    }
}

