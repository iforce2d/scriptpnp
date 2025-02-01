
#include <pthread.h>
#include <map>
#include <chrono>
#include <regex>
#include <scriptarray/scriptarray.h>
#include "script_serial.h"
#include "serialPortInfo.h"
#include "script/engine.h"
#include "scriptlog.h"

using namespace std;

extern pthread_t mainThreadId;

unsigned long nextHandle = 1;
sp_port* selectedSerial = NULL;

map<int, sp_port*> handleToPortNameMap;

string script_getSerialPortByDescription(string fragment) {
    vector<serialPortInfo*> ports = getSerialPortInfos();

    for (serialPortInfo* p : ports) {
        if ( p->description.find(fragment) != string::npos ) {
            return p->name;
        }
    }

    g_log.log(LL_ERROR, "getSerialPortByDescription failed: no port matching description %s", fragment.c_str());

    return "(none)";
}

int script_openSerial(string name, int baud)
{
    vector<serialPortInfo*> ports = getSerialPortInfos();

    for (serialPortInfo* p : ports) {
        if ( p->name == name ) {

            sp_port* alreadyOpen = findConnectedPort( name );

            if ( alreadyOpen ) {
                for ( pair<int, sp_port*> hm : handleToPortNameMap ) {
                    if ( hm.second == alreadyOpen ) {
                        selectedSerial = hm.second;
                        return hm.first;
                    }
                }
                g_log.log(LL_ERROR, "openSerial failed: could not find handle in map");
                return 0;
            }

            selectedSerial = openPort( name, baud );

            if ( ! selectedSerial ) {
                g_log.log(LL_ERROR, "openSerial failed: could not open port %s", name.c_str());
                return 0;
            }

            int thisHandle = nextHandle;
            handleToPortNameMap[thisHandle] = selectedSerial;
            nextHandle++;

            return thisHandle;
        }
    }

    g_log.log(LL_ERROR, "openSerial failed: no port with name %s", name.c_str());

    selectedSerial = NULL;
    return false;
}

bool script_selectSerial(int handle)
{
    for ( pair<int, sp_port*> hm : handleToPortNameMap ) {
        if ( hm.first == handle ) {
            selectedSerial = hm.second;
            return true;
        }
    }

    ScriptLog* log = (ScriptLog*)getActiveScriptLog();
    if ( log )
        log->log(LL_ERROR, NULL, 0, "selectSerial failed: invalid handle %d", handle);

    return false;
}

// bool script_sendSerial(string code)
// {
//     if ( pthread_self() == mainThreadId )
//         return false;

//     if ( ! selectedSerial ) {
//         ScriptLog* log = (ScriptLog*)getActiveScriptLog();
//         if ( log )
//             log->log(LL_ERROR, NULL, 0, "sendCode failed: no port selected");
//         return false;
//     }

//     char buf[64];
//     sprintf(buf, "%s\n\n", code.c_str());
//     int len = strlen( buf );

//     int actual = sp_nonblocking_write(selectedSerial, buf, len/*, 100*/);
//     if ( len != actual ) {
//         ScriptLog* log = (ScriptLog*)getActiveScriptLog();
//         if ( log )
//             log->log(LL_ERROR, NULL, 0, "sendCode failed: expected to write %d bytes, only wrote %d", len, actual);
//         return false;
//     }

//     return true;
// }

script_serialReply* script_sendSerial(string str, int timeoutMs, string pattern)
{
    script_serialReply* reply = new script_serialReply();
    reply->ok = false;
    reply->vals = NULL;
    reply->str = "";

    if ( pthread_self() == mainThreadId ) // don't allow live camera scripts to do this at 30fps!
        return reply;

    ScriptLog* log = (ScriptLog*)getActiveScriptLog();

    if ( ! selectedSerial ) {
        if ( log )
            log->log(LL_ERROR, NULL, 0, "sendSerial failed: no port selected");
        return reply;
    }

    char buf[128];

    // clear old incoming data
    while ( sp_input_waiting(selectedSerial) )
        sp_nonblocking_read(selectedSerial, buf, sizeof(buf));

    // add newline to command string
    sprintf(buf, "%s\n\n", str.c_str());
    int len = strlen( buf );

    int actual = sp_nonblocking_write(selectedSerial, buf, len);
    if ( len != actual ) {
        if ( log )
            log->log(LL_ERROR, NULL, 0, "sendSerial failed: expected to write %d bytes, only wrote %d", len, actual);
        return reply;
    }

    // now wait for reply

    int timePassed = 0;
    string tmpStr;
    string finalStr;

    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();

    while ( timePassed < timeoutMs )
    {
        if ( sp_input_waiting(selectedSerial) )
        {
            //memset(buf, 0, sizeof(buf));
            int bytesRead = sp_nonblocking_read(selectedSerial, buf, sizeof(buf));

            for (int i = 0; i < bytesRead; i++)
            {
                char c = buf[i];
                if ( c == '\n' ) {
                    finalStr = tmpStr;
                    tmpStr.clear();
                }
                else {
                    tmpStr += c;
                }
            }
        }

        if ( ! finalStr.empty() )
            break;

        t1 = std::chrono::steady_clock::now();
        timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    }

    log = (ScriptLog*)getActiveScriptLog();
    //if ( log )
    //    log->log(LL_INFO, NULL, 0, "timePassed: %d, finalStr:'%s'", timePassed, finalStr.c_str());

    if ( finalStr.empty() )
        return reply;

    if ( pattern.empty() ) {
        reply->ok = true;
        reply->str = finalStr;
        return reply;
    }

    std::regex regexPattern( pattern );

    //finalStr = "ok X:1.23 Y:4.56 Z:7.89";

    std::smatch matches;
    if ( std::regex_search(finalStr, matches, regexPattern) ) {

        //log->log(LL_INFO, NULL, 0, "%d matches found", (int)matches.size());

        int numMatches = matches.size() - 1;

        asITypeInfo* t = GetScriptTypeIdByDecl("array<float>");
        CScriptArray* arr = CScriptArray::Create(t, numMatches);

        int arrInd = 0;
        for (int i = 1; i < (int)matches.size(); ++i) {
            //log->log(LL_INFO, NULL, 0, "%d: %s", i, matches[i].str().c_str());

            float* f = static_cast<float *>(arr->At(arrInd++));
            *f = atof( matches[i].str().c_str() );
        }

        reply->ok = true;
        reply->str = finalStr;
        reply->vals = arr;
    }
    else {
        if ( log )
            log->log(LL_ERROR, NULL, 0, "Match not found");
    }

    return reply;
}

























