#ifndef SCRIPT_SERIAL_H
#define SCRIPT_SERIAL_H

#include <string>
#include "script/api.h"

std::string script_getSerialPortByDescription(std::string fragment);
int script_openSerial(std::string name, int baud);
bool script_selectSerial(int handle);
script_serialReply *script_sendSerial(std::string str, int timeoutMs, std::string pattern);

#endif // SCRIPT_SERIAL_H
