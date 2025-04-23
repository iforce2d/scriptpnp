#include <map>
#include <vector>

#include "script_globals.h"
#include "db.h"
#include "script/engine.h"

using namespace std;

map<string, float> globalValuesMap;
map<string, string> globalStringMap;
map<string, script_vec3> globalVec3Map;
map<string, script_affine> globalAffineMap;

void script_setMemoryValue(string name, float v)
{
    if ( getActivePreviewOnly() )
        return;

    globalValuesMap[name] = v;
}

float script_getMemoryValue(string name)
{
    map<string, float>::iterator it = globalValuesMap.find(name);
    if ( it == globalValuesMap.end() )
        return 0;
    return it->second;
}

bool script_haveMemoryValue(string name)
{
    return globalValuesMap.find(name) != globalValuesMap.end();
}



void script_setMemoryString(string name, string s)
{
    if ( getActivePreviewOnly() )
        return;

    globalStringMap[name] = s;
}

string script_getMemoryString(string name)
{
    map<string, string>::iterator it = globalStringMap.find(name);
    if ( it == globalStringMap.end() )
        return "";
    return it->second;
}

bool script_haveMemoryString(string name)
{
    return globalStringMap.find(name) != globalStringMap.end();
}









vector< vector<string> > dbValueResult;

static int globalValue_callback(void *NotUsed, int argc, char **argv, char **azColName) {

    vector<string> cols;

    for ( int i = 0; i < argc; i++ )
        cols.push_back( argv[i] ? argv[i] : "NULL" );

    dbValueResult.push_back( cols );

    return 0;
}





void script_setDBString(string name, string val)
{
    if ( getActivePreviewOnly() )
        return;

    string errMsg;
    string sql;

    if ( script_haveDBString(name) )
        sql = "update global_string set value = '"+val+"' where name = '"+name+"'";
    else
        sql = "insert into internal_dbstring (name, value) values ('"+name+"', '"+val+"')";
    executeDatabaseStatement(sql, globalValue_callback, errMsg);
}

string script_getDBString(string name)
{
    string errMsg;

    dbValueResult.clear();

    string sql = "select value from internal_dbstring where name = '"+name+"'";
    if ( ! executeDatabaseStatement(sql, globalValue_callback, errMsg) )
        return "";

    if ( dbValueResult.empty() )
        return "";

    vector<string> &cols = dbValueResult[0];
    return cols[0];
}

bool script_haveDBString(string name)
{
    string errMsg;

    dbValueResult.clear();

    string sql = "select value from internal_dbstring where name = '"+name+"'";
    if ( ! executeDatabaseStatement(sql, globalValue_callback, errMsg) )
        return false;

    return ! dbValueResult.empty();
}





void script_setDBValue(string name, float val)
{
    if ( getActivePreviewOnly() )
        return;

    string errMsg;
    string sql;

    if ( script_haveDBValue(name) )
        sql = "update global_number set value = "+to_string(val)+" where name = '"+name+"'";
    else
        sql = "insert into internal_dbnumber (name, value) values ('"+name+"', "+to_string(val)+")";
    executeDatabaseStatement(sql, globalValue_callback, errMsg);
}

float script_getDBValue(string name)
{
    string errMsg;

    dbValueResult.clear();

    string sql = "select value from internal_dbnumber where name = '"+name+"'";
    if ( ! executeDatabaseStatement(sql, globalValue_callback, errMsg) )
        return 0;

    if ( dbValueResult.empty() )
        return 0;

    vector<string> &cols = dbValueResult[0];
    return atof( cols[0].c_str() );
}

bool script_haveDBValue(string name)
{
    string errMsg;

    dbValueResult.clear();

    string sql = "select value from internal_dbnumber where name = '"+name+"'";
    if ( ! executeDatabaseStatement(sql, globalValue_callback, errMsg) )
        return false;

    return ! dbValueResult.empty();
}



void script_setMemoryVec3(string name, script_vec3& v)
{
    if ( getActivePreviewOnly() )
        return;

    globalVec3Map[name] = v;
}

script_vec3 script_getMemoryVec3(string name)
{
    map<string, script_vec3>::iterator it = globalVec3Map.find(name);
    if ( it == globalVec3Map.end() )
        return script_vec3();
    return it->second;
}

bool script_haveMemoryVec3(string name)
{
    return globalVec3Map.find(name) != globalVec3Map.end();
}



void script_setMemoryAffine(string name, script_affine& v)
{
    if ( getActivePreviewOnly() )
        return;

    globalAffineMap[name] = v;
}

script_affine script_getMemoryAffine(string name)
{
    map<string, script_affine>::iterator it = globalAffineMap.find(name);
    if ( it == globalAffineMap.end() )
        return script_affine();
    return it->second;
}

bool script_haveMemoryAffine(string name)
{
    return globalAffineMap.find(name) != globalAffineMap.end();
}













