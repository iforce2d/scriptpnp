
#include <fstream>
#include <sqlite3.h>

#include "db.h"
#include "log.h"

using namespace std;

static sqlite3 *db = NULL;

bool ensureDBFileExists(std::string filename) {

    ifstream f(filename.c_str());

    if (f.good())
    {
        f.close();
        return true;
    }
    else
    {
        g_log.log(LL_INFO, "Creating empty DB file: %s", filename.c_str());

        ofstream f;
        f.open(filename, ios::out);
        if ( ! f.good() ) {
            g_log.log(LL_FATAL, "Could not create empty DB file: %s", filename.c_str());
            return false;
        }

        f.close();
        return true;
    }
}

bool openDatabase(std::string filename)
{
    if ( db ) {
        g_log.log(LL_WARN, "openDatabase called while database already open");
        return true;
    }

    if ( ! ensureDBFileExists(filename) )
        return false;

    int rc = sqlite3_open_v2(filename.c_str(), &db, SQLITE_OPEN_READWRITE, NULL);
    if( SQLITE_OK != rc ) {
        //g_log.log(LL_ERROR, "Can't open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }

    string errMsg;

    string createTable_file = "CREATE TABLE IF NOT EXISTS internal_file ( type TEXT NOT NULL, path TEXT NOT NULL, content BLOB, UNIQUE(type,path) )";
    executeDatabaseStatement(createTable_file, NULL, errMsg);

    string createTable_eventHook = "CREATE TABLE IF NOT EXISTS internal_eventhook ( id INTEGER PRIMARY KEY AUTOINCREMENT, type TEXT NOT NULL, label TEXT NOT NULL, entryfunction TEXT NOT NULL, preview int, UNIQUE(type,label) )";
    executeDatabaseStatement(createTable_eventHook, NULL, errMsg);

    string createTable_tweak = "CREATE TABLE IF NOT EXISTS internal_tweak ( id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, minval REAL NOT NULL, maxval REAL NOT NULL, floatval REAL NOT NULL, UNIQUE(name) )";
    executeDatabaseStatement(createTable_tweak, NULL, errMsg);

    string createTable_windowpos = "CREATE TABLE IF NOT EXISTS internal_windowpos ( id INTEGER PRIMARY KEY AUTOINCREMENT, layouttitle TEXT NOT NULL, windowtitle TEXT NOT NULL, x INT NOT NULL, y INT NOT NULL, w INT NOT NULL, h INT NOT NULL, UNIQUE(layouttitle,windowtitle) )";
    executeDatabaseStatement(createTable_windowpos, NULL, errMsg);

    string createTable_windowsopen = "CREATE TABLE IF NOT EXISTS internal_windowsopen ( layouttitle TEXT PRIMARY KEY, internal text, tables text, tweaks text, buttons text)";
    executeDatabaseStatement(createTable_windowsopen, NULL, errMsg);

    string createTable_globalString = "CREATE TABLE IF NOT EXISTS internal_dbstring ( name TEXT PRIMARY KEY NOT NULL, value TEXT )";
    executeDatabaseStatement(createTable_globalString, NULL, errMsg);

    string createTable_globalNumber = "CREATE TABLE IF NOT EXISTS internal_dbnumber ( name TEXT PRIMARY KEY NOT NULL, value REAL )";
    executeDatabaseStatement(createTable_globalNumber, NULL, errMsg);

    return true;
}

void closeDatabase()
{
    if ( ! db ) {
        g_log.log(LL_WARN, "closeDatabase called while none open");
        return;
    }

    sqlite3_close(db);
}

bool executeDatabaseStatement(std::string statement, dbRowCallback cb, std::string &errMsg)
{
    if ( ! db ) {
        g_log.log(LL_ERROR, "executeDatabaseStatement called while no database open");
        errMsg = "No database open";
        return false;
    }

    g_log.log(LL_DEBUG, "executeDatabaseStatement: %s", statement.c_str());

    char *zErrMsg = 0;
    int rc = sqlite3_exec(db, statement.c_str(), cb, 0, &zErrMsg);
    if( rc != SQLITE_OK ) {
        g_log.log(LL_ERROR, "SQL error: %s", zErrMsg);
        errMsg = "SQL error: " + string(zErrMsg);
        sqlite3_free(zErrMsg);
        return false;
    }

    return true;
}

vector<vector<string> > *genericQueryCallbackReceiver = NULL;

static int genericQuery_callback(void *NotUsed, int argc, char **argv, char **azColName) {

    if ( ! genericQueryCallbackReceiver )
        return 0;

    if ( genericQueryCallbackReceiver->empty() ) {
        vector<string> colNames;
        for ( int i = 0; i < argc; i++ )
            colNames.push_back( azColName[i] ? azColName[i] : "?" );
        genericQueryCallbackReceiver->push_back( colNames );
    }

    vector<string> cols;

    for ( int i = 0; i < argc; i++ )
        cols.push_back( argv[i] ? argv[i] : "NULL" );

    genericQueryCallbackReceiver->push_back( cols );

    return 0;
}

bool executeDatabaseStatement_generic(string statement, vector< vector<string> > * dst, string &errMsg)
{
    if ( dst ) {
        dst->clear();
        genericQueryCallbackReceiver = dst;
    }

    bool ok = executeDatabaseStatement(statement, genericQuery_callback, errMsg);

    genericQueryCallbackReceiver = NULL;

    return ok;
}

bool saveTextToDBFile(string &text, string dbFileType, string path, bool allowOverwriteExisting, string &errMsg)
{
    if ( ! db ) {
        g_log.log(LL_ERROR, "saveTextToDBFile called while no database open");
        errMsg = "No database open";
        return false;
    }

    bool ok = false;

    string insertStr = string(allowOverwriteExisting ? "replace" : "insert") +
                       " into internal_file (type, path, content) values ('"+ dbFileType +"','"+ path +"', ?)";

    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2( db, insertStr.c_str(), -1, &stmt, NULL );
    if (rc != SQLITE_OK) {
        errMsg = sqlite3_errmsg(db);
        g_log.log(LL_ERROR, "saveTextToDBFile sqlite3_prepare_v2 failed: %s", errMsg.c_str());
    } else {
        // SQLITE_STATIC because the statement is finalized before the buffer is freed:
        rc = sqlite3_bind_blob(stmt, 1, text.c_str(), text.length(), SQLITE_STATIC);
        if (rc != SQLITE_OK) {
            errMsg = sqlite3_errmsg(db);
            g_log.log(LL_ERROR, "saveTextToDBFile sqlite3_bind_blob failed: %s", errMsg.c_str());
        } else {
            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                if ( ! allowOverwriteExisting ) {
                    errMsg = "Path already exists";
                    g_log.log(LL_ERROR, "saveTextToDBFile sqlite3_step failed: %s", sqlite3_errmsg(db));
                }
                else {
                    errMsg = sqlite3_errmsg(db);
                    g_log.log(LL_ERROR, "saveTextToDBFile sqlite3_step failed: %s", errMsg.c_str());
                }
            }
            else
                ok = true;
        }
    }

    sqlite3_finalize(stmt);

    return ok;
}


bool loadTextFromDBFile(std::string &text, string dbFileType, std::string path, std::string &errMsg)
{
    if ( ! db ) {
        g_log.log(LL_ERROR, "loadTextFromDBFile called while no database open");
        errMsg = "No database open";
        return false;
    }

    bool ok = false;

    string selectStr = "select content from internal_file where type = '"+ dbFileType +"' and  path = '"+ path +"'";

    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2( db, selectStr.c_str(), selectStr.length(), &stmt, NULL );
    if (rc != SQLITE_OK) {
        errMsg = sqlite3_errmsg(db);
        g_log.log(LL_ERROR, "loadTextFromDBFile sqlite3_prepare_v2 failed: %s", errMsg.c_str());
    } else {
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            errMsg = "(" + dbFileType + ") path not found: '" + path + "'";
            g_log.log(LL_ERROR, "loadTextFromDBFile sqlite3_step failed: %s", errMsg.c_str());
        } else {
            text.assign( (char*)sqlite3_column_text(stmt, 0) );
            ok = true;
        }
    }

    sqlite3_finalize(stmt);

    return ok;
}

int getAllPathsOfTypeFromDBFile(std::vector<std::string> &paths, std::string dbFileType, std::string &errMsg)
{
    if ( ! db ) {
        g_log.log(LL_ERROR, "getAllPathsOfTypeFromDBFile called while no database open");
        errMsg = "No database open";
        return false;
    }

    string selectStr = "select path from internal_file where type = '"+ dbFileType +"' order by path";

    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2( db, selectStr.c_str(), selectStr.length(), &stmt, NULL );
    if (rc != SQLITE_OK) {
        errMsg = sqlite3_errmsg(db);
        g_log.log(LL_ERROR, "getAllPathsOfTypeFromDBFile sqlite3_prepare_v2 failed: %s", errMsg.c_str());
    } else {
        while (SQLITE_ROW == (rc = sqlite3_step(stmt))) {
            paths.push_back( (char*)sqlite3_column_text(stmt, 0) );
        }
    }

    sqlite3_finalize(stmt);

    return paths.size();
}

int loadAllTextOfTypeFromDBFile(std::vector<dbTextFileInfo> &entries, std::string dbFileType, vector<string> &excludePaths, std::string &errMsg)
{
    if ( ! db ) {
        g_log.log(LL_ERROR, "loadAllTextOfTypeFromDBFile called while no database open");
        errMsg = "No database open";
        return false;
    }

    string excludePathStr;
    for (int i = 0; i < (int)excludePaths.size(); i++) {
        if ( i > 0 )
            excludePathStr += ",";
        excludePathStr += "'"+ excludePaths[i] + "'";
    }

    string selectStr = "select path, content from internal_file where type = '"+ dbFileType +"' and path not in ("+excludePathStr+")";

    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2( db, selectStr.c_str(), selectStr.length(), &stmt, NULL );
    if (rc != SQLITE_OK) {
        errMsg = sqlite3_errmsg(db);
        g_log.log(LL_ERROR, "loadAllTextOfTypeFromDBFile sqlite3_prepare_v2 failed: %s", errMsg.c_str());
    } else {
        while (SQLITE_ROW == (rc = sqlite3_step(stmt))) {
            dbTextFileInfo info;
            info.path.assign( (char*)sqlite3_column_text(stmt, 0) );
            info.text.assign( (char*)sqlite3_column_text(stmt, 1) );
            entries.push_back(info);
        }
    }

    sqlite3_finalize(stmt);

    return entries.size();
}

int getNextUntitledPathOfTypeFromDB(std::string dbFileType, std::string &errMsg)
{
    if ( ! db ) {
        g_log.log(LL_ERROR, "getNextUntitledPathOfTypeFromDB called while no database open");
        errMsg = "No database open";
        return false;
    }

    string selectStr = "select ifnull(max(cast (substr(path,9) as INTEGER)),0) as c from internal_file where type = '"+ dbFileType +"' and path like 'untitled%'";

    sqlite3_stmt *stmt = NULL;

    vector<int> existingVals;

    int val = 1;

    int rc = sqlite3_prepare_v2( db, selectStr.c_str(), selectStr.length(), &stmt, NULL );
    if (rc != SQLITE_OK) {
        errMsg = sqlite3_errmsg(db);
        g_log.log(LL_ERROR, "getNextUntitledPathOfTypeFromDB sqlite3_prepare_v2 failed: %s", errMsg.c_str());
    } else {
        if (SQLITE_ROW == (rc = sqlite3_step(stmt))) {
            val = sqlite3_column_int(stmt, 0);
        }
    }

    sqlite3_finalize(stmt);

    return val + 1;
}

int getLastInsertId()
{
    return (int)sqlite3_last_insert_rowid( db );
}


























