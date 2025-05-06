
#include <chrono>
#include <thread>

#include "scriptexecution.h"
#include "script/engine.h"
#include "plangroup.h"
#include "codeEditorDocument.h"
#include "commandEditorWindow.h"
#include "scriptEditorWindow.h"
#include "db.h"
#include "preview.h"
#include "run.h"
#include "util.h"
#include "script/api.h"
#include "tableView.h"

using namespace std;

string scriptPrintBuffer;
vector<codeCompileErrorInfo> compileErrorInfos;

vector<CodeEditorDocument*> commandDocuments;
vector<CodeEditorDocument*> scriptDocuments;
vector<CodeEditorWindow*> commandEditorWindows;
vector<CodeEditorWindow*> scriptEditorWindows;

void addCompileErrorInfo(codeCompileErrorInfo &info)
{
    compileErrorInfos.push_back(info);
}

vector<codeCompileErrorInfo> &getCompileErrorInfos() {
    return compileErrorInfos;
}

void clearCompileErrorInfos() {
    compileErrorInfos.clear();
}


int getUnusedIndex(vector<CodeEditorWindow*> &windows) {
    int unusedIndex = -1;
    bool foundUnusedIndex = false;
    while (! foundUnusedIndex) {
        unusedIndex++;
        foundUnusedIndex = true;
        for (auto w : windows) {
            if ( w->windowIndex == unusedIndex ) {
                foundUnusedIndex = false;
                break;
            }
        }
    }
    return unusedIndex;
}

int getNumOpenScriptEditorWindows()
{
    return (int)scriptEditorWindows.size();
}

int getNumOpenCommandEditorWindows()
{
    return (int)commandEditorWindows.size();
}

void openCommandEditorWindow() {
    int index = getUnusedIndex(commandEditorWindows);
    CodeEditorWindow* w = new CommandEditorWindow(&commandDocuments, "command", index);
    w->init();
    commandEditorWindows.push_back(w);
}

void openScriptEditorWindow() {
    int index = getUnusedIndex(scriptEditorWindows);
    CodeEditorWindow* w = new ScriptEditorWindow(&scriptDocuments, "script", index);
    w->init();
    scriptEditorWindows.push_back(w);
}

CodeEditorWindow* ensureEditorOpenForDocument( string type, string path )
{
    bool ok = false;
    if ( type == "command list" ) {
        string errMsg;
        ok = getDocumentByPath(&commandDocuments, path) || loadDBFile(&commandDocuments, type, path, errMsg);
        if ( ok && commandEditorWindows.empty() )
            openCommandEditorWindow();
        return commandEditorWindows[0];
    }
    else if ( type == "script" ) {
        string errMsg;
        ok = getDocumentByPath(&scriptDocuments, path) || loadDBFile(&scriptDocuments, type, path, errMsg);
        if ( ok && scriptEditorWindows.empty() )
            openScriptEditorWindow();
        return scriptEditorWindows[0];
    }
    return NULL;
}

void ensureNScriptEditorWindowsOpen(int n) {
    int numOpenNow = (int)scriptEditorWindows.size();
    int toClose = numOpenNow - n;
    while ( toClose > 0 ) {
        CodeEditorWindow* w = scriptEditorWindows[numOpenNow-toClose];
        w->shouldRemainOpen = false;
        toClose--;
    }
    int count = 0;
    while ( count++ < 20 && (int)scriptEditorWindows.size() < n)
        openScriptEditorWindow();
}

void ensureNCommandEditorWindowsOpen(int n) {
    int numOpenNow = (int)commandEditorWindows.size();
    int toClose = numOpenNow - n;
    while ( toClose > 0 ) {
        CodeEditorWindow* w = commandEditorWindows[numOpenNow-toClose];
        w->shouldRemainOpen = false;
        toClose--;
    }
    int count = 0;
    while ( count++ < 20 && (int)commandEditorWindows.size() < n)
        openCommandEditorWindow();
}

void saveAllDocuments(vector<CodeEditorDocument*> &documents) {
    for (CodeEditorDocument* d : documents) {
        if ( d->hasOwnFile && d->dirty ) {
            string errMsg;
            saveDBFile(d, d->filename, true, errMsg);
        }
    }
}

bool isAnyDocumentDirty(bool &allDocsHaveOwnFile) {
    allDocsHaveOwnFile = true;
    bool anyDirty = false;
    for (CodeEditorDocument* d : commandDocuments) {
        allDocsHaveOwnFile &= d->hasOwnFile;
        anyDirty |= d->dirty;
    }
    for (CodeEditorDocument* d : scriptDocuments) {
        allDocsHaveOwnFile &= d->hasOwnFile;
        anyDirty |= d->dirty;
    }
    return anyDirty;
}

void beforeRunScript()
{
    saveAllDocuments(commandDocuments);
    for (CodeEditorDocument* cd : commandDocuments)
        cd->clearErrorMarkers();
}

void afterRunScript()
{
    animLoc = lastActualPos;
    memcpy(animRots, lastActualRots, sizeof(animRots));
}

static bool isRunningScriptThread = false;
std::chrono::steady_clock::time_point scriptStartTime = {};

void setIsRunningScriptThread(bool b) {
    g_log.log(LL_DEBUG, "setIsRunningScript %d", b);
    isRunningScriptThread = b;
}

bool getIsRunningScriptThread() {
    return isRunningScriptThread;
}

bool runScript(string moduleName, string funcName, bool previewOnly, void *codeEditorWindow, scriptParams_t *params)
{
    if ( currentlyRunningScriptThread() )
        return false;

    compiledScript_t compiled;

    if ( ! compileScript(moduleName, compiled, codeEditorWindow) )
        return false;

    if ( ! setScriptFunc(compiled, funcName, params, codeEditorWindow) )
        return false;

    planGroup_preview.clear();
    planGroup_preview.setType(1);
    planGroup_run.clear();
    resetTraversePointsAndEvents();

    if ( previewOnly ) {
        setPreviewMoveLimitsFromCurrentActual();
    }

    scriptStartTime = std::chrono::steady_clock::now();

    bool ok = runCompiledFunction(compiled, previewOnly, codeEditorWindow, params);

    std::chrono::steady_clock::time_point scriptEndTime =   std::chrono::steady_clock::now();

    if ( previewOnly ) {
        calculateTraversePointsAndEvents();
    }

    if ( ! ok ) {
        g_log.log(LL_ERROR, "runScript failed for module: %s", compiled.mod->GetName());
        return false;
    }
    else {
        long long timeTaken = std::chrono::duration_cast<std::chrono::microseconds>(scriptEndTime - scriptStartTime).count();
        CodeEditorWindow* w = (CodeEditorWindow*)codeEditorWindow;
        if ( ! w ) {
            if ( ! scriptEditorWindows.empty() )
                w = scriptEditorWindows[0];
        }
        if ( w && ! getIsRunningScriptThread() )
            w->log.log(LL_INFO, NULL, timeTaken, "%s", scriptPrintBuffer.c_str());
    }

    // only discard the module if it's not still running in separate thread!
    if ( previewOnly )
        discardScriptModule(compiled.mod);

    return true;
}

bool setScriptFunc(compiledScript_t &compiled, string funcName, scriptParams_t *params, void *codeEditorWindow) {

    string funcSig = "void  main()";  // keep the two spaces after 'void', so it can be replaced with 'float' below

    if ( ! funcName.empty() ) {
        if ( params ) {
            funcSig = "void  " + funcName + "("; // keep the two spaces after 'void'

            vector<string> paramSigs;
            for ( scriptParam_t& p : params->paramList ) {
                if ( p.type == SPT_INT ) {
                    paramSigs.push_back( "int" );
                }
                else if ( p.type == SPT_STRING ) {
                    paramSigs.push_back( "string" );
                }
            }

            funcSig += joinStringVec( paramSigs, "," );

            funcSig += ")";
        }
        else {
            funcSig = "void  " +funcName +"()"; // keep the two spaces after 'void'
        }
    }

    CodeEditorWindow* w = (CodeEditorWindow*)codeEditorWindow;
    if ( ! w ) {
        if ( ! scriptEditorWindows.empty() )
            w = scriptEditorWindows[0];
    }

    vector<string> otherSigs;
    otherSigs.push_back( "bool  " );
    otherSigs.push_back( "int   " );
    otherSigs.push_back( "float " );

    asIScriptFunction *func = compiled.mod->GetFunctionByDecl( funcSig.c_str() );
    if ( ! func ) {
        for (int i = 0; i < (int)otherSigs.size(); i++) {
            string otherSig = otherSigs[i];
            for (int c = 0; c < 5; c++)
                funcSig[c] = otherSig[c];
            if ( (func = compiled.mod->GetFunctionByDecl( funcSig.c_str() )) )
                break;
        }
    }

    if ( ! func ) {

        funcSig = funcSig.substr(6);

        g_log.log(LL_ERROR, "GetFunctionByDecl failed looking for '%s'", funcSig.c_str());
        if ( w )
            w->log.log(LL_ERROR, NULL, 0, "[%s] Could not find entry point '%s'", logPrefixArray[LL_ERROR], funcSig.c_str());
        discardScriptModule(compiled.mod);
        return false;
    }

    compiled.func = func;

    return true;
}

string autogenPrefix = "DB";

extern vector<string> autogenScriptTableNames;

bool appendGeneratedClasses(string moduleName) {
    /*vector<string> generatedClassTables;
    generatedClassTables.push_back( "Feeder" );
    generatedClassTables.push_back( "Feedertype" );
    generatedClassTables.push_back( "Part" );
    generatedClassTables.push_back( "Tape" );
    generatedClassTables.push_back( "Package" );*/

    for (string tableName : autogenScriptTableNames ) {

        TableData td;
        td.name = tableName;
        fetchTableData_basic(td);

        tableName = upperCaseInitial(tableName);

        if ( td.primaryKeyColumnIndex > -1 ) {
            string classDef = "class "+autogenPrefix+tableName+" {\n";

            classDef += "    bool valid;\n";

            // direct member variables
            for (int i = 0; i < (int)td.colNames.size(); i++) {
                string colName = td.colNames[i];
                int colType = td.colTypes[i];
                if ( colType == CDT_INTEGER ) {
                    classDef += "    int ";
                }
                else if ( colType == CDT_REAL ) {
                    classDef += "    float ";
                }
                else if ( colType == CDT_TEXT ) {
                    classDef += "    string ";
                }
                classDef += colName+";\n";
            }

            // relation member variables (other auto-generated classes)
            for (int i = 0; i < (int)td.relations.size(); i++) {
                TableRelation& tr = td.relations[i];
                if ( ! tr.otherTableName.empty() ) {
                    classDef += "    "+autogenPrefix + upperCaseInitial(tr.otherTableName) + " " + tr.otherTableName + ";\n";
                }
            }

            // constructor
            classDef += "    "+autogenPrefix+tableName+"() { valid = false; }\n";

            // str() function
            classDef += "    string str(bool fullTree = false, string indent = '  ') {\n        return '"+tableName+":'";
            classDef += "+'\\n'+indent+'valid = '+valid";
            for (int i = 0; i < (int)td.colNames.size(); i++) {
                string colName = td.colNames[i];
                classDef += "+'\\n'+indent+'"+colName+" = '+"+colName;
            }
            for (int i = 0; i < (int)td.relations.size(); i++) {
                TableRelation& tr = td.relations[i];
                if ( ! tr.otherTableName.empty() ) {
                    classDef += "+'\\n'+indent+'"+tr.otherTableName+" = '+(fullTree?"+tr.otherTableName+".str(true,indent+indent):'(object)')";
                }
            }
            classDef += ";\n    }\n";

            // save() function
            classDef += "void save() {\n";
                classDef += "    string sql = 'replace into "+tableName+" (";
                classDef += joinStringVec( td.colNames, "," );
                classDef += ") values ('+";
                vector<string> vals;
                for (int i = 0; i < (int)td.colNames.size(); i++) {
                    string colName = td.colNames[i];
                    int colType = td.colTypes[i];
                    if ( colType == CDT_TEXT )
                        vals.push_back( "'\"'+"+colName+"+'\"'" );
                    else
                        vals.push_back( colName );
                }
                classDef += joinStringVec( vals, "+','+" );
                classDef += "+')';\n";
                //classDef += "    print(sql);\n";
                classDef += "    dbQuery(sql);\n";
                classDef += "    refreshTableView('"+tableName+"');\n";
            classDef += "}\n";

            classDef += "}\n"; // end of class

            // ---------------------------------------------
            // global functions follow

            // print
            classDef += "void print("+autogenPrefix+tableName+" obj) { print( obj.str() ); }\n";

            // get single object by id
            classDef += autogenPrefix+tableName+" getDB"+tableName+"(int id) {\n";
            classDef += "    "+autogenPrefix+tableName+" instance;\n";
            classDef += "    dbResult res = dbQuery('select * from "+tableName+" where id = '+id);\n";
            classDef += "    if (res.numRows < 1) \n";
            classDef += "        return instance;\n";
            classDef += "    dbRow row = res.row(0);\n";
            for (int i = 0; i < (int)td.colNames.size(); i++) {
                string colName = td.colNames[i];
                int colType = td.colTypes[i];
                if ( colType == CDT_INTEGER )
                    classDef += "    instance."+colName+" = parseInt( row.col('"+colName+"') );\n";
                else if ( colType == CDT_REAL )
                    classDef += "    instance."+colName+" = parseFloat( row.col('"+colName+"') );\n";
                else if ( colType == CDT_TEXT )
                    classDef += "    instance."+colName+" = row.col('"+colName+"');\n";
            }
            for (int i = 0; i < (int)td.relations.size(); i++) {
                TableRelation& tr = td.relations[i];
                if ( ! tr.otherTableName.empty() ) {
                    classDef += "    instance."+tr.otherTableName+" = getDB"+upperCaseInitial(tr.otherTableName)+"(instance."+tr.fullColumnName+");\n";
                }
            }
            classDef += "    instance.valid = true;\n";
            classDef += "    return instance;\n";
            classDef += "}\n";

            // get array of objects by 'where' clause
            classDef += autogenPrefix+tableName+"[] getDB"+tableName+"s(string whereClause = '') {\n";
            classDef += "    "+autogenPrefix+tableName+"[] arr;\n";
            classDef += "    dbResult res = dbQuery('select * from "+tableName+" '+whereClause);\n";
            classDef += "    if (res.numRows < 1) \n";
            classDef += "        return arr;\n";
            classDef += "    for (uint i = 0; i < res.numRows; i++) { \n";
            classDef += "        dbRow row = res.row(i);\n";
            classDef += "        "+autogenPrefix+tableName+" instance;\n";
            for (int i = 0; i < (int)td.colNames.size(); i++) {
                string colName = td.colNames[i];
                int colType = td.colTypes[i];
                if ( colType == CDT_INTEGER )
                    classDef += "        instance."+colName+" = parseInt( row.col('"+colName+"') );\n";
                else if ( colType == CDT_REAL )
                    classDef += "        instance."+colName+" = parseFloat( row.col('"+colName+"') );\n";
                else if ( colType == CDT_TEXT )
                    classDef += "        instance."+colName+" = row.col('"+colName+"');\n";
            }
            for (int i = 0; i < (int)td.relations.size(); i++) {
                TableRelation& tr = td.relations[i];
                if ( ! tr.otherTableName.empty() ) {
                    classDef += "        instance."+tr.otherTableName+" = getDB"+upperCaseInitial(tr.otherTableName)+"(instance."+tr.fullColumnName+");\n";
                }
            }
            classDef += "        instance.valid = true;\n";
            classDef += "        arr.insertLast( instance );\n";
            classDef += "    }\n";
            classDef += "    return arr;\n";
            classDef += "}\n";


            // get by other column values
            for (int i = 0; i < (int)td.colNames.size(); i++) {
                string colName = td.colNames[i];
                if ( colName == "id" )
                    continue;
                int colType = td.colTypes[i];
                string valType = "int";
                string val = "val";
                if ( colType == CDT_REAL )
                    valType = "float";
                else if ( colType == CDT_TEXT ) {
                    valType = "string";
                    val = "'\"'+val+'\"'";
                }
                classDef += autogenPrefix+tableName+" getDB"+tableName+"By"+upperCaseInitial(colName)+"("+valType+" val, bool create = false) {\n";
                classDef += "    dbResult res = dbQuery('select id from "+tableName+" where "+colName+" = '+"+val+");\n";
                classDef += "    if (res.numRows < 1) {\n";
                classDef += "        if ( create ) {\n";
                classDef += "            string sql = 'insert into "+tableName+" (id,"+colName+") values (NULL,'+"+val+"+')';\n";
                classDef += "            print(sql);\n";
                classDef += "            dbQuery(sql);\n";
                classDef += "            refreshTableView('"+tableName+"');\n";
                classDef += "            "+autogenPrefix+tableName+" instance;\n";
                classDef += "            instance.id = getLastInsertId();\n";
                classDef += "            instance."+colName+" = val;\n";
                classDef += "            instance.valid = true;\n";
                classDef += "            return instance;\n";
                classDef += "        }\n";
                classDef += "        return "+autogenPrefix+tableName+"();\n";
                classDef += "    }\n";
                classDef += "    return getDB"+tableName+"( parseInt(res.row(0).col('id')) );\n";
                classDef += "}\n";
            }


            printf( classDef.c_str() ); fflush(stdout);

            string sectionName = tableName+"_section";
            if ( ! addScriptSection(moduleName, sectionName.c_str(), classDef.c_str()) ) {
                g_log.log(LL_ERROR, "addScriptSection failed for '%s'", sectionName.c_str());
                return false;
            }
        }
    }

    return true;
}

bool compileScript(string moduleName, compiledScript_t &compiled, void *codeEditorWindow)
{
    saveAllDocuments(commandDocuments);

    CodeEditorWindow* w = (CodeEditorWindow*)codeEditorWindow;

    if ( ! w ) {
        if ( ! scriptEditorWindows.empty() )
            w = scriptEditorWindows[0];
    }

    scriptPrintBuffer = "";
    clearCompileErrorInfos();
    //log.grayOutExistingText();

    //string moduleName = "mymodule";

    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();

    asIScriptModule* mod = createScriptModule(moduleName);

    if ( ! appendGeneratedClasses(moduleName) ) {
        discardScriptModule(mod);
        return false;
    }

    vector<string> openDocumentFiles;
    for ( CodeEditorDocument* d : scriptDocuments) {
        d->clearErrorMarkers();
        openDocumentFiles.push_back(d->filename);
        if ( ! addScriptSection(moduleName, d->filename, d->editor.GetText()) ) {
            g_log.log(LL_ERROR, "addScriptSection failed for '%s'", d->filename.c_str());
            discardScriptModule(mod);
            return false;
        }
    }

    vector<dbTextFileInfo> scriptSections;
    string errMsg;
    loadAllTextOfTypeFromDBFile(scriptSections, "script", openDocumentFiles, errMsg);
    for (dbTextFileInfo &info : scriptSections) {
        if ( ! addScriptSection(moduleName, info.path, info.text) ) {
            g_log.log(LL_ERROR, "addScriptSection failed for '%s'", info.path.c_str());
            discardScriptModule(mod);
            return false;
        }
    }

    bool ok = buildScriptModule(mod);

    std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
    long long compileTime = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    g_log.log(LL_DEBUG, "Script compile time: %lld us", compileTime);

    if ( w )
        w->setupErrorMarkers(NULL);

    if  ( ! ok ) {
        g_log.log(LL_SCRIPT_ERROR, "buildScriptModule failed for module: %s", moduleName.c_str());
        discardScriptModule(mod);
        return false;
    }

    compiled.mod = mod;

    return true;
}

pthread_t scriptRunThread;
bool didScriptRunJustComplete = false; // true if the script ran all the way to the end

// this includes the state where a script is paused
bool currentlyRunningScriptThread() {
    return getIsRunningScriptThread();
}

struct scriptRunThreadInfo {
    asIScriptContext* ctx;
    asIScriptModule* mod;
};

// need to keep this in scope, used for resume as well as initial startup
scriptRunThreadInfo scriptThreadStartupInfo;

void* scriptRunThreadFunc(void* ptr)
{
    UNUSED( ptr ); // obtain from global variable above
    //scriptRunThreadInfo* info = (scriptRunThreadInfo*)ptr;
    //asIScriptContext *ctx = info->ctx;
    //asIScriptModule* mod = info->mod;

    bool stillRunning = executeScriptContext( scriptThreadStartupInfo.ctx );

    std::chrono::steady_clock::time_point t1 =   std::chrono::steady_clock::now();
    long long timeTaken = std::chrono::duration_cast<std::chrono::microseconds>(t1 - scriptStartTime).count();
    g_log.log(LL_INFO, "Script%s run took %lld us", stillRunning?" (partial)":"", timeTaken);

    if ( ! stillRunning )
        discardScriptModule( scriptThreadStartupInfo.mod );

    didScriptRunJustComplete = ! stillRunning;

    return (void*)1;
}

bool checkScriptRunThreadComplete()
{
    if ( didScriptRunJustComplete ) {
        void* ok = 0;
        pthread_join(scriptRunThread, &ok);

        didScriptRunJustComplete = false;

        setActivePreviewOnly(false);
        setActiveScriptLog(NULL);

        //this_thread::sleep_for( 2000ms );

        setIsRunningScriptThread( false );

        g_log.log(LL_DEBUG, "finished script");

        return true;
    }

    return false;
}

bool runCompiledFunction(compiledScript_t &compiled, bool previewOnly, void *codeEditorWindow, scriptParams_t *params)
{
    if ( ! compiled.mod || ! compiled.func ) {
        g_log.log(LL_ERROR, "runCompiledFunction: invalid compiled function");
        return false;
    }

    CodeEditorWindow* w = (CodeEditorWindow*)codeEditorWindow;

    if ( ! w ) {
        if ( ! scriptEditorWindows.empty() )
            w = scriptEditorWindows[0];
    }

    setActivePreviewOnly(previewOnly);
    if ( w )
        setActiveScriptLog(&w->log);

    bool runThreaded = ! previewOnly;
    if ( runThreaded ) {

        //asIScriptContext* ctx = createScriptContext(compiled.func);

        didScriptRunJustComplete = false;
        setIsRunningScriptThread( true );

        scriptThreadStartupInfo.ctx = createScriptContext(compiled.func, params);
        scriptThreadStartupInfo.mod = compiled.mod;

        int ret = pthread_create(&scriptRunThread, NULL, scriptRunThreadFunc, &scriptThreadStartupInfo);
        if ( ret ) {
            g_log.log(LL_FATAL, "Script run thread creation failed!");
            return false;
        }

        return true; // for threaded case, return 'still running' status
    }

    asIScriptContext* ctx = createScriptContext(compiled.func, params);

    executeScriptContext(ctx);

    setActivePreviewOnly(false);
    setActiveScriptLog(NULL);

    //isRunningScriptThread = false;

    return true; // always return true for non-threaded preview case, meaning success
}

bool runCompiledFunction_simple(compiledScript_t &compiled)
{
    if ( ! compiled.mod || ! compiled.func ) {
        g_log.log(LL_ERROR, "runCompiledFunction_simple: invalid compiled function");
        return false;
    }

    CodeEditorWindow* w = NULL;
    if ( ! scriptEditorWindows.empty() )
        w = scriptEditorWindows[0];

    if ( w )
        setActiveScriptLog(&w->log);

    asIScriptContext* ctx = createScriptContext(compiled.func, NULL);
    int r = ctx->Execute();
    if ( r == asEXECUTION_EXCEPTION )
        g_log.log(LL_ERROR, "Exception occurred while executing script: %s", ctx->GetExceptionString());
    ctx->Release();

    return true; // always return true for non-threaded preview case, meaning success
}

void discardCompiledFunction(compiledScript_t &compiled)
{
    if ( compiled.mod ) {
        discardScriptModule(compiled.mod);
    }
    compiled.mod = NULL;
    compiled.func = NULL;
}




void pauseScript()
{
    if ( currentlyRunningScriptThread() ) {
        asIScriptContext *ctx = getCurrentScriptContext();
        if ( ctx ) {

            g_log.log(LL_DEBUG, "pauseScript");

            ctx->Suspend();
            //isScriptPaused = true; let executeScriptContext do this, because the script may take a while to actually respond

            std::chrono::steady_clock::time_point t1 =   std::chrono::steady_clock::now();
            long long timeTaken = std::chrono::duration_cast<std::chrono::microseconds>(t1 - scriptStartTime).count();
            g_log.log(LL_INFO, "Script run took %lld us", timeTaken);
        }
    }
}



bool resumeScript()
{
    if ( currentlyPausingScript() ) {
        asIScriptContext *ctx = getCurrentScriptContext();
        if ( ctx ) {

            g_log.log(LL_DEBUG, "resumeScript");

            scriptStartTime = std::chrono::steady_clock::now();

            int ret = pthread_create(&scriptRunThread, NULL, scriptRunThreadFunc, &scriptThreadStartupInfo);
            if ( ret ) {
                g_log.log(LL_FATAL, "Script run thread creation failed!");
                return false;
            }

            //isScriptPaused = false;
        }
    }

    return false;
}


extern bool waitingForPreviousActualRun;

void abortScript()
{
    if ( currentlyRunningScriptThread() ) {
        asIScriptContext *ctx = getCurrentScriptContext();

        if ( ctx ) {
            if ( currentlyPausingScript() ) { // script is NOT running, so it will NOT periodically check if it needs to abort, and NOT continue through executeScriptContext, so need to clear everything here
                g_log.log(LL_DEBUG, "abortScript during pause");

                setIsScriptPaused( false );
                setIsRunningScriptThread( false );

                ctx->Abort();

                cleanupScriptContext(ctx);
                discardScriptModule( scriptThreadStartupInfo.mod );
            }
            else { // script is running, so it will periodically check if it needs to abort

                g_log.log(LL_DEBUG, "abortScript during run");

                //setIsRunningScriptThread( false );

                ctx->Abort(); // this sets the script context status to aborted, but it can only check the status in between script function calls
                waitingForPreviousActualRun = false; // need to exit potential wait loop in doActualRun

                //cleanupScriptContext(ctx);  don't do any cleanup here, let executeScriptContext do it
            }
        }
    }
}





































