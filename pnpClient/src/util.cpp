
#include "util.h"

#include <algorithm>
#include <sstream>
#include <math.h>

using namespace std;

bool stringVecContains( vector<string> haystack, string needle ) {
    return find( haystack.begin(), haystack.end(), needle ) != haystack.end();
}

int splitStringVec(vector<string>& v, string toSplit, char delimiter) {
    istringstream f(toSplit);
    string s;
    while (getline(f, s, delimiter)) {
        v.push_back(s);
    }
    return (int)v.size();
}

string joinStringVec(vector<string>& v, string delimiter)
{
    if (v.empty()) return "";

    string str;
    for (auto &i : v)
        str += i + delimiter;
    str = str.substr(0, str.size() - delimiter.size());
    return str;
}

int getExponentStringLength(float d) {
    d = fabs(d);
    if (d < 1)
        return 0;

    return (int)log10(d) + 1;
}

std::string fformat(float d, int precision)
{
    char buf[64];
    sprintf(buf, "%.*g", getExponentStringLength(d) + precision, d);
    return string(buf);
}


bool stringStartsWith(std::string str, std::string withWhat)
{
    return str.rfind(withWhat, 0) == 0;
}

string upperCaseInitial(std::string s)
{
    string str = string(s);
    str[0] = toupper(str[0]);
    return str;
}
