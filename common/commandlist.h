#ifndef PROGRAM_H
#define PROGRAM_H

#include <vector>
#include "pnpMessages.h"
#include "commands.h"
#include "packable.h"

#include "scv/vec3.h"

class CommandList : public Packable {

public:
    cornerBlendMethod_e cornerBlendMethod;
    float cornerBlendMaxFraction;
    scv::vec3 posLimitLower;
    scv::vec3 posLimitUpper;
    scv::vec2 rotationPositionLimits[NUM_ROTATION_AXES];

    std::vector<Command*> commands;

    CommandList(cornerBlendMethod_e blendMethod);
    ~CommandList();
    int getSize();
    int pack(uint8_t* data);
    bool unpack(uint8_t* data);
    void clear();
    bool empty();

};

bool sanityCheckCommandList(CommandList& program);
void dumpCommandList(CommandList& program);

#endif
