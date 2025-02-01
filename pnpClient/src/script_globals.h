#ifndef SCRIPT_GLOBALS_H
#define SCRIPT_GLOBALS_H

#include <string>

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

#endif // SCRIPT_GLOBALS_H
