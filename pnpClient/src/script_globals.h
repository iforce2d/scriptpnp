#ifndef SCRIPT_GLOBALS_H
#define SCRIPT_GLOBALS_H

#include <string>
#include "script/api.h"

#define DBSTRING_AUTOGEN_SCRIPT_SELECTION   "internal_autogenScriptTables"
#define DBSTRING_HIDE_TABLE_NAMES           "internal_hideInMainViewTableNames"
#define DBSTRING_USB_CAMERA_FUNCTIONS       "internal_usbCameraFunctions"
#define DBSTRING_TABLE_BUTTON_FUNCTIONS     "internal_dbTableButtonFunctions"

void script_setMemoryValue(std::string name, float v);
float script_getMemoryValue(std::string name);
bool script_haveMemoryValue(std::string name);

void script_setMemoryString(std::string name, std::string v);
std::string script_getMemoryString(std::string name);
bool script_haveMemoryString(std::string name);

void script_setDBValue(std::string name, float v);
float script_getDBValue(std::string name);
bool script_haveDBValue(std::string name);

void script_setDBString(std::string name, std::string v);
std::string script_getDBString(std::string name);
bool script_haveDBString(std::string name);

void script_setMemoryVec3(std::string name, script_vec3& v);
script_vec3 script_getMemoryVec3(std::string name);
bool script_haveMemoryVec3(std::string name);

void script_setMemoryAffine(std::string name, script_affine& v);
script_affine script_getMemoryAffine(std::string name);
bool script_haveMemoryAffine(std::string name);

#endif // SCRIPT_GLOBALS_H
