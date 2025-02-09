
#include <scriptstdstring/scriptstdstring.h>
#include <scriptarray/scriptarray.h>
#include <scriptdictionary/scriptdictionary.h>
#include <scriptmath/scriptmath.h>

#include <vector>
#include <algorithm>
#include <cassert>
#include <fstream>

#include "json/json.h"

#include "engine.h"
#include "api.h"
#include "log.h"
#include "scriptlog.h"
#include "script_tweak.h"
#include "script_vision.h"
#include "script_globals.h"
#include "script_serial.h"
#include "usbcamera.h"
#include "tableView.h"
#include "notify.h"
#include "util.h"

#include "workspace.h"
#include "script_probing.h"

using namespace std;

asIScriptEngine *engine = NULL;

void* activeScriptLog = NULL;
string activeCommandListPath = "";
bool activePreviewOnly = false;

void MessageCallback(const asSMessageInfo *msg, void *param)
{
    logLevel_e level = LL_ERROR;
    if( msg->type == asMSGTYPE_WARNING )
        level = LL_WARN;
    else if( msg->type == asMSGTYPE_INFORMATION )
        level = LL_INFO;
    g_log.log(level, "%s (%d, %d) : %s", msg->section, msg->row, msg->col, msg->message);

    codeCompileErrorInfo info;
    info.fileType = CT_SCRIPT;
    info.section = msg->section;
    info.row = msg->row;
    info.col = msg->col;
    info.type = msg->type == asMSGTYPE_INFORMATION ? LL_INFO : msg->type == asMSGTYPE_WARNING ? LL_WARN : LL_ERROR;
    info.message = msg->message;
    addCompileErrorInfo(info);
}

void setActiveScriptLog(void* sl) {
    activeScriptLog = sl;
}

void* getActiveScriptLog() {
    return activeScriptLog;
}

void removeActiveScriptLog(void* sl) {
    if ( activeScriptLog == sl )
        activeScriptLog = NULL;
}

void setActiveCommandListPath(string s) {
    activeCommandListPath= s;
}

string getActiveCommandListPath() {
    return activeCommandListPath;
}

void setActivePreviewOnly(bool b) {
    activePreviewOnly = b;
}

bool getActivePreviewOnly() {
    return activePreviewOnly;
}




void vec3_defaultConstructor(void *self)
{
    new(self) script_vec3();
}

void vec3_initConstructor(float x, float y, float z, script_vec3 *self)
{
    new(self) script_vec3(x,y,z);
}


float script_DEGTORAD = DEGTORAD;
float script_RADTODEG = RADTODEG;

bool setupScriptEngine()
{
    //g_log.log(LL_TRACE, "setupScriptEngine");

    if ( engine ) {
        g_log.log(LL_WARN, "setupScriptEngine called while engine already set up");
        return true;
    }

    engine = asCreateScriptEngine();

    int r = engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);
    if ( r < 0 ) {
        g_log.log(LL_ERROR, "SetMessageCallback failed");
        return false;
    }

    RegisterStdString(engine);
    RegisterScriptArray(engine, true);
    RegisterStdStringUtils(engine);
    RegisterScriptDictionary(engine);
    RegisterScriptMath(engine);

    // find contour type
    r = engine->RegisterGlobalProperty("const int FC_ALL", &script_FC_ALL);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int FC_ROW", &script_FC_ROW);
    assert( r >= 0 );

    // flip frame type
    r = engine->RegisterGlobalProperty("const int FF_VERT", &script_FF_VERT);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int FF_HORZ", &script_FF_HORZ);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int FF_BOTH", &script_FF_BOTH);
    assert( r >= 0 );

    r = engine->RegisterGlobalProperty("const int MM_NONE", &script_MM_NONE);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int MM_TRAJ", &script_MM_TRAJECTORY);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int MM_JOG",  &script_MM_JOG);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int MM_HOMING", &script_MM_HOMING);
    assert( r >= 0 );

    r = engine->RegisterGlobalProperty("const int NT_NONE", &script_NT_NONE);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int NT_SUCCESS", &script_NT_SUCCESS);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int NT_WARNING", &script_NT_WARNING);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int NT_ERROR", &script_NT_ERROR);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int NT_INFO", &script_NT_INFO);
    assert( r >= 0 );

    r = engine->RegisterGlobalProperty("const int VP_ALL", &script_VP_ALL);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int VP_HUE", &script_VP_HUE);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int VP_SAT", &script_VP_SAT);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int VP_VAR", &script_VP_VAR);
    assert( r >= 0 );

    r = engine->RegisterGlobalProperty("const int VP_RED", &script_VP_RED);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int VP_GRN", &script_VP_GRN);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int VP_BLU", &script_VP_BLU);
    assert( r >= 0 );

    r = engine->RegisterGlobalProperty("const float DEGTORAD", &script_DEGTORAD);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const float RADTODEG", &script_RADTODEG);
    assert( r >= 0 );


    r = engine->RegisterGlobalProperty("const int MR_NONE", &script_MR_NONE);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int MR_SUCCESS", &script_MR_SUCCESS);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int MR_FAIL_CONFIG", &script_MR_FAIL_CONFIG);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int MR_FAIL_NOT_HOMED", &script_MR_FAIL_NOT_HOMED);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int MR_FAIL_TIMED_OUT", &script_MR_FAIL_TIMED_OUT);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int MR_FAIL_NOT_TRIGGERED", &script_MR_FAIL_NOT_TRIGGERED);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int MR_FAIL_OUTSIDE_BOUNDS", &script_MR_FAIL_OUTSIDE_BOUNDS);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int MR_FAIL_FOLLOWING_ERROR", &script_MR_FAIL_FOLLOWING_ERROR);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int MR_FAIL_LIMIT_ALREADY_TRIGGERED", &script_MR_FAIL_LIMIT_TRIGGERED);
    assert( r >= 0 );


    r = engine->RegisterGlobalProperty("const int PT_DIGITAL", &script_PT_DIGITAL);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int PT_LOADCELL", &script_PT_LOADCELL);
    assert( r >= 0 );
    r = engine->RegisterGlobalProperty("const int PT_VACUUM", &script_PT_VACUUM);
    assert( r >= 0 );


    r = engine->RegisterGlobalFunction("void print(string s)", asFUNCTION(script_print), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void print(string[] &arr)", asFUNCTION(script_printarray_string), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterObjectType("dbRow", 0, asOBJ_REF);
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour("dbRow", asBEHAVE_ADDREF, "void f()", asMETHOD(dbRow,IncRef), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour("dbRow", asBEHAVE_RELEASE, "void f()", asMETHOD(dbRow,DecRef), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour("dbRow", asBEHAVE_FACTORY, "dbRow@ f()", asFUNCTION(script_dbRowFactory), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("dbRow", "uint get_numCols() property", asMETHOD(dbRow,get_numCols), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("dbRow", "string str()", asMETHOD(dbRow,dump), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("dbRow", "string col(uint columnIndex)", asMETHOD(dbRow,col_int), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("dbRow", "string col(string columnName)", asMETHOD(dbRow,col_string), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("dbRow", "dbRow@ opAssign(dbRow@)", asMETHOD(dbRow,opAssign), asCALL_THISCALL);
    assert( r >= 0 );

    r = engine->RegisterObjectType("dbResult", 0, asOBJ_REF);
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour("dbResult", asBEHAVE_ADDREF, "void f()", asMETHOD(dbResult,IncRef), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour("dbResult", asBEHAVE_RELEASE, "void f()", asMETHOD(dbResult,DecRef), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour("dbResult", asBEHAVE_FACTORY, "dbResult@ f()", asFUNCTION(script_dbResultFactory), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("dbResult", "bool get_status() property", asMETHOD(dbResult,get_status), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("dbResult", "uint get_numRows() property", asMETHOD(dbResult,get_numRows), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("dbResult", "uint get_numCols() property", asMETHOD(dbResult,get_numCols), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("dbResult", "string str()", asMETHOD(dbResult,dump), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("dbResult", "string colName(uint columnIndex)", asMETHOD(dbResult,columnName), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("dbResult", "dbRow@ row(uint rowIndex)", asMETHOD(dbResult,row), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("dbResult", "dbResult@ opAssign(dbResult@)", asMETHOD(dbResult,opAssign), asCALL_THISCALL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void print(dbResult@ result)", asFUNCTION(script_print_dbResult), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void print(dbRow@ row)", asFUNCTION(script_print_dbRow), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("string str(string[] &arr)", asFUNCTION(script_strArray), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("string str(dictionary &dict)", asFUNCTION(script_strDictionary), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("dbResult@ dbQuery(string sql)", asFUNCTION(script_dbQuery), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("bool runCommand(string cmd)", asFUNCTION(script_runCommand), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("bool runCommandList(string filename)", asFUNCTION(script_runCommandList), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("bool runCommandList(string filename, const dictionary &in substitutions)", asFUNCTION(script_runCommandList_dict), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("bool setUSBCameraParams(int index, int zoom, int focus, int exposure, int whiteBalance, int saturation)", asFUNCTION(script_setUSBCameraParams), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("float getTweakValue(string tweakKey)", asFUNCTION(script_getTweakValue), asCALL_CDECL);
    assert( r >= 0 );



    r = engine->RegisterGlobalFunction("void rgbCopy()", asFUNCTION(script_copyRGB), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void rgbOverlay(float opacity = 1)", asFUNCTION(script_overlayRGB), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void rgb2gray()", asFUNCTION(script_RGB2BGR), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void rgb2hsv(float planeForVisual = VP_ALL)", asFUNCTION(script_RGB2HSV_F), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void rgb2rgb(float planeForVisual = VP_RED)", asFUNCTION(script_RGB2RGB_F), asCALL_CDECL);
    assert( r >= 0 );


    r = engine->RegisterObjectType("blob", sizeof(script_blob), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS );
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("blob", "float ax", offsetof(script_blob,ax));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("blob", "float ay", offsetof(script_blob,ay));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("blob", "float cx", offsetof(script_blob,cx));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("blob", "float cy", offsetof(script_blob,cy));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("blob", "int w", offsetof(script_blob,w));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("blob", "int h", offsetof(script_blob,h));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("blob", "int lx", offsetof(script_blob,bb_x1));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("blob", "int ux", offsetof(script_blob,bb_x2));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("blob", "int ly", offsetof(script_blob,bb_y1));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("blob", "int uy", offsetof(script_blob,bb_y2));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("blob", "int pixelCount", offsetof(script_blob,pixels));
    assert( r >= 0 );


    r = engine->RegisterObjectType("rect", sizeof(script_rotatedRect), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_ALLFLOATS );
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("rect", "float x", offsetof(script_rotatedRect,x));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("rect", "float y", offsetof(script_rotatedRect,y));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("rect", "float w", offsetof(script_rotatedRect,w));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("rect", "float h", offsetof(script_rotatedRect,h));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("rect", "float angle", offsetof(script_rotatedRect,angle));
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("int getUSBCameraIndexByHash(string fragment)", asFUNCTION(script_getUSBCameraIndexByHash), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("bool grabFrame(int cameraIndex = 0)", asFUNCTION(script_grabFrame), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("bool saveImage(string filename)", asFUNCTION(script_saveImage), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("bool loadImage(string filename)", asFUNCTION(script_loadImage), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void setVisionWindowSize(float size)", asFUNCTION(script_setVisionWindowSizeF), asCALL_CDECL);
    assert( r >= 0 );
    //r = engine->RegisterGlobalFunction("blob[]@ quickblob()", asFUNCTION(script_quickblob_default), asCALL_CDECL);
    //assert( r >= 0 );
    r = engine->RegisterGlobalFunction("blob[]@ quickblob(int targetColor = -1, int minpixels = -1, int maxpixels = -1, int minwidth = -1, int maxwidth = -1)", asFUNCTION(script_quickblob), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void blur(int kernelSize)", asFUNCTION(script_blur), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void rgbThreshold(int minRed, int maxRed, int minGreen, int maxGreen, int minBlue, int maxBlue)", asFUNCTION(script_rgbThreshold), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void findContour(int method = FC_ALL)", asFUNCTION(script_findContour), asCALL_CDECL);
    assert( r >= 0 );
    //r = engine->RegisterGlobalFunction("rect& minAreaRect()", asFUNCTION(script_minAreaRect_default), asCALL_CDECL);
    //assert( r >= 0 );
    r = engine->RegisterGlobalFunction("rect& minAreaRect(float windowMinX = -1, float windowMaxX = -1, float windowMinY = -1, float windowMaxY = -1)", asFUNCTION(script_minAreaRectF), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void convexHull(bool drawLines = false)", asFUNCTION(script_convexHull), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void flipFrame(int direction = FF_BOTH)", asFUNCTION(script_flipFrame), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("float[]@ findCircles(float diameter)", asFUNCTION(script_findCircles), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void hsvThreshold(float hueCenter, float hueRange, float minSat, float maxSat, float minVar, float maxVar)", asFUNCTION(script_hsvThresholdF), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void blur(float kernelSize)", asFUNCTION(script_blurF), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void setDrawColor(int red, int green, int blue)", asFUNCTION(script_setDrawColor), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void drawWindow()", asFUNCTION(script_drawWindow), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void drawRect(float minX, float maxX, float minY, float maxY)", asFUNCTION(script_drawRectF), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void drawLine(float x1, float x2, float y1, float y2)", asFUNCTION(script_drawLineF), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void drawCross(float x, float y, float size, float angleDegrees)", asFUNCTION(script_drawCrossF), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void drawCircle(float x, float y, float radius)", asFUNCTION(script_drawCircleF), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void drawRect(rect &in r)", asFUNCTION(script_drawRotatedRect), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void loadWorkspaceLayout(string layoutName)", asFUNCTION(requestWorkspaceInfoLoad), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void setMemoryNumber(string name, float val)", asFUNCTION(script_setMemoryValue), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("float getMemoryNumber(string name)", asFUNCTION(script_getMemoryValue), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("bool haveMemoryNumber(string name)", asFUNCTION(script_haveMemoryValue), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void setMemoryString(string name, string val)", asFUNCTION(script_setMemoryString), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("string getMemoryString(string name)", asFUNCTION(script_getMemoryString), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("bool haveMemoryString(string name)", asFUNCTION(script_haveMemoryString), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void setDBNumber(string name, float val)", asFUNCTION(script_setDBValue), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("float getDBNumber(string name)", asFUNCTION(script_getDBValue), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("bool haveDBNumber(string name)", asFUNCTION(script_haveDBValue), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void setDBString(string name, string val)", asFUNCTION(script_setDBString), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("string getDBString(string name)", asFUNCTION(script_getDBString), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("bool haveDBString(string name)", asFUNCTION(script_haveDBString), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void wait(int milliseconds)", asFUNCTION(script_wait), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("bool isPreview()", asFUNCTION(script_isPreview), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("uint64 millis()", asFUNCTION(script_millis), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("int getMachineMode()", asFUNCTION(script_getMachineMode), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("int getTrajectoryResult()", asFUNCTION(script_getTrajectoryResult), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("int getHomingResult()", asFUNCTION(script_getHomingResult), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void setDigitalOut(int whichZeroIndexed, int toWhat)", asFUNCTION(script_setDigitalOut), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void setPWMOut(float valZeroToOne)", asFUNCTION(script_setPWMOut), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("bool getDigitalIn(int whichZeroIndexed)", asFUNCTION(script_getDigitalIn), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("bool getDigitalOut(int whichZeroIndexed)", asFUNCTION(script_getDigitalOut), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("float getVacuum()", asFUNCTION(script_getVacuum), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("float getADC(int whichZeroIndexed)", asFUNCTION(script_getADC), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("int getEncoder()", asFUNCTION(script_getEncoder), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("int getLoadcell()", asFUNCTION(script_getLoadcell), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("float getWeight()", asFUNCTION(script_getWeight), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterObjectType("vec3", sizeof(script_vec3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS | asOBJ_APP_CLASS_ALLFLOATS  );
    //r = engine->RegisterObjectType("vec3", sizeof(script_vec3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CA | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<script_vec3>() );
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("vec3", "float x", offsetof(script_vec3,x));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("vec3", "float y", offsetof(script_vec3,y));
    assert( r >= 0 );
    r = engine->RegisterObjectProperty("vec3", "float z", offsetof(script_vec3,z));
    assert( r >= 0 );    
    r = engine->RegisterObjectBehaviour("vec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(vec3_defaultConstructor), asCALL_CDECL_OBJLAST);
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour("vec3", asBEHAVE_CONSTRUCT, "void f(float x, float y = 0, float z = 0)", asFUNCTION(vec3_initConstructor), asCALL_CDECL_OBJLAST);
    assert( r >= 0 );

    r = engine->RegisterObjectMethod("vec3", "vec3 opAdd(const vec3 &in) const", asFUNCTION(vec3_opAdd_vec3), asCALL_GENERIC);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "vec3 opSub(const vec3 &in) const", asFUNCTION(vec3_opSub_vec3), asCALL_GENERIC);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "vec3 opAddAssign(const vec3 &in) const", asFUNCTION(vec3_opAddAssign_vec3), asCALL_GENERIC);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "vec3 opSubAssign(const vec3 &in) const", asFUNCTION(vec3_opSubAssign_vec3), asCALL_GENERIC);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "vec3 opMul(const float) const", asFUNCTION(vec3_opMul_float), asCALL_GENERIC);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "vec3 opDiv(const float) const", asFUNCTION(vec3_opDiv_float), asCALL_GENERIC);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "vec3 opMulAssign(const float) const", asFUNCTION(vec3_opMulAssign_float), asCALL_GENERIC);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "vec3 opDivAssign(const float) const", asFUNCTION(vec3_opDivAssign_float), asCALL_GENERIC);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "vec3 opNeg() const", asFUNCTION(vec3_opNeg), asCALL_GENERIC);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "vec3 set(float x = 0, float y = 0, float z = 0)", asMETHOD(script_vec3,set_floatfloatfloat), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "float length()", asMETHOD(script_vec3,length), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "float normalize()", asMETHOD(script_vec3,normalize), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "vec3 normalized()", asMETHOD(script_vec3,normalized), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "float distanceTo(vec3 other)", asMETHOD(script_vec3,distTo), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "float distanceToXY(vec3 other)", asMETHOD(script_vec3,distToXY), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("vec3", "vec3 rotatedBy(float angleDegrees)", asMETHOD(script_vec3,rotatedBy), asCALL_THISCALL);
    assert( r >= 0 );




    r = engine->RegisterGlobalFunction("vec3 getActualPos()", asFUNCTION(script_getActualPos), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("float getActualRot()", asFUNCTION(script_getActualRot), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void print(bool)", asFUNCTION(script_print_bool), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void print(int)", asFUNCTION(script_print_int), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void print(float)", asFUNCTION(script_print_float), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("string str(float)", asFUNCTION(script_str_float), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("string str(vec3 &in)", asFUNCTION(script_str_vec3), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void print(vec3 &in)", asFUNCTION(script_print_vec3), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("string getSerialPortByDescription(string fragment)", asFUNCTION(script_getSerialPortByDescription), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("int openSerial(string port, int baud = 115200)", asFUNCTION(script_openSerial), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("bool selectSerial(int portHandle)", asFUNCTION(script_selectSerial), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterObjectType("serialReply", sizeof(script_serialReply), asOBJ_REF );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour("serialReply", asBEHAVE_ADDREF, "void f()", asMETHOD(script_serialReply,IncRef), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour("serialReply", asBEHAVE_RELEASE, "void f()", asMETHOD(script_serialReply,DecRef), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour("serialReply", asBEHAVE_FACTORY, "serialReply@ f()", asFUNCTION(script_serialReplyFactory), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("serialReply", "serialReply@ opAssign(serialReply@ r)", asMETHOD(script_serialReply,opAssign), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("serialReply", "bool get_ok() property", asMETHOD(script_serialReply,get_ok), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("serialReply", "float[]@ get_vals() property", asMETHOD(script_serialReply,get_vals), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("serialReply", "string get_str() property", asMETHOD(script_serialReply,get_str), asCALL_THISCALL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("serialReply@ sendSerial(string str, int timeoutMillis = 0, string regexp = '')", asFUNCTION(script_sendSerial), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void print(serialReply@ r)", asFUNCTION(script_print_serialReply), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void exit()", asFUNCTION(script_exit), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void notify(string msg, int type = NT_INFO, int timeoutMillis = 5000)", asFUNCTION(notify), asCALL_CDECL);
    assert( r >= 0 );


    r = engine->RegisterGlobalFunction("void refreshTableView(string tablename)", asFUNCTION(script_refreshTableView), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("int getProbeResult()", asFUNCTION(script_getProbeResult), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("float getProbedHeight()", asFUNCTION(script_getProbedHeight), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void probe(float depth, int type, float minForce = 0)", asFUNCTION(script_probe), asCALL_CDECL);
    assert( r >= 0 );


    r = engine->RegisterObjectType("qrcode", sizeof(script_qrcode), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("qrcode", "string getValue()", asMETHOD(script_qrcode,getValue), asCALL_THISCALL);
    assert( r >= 0 );
    r = engine->RegisterObjectMethod("qrcode", "vec3 getCenter()", asMETHOD(script_qrcode,getCenter), asCALL_THISCALL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("qrcode[]@ findQRCodes(int howMany = 1)", asFUNCTION(script_findQRCodes), asCALL_CDECL);
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction("void drawQRCode(qrcode &in c, float fontSize = 20)", asFUNCTION(script_drawQRCode), asCALL_CDECL);
    assert( r >= 0 );

    r = engine->RegisterGlobalFunction("void drawText(string msg, float x, float y, float fontSize = 20)", asFUNCTION(script_drawText), asCALL_CDECL);
    assert( r >= 0 );




    return true;
}

void cleanupScriptEngine()
{
    if ( ! engine ) {
        g_log.log(LL_WARN, "cleanupScriptEngine called with no engine set up");
        return;
    }

    engine->ShutDownAndRelease();
    engine = NULL;
}

asITypeInfo* GetScriptTypeIdByDecl(const char* decl)
{
    if ( ! engine ) {
        g_log.log(LL_ERROR, "GetScriptTypeIdByDecl called with no engine set up");
        return NULL;
    }

    return engine->GetTypeInfoByDecl(decl);
}

asIScriptModule* createScriptModule(string moduleName)
{
    if ( ! engine ) {
        g_log.log(LL_ERROR, "createScriptModule failed: engine is null");
        return NULL;
    }

    asIScriptModule* mod = engine->GetModule(moduleName.c_str(), asGM_CREATE_IF_NOT_EXISTS);
    if( ! mod )
    {
        g_log.log(LL_ERROR, "GetModule failed in createModule");
        return NULL;
    }

    return mod;
}

void discardScriptModule(asIScriptModule * mod)
{
    g_log.log(LL_ERROR, "discardScriptModule");

    if ( ! mod ) {
        g_log.log(LL_ERROR, "discardScriptModule failed: module is null");
        return;
    }

    if ( ! engine ) {
        g_log.log(LL_ERROR, "discardScriptModule failed: engine is null");
        return;
    }

    engine->DiscardModule( mod->GetName() );
}

bool readScriptFile(string file, string &script)
{
    FILE *f = fopen(file.c_str(), "rb");

    if ( ! f ) {
        g_log.log(LL_ERROR, "Failed to read file %s", file.c_str());
        return false;
    }

    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    fseek(f, 0, SEEK_SET);

    script.resize(len);
    fread(&script[0], len, 1, f);

    fclose(f);

    return true;
}

bool addScriptSectionFromFile(string moduleName, string sectionName, string file)
{
    asIScriptModule *mod = engine->GetModule( moduleName.c_str() );
    if ( ! mod ) {
        g_log.log(LL_ERROR, "GetModule failed in addScriptSectionFromFile");
        return false;
    }

    string code;
    if ( ! readScriptFile(file, code) ) {
        g_log.log(LL_ERROR, "addScriptSectionFromFile failed to read file %s", file.c_str());
        return false;
    }

    int r = mod->AddScriptSection( sectionName.c_str(), code.c_str(), code.size(), 0 );
    if( r < 0 )
    {
        g_log.log(LL_ERROR, "addScriptSectionFromFile failed");
        return false;
    }

    return true;
}

bool addScriptSection(std::string moduleName, std::string sectionName, std::string code)
{
    asIScriptModule *mod = engine->GetModule( moduleName.c_str() );
    if ( ! mod ) {
        g_log.log(LL_ERROR, "GetModule failed in addScriptSection");
        return false;
    }

    int r = mod->AddScriptSection( sectionName.c_str(), code.c_str(), code.size(), 0 );
    if( r < 0 )
    {
        g_log.log(LL_ERROR, "addScriptSection failed");
        return false;
    }

    return true;
}

bool buildScriptModule(asIScriptModule* mod)
{
    // asIScriptModule *mod = engine->GetModule( moduleName.c_str() );
    // if ( ! mod ) {
    //     g_log.log(LL_ERROR, "GetModule failed in buildModule");
    //     return false;
    // }

    int r = mod->Build();
    if ( r < 0 )
    {
        g_log.log(LL_ERROR, "Module build failed in buildScriptModule");
        return false;
    }

    return true;
}

asIScriptContext *currentScriptContext = NULL;

asIScriptContext *createScriptContext(asIScriptFunction *func)
{
    asIScriptContext *ctx = engine->CreateContext();
    ctx->Prepare(func);
    return ctx;
}

bool isScriptPaused = false;

void setIsScriptPaused(bool tf) {
    isScriptPaused = tf;
}

bool currentlyPausingScript()
{
    return isScriptPaused;
}

// return true if still running (ie. suspended and can be resumed)
bool executeScriptContext(asIScriptContext *ctx)
{
    currentScriptContext = ctx;

    isScriptPaused = false;

    int r = ctx->Execute();

    if ( r != asEXECUTION_FINISHED )
    {
        if ( r == asEXECUTION_EXCEPTION ) {
            g_log.log(LL_ERROR, "Exception occurred while executing script: %s", ctx->GetExceptionString());
            currentScriptContext = NULL;
            cleanupScriptContext(ctx);
            return false;
        }
        else if ( r == asEXECUTION_ABORTED ) {
            g_log.log(LL_INFO, "Aborted while executing script");
            currentScriptContext = NULL;
            cleanupScriptContext(ctx);
            return false;
        }
        else if ( r == asEXECUTION_SUSPENDED ) {
            g_log.log(LL_INFO, "Suspended while executing script");
            isScriptPaused = true;
            // leave context as is !
            return true; // still running
        }
        return false;
    }

    //cleanupScriptContext(ctx);

    return false;
}

asIScriptContext *getCurrentScriptContext()
{
    return currentScriptContext;
}

void cleanupScriptContext(asIScriptContext* ctx)
{
    g_log.log(LL_ERROR, "cleanupScriptContext");

    if ( ctx )
        ctx->Release();
    currentScriptContext = NULL;
}



int getScopeFunctions(string scope, vector<string>& list)
{
    if ( scope == "global" ) {
        int numGlobalFunctions = engine->GetGlobalFunctionCount();
        for (int i = 0; i < numGlobalFunctions; i++) {
            asIScriptFunction* function = engine->GetGlobalFunctionByIndex(i);
            list.push_back( function->GetDeclaration(false,false,true) );
        }
    }
    else {
        int numObjectTypes = engine->GetObjectTypeCount();
        for (int t = 0; t < numObjectTypes; t++) {
            asITypeInfo* objectType = engine->GetObjectTypeByIndex(t);
            if ( scope != objectType->GetName() )
                continue;
            int numProperties = objectType->GetPropertyCount();
            for (int i = 0; i < numProperties; i++) {
                list.push_back( objectType->GetPropertyDeclaration(i) );
            }
            int numMethods = objectType->GetMethodCount();
            for (int i = 0; i < numMethods; i++) {
                asIScriptFunction* function = objectType->GetMethodByIndex(i);
                list.push_back( function->GetDeclaration(false,false,true) );
            }
        }
    }
    return list.size();
}

bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}

void replaceAll(std::string& str, const std::string& from, const std::string& to) {
    if(from.empty())
        return;
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

class ScriptFunctionHelpEntry {
public:
    string decl;
    string help;
public:
    ScriptFunctionHelpEntry(string d, string h) { decl = d; help = h; }
};

map<string, ScriptFunctionHelpEntry*> scriptFunctionHelpEntries;

ScriptFunctionHelpEntry* getFunctionHelpEntry(string scope, string functionDecl)
{
    string key = scope + "::" + functionDecl;
    if ( scriptFunctionHelpEntries.count( key ) )
        return scriptFunctionHelpEntries[key];
    return NULL;
}

#define MAKESFHE(thescope, thedecl, thehelp) {\
    ScriptFunctionHelpEntry* sfhe = new ScriptFunctionHelpEntry( thedecl, thehelp );\
    scriptFunctionHelpEntries[ string(thescope) + "::" + thedecl ] = sfhe;\
}

void generateScriptDocs()
{
    /////////////////////////////////////////////
    //#include "scriptHelpDefs.h"
    /////////////////////////////////////////////

    vector<string> scopes;
    scopes.push_back("global");
    scopes.push_back("vec3");
    scopes.push_back("dbResult");
    scopes.push_back("dbRow");
    scopes.push_back("blob");
    scopes.push_back("rect");
    scopes.push_back("serialReply");
    scopes.push_back("qrcode");

    vector<string> ignoredDecls;

    ignoredDecls.push_back("vec3 opAdd(vec3) const");
    ignoredDecls.push_back("vec3 opSub(vec3) const");
    ignoredDecls.push_back("vec3 opAddAssign(vec3) const");
    ignoredDecls.push_back("vec3 opSubAssign(vec3) const");
    ignoredDecls.push_back("vec3 opMul(float) const");
    ignoredDecls.push_back("vec3 opDiv(float) const");
    ignoredDecls.push_back("vec3 opMulAssign(float) const");
    ignoredDecls.push_back("vec3 opDivAssign(float) const");
    ignoredDecls.push_back("vec3 opMul_r(float) const");
    ignoredDecls.push_back("vec3 opNeg() const");

    ignoredDecls.push_back("dbResult@ opAssign(dbResult@)");

    string stringsFile = "helpStrings.json";

    Json::Value helpTreeValue;
    Json::Value helpStringsValue;

    std::ifstream ifs;
    ifs.open(stringsFile, std::ios::in);
    if ( ifs ) {
        Json::Reader reader;
        if ( ! reader.parse(ifs, helpStringsValue) )
        {
            ifs.close();
            g_log.log(LL_ERROR, "Failed to parse JSON from file '%s' : %s", stringsFile.c_str(), reader.getFormatedErrorMessages().c_str() );
            return;
        }
        ifs.close();
    }

    for (string scope : scopes) {
        // printf("\n//--------------------\n");
        // printf("//    %s\n", scope.c_str());
        // printf("//--------------------\n\n");

        Json::Value scopeValue;

        vector<string> functions;
        getScopeFunctions(scope, functions);
        for (string functionDecl : functions) {

            Json::Value functionValue;

            string simplifiedDecl = functionDecl;
            replaceAll(simplifiedDecl, "&inout","");
            replaceAll(simplifiedDecl, "&out","");
            replaceAll(simplifiedDecl, "&in","");
            replaceAll(simplifiedDecl, "[]&","[]");
            replaceAll(simplifiedDecl, "&","");
            replaceAll(simplifiedDecl, "@","");
            replaceAll(simplifiedDecl, "const ","");

            functionValue[ "simp" ] = simplifiedDecl;

            string stringsKey = scope + "::" + functionDecl;
            if ( ! helpStringsValue.isMember(stringsKey) ) {
                g_log.log(LL_WARN, "Adding help string key: %s", stringsKey.c_str());

                string localizedValue = "xxxxxxxxxxxxxxxxxxxxxx";
                //Json::Value localizedValue;
                //localizedValue["desc"] = "xxxxxxxxxxxxxxxxxxxxxx";

                helpStringsValue[ stringsKey ] = localizedValue;
            }

            ScriptFunctionHelpEntry* sfhe = getFunctionHelpEntry(scope, simplifiedDecl);
            if ( !sfhe ) {
                if ( find(ignoredDecls.begin(), ignoredDecls.end(), simplifiedDecl ) == ignoredDecls.end() )
                    scopeValue[ functionDecl ] = functionValue;
            }
        }

        helpTreeValue[ scope ] = scopeValue;
    }

    std::ofstream ofs;
    ofs.open("helpTree.json", std::ios::out);
    if ( ofs ) {
        Json::StyledStreamWriter writer("  ");
        writer.write( ofs, helpTreeValue );
        ofs.close();
    }

    std::ofstream ofs2;
    ofs2.open(stringsFile, std::ios::out);
    if ( ofs2 ) {
        Json::StyledStreamWriter writer("  ");
        writer.write( ofs2, helpStringsValue );
        ofs2.close();
    }
}











































