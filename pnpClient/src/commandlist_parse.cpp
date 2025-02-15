
#include <vector>
#include <map>
#include <string>
#include <algorithm>

#include "commandlist.h"
#include "pnpMessages.h"

#ifdef CLIENT
#include "commandEditorWindow.h"
#include "log.h"
#include "commandlist_parse.h"
#include "script/engine.h"
#include "scriptlog.h"

#include "scv/planner.h"

using namespace std;

//#define NUM_ROTATION_AXES 4

#define INVALID_FLOAT   0xFFFFFFFF


char parseWarningBuffer[2048];
char parseErrorBuffer[2048];
//const char* errorString = NULL;

struct programParseState_t {
    CodeEditorDocument* document;
    CommandList* program;
    int line;
    bool doingAnyRotation;
    //bool doingAsyncRotation[NUM_ROTATION_AXES];
    int lastAsyncRotationLine[NUM_ROTATION_AXES];
};

#define EAT_SPACE(x)        \
    while ( x[0] == ' ' )   \
        x.erase(x.begin());


bool parseFloat(string& s, float* f, uint8_t* flags) {

    EAT_SPACE(s);
    if ( s.empty() )
        return false;

    if ( s[0] == '<' ) {
        *flags = MOVE_FLAG_LESS_THAN;
        s.erase(s.begin());
        EAT_SPACE(s);
    }
    else if ( s[0] == '>' ) {
        *flags = MOVE_FLAG_MORE_THAN;
        s.erase(s.begin());
        EAT_SPACE(s);
    }
    else if ( s[0] == '@' ) {
        *flags = MOVE_FLAG_RELATIVE;
        s.erase(s.begin());
        EAT_SPACE(s);
    }

    int sign = 1;
    if ( s[0] == '-' ) {
        sign = -1;
        s.erase(s.begin());
        EAT_SPACE(s);
    }
    else if ( s[0] == '+' ) {
        s.erase(s.begin());
        EAT_SPACE(s);
    }

    float val = 0;
    bool haveNumber = false;
    float decimalScale = 0;
    while ( (s[0] >= '0' && s[0] <= '9') || s[0] == '.' ) {

        if ( s[0] == '.' ) {
            if ( decimalScale ) {
                return false;
            }
            decimalScale = 0.1;
            s.erase(s.begin());
        }
        else {
            int digit = s[0] - '0';
            if ( ! haveNumber ) {
                if ( decimalScale ) {
                    val = decimalScale * digit;
                    decimalScale *= 0.1;
                }
                else
                    val = digit;
                haveNumber = true;
            }
            else {
                if ( decimalScale ) {
                    val += decimalScale * digit;
                    decimalScale *= 0.1;
                }
                else {
                    val *= 10;
                    val += digit;
                }
            }
            s.erase(s.begin());
        }
    }
    *f = sign * val;
    return haveNumber;
}

bool parseInt(string& s, int* b) {

    EAT_SPACE(s);

    int sign = 1;
    if ( s[0] == '-' ) {
        sign = -1;
        s.erase(s.begin());
    }
    int val = 0;
    bool haveNumber = false;
    while ( (s[0] >= '0' && s[0] <= '9') ) {

        int digit = s[0] - '0';
        if ( ! haveNumber ) {
            val = digit;
            haveNumber = true;
        }
        else {
            val *= 10;
            val += digit;
        }
        s.erase(s.begin());
    }
    *b = sign * val;
    return haveNumber;
}

#define CHECK_FLOAT(s, f, bf, c)        \
    if ( s[0] == c ) {                  \
        s.erase(s.begin());             \
        if ( ! parseFloat(s, &f, &bf) ) \
            return false;               \
    }

struct fourFloats {
    float x, y, z, w;
    uint8_t fx, fy, fz, fw; // flags for relative, less than, more than
    fourFloats() {
        x = INVALID_FLOAT;
        y = INVALID_FLOAT;
        z = INVALID_FLOAT;
        w = INVALID_FLOAT;
        fx = fy = fz = fw = 0;
    }
};

bool parseFourFloats(string &params, fourFloats& f, const char* labels) {

    while ( ! params.empty() && ( f.x == INVALID_FLOAT ||
                                  f.y == INVALID_FLOAT ||
                                  f.z == INVALID_FLOAT ||
                                  f.w == INVALID_FLOAT) ) {

        EAT_SPACE(params);
        if ( params.empty() )
            break;

             CHECK_FLOAT(params,f.x,f.fx,labels[0])
        else CHECK_FLOAT(params,f.y,f.fy,labels[1])
        else CHECK_FLOAT(params,f.z,f.fz,labels[2])
        else CHECK_FLOAT(params,f.w,f.fw,labels[3])
        else {
            return false;
        }
    }

    if ( f.x == INVALID_FLOAT &&
         f.y == INVALID_FLOAT &&
         f.z == INVALID_FLOAT &&
         f.w == INVALID_FLOAT)
        return false;
    return true;
}

bool parseThreeFloats(string &params, fourFloats& f, const char* labels) {

    while ( ! params.empty() && ( f.x == INVALID_FLOAT ||
                                  f.y == INVALID_FLOAT ||
                                  f.z == INVALID_FLOAT) ) {

        EAT_SPACE(params);
        if ( params.empty() )
            break;

        uint8_t dummy = 0;
        (void)dummy; // silence 'unused' warning

        CHECK_FLOAT(params,f.x,dummy,labels[0])
        else CHECK_FLOAT(params,f.y,dummy,labels[1])
        else CHECK_FLOAT(params,f.z,dummy,labels[2])
        else {
            return false;
        }
    }

    if ( f.x == INVALID_FLOAT &&
         f.y == INVALID_FLOAT &&
         f.z == INVALID_FLOAT)
        return false;
    return true;
}

bool anyInvalid(float* f, int num) {
    for (int i = 0; i < num; i++) {
        if (f[i] == INVALID_FLOAT)
            return true;
    }
    return false;
}

bool allInvalid(float* f, int num) {
    for (int i = 0; i < num; i++) {
        if (f[i] != INVALID_FLOAT)
            return false;
    }
    return true;
}

bool anyTrue(int* b, int num) {
    for (int i = 0; i < num; i++) {
        if ( b[i] )
            return true;
    }
    return false;
}

bool parseNFloats(string &s, const char* labels, float* f, int numRequired) {

    int num = strlen(labels);

    uint8_t dummy = 0;
    (void)dummy; // silence 'unused' warning

    while ( ! s.empty() && anyInvalid(f,num) ) {

        EAT_SPACE(s);
        if ( s.empty() )
            break;

        bool foundValidLabel = false;

        for (int i = 0; i < num; i++) {
            if ( s[0] == labels[i] ) {
                foundValidLabel = true;
                s.erase(s.begin());
                if ( ! parseFloat(s, &f[i], &dummy) )
                    return false;
            }
        }

        if ( ! foundValidLabel )
            return false;
    }

    if ( allInvalid(f,numRequired) )
        return false;

    return true;
}

#define MOVE_OOB(axis)\
    if ( ff.axis < parseState.program->posLimitLower.axis ) {\
        sprintf(parseErrorBuffer, "move command %s is outside bounds (%f < %f)", #axis, ff.axis, parseState.program->posLimitLower.axis);\
        g_log.log(LL_ERROR, "%s", parseErrorBuffer);\
        sane = false;\
    }\
    if ( ff.axis > parseState.program->posLimitUpper.axis ) {\
        sprintf(parseErrorBuffer, "move command %s is outside bounds (%f > %f)", #axis, ff.axis, parseState.program->posLimitUpper.axis);\
        g_log.log(LL_ERROR, "%s", parseErrorBuffer);\
        sane = false;\
    }

Command* parse_move(programParseState_t& parseState, string params) {

    fourFloats ff;
    if ( ! parseFourFloats(params, ff, "xyzw") ) {
        sprintf(parseErrorBuffer, "valid parameters for 'move' are: x, y, z, w");
        return NULL;
    }

    //printf("Found m command: %f, %f, %f, %f\n", ff.x, ff.y, ff.z, ff.w);

    bool sane = true;
    if (ff.x != INVALID_FLOAT && ff.fx != MOVE_FLAG_RELATIVE ) {
        MOVE_OOB(x);
    }
    if (ff.y != INVALID_FLOAT && ff.fy != MOVE_FLAG_RELATIVE ) {
        MOVE_OOB(y);
    }
    if (ff.z != INVALID_FLOAT && ff.fz != MOVE_FLAG_RELATIVE ) {
        MOVE_OOB(z);
    }
    if ( ! sane )
        return NULL;

    Command_moveTo* cmd = new Command_moveTo();
    cmd->dst.x = ff.x;
    cmd->dst.y = ff.y;
    cmd->dst.z = ff.z;
    cmd->dst.w = ff.w;
    cmd->dst.flags_x = ff.fx;
    cmd->dst.flags_y = ff.fy;
    cmd->dst.flags_z = ff.fz;
    cmd->dst.flags_w = ff.fw;

    return cmd;
}

Command* parse_setmovelimits(programParseState_t& parseState, string params) {

    fourFloats ff;
    if ( ! parseThreeFloats(params, ff, "vaj") ) {
        sprintf(parseErrorBuffer, "valid parameters for 'set move limits' are: v, a, j");
        return NULL;
    }

    //printf("Found sml command: %f, %f, %f\n", ff.x, ff.y, ff.z);
    Command_setMoveLimits* cmd = new Command_setMoveLimits();
    cmd->limits.vel = ff.x;
    cmd->limits.acc = ff.y;
    cmd->limits.jerk = ff.z;

    return cmd;
}

Command* parse_setrotatelimits(programParseState_t& parseState, string params) {

    fourFloats ff;
    if ( ! parseThreeFloats(params, ff, "vaj") ) {
        sprintf(parseErrorBuffer, "valid parameters for 'set rotate limits' are: v, a, j");
        return NULL;
    }

    //printf("Found srl command: %f, %f, %f\n", ff.x, ff.y, ff.z);
    Command_setRotateLimits* cmd = new Command_setRotateLimits();
    cmd->axis = 0;
    cmd->limits.vel = ff.x;
    cmd->limits.acc = ff.y;
    cmd->limits.jerk = ff.z;

    return cmd;
}

Command* parse_digitalout(programParseState_t& parseState, string params) {

    uint16_t bits = 0;
    uint16_t changed = 0;
    float delay = 0;

    uint8_t dummy = 0;
    (void)dummy; // silence 'unused' warning

    while ( ! params.empty() ) {

        EAT_SPACE(params);
        if ( params.empty() )
            break;

        if ( params[0] == 't' ) {
            params.erase(params.begin());
            if ( ! parseFloat(params, &delay, &dummy) )
                return NULL;
            continue;
        }

        int b = 0;
        if ( ! parseInt(params, &b) || b == 0 || abs(b) > 16 )
            return NULL;
        int pos = abs(b) - 1;
        if ( b > 0 )
            bits |= (1 << pos);
        changed |= (1 << pos);
    }

    if ( ! changed )
        return NULL;

    //printf("Found d command: %d %d %f\n", bits, changed, delay);
    Command_digitalOutput* cmd = new Command_digitalOutput();
    cmd->bits = bits;
    cmd->changed = changed;
    cmd->delay = delay;

    return cmd;
}

Command* parse_wait(programParseState_t& parseState, string params) {

    EAT_SPACE(params);
    if ( params.empty() )
        return NULL;

    uint8_t dummy = 0;
    (void)dummy; // silence 'unused' warning

    float f = 0;
    if ( ! parseFloat(params, &f, &dummy)  )
        return NULL;

    if ( f < 0 ) {
        sprintf(parseErrorBuffer, "'wait' parameter should not be negative");
        return NULL;
    }

    //printf("Found w command: %f\n", f);
    Command_wait* cmd = new Command_wait();
    cmd->duration = f;

    return cmd;
}

Command* parse_cornerBlendOverlap(programParseState_t& parseState, string params) {

    EAT_SPACE(params);
    if ( params.empty() )
        return NULL;

    uint8_t dummy = 0;
    (void)dummy; // silence 'unused' warning

    float f = 0;
    if ( ! parseFloat(params, &f, &dummy)  )
        return NULL;

    if ( f < 0 || f > 1 ) {
        sprintf(parseErrorBuffer, "'set blend overlap' value should be between 0 and 1");
        return NULL;
    }

    //printf("Found cbo command: %f\n", f);
    Command_setCornerBlendOverlap* cmd = new Command_setCornerBlendOverlap();
    cmd->overlap = f;

    return cmd;
}

Command* parse_pwm(programParseState_t& parseState, string params) {

    float f[5];
    for (int i = 0; i < 4; i++)
        f[i] = INVALID_FLOAT;
    f[4] = 0; // delay default

    if ( ! parseNFloats(params, "abcdt", f, 4) ) {
        sprintf(parseErrorBuffer, "valid parameters for 'pwm output' are: a, b, c, d, t");
        return NULL;
    }

    for (int i = 0; i < NUM_PWM_VALS; i++) {
        if ( f[i] != INVALID_FLOAT && (f[i] < 0 || f[i] > 1) ) {
            sprintf(parseErrorBuffer, "'pwm output' value should be between 0 and 1");
            return NULL;
        }
    }

    //printf("Found pwm command: %f, %f, %f, %f, %f\n", f[0], f[1], f[2], f[3], f[4]);
    Command_setPWM* cmd = new Command_setPWM();
    for (int i = 0; i < 4; i++)
        cmd->vals[i] = f[i];

    cmd->delay = f[4];

    return cmd;
}

Command* parse_rotate(programParseState_t& parseState, string params) {

    float f[5];
    for (int i = 0; i < 4; i++)
        f[i] = INVALID_FLOAT;
    f[4] = 0; // delay default

    // this just means a rotation line existed. Note this to avoid marking the next 'sync' as 'not needed'
    parseState.doingAnyRotation = true;

    const char* letters = "abcdt";
    if ( ! parseNFloats(params, letters, f, NUM_ROTATION_AXES) ) {
        sprintf(parseErrorBuffer, "valid parameters for 'rotate' are: a, b, c, d, t");
        return NULL;
    }

    //printf("Found r command: %f, %f, %f, %f, %f\n", f[0], f[1], f[2], f[3], f[4]);

    for (int i = 0; i < NUM_ROTATION_AXES; i++) {

        if ( f[i] != INVALID_FLOAT  ) {

            int previousRotateCommandLineForThisAxis = parseState.lastAsyncRotationLine[i];

            parseState.lastAsyncRotationLine[i] = parseState.line;

            if ( previousRotateCommandLineForThisAxis ) {
                sprintf(parseErrorBuffer, "Multiple async rotation issued for axis %c.\nPrevious command is on line %d.\nUse 'sync' to reset rotation state first.", letters[i], previousRotateCommandLineForThisAxis);
                return NULL;
            }

            if ( f[i] < parseState.program->rotationPositionLimits[i].x ) {
                sprintf(parseErrorBuffer, "rotate command for axis %c is outside bounds (%f < %f)", letters[i], f[i], parseState.program->rotationPositionLimits[i].x);
                return NULL;
            }
            if ( f[i] > parseState.program->rotationPositionLimits[i].y ) {
                sprintf(parseErrorBuffer, "rotate command for axis %c is outside bounds (%f > %f)", letters[i], f[i], parseState.program->rotationPositionLimits[i].y);
                return NULL;
            }
        }
    }

    Command_rotateTo* cmd = new Command_rotateTo();
    for (int i = 0; i < NUM_ROTATION_AXES; i++)
        cmd->dst[i] = f[i];
    cmd->delay = f[4];

    return cmd;
}

Command* parse_sync(programParseState_t& parseState, string params) {

    //printf("Found sync command\n");

    if ( ! (parseState.doingAnyRotation || anyTrue(parseState.lastAsyncRotationLine, NUM_ROTATION_AXES)) ) {
        //sprintf(parseErrorBuffer, "sync command not needed");

        // do all the marker addition here and then reset buffer so it doesn't get detected as an error in the main loop
        sprintf(parseWarningBuffer, "sync command not needed");
        g_log.log(LL_WARN, "%s", parseWarningBuffer);
        //parseState.document->addErrorMarker(parseState.line, EMT_WARNING, parseErrorBuffer);
        //parseErrorBuffer[0] = 0;

        //return NULL;
    }

    parseState.doingAnyRotation = false;
    for (int i = 0; i < NUM_ROTATION_AXES; i++)
        parseState.lastAsyncRotationLine[i] = 0; // ie. none

    Command_sync* cmd = new Command_sync();
    return cmd;
}



bool getCommandString(string& s, const char* what) {
    int len = strlen(what);
    if ( (int)s.length() < len )
        return false;
    for (int i = 0; i < len; i++) {
        if ( s[i] != what[i] ) {
            return false;
        }
    }
    s.erase(0, len);    
    return true;
}

void wipeComments(vector<string> &lines) {
    for (int i = 0; i < (int)lines.size(); i++) {
        string& s = lines[i];
        int len = s.length();
        if ( len < 2 )
            continue;
        for ( int k = 1; k < len; k++ ) {
            if ( s[k-1] == '/' && s[k] == '/' ) {
                s.erase(k-1);
                break;
            }
        }
    }
}

typedef int (*dbRowCallback)(void*,int,char**,char**);
typedef Command* (*commandParseFunc)(programParseState_t&, string);

struct commandParseConfig {
    bool needParams;
    commandParseFunc parsefunc;
};

commandParseConfig makeCommandParseConfig(commandParseFunc f, bool n) {
    commandParseConfig config;
    config.needParams = n;
    config.parsefunc = f;
    return config;
}

map<string, commandParseConfig> parseMappings;

void setupCommandParseMappings() {    
    parseMappings["setblendoverlap"] =  makeCommandParseConfig(parse_cornerBlendOverlap, true);
    parseMappings["sbo"] =              makeCommandParseConfig(parse_cornerBlendOverlap, true);

    parseMappings["setmovelimit"] =     makeCommandParseConfig(parse_setmovelimits, true);
    parseMappings["sml"] =              makeCommandParseConfig(parse_setmovelimits, true);

    parseMappings["setrotatelimit"] =   makeCommandParseConfig(parse_setrotatelimits, true);
    parseMappings["srl"] =              makeCommandParseConfig(parse_setrotatelimits, true);

    parseMappings["move"] =             makeCommandParseConfig(parse_move, true);
    parseMappings["m"] =                makeCommandParseConfig(parse_move, true);

    parseMappings["digitalout"] =       makeCommandParseConfig(parse_digitalout, true);
    parseMappings["d"] =                makeCommandParseConfig(parse_digitalout, true);

    parseMappings["wait"] =             makeCommandParseConfig(parse_wait, true);
    parseMappings["w"] =                makeCommandParseConfig(parse_wait, true);

    parseMappings["pwm"] =              makeCommandParseConfig(parse_pwm, true);

    parseMappings["rotate"] =           makeCommandParseConfig(parse_rotate, true);
    parseMappings["r"] =                makeCommandParseConfig(parse_rotate, true);

    parseMappings["sync"] =             makeCommandParseConfig(parse_sync, false);
}

//bool parseCommandList(CodeEditorDocument* document, CommandList& program) {
bool parseCommandList(vector<string> &lines, CommandList& program)
{
    program.commands.clear();

    int numLines = (int)lines.size();// document->editor.GetTextLines(lines);
    if (numLines < 1) {
        g_log.log(LL_WARN, "Empty program");
        return false;
    }

    wipeComments(lines);
    programParseState_t parseState = {0};
    parseState.program = &program;

    bool allOk = true;

    for (int i = 0; i < numLines; i++) {

        parseState.line = i+1;

        string s = lines[i];

        EAT_SPACE(s);

        if ( s.empty() )
            continue;

        if ( s.size() > 1 && s[0] == '/' && s[1] == '/' )
            continue;

        parseWarningBuffer[0] = 0;
        parseErrorBuffer[0] = 0;

        string firstToken = s.substr(0, s.find(' '));

        auto it = parseMappings.find(firstToken);
        if ( it != parseMappings.end() ) {
            commandParseConfig &config = it->second;
            s = s.substr( firstToken.size() );
            EAT_SPACE(s);
            if ( config.needParams && s.empty() )
                sprintf(parseErrorBuffer, "Missing params for %s", firstToken.c_str());
            else {
                Command* cmd = config.parsefunc(parseState, s);
                if ( cmd )
                    program.commands.push_back(cmd);
                else {
                    if ( ! parseErrorBuffer[0] )
                        sprintf(parseErrorBuffer, "Invalid params for %s", firstToken.c_str());
                }
            }
        }
        else {
            sprintf(parseErrorBuffer, "Unrecognized command: %s", firstToken.c_str());
        }

        if ( parseWarningBuffer[0] ) {
            codeCompileErrorInfo info;
            info.fileType = CT_COMMAND_LIST;
            info.section = getActiveCommandListPath();
            info.row = i+1;
            info.type = LL_WARN;
            info.message = /*"Warning on line " + to_string(i+1) + ": " +*/ parseWarningBuffer;
            addCompileErrorInfo(info);

            ScriptLog* log = (ScriptLog*)getActiveScriptLog();
            if ( log ) {
                string errMsg = string(info.message);
                string clp = getActiveCommandListPath();
                if ( clp != "" )
                    errMsg = clp + ": " + errMsg;
                log->log(LL_SCRIPT_WARN, &info, 0, "%s", errMsg.c_str());
            }

            g_log.log(LL_WARN, "%s", info.message.c_str());
        }

        if ( parseErrorBuffer[0] ) {
            codeCompileErrorInfo info;
            info.fileType = CT_COMMAND_LIST;
            info.section = getActiveCommandListPath();
            info.row = i+1;
            info.type = LL_ERROR;
            info.message = /*"Error on line " + to_string(i+1) + ": " +*/ parseErrorBuffer;
            addCompileErrorInfo(info);

            ScriptLog* log = (ScriptLog*)getActiveScriptLog();
            if ( log ) {
                string errMsg = string(info.message);
                string clp = getActiveCommandListPath();
                if ( clp != "" )
                    errMsg = clp + ": " + errMsg;
                log->log(LL_SCRIPT_ERROR, &info, 0, "%s", errMsg.c_str());
            }

            g_log.log(LL_ERROR, "%s", info.message.c_str());
            allOk = false;
        }
    }
    fflush(stdout);

    if ( ! allOk )
        program.commands.clear();

    //document->showErrorMarkers();

    return ! program.commands.empty();
}
#endif // CLIENT







