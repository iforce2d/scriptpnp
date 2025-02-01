#ifndef COMMANDS_H
#define COMMANDS_H

#include <string.h>

#include "scv/planner.h"

#define INVALID_FLOAT 0xFFFFFFFF

//#define NUM_ROTATION_AXES 4

#ifndef PACKED
#define PACKED __attribute__((__packed__))
#endif


#define COMMAND_TYPE_LIST \
    tmpMacro(CT_MOVETO)\
    tmpMacro(CT_DIGITAL_OUTPUT)\
    tmpMacro(CT_WAIT)\
    tmpMacro(CT_SET_MOVE_LIMITS)\
    tmpMacro(CT_SET_ROTATE_LIMITS)\
    tmpMacro(CT_PWM_OUTPUT)\
    tmpMacro(CT_ROTATETO)\
    tmpMacro(CT_SYNC)\
    tmpMacro(CT_SET_CORNER_BLEND_OVERLAP)\
    tmpMacro(CT_MAX)


#define tmpMacro(blah) blah,

enum commandType_e {
    CT_NONE = 0,
    COMMAND_TYPE_LIST
};

#undef tmpMacro

//enum rotateSyncType_e {
//    ST_SEQUENTIAL,
//    ST_CONCURRENT
//};

#define MOVE_FLAG_LESS_THAN     0x01
#define MOVE_FLAG_MORE_THAN     0x02
#define MOVE_FLAG_RELATIVE      0x04

typedef struct PACKED {
    float x;
    float y;
    float z;
    float w;
    int8_t flags_x;
    int8_t flags_y;
    int8_t flags_z;
    int8_t flags_w;
} pose;

typedef struct PACKED {
    float vel;
    float acc;
    float jerk;
} motionLimits;

//typedef struct PACKED {
//    float val[4];
//} pwmVal;

class Command {
public:
    uint16_t type;
    virtual ~Command() {}
    virtual int getSize() { return sizeof(type); }
    virtual int pack(uint8_t* data)   { memcpy(data, &type, sizeof(type)); return sizeof(type); }
    virtual int unpack(uint8_t* data) { memcpy(&type, data, sizeof(type)); return sizeof(type); }

    static Command* unpack(uint8_t* data, int& pos);
};

#define PACK_FIELD(x)   memcpy(&data[pos], &x, sizeof(x)); pos += sizeof(x);
#define UNPACK_FIELD(x) memcpy(&x, &data[pos], sizeof(x)); pos += sizeof(x);

class Command_setCornerBlendOverlap : public Command {
public:
    float overlap;

    Command_setCornerBlendOverlap() { type = CT_SET_CORNER_BLEND_OVERLAP; }

    int getSize() {
        return Command::getSize()
               + sizeof(overlap);
    }

    int pack(uint8_t* data) {
        int pos = Command::pack(data);
        PACK_FIELD(overlap);
        return pos;
    }

    int unpack(uint8_t* data) {
        int pos = Command::unpack(data);
        UNPACK_FIELD(overlap);
        return pos;
    }
};

class Command_moveTo : public Command {
public:
    pose dst;

    Command_moveTo() { type = CT_MOVETO; }

    int getSize() {
        return Command::getSize()
                + sizeof(dst);
    }

    int pack(uint8_t* data) {
        int pos = Command::pack(data);
        PACK_FIELD(dst);
        return pos;
    }

    int unpack(uint8_t* data) {
        int pos = Command::unpack(data);
        UNPACK_FIELD(dst);
        return pos;
    }
};

class Command_digitalOutput : public Command {
public:
    uint16_t bits;
    uint16_t changed;
    float delay;

    Command_digitalOutput() { type = CT_DIGITAL_OUTPUT; }

    int getSize() {
        return Command::getSize()
                + sizeof(bits)
                + sizeof(changed)
                + sizeof(delay);
    }

    int pack(uint8_t* data) {
        int pos = Command::pack(data);
        PACK_FIELD(bits);
        PACK_FIELD(changed);
        PACK_FIELD(delay);
        return pos;
    }

    int unpack(uint8_t* data) {
        int pos = Command::unpack(data);
        UNPACK_FIELD(bits);
        UNPACK_FIELD(changed);
        UNPACK_FIELD(delay);
        return pos;
    }
};

class Command_wait : public Command {
public:
    float duration;

    Command_wait() { type = CT_WAIT; }

    int getSize() {
        return Command::getSize()
                + sizeof(duration);
    }

    int pack(uint8_t* data) {
        int pos = Command::pack(data);
        PACK_FIELD(duration);
        return pos;
    }

    int unpack(uint8_t* data) {
        int pos = Command::unpack(data);
        UNPACK_FIELD(duration);
        return pos;
    }
};

class Command_setMoveLimits : public Command {
public:
    motionLimits limits;

    Command_setMoveLimits() { type = CT_SET_MOVE_LIMITS; }

    int getSize() {
        return Command::getSize()
                + sizeof(limits);
    }

    int pack(uint8_t* data) {
        int pos = Command::pack(data);
        PACK_FIELD(limits);
        return pos;
    }

    int unpack(uint8_t* data) {
        int pos = Command::unpack(data);
        UNPACK_FIELD(limits);
        return pos;
    }
};

class Command_setRotateLimits : public Command {
public:
    uint8_t axis;
    motionLimits limits;

    Command_setRotateLimits() { type = CT_SET_ROTATE_LIMITS; }

    int getSize() {
        return Command::getSize()
                + sizeof(axis)
                + sizeof(limits);
    }

    int pack(uint8_t* data) {
        int pos = Command::pack(data);
        PACK_FIELD(axis);
        PACK_FIELD(limits);
        return pos;
    }

    int unpack(uint8_t* data) {
        int pos = Command::unpack(data);
        UNPACK_FIELD(axis);
        UNPACK_FIELD(limits);
        return pos;
    }
};

class Command_setPWM : public Command {
public:
    float vals[4];
    float delay;

    Command_setPWM() { type = CT_PWM_OUTPUT; }

    int getSize() {
        return Command::getSize()
                + sizeof(vals)
                + sizeof(delay);
    }

    int pack(uint8_t* data) {
        int pos = Command::pack(data);
        PACK_FIELD(vals);
        PACK_FIELD(delay);
        return pos;
    }

    int unpack(uint8_t* data) {
        int pos = Command::unpack(data);
        UNPACK_FIELD(vals);
        UNPACK_FIELD(delay);
        return pos;
    }
};

class Command_rotateTo : public Command {
public:
    float dst[NUM_ROTATION_AXES];
    float delay;

    Command_rotateTo() { type = CT_ROTATETO; }

    int getSize() {
        return Command::getSize()
                + sizeof(dst)
                + sizeof(delay);
    }

    int pack(uint8_t* data) {
        int pos = Command::pack(data);
        PACK_FIELD(dst[0]);
        PACK_FIELD(dst[1]);
        PACK_FIELD(dst[2]);
        PACK_FIELD(dst[3]);
        PACK_FIELD(delay);
        return pos;
    }

    int unpack(uint8_t* data) {
        int pos = Command::unpack(data);
        UNPACK_FIELD(dst[0]);
        UNPACK_FIELD(dst[1]);
        UNPACK_FIELD(dst[2]);
        UNPACK_FIELD(dst[3]);
        UNPACK_FIELD(delay);
        return pos;
    }
};

class Command_sync : public Command {
public:

    Command_sync() { type = CT_SYNC; }

    int getSize() {
        return Command::getSize();
    }

    int pack(uint8_t* data) {
        int pos = Command::pack(data);
        return pos;
    }

    int unpack(uint8_t* data) {
        int pos = Command::unpack(data);
        return pos;
    }
};

Command* getCommandOfType(int t);
const char* getCommandName(uint8_t type);

#endif
