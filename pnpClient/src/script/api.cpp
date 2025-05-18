
#include <algorithm>
#include <chrono>
#include <sstream>
#include <regex>
#include <thread>

//#include "imgui.h"
//#include "imgui_notify/imgui_notify.h"

#include <scriptarray/scriptarray.h>
#include <scriptdictionary/scriptdictionary.h>

#include "engine.h"
#include "api.h"
#include "db.h"
#include "log.h"
#include "scriptlog.h"
#include "preview.h"
#include "run.h"
#include "net_requester.h"
#include "overrides.h"
#include "util.h"
#include "plangroup.h"

#ifndef DEGTORAD
#define DEGTORAD 0.01745329252
#define RADTODEG 57.2957795131
#endif

int script_MM_NONE = 0;
int script_MM_TRAJECTORY = 1;
int script_MM_JOG = 2;
int script_MM_HOMING = 3;

int script_MR_NONE = 0;
int script_MR_SUCCESS = 1;
int script_MR_FAIL_CONFIG = 2;
int script_MR_FAIL_NOT_HOMED = 3;
int script_MR_FAIL_TIMED_OUT = 4;
int script_MR_FAIL_NOT_TRIGGERED = 5;
int script_MR_FAIL_OUTSIDE_BOUNDS = 6;
int script_MR_FAIL_FOLLOWING_ERROR = 7;
int script_MR_FAIL_LIMIT_TRIGGERED = 8;

int script_NT_NONE      = 0;
int script_NT_SUCCESS   = 1;
int script_NT_WARNING   = 2;
int script_NT_ERROR     = 3;
int script_NT_INFO      = 4;

using namespace std;

void script_print(string &msg)
{
    //printf("%s", msg.c_str());
    //scriptPrintBuffer.append(msg);
    ScriptLog* log = (ScriptLog*)getActiveScriptLog();
    if ( log )
        log->log(LL_INFO, NULL, 0, "%s", msg.c_str());
}

dbResult* currentResult = NULL;

static int dbQuery_callback(void *NotUsed, int argc, char **argv, char **azColName) {

    if ( ! currentResult )
        return -1;

    //g_log.log(LL_DEBUG, "%d entries:", argc);

    vector<string> cols;

    for(int i = 0; i < argc; i++){
        cols.push_back( argv[i] ? argv[i] : "NULL" );
        if ( ! currentResult->haveNames )
            currentResult->columnNames.push_back( azColName[i] );
        //g_log.log(LL_DEBUG, "%s = %s", azColName[i], argv[i] ? argv[i] : "NULL");
    }

    currentResult->rows.push_back( cols );

    currentResult->haveNames = true;

    return 0;
}

dbResult* script_dbQuery(string &query)
{
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();

    dbResult* res = new dbResult();
    currentResult = res;

    //printf("creating dbResult %p\n", res);

    string errMsg;
    if ( ! executeDatabaseStatement(query, dbQuery_callback, errMsg) ) {
        res->status = DBRS_FAILED;
        res->errorMessage = errMsg;
        ScriptLog* log = (ScriptLog*)getActiveScriptLog();
        if ( log )
            log->log(LL_ERROR, NULL, 0, "%s", errMsg.c_str());
    }
    else {
        res->status = DBRS_SUCCEEDED;
    }

    std::chrono::steady_clock::time_point t1 =   std::chrono::steady_clock::now();

    res->query = query;
    res->executionTime = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

    return res;
}

int script_getLastInsertId() {
    return getLastInsertId();
}

void setLongest( vector<string> &strs, vector<size_t> &lengths) {
    for (int k = 0; k < (int)strs.size(); k++) {
        string &s = strs[k];
        if ( s.length() > lengths[k] )
            lengths[k] = s.length();
    }
}

void buildHorizontalLinesString(string &lines, vector<size_t> &lengths) {
    lines = "-";
    for (int k = 0; k < (int)lengths.size(); k++)
        lines += string(lengths[k]+3, '-');
    lines += "\n";
}

void buildColumnContentString(string &str, vector<string> &columns, vector<size_t> &lengths) {
    str += "| ";
    for (int k = 0; k < (int)columns.size(); k++) {
        string &s = columns[k];
        if ( k > 0 )
            str += " | ";
        str += s;
        if ( lengths[k] > s.length() ) {
            str += string(lengths[k] - s.length(), ' ');
        }
    }
    str += " |\n";
}




dbRow::dbRow(dbResult* res, int row)
{
    //printf("dbRow constructor %p\n", this);
    result = res;
    rowNum = row;
    refCount = 1;
    if ( result )
        result->IncRef();
}

// string dbRow::name()
// {
//     return "dbRow";
// }

int dbRow::IncRef() {
    ++refCount;
    //printf("dbRow IncRef %p, now = %d\n", this, refCount);
    return refCount;
}

int dbRow::DecRef() {
    --refCount;
    //printf("dbRow DecRef %p, now = %d\n", this, refCount);
    if( refCount == 0 )
    {
        //printf("dbRow %p finished, decrementing owner\n", this);
        if ( result )
            result->DecRef();
        //printf("deleting dbRow %p\n", this);
        delete this;
        return 0;
    }
    return refCount;
}

dbRow *dbRow::opAssign(dbRow *other)
{
    if ( ! other )
        return NULL;
    //printf("dbRow opAssign %p %p\n", this, other); fflush(stdout);
    if ( result )
        result->DecRef();
    result = other->result;
    rowNum = other->rowNum;
    //refCount = 1;
    result->IncRef();
    return other;
}

int dbRow::get_numCols()
{
    if ( ! result )
        return 0;
    return result->get_numCols();
}

std::string dbRow::dump()
{
    if ( ! result )
        return "(invalid row)\n";

    return result->dump_row(rowNum);
}

string dbRow::col_int(unsigned int i) {
    if ( ! result ) {
        g_log.log(LL_ERROR, "attempted to retreived column from invalid row");

        ScriptLog* slog = (ScriptLog*)getActiveScriptLog();
        if ( slog )
            slog->log(LL_ERROR, NULL, 0, "attempted to retreived column from invalid row");

        return "(invalid row)";
    }
    return result->getRowCol(rowNum, i);
}

string dbRow::col_string(string colName) {
    if ( ! result ) {
        g_log.log(LL_ERROR, "attempted to retreived column '%s' from invalid row", colName.c_str());

        ScriptLog* slog = (ScriptLog*)getActiveScriptLog();
        if ( slog )
            slog->log(LL_ERROR, NULL, 0, "attempted to retreived column '%s' from invalid row", colName.c_str());

        return "(invalid row)";
    }
    int ind = result->getColumnIndexByName(colName);
    if ( ind < 0 )
        return "invalid column name: " + colName;
    return col_int(ind);
}

void script_print_dbRow(dbRow &row) {
    ScriptLog* log = (ScriptLog*)getActiveScriptLog();
    if ( log )
        log->log(LL_INFO, NULL, 0, "%s", row.dump().c_str());
}






dbResult::dbResult() {
    other = NULL;
    refCount = 1;
    haveNames = false;
    status = DBRS_FAILED;
    executionTime = -1;
}

dbResult::~dbResult() {
    //printf("destroying dbResult\n");
    // for (int i = 0; i < (int)rowRefs.size(); i++) {
    //     printf("destroying rowRef %p\n", rowRefs[i]);
    //     delete rowRefs[i];
    // }
}

// string dbResult::name()
// {
//     return "dbResult";
// }

int dbResult::IncRef() {
    ++refCount;
    //printf("%s AddRef, now = %d\n", name().c_str(), refCount);
    return refCount;
}

int dbResult::DecRef() {
    --refCount;
    //printf("%s release, now = %d\n", name().c_str(), refCount);
    if( refCount == 0 )
    {
        //printf("deleting dbResult %p\n", this);
        if ( other )
            other->DecRef();
        delete this;
        return 0;
    }
    return refCount;
}

dbResult *dbResult::opAssign(dbResult *ref)
{
    //printf("opAssign %p %p\n", this, ref); fflush(stdout);
    if ( other )
        other->DecRef();
    other = ref;
    other->IncRef();
    return other;
}

int dbResult::get_status()
{
    if ( other )
        return other->status;
    return status;
}

int dbResult::get_numCols()
{
    if ( other )
        return other->get_numCols();
    if ( rows.empty() )
        return 0;
    return (int)rows[0].size();
}

string dbResult::dump() {
    if ( other )
        return other->dump_row(-1);
    return dump_row(-1);
}

string dbResult::dump_row(int rowNum)
{
    if ( other )
        return other->dump_row(rowNum);

    if ( status == DBRS_FAILED ) {
        return errorMessage + "\nQuery was: " + query + "\n";
    }

    if ( rows.empty() ) {
        return "(result has no rows)\n";
    }

    if ( rowNum >= (int)rows.size() ) {
        return "invalid row number: " + to_string(rowNum);
    }

    vector<size_t> longestContent;
    longestContent.resize( get_numCols(), 0 );

    setLongest( columnNames, longestContent );

    if ( rowNum > -1 ) {
        vector<string> &row = rows[rowNum];
        setLongest(row, longestContent);
    }
    else {
        for (int i = 0; i < (int)rows.size(); i++) {
            vector<string> &row = rows[i];
            setLongest(row, longestContent);
        }
    }

    string str;

    if ( rowNum < 0 )
        str += "Result for query: " +  query + "\n";

    string lines;
    buildHorizontalLinesString(lines, longestContent);
    str += lines;
    buildColumnContentString(str, columnNames, longestContent);
    str += lines;
    if ( rowNum > -1 ) {
        vector<string> &row = rows[rowNum];
        buildColumnContentString(str, row, longestContent);
    }
    else {
        for (int i = 0; i < (int)rows.size(); i++) {
            vector<string> &row = rows[i];
            buildColumnContentString(str, row, longestContent);
        }
    }
    str += lines;

    if ( rowNum < 0 )
        str += to_string(get_numRows()) + " rows in set (" + to_string(executionTime) + string(" us)\n");

    return str;
}

string dbResult::columnName(unsigned int i)
{
    if ( other )
        return other->columnName(i);
    if ( i >= columnNames.size() )
        return "invalid column: " + to_string(i);
    return columnNames[i];
}

int dbResult::get_numRows()
{
    if ( other )
        return other->get_numRows();
    return (int)rows.size();
}

dbRow* dbResult::row(unsigned int i) {
    if ( other )
        return other->row(i);

    if ( i >= rows.size() ) {
        return NULL;
        //printf("invalid row: %d\n", i);
        dbRow* r = new dbRow(this, 0);
        //rowRefs.push_back(r);
        return r;
    }
    //printf("returning row\n");
    dbRow* r = new dbRow(this, i);
    //rowRefs.push_back(r);
    return r;
}

string dbResult::getRowCol(int r, int c)
{
    if ( other )
        return other->getRowCol(r, c);

    if ( r >= (int)rows.size() ) {
        return "invalid row: " + to_string(r);
    }
    vector<string> &row = rows[r];
    if ( c >= (int)row.size() ) {
        return "invalid column: " + to_string(c);
    }
    return row[c];
}

bool caseInsensitiveComp(string s1, string s2) {
    transform(s1.begin(), s1.end(), s1.begin(), ::tolower);
    transform(s2.begin(), s2.end(), s2.begin(), ::tolower);
    if(s1.compare(s2) == 0)
        return true; //The strings are same
    return false; //not matched
}

int dbResult::getColumnIndexByName(std::string name)
{
    if ( other )
        return other->getColumnIndexByName(name);

    for (int i = 0; i < (int)columnNames.size(); i++) {
        if ( caseInsensitiveComp(columnNames[i], name) )
            return i;
    }
    return -1;
}

void script_print_dbResult(dbResult &res) {
    ScriptLog* log = (ScriptLog*)getActiveScriptLog();
    if ( log )
        log->log(LL_INFO, NULL, 0, "%s", res.dump().c_str());
}





script_serialReply::script_serialReply()
{
    refCount = 1;
    ok = false;
    str = "";
    vals = NULL;
}

int script_serialReply::IncRef() {
    ++refCount;
    return refCount;
}

int script_serialReply::DecRef() {
    --refCount;
    if( refCount == 0 )
    {
        delete this;
        return 0;
    }
    return refCount;
}

script_serialReply *script_serialReply::opAssign(script_serialReply *ref)
{
    refCount++;
    this->ok = ref->ok;
    this->str = ref->str;
    this->vals = ref->vals;
    return this;
}

bool script_serialReply::get_ok() {
    return ok;
}

CScriptArray* script_serialReply::get_vals() {
    return vals;
}

string script_serialReply::get_str() {
    return str;
}






dbResult *script_dbResultFactory()
{
    return new dbResult();
}

dbRow *script_dbRowFactory()
{
    //printf("dbRowFactory\n");
    return new dbRow(NULL, 0);
}

script_serialReply *script_serialReplyFactory()
{
    //printf("dbRowFactory\n");
    return new script_serialReply();
}


void script_printarray_string(void *v)
{
    CScriptArray* array = (CScriptArray*)v;

    string str = "[";
    for (int i = 0; i < (int)array->GetSize(); i++) {
        string * p = static_cast<string *>(array->At(i));
        if ( i > 0 )
            str += ",";
        str += *p;
    }
    str += "]";
    printf("%s", str.c_str());
}

string script_strArray( void* v )
{
    CScriptArray* array = (CScriptArray*)v;

    string str = "[";
    for (int i = 0; i < (int)array->GetSize(); i++) {
        string * p = static_cast<string *>(array->At(i));
        if ( i > 0 )
            str += ",";
        str += *p;
    }
    str += "]";
    return str;
}

std::string script_strDictionaryKeys( void* d )
{
    CScriptDictionary* dict = (CScriptDictionary*)d;

    string str = "keys = [";
    CScriptArray *keys = dict->GetKeys();
    int count = 0;
    for (auto it : *dict)
    {
        std::string keyName = it.GetKey();
        if ( count++ > 0 )
            str += ",";
        str += keyName;
    }
    str += "]";

    keys->Release();

    return str;
}

extern asIScriptEngine* engine;

std::string script_strDictionary( void* d )
{
    int stringTypeId = engine->GetTypeIdByDecl("string");

    CScriptDictionary* dict = (CScriptDictionary*)d;

    string str = "[\n";

    for (auto it : *dict)
    {
        std::string keyName = it.GetKey();

        str += "  key: " + keyName + ", ";
        str += "val: ";
        int typeId = it.GetTypeId();
        if ( typeId == stringTypeId ) {
            const string *p = static_cast<const string *>(it.GetAddressOfValue());
            str += "'" + *p + "'\n";
        }
        else if ( typeId == asTYPEID_BOOL ) {
            const bool *p = static_cast<const bool *>(it.GetAddressOfValue());
            str += std::to_string(*p) + "\n";
        }
        else if ( typeId == asTYPEID_INT32 ) {
            const int *p = static_cast<const int *>(it.GetAddressOfValue());
            str += std::to_string(*p) + "\n";
        }
        else if ( typeId == asTYPEID_INT64 ) {
            const int64_t *p = static_cast<const int64_t *>(it.GetAddressOfValue());
            str += std::to_string(*p) + "\n";
        }
        else if ( typeId == asTYPEID_FLOAT ) {
            const float *p = static_cast<const float *>(it.GetAddressOfValue());
            str += std::to_string(*p) + "\n";
        }
        else if ( typeId == asTYPEID_DOUBLE ) {
            const double *p = static_cast<const double *>(it.GetAddressOfValue());
            str += std::to_string(*p) + "\n";
        }
        else {
            str += "?\n";
        }
    }

    str += "]";

    return str;
}

bool script_runCommand(std::string text)
{
    g_log.log(LL_DEBUG, "Running single command: %s", text.c_str());

    std::istringstream origStream(text);
    vector<string> lines;
    std::string curLine;
    while (std::getline(origStream, curLine))
    {
        //if ( ! curLine.empty() )
        lines.push_back(curLine);
    }

    //setActiveCommandListPath(filename);

    //bool ok = doPreview(lines);

    bool ok = false;
    if ( getActivePreviewOnly() )
        ok = doPreview(lines);
    else
        ok = doActualRun(lines);

    //setActiveCommandListPath("");

    return ok;
}

string strReplaceAll(string& str, const string& from, const string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

bool script_runCommandList(std::string filename)
{
    return script_runCommandList_dict(filename, NULL);
}

bool script_runCommandList_dict(std::string filename, void *d)
{
    map<string, string> subs;

    if ( d ) {
        CScriptDictionary* dict = (CScriptDictionary*)d;
        std::regex regexPattern("[a-zA-Z0-9_]+");
        for (auto it : *dict)
        {
            std::string keyName = it.GetKey();
            float val = INVALID_FLOAT;

            if ( regex_match(keyName, regexPattern) ) {
                int typeId = it.GetTypeId();
                if ( typeId == asTYPEID_INT32 ) {
                    const int *p = static_cast<const int *>(it.GetAddressOfValue());
                    val = *p;
                }
                else if ( typeId == asTYPEID_INT64 ) {
                    const int64_t *p = static_cast<const int64_t *>(it.GetAddressOfValue());
                    val = *p;
                }
                else if ( typeId == asTYPEID_FLOAT ) {
                    const float *p = static_cast<const float *>(it.GetAddressOfValue());
                    val = *p;
                }
                else if ( typeId == asTYPEID_DOUBLE ) {
                    const double *p = static_cast<const double *>(it.GetAddressOfValue());
                    val = *p;
                }
            }

            if ( val == INVALID_FLOAT )
                g_log.log(LL_WARN, "Ignoring dictionary entry '%s'", keyName.c_str());
            else if ( val != val )
                g_log.log(LL_WARN, "Ignoring invalid (NaN) dictionary entry '%s'", keyName.c_str());
            else {
                char buf[64];
                // I went back and forward between %f and %g a couple times. There was some
                // problem with %f having trailing zeroes that I don't recall. Using %g did
                // fix that, but very small values would end up with a "-e06 " suffix which
                // was even worse. To compromise, use %f and remove trailing zeroes myself.
                sprintf(buf, "%.8f", val);
                int len = strlen(buf)-1;
                while ( buf[len] == '0' && len > 0 ) {
                    buf[len--] = 0;
                }
                if ( buf[len] == '.' && len > 0 )
                    buf[len] = 0;
                subs[keyName] = string(buf);
            }
        }
    }

    string text, errMsg;
    if ( ! loadTextFromDBFile(text, "command list", filename, errMsg) ) {
        g_log.log(LL_ERROR, "loadTextFromDBFile failed: %s", errMsg.c_str());
        ScriptLog* log = (ScriptLog*)getActiveScriptLog();
        if ( log )
            log->log(LL_ERROR, NULL, 0, "Could not load command list file: %s", filename.c_str());
        return false;
    }

    g_log.log(LL_DEBUG, "Running command list file: %s", filename.c_str());

    std::istringstream origStream(text);
    vector<string> lines;
    std::string curLine;
    while (std::getline(origStream, curLine))
    {
        //if ( ! curLine.empty() )
            lines.push_back(curLine);
    }

    for (string &line : lines ) {
        for (map<string, string>::iterator it = subs.begin(); it != subs.end(); it++) {
            string key = "$" + it->first;
            string val = it->second;
            strReplaceAll( line, key, val );
        }
    }

    for (string &line : lines ) {
        g_log.log(LL_DEBUG, "  %s", line.c_str());
    }

    setActiveCommandListPath(filename);

    bool ok = false;
    if ( getActivePreviewOnly() )
        ok = doPreview(lines);
    else
        ok = doActualRun(lines);

    setActiveCommandListPath("");

    return ok;
}

void script_wait(int millis)
{
    if ( millis < 1 )
        return;
    if ( getActivePreviewOnly() ) {
        planGroup_preview.addWaitTime(millis);
        return;
    }
    std::this_thread::sleep_for( chrono::milliseconds(millis) );
}

void script_setDigitalOut(int which, int toWhat)
{
    if ( getActivePreviewOnly() )
        return;

    commandRequest_t req = createCommandRequest(MT_SET_DIGITAL_OUTPUTS);
    req.setDigitalOutputs.bits = toWhat ? 0xff : 0;
    req.setDigitalOutputs.changed = setBitPosition(which);
    sendCommandRequest(&req);
}

void script_setPWMOut(float toWhat)
{
    if ( getActivePreviewOnly() )
        return;

    if ( toWhat < 0 ) toWhat = 0;
    if ( toWhat > 1 ) toWhat = 1;
    commandRequest_t req = createCommandRequest(MT_SET_PWM_OUTPUT);
    req.setPWMOutput.val = 65535 * toWhat;
    sendCommandRequest(&req);
}

extern clientReport_t lastStatusReport;

bool script_getDigitalIn(int which)
{
    uint16_t mask = (1 << which);
    if ( mask & lastStatusReport.inputs )
        return true;
    return false;
}

bool script_getDigitalOut(int which)
{
    uint16_t mask = (1 << which);
    if ( mask & lastStatusReport.outputs )
        return true;
    return false;
}

script_vec3 script_getActualPos()
{
    script_vec3 v;
    v.x = lastStatusReport.actualPosX;
    v.y = lastStatusReport.actualPosY;
    v.z = lastStatusReport.actualPosZ;
    return v;
}

float script_getActualRot()
{
    return lastStatusReport.actualRots[0];
}

float script_getVacuum()
{
    float vac = ((float)lastStatusReport.pressure - 50000) / 500.0f;
    return vac;
}

int script_getLoadcell()
{
    return lastStatusReport.loadcell;
}

float script_getWeight()
{
    return lastStatusReport.weight;
}

float script_getADC(int i)
{
    if ( i < 0 || i > 1 )
        return 0;
    return lastStatusReport.adc[i] / 4096.0f;
}

int32_t script_getEncoder() {
    return lastStatusReport.rotary;
}

bool script_isPreview()
{
    return getActivePreviewOnly();
}

uint64_t script_millis()
{
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    uint64_t m = std::chrono::duration_cast<std::chrono::milliseconds>( t0.time_since_epoch() ).count();
    return m;
}

int script_getMachineMode()
{
    switch ( lastStatusReport.mode ) {
    case MM_NONE:           return script_MM_NONE;
    case MM_TRAJECTORY:     return script_MM_TRAJECTORY;
    case MM_JOG:            return script_MM_JOG;
    case MM_HOMING:         return script_MM_HOMING;
    }
    return -1;
}

extern trajectoryResult_e lastTrajResult;
extern homingResult_e lastHomingResult;
extern probingResult_e lastProbingResult;

int script_getTrajectoryResult()
{
    switch ( lastTrajResult ) {
    case TR_NONE:                   return script_MR_NONE;
    case TR_SUCCESS:                return script_MR_SUCCESS;
    case TR_FAIL_CONFIG:            return script_MR_FAIL_CONFIG;
    case TR_FAIL_NOT_HOMED:         return script_MR_FAIL_NOT_HOMED;
    case TR_FAIL_OUTSIDE_BOUNDS:    return script_MR_FAIL_OUTSIDE_BOUNDS;
    case TR_FAIL_FOLLOWING_ERROR:   return script_MR_FAIL_FOLLOWING_ERROR;
    case TR_FAIL_LIMIT_TRIGGERED:   return script_MR_FAIL_LIMIT_TRIGGERED;
    }
    return -1;
}

int script_getHomingResult()
{
    switch ( lastHomingResult ) {
    case HR_NONE:                           return script_MR_NONE;
    case HR_SUCCESS:                        return script_MR_SUCCESS;
    case HR_FAIL_CONFIG:                    return script_MR_FAIL_CONFIG;
    case HR_FAIL_TIMED_OUT:                 return script_MR_FAIL_TIMED_OUT;
    case HR_FAIL_LIMIT_ALREADY_TRIGGERED:   return script_MR_FAIL_LIMIT_TRIGGERED;
    }
    return -1;
}

int script_getProbingResult()
{
    switch ( lastProbingResult ) {
    case PR_NONE:                           return script_MR_NONE;
    case PR_SUCCESS:                        return script_MR_SUCCESS;
    case PR_FAIL_CONFIG:                    return script_MR_FAIL_CONFIG;
    case PR_FAIL_NOT_HOMED:                 return script_MR_FAIL_NOT_HOMED;
    case PR_FAIL_NOT_TRIGGERED:             return script_MR_FAIL_NOT_TRIGGERED;
    case PR_FAIL_ALREADY_TRIGGERED:         return script_MR_FAIL_LIMIT_TRIGGERED;
    }
    return -1;
}

void script_print_bool(bool b)
{
    ScriptLog* log = (ScriptLog*)getActiveScriptLog();
    if ( log )
        log->log(LL_INFO, NULL, 0, "%s", b ? "true" : "false");
}

void script_print_int(int i)
{
    ScriptLog* log = (ScriptLog*)getActiveScriptLog();
    if ( log )
        log->log(LL_INFO, NULL, 0, "%d", i);
}

void script_print_float(float f)
{
    ScriptLog* log = (ScriptLog*)getActiveScriptLog();
    if ( log ) {
        string s = fformat(f, 8);
        log->log(LL_INFO, NULL, 0, "%s", s.c_str());
    }
}

string script_str_float(float f)
{
    return fformat(f, 8);
}

string script_str_vec3(script_vec3 &v)
{
    return fformat(v.x, 8) + ", " + fformat(v.y, 8) + ", " + fformat(v.z, 8);
}

void script_print_vec3(script_vec3 &v)
{
    ScriptLog* log = (ScriptLog*)getActiveScriptLog();
    if ( log ) {
        //string x = fformat(v.x, 8);
        //string y = fformat(v.y, 8);
        //string z = fformat(v.z, 8);
        log->log(LL_INFO, NULL, 0, "%s", script_str_vec3(v).c_str());
    }
}

void script_print_affine(script_affine &a)
{
    ScriptLog* log = (ScriptLog*)getActiveScriptLog();
    if ( log ) {
        log->log(LL_INFO, NULL, 0, "%s", a.str().c_str());
    }
}

void script_print_serialReply(script_serialReply &r)
{
    ScriptLog* log = (ScriptLog*)getActiveScriptLog();
    if ( log ) {
        log->log(LL_INFO, NULL, 0, "ok: %s", r.ok?"true":"false");
        log->log(LL_INFO, NULL, 0, "str: '%s'", r.str.c_str());
        if ( r.vals ) {
            log->log(LL_INFO, NULL, 0, "vals.length(): %d", r.vals->GetSize());
            for (int i = 0; i < (int)r.vals->GetSize(); i++) {
                float* f = static_cast<float *>(r.vals->At(i));
                log->log(LL_INFO, NULL, 0, "  %d: %.6f", i, *f);
            }
        }
        else {
            log->log(LL_INFO, NULL, 0, "vals.length(): 0");
        }
    }
}



script_vec3 script_vec3::operator +(const script_vec3& other) {
    script_vec3 ret;
    ret.x = x + other.x;
    ret.y = y + other.y;
    ret.z = z + other.z;
    return ret;
}

script_vec3 script_vec3::operator -(const script_vec3& other) {
    script_vec3 ret;
    ret.x = x - other.x;
    ret.y = y - other.y;
    ret.z = z - other.z;
    return ret;
}

script_vec3 script_vec3::operator +=(const script_vec3& other) {
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
}

script_vec3 script_vec3::operator -=(const script_vec3& other) {
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
}

void vec3_opAdd_vec3(asIScriptGeneric * gen) {
    script_vec3* a = static_cast<script_vec3*>(gen->GetObject());
    script_vec3* b = static_cast<script_vec3*>(gen->GetArgAddress(0));
    script_vec3 ret = *a + *b;
    gen->SetReturnObject(&ret);
}

void vec3_opSub_vec3(asIScriptGeneric * gen) {
    script_vec3* a = static_cast<script_vec3*>(gen->GetObject());
    script_vec3* b = static_cast<script_vec3*>(gen->GetArgAddress(0));
    script_vec3 ret = *a - *b;
    gen->SetReturnObject(&ret);
}

void vec3_opAddAssign_vec3(asIScriptGeneric * gen) {
    script_vec3* self = static_cast<script_vec3*>(gen->GetObject());
    script_vec3* b = static_cast<script_vec3*>(gen->GetArgAddress(0));
    *self += *b;
    gen->SetReturnObject(self);
}

void vec3_opSubAssign_vec3(asIScriptGeneric * gen) {
    script_vec3* self = static_cast<script_vec3*>(gen->GetObject());
    script_vec3* b = static_cast<script_vec3*>(gen->GetArgAddress(0));
    *self -= *b;
    gen->SetReturnObject(self);
}



void vec3_opMul_float(asIScriptGeneric * gen) {
    script_vec3* a = static_cast<script_vec3*>(gen->GetObject());
    float f = gen->GetArgFloat(0);
    script_vec3 ret;
    ret.x = f * a->x;
    ret.y = f * a->y;
    ret.z = f * a->z;
    gen->SetReturnObject(&ret);
}

void vec3_opDiv_float(asIScriptGeneric * gen) {
    script_vec3* a = static_cast<script_vec3*>(gen->GetObject());
    float f = gen->GetArgFloat(0);

    if ( ! f ) {
        ScriptLog* log = (ScriptLog*)getActiveScriptLog();
        if ( log ) {
            log->log(LL_ERROR, NULL, 0, "divide by zero in vec3_opDiv_float");
        }
        return;
    }

    script_vec3 ret;
    ret.x = a->x / f;
    ret.y = a->y / f;
    ret.z = a->z / f;
    gen->SetReturnObject(&ret);
}

void vec3_opMulAssign_float(asIScriptGeneric * gen) {
    script_vec3* self = static_cast<script_vec3*>(gen->GetObject());
    float f = gen->GetArgFloat(0);
    self->x *= f;
    self->y *= f;
    self->z *= f;
    gen->SetReturnObject(self);
}

void vec3_opDivAssign_float(asIScriptGeneric * gen) {
    script_vec3* self = static_cast<script_vec3*>(gen->GetObject());
    float f = gen->GetArgFloat(0);

    if ( ! f ) {
        ScriptLog* log = (ScriptLog*)getActiveScriptLog();
        if ( log ) {
            log->log(LL_ERROR, NULL, 0, "divide by zero in vec3_opDivAssign_float");
        }
        return;
    }

    self->x /= f;
    self->y /= f;
    self->z /= f;
    gen->SetReturnObject(self);
}



void vec3_opNeg(asIScriptGeneric * gen) {
    script_vec3* self = static_cast<script_vec3*>(gen->GetObject());
    self->x *= -1;
    self->y *= -1;
    self->z *= -1;
    gen->SetReturnObject(self);
}

script_vec3 &script_vec3::operator=(const script_vec3 &other)
{
    x = other.x;
    y = other.y;
    z = other.z;
    return *this;
}

script_vec3 script_vec3::set_floatfloatfloat(float _x, float _y, float _z) {
    x = _x;
    y = _y;
    z = _z;
    return *this;
}

float script_vec3::length() {
    return sqrtf( x*x + y*y + z*z );
}

float script_vec3::normalize() {
    float len = length();
    if ( len == 0 )
        return 0;
    float invLen = 1 / len;
    x *= invLen;
    y *= invLen;
    z *= invLen;
    return len;
}

script_vec3 script_vec3::normalized() {
    script_vec3 ret;
    float len = length();
    if ( len == 0 ) {

        if ( ! len ) {
            ScriptLog* log = (ScriptLog*)getActiveScriptLog();
            if ( log ) {
                log->log(LL_ERROR, NULL, 0, "divide by zero in vec3_opDivAssign_float");
            }
            return ret;
        }

        ret.x = 0;
        ret.y = 0;
        ret.z = 0;
    }
    else {
        float invLen = 1 / len;
        ret.x = x * invLen;
        ret.y = y * invLen;
        ret.z = z * invLen;
    }
    return ret;
}

float script_vec3::distTo(const script_vec3 other) {
    float dx = x - other.x;
    float dy = y - other.y;
    float dz = z - other.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

float script_vec3::distToXY(const script_vec3 other) {
    float dx = x - other.x;
    float dy = y - other.y;
    return sqrtf(dx*dx + dy*dy);
}

script_vec3 script_vec3::rotatedBy(float degrees)
{
    float radians = degrees * DEGTORAD;
    float c = cosf(radians);
    float s = sinf(radians);
    script_vec3 ret;
    ret.x = c * x - s * y;
    ret.y = s * x + c * y;
    ret.z = z;
    return ret;
}

void script_exit() {
    abortScript();
}



script_affine::script_affine(const script_vec3 a0, const script_vec3 a1, const script_vec3 a2,
                             const script_vec3 b0, const script_vec3 b1, const script_vec3 b2) {

    matrix[0][0] = a0.x;
    matrix[0][1] = a0.y;
    matrix[0][2] = 1;
    matrix[0][3] = 0;
    matrix[0][4] = 0;
    matrix[0][5] = 0;
    matrix[0][6] = b0.x;

    matrix[1][0] = 0;
    matrix[1][1] = 0;
    matrix[1][2] = 0;
    matrix[1][3] = a0.x;
    matrix[1][4] = a0.y;
    matrix[1][5] = 1;
    matrix[1][6] = b0.y;

    matrix[2][0] = a1.x;
    matrix[2][1] = a1.y;
    matrix[2][2] = 1;
    matrix[2][3] = 0;
    matrix[2][4] = 0;
    matrix[2][5] = 0;
    matrix[2][6] = b1.x;

    matrix[3][0] = 0;
    matrix[3][1] = 0;
    matrix[3][2] = 0;
    matrix[3][3] = a1.x;
    matrix[3][4] = a1.y;
    matrix[3][5] = 1;
    matrix[3][6] = b1.y;

    matrix[4][0] = a2.x;
    matrix[4][1] = a2.y;
    matrix[4][2] = 1;
    matrix[4][3] = 0;
    matrix[4][4] = 0;
    matrix[4][5] = 0;
    matrix[4][6] = b2.x;

    matrix[5][0] = 0;
    matrix[5][1] = 0;
    matrix[5][2] = 0;
    matrix[5][3] = a2.x;
    matrix[5][4] = a2.y;
    matrix[5][5] = 1;
    matrix[5][6] = b2.y;

    gaussEliminate();

    /*matrix = {
            {a0.x, a0.y, 1, 0, 0, 0, b0.x},
            {0, 0, 0, a0.x, a0.y, 1, b0.y},
            {a1.x, a1.y, 1, 0, 0, 0, b1.x},
            {0, 0, 0, a1.x, a1.y, 1, b1.y},
            {a2.x, a2.y, 1, 0, 0, 0, b2.x},
            {0, 0, 0, a2.x, a2.y, 1, b2.y}
        };*/
}

script_affine &script_affine::operator=(const script_affine &other)
{
    valid = other.valid;
    memcpy(matrix, other.matrix, sizeof(matrix));
    rotation = other.rotation;
    scaleX = other.scaleX;
    scaleY = other.scaleY;
    return *this;
}

void script_affine::setIdentity()
{
    memset(matrix, 0, sizeof(matrix));
    matrix[0][0] = 1;
    matrix[0][1] = 1;
    matrix[0][2] = 1;
    matrix[0][6] = 1;

    matrix[1][1] = -1;

    matrix[2][2] = 1;

    matrix[3][3] = 1;
    matrix[3][5] = 1;

    matrix[4][4] = 1;
    matrix[4][6] = 1;

    matrix[5][5] = 1;

    rotation = 0;
    scaleX = 1;
    scaleY = 1;
    valid = true;
}

void script_affine::gaussEliminate() {
    int N = 6;
    for (int i = 0; i < N; i++) {
        // Find the maximum element for pivoting
        float maxVal = matrix[i][i];
        int row = i;
        for (int j = i+1; j < N; j++) {
            if (abs(matrix[j][i]) > abs(maxVal)) {
                maxVal = matrix[j][i];
                row = j;
            }
        }

        // Swap rows
        if (row != i) {
            float tmp[7];
            memcpy(tmp, matrix[i], sizeof(matrix[i]));
            memcpy(matrix[i], matrix[row], sizeof(matrix[i]));
            memcpy(matrix[row], tmp, sizeof(matrix[i]));
        }

        // Eliminate column below pivot
        for (int j = i+1; j < N; j++) {
            float factor = matrix[j][i] / matrix[i][i];
            for (int k = i; k <= N; k++) {
                matrix[j][k] -= factor * matrix[i][k];
            }
        }
    }

    // Back substitution
    for (int i = N-1; i >= 0; i--) {
        for (int j = i+1; j < N; j++) {
            matrix[i][N] -= matrix[i][j] * matrix[j][N];
        }
        matrix[i][N] /= matrix[i][i];
    }


    float a11 = matrix[0][6];
    float a12 = matrix[1][6];
    float a21 = matrix[3][6];
    float a22 = matrix[4][6];

    // extract rotation
    rotation = atan2(a21, a11) * RADTODEG;
    scaleX = sqrt(a11 * a11 + a21 * a21);
    scaleY = sqrt(a12 * a12 + a22 * a22);

    valid = true;
}

script_vec3 script_affine::transform(const script_vec3 &p) {
    script_vec3 t;
    t.x = matrix[0][6] * p.x + matrix[1][6] * p.y + matrix[2][6];
    t.y = matrix[3][6] * p.x + matrix[4][6] * p.y + matrix[5][6];
    t.z = p.z;
    return t;
}

string script_affine::str() {
    return  string("valid = ") + (valid?"true":"false") +
           ", a11 = " + to_string(matrix[0][6]) + ", a12 = " + to_string(matrix[1][6]) + ", tx = " + to_string(matrix[2][6]) +
           ", a21 = " + to_string(matrix[3][6]) + ", a22 = " + to_string(matrix[4][6]) + ", ty = " + to_string(matrix[5][6]) +
           ", rotation = " + to_string(rotation) + ", scaleX = " + to_string(scaleX) + ", scaleY = " + to_string(scaleY);
}















