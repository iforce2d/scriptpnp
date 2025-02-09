
#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>

#define UNUSED(_VAR) ((void)(_VAR))

#ifndef DEGTORAD
#define DEGTORAD 0.01745329252
#define RADTODEG 57.2957795131
#endif

bool stringVecContains( std::vector<std::string> haystack, std::string needle );
int splitStringVec(std::vector<std::string>& v, std::string toSplit, char delimiter);
std::string joinStringVec(std::vector<std::string>& arr, std::string delimiter);
std::string fformat(float d, int precision);

#endif
