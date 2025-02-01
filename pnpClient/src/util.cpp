
#include "util.h"

#include <algorithm>
#include <sstream>

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
