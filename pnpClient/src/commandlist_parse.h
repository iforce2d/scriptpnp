#ifndef PROGRAM_PARSE_H
#define PROGRAM_PARSE_H

#include <vector>
#include "pnpMessages.h"
#include "commandlist.h"

/*struct commandListCompileErrorInfo
{
    //std::string section;
    int         row;
    logLevel_e  type;
    std::string message;
};*/

void setupCommandParseMappings();
bool parseCommandList(std::vector<std::string> &lines, CommandList& program);

#endif
