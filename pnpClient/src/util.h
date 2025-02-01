
#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>

bool stringVecContains( std::vector<std::string> haystack, std::string needle );
int splitStringVec(std::vector<std::string>& v, std::string toSplit, char delimiter);
std::string joinStringVec(std::vector<std::string>& arr, std::string delimiter);

#endif
