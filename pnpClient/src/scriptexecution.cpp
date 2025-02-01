
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
#include "script/api.h"

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

void setIsRunningScript(bool b) {
    g_log.log(LL_DEBUG, "setIsRunningScript %d", b);
    isRunningScriptThread = b;
}

bool getIsRunningScript() {
    return isRunningScriptThread;
}

bool runScript(string moduleName, string funcName, bool previewOnly, void *codeEditorWindow)
{
    if ( currentlyRunningScriptThread() )
        return false;

    scriptStartTime = std::chrono::steady_clock::now();

    compiledScript_t compiled;

    if ( ! compileScript(moduleName, funcName, compiled, codeEditorWindow) )
        return false;

    planGroup_preview.clear();
    planGroup_run.clear();
    resetTraversePointsAndEvents();

    if ( previewOnly ) {
        setPreviewMoveLimitsFromCurrentActual();
    }

    bool ok = runCompiledFunction(compiled, previewOnly, codeEditorWindow);

    if ( previewOnly ) {
        calculateTraversePointsAndEvents();
    }

    if ( ! ok ) {
        g_log.log(LL_ERROR, "runScript failed for module: %s", compiled.mod->GetName());
        return false;
    }
    else {
        std::chrono::steady_clock::time_point t1 =   std::chrono::steady_clock::now();
        long long timeTaken = std::chrono::duration_cast<std::chrono::microseconds>(t1 - scriptStartTime).count();
        CodeEditorWindow* w = (CodeEditorWindow*)codeEditorWindow;
        if ( ! w ) {
            if ( ! scriptEditorWindows.empty() )
                w = scriptEditorWindows[0];
        }
        if ( w && ! getIsRunningScript() )
            w->log.log(LL_INFO, NULL, timeTaken, "%s", scriptPrintBuffer.c_str());
    }

    discardScriptModule(compiled.mod);

    return true;
}

bool compileScript(string moduleName, string funcName, compiledScript_t &compiled, void *codeEditorWindow)
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

    asIScriptModule* mod = createScriptModule(moduleName);

    vector<string> openDocumentFiles;
    for ( CodeEditorDocument* d : scriptDocuments) {
        d->clearErrorMarkers();
        openDocumentFiles.push_back(d->filename);
        if ( ! addScriptSection(moduleName, d->filename, d->editor.GetText()) ) {
            g_log.log(LL_ERROR, "addScriptSection failed for '%s'", d->filename.c_str());
            discardScriptModule(mod);
            return NULL;
        }
    }

    vector<dbTextFileInfo> scriptSections;
    string errMsg;
    loadAllTextOfTypeFromDBFile(scriptSections, "script", openDocumentFiles, errMsg);
    for (dbTextFileInfo &info : scriptSections) {
        if ( ! addScriptSection(moduleName, info.path, info.text) ) {
            g_log.log(LL_ERROR, "addScriptSection failed for '%s'", info.path.c_str());
            discardScriptModule(mod);
            return NULL;
        }
    }

    bool ok = buildScriptModule(mod);

    if ( w )
        w->setupErrorMarkers(NULL);

    if  ( ! ok ) {
        g_log.log(LL_SCRIPT_ERROR, "buildScriptModule failed for module: %s", moduleName.c_str());
        discardScriptModule(mod);
        return NULL;
    }

    // string funcName = entryFunction;
    if ( funcName.empty() )
        funcName = "main";
    funcName = "void " +funcName +"()";

    asIScriptFunction *func = mod->GetFunctionByDecl( funcName.c_str() );
    if( ! func )
    {
        g_log.log(LL_ERROR, "GetFunctionByDecl failed looking for '%s'", funcName.c_str());
        if ( w )
            w->log.log(LL_ERROR, NULL, 0, "[%s] Could not find entry point '%s'", logPrefixArray[LL_ERROR], funcName.c_str());

        discardScriptModule(mod);
        return NULL;
    }

    compiled.mod = mod;
    compiled.func = func;

    return true;
}

pthread_t scriptRunThread;
bool isScriptRunThreadComplete = false;

// this includes the state where a script is paused
bool currentlyRunningScriptThread() {
    return getIsRunningScript();
}

void* scriptRunThreadFunc(void* ptr)
{
    asIScriptContext *ctx = (asIScriptContext*)ptr;

    bool stillRunning = executeScriptContext(ctx);

    std::chrono::steady_clock::time_point t1 =   std::chrono::steady_clock::now();
    long long timeTaken = std::chrono::duration_cast<std::chrono::microseconds>(t1 - scriptStartTime).count();
    g_log.log(LL_INFO, "Script%s run took %lld us", stillRunning?" (partial)":"", timeTaken);

    isScriptRunThreadComplete = ! stillRunning;

    return (void*)1;
}

bool checkScriptRunThreadComplete()
{
    if ( isScriptRunThreadComplete ) {
        void* ok = 0;
        pthread_join(scriptRunThread, &ok);

        isScriptRunThreadComplete = false;

        setActivePreviewOnly(false);
        setActiveScriptLog(NULL);

        //this_thread::sleep_for( 2000ms );

        g_log.log(LL_DEBUG, "finished script");

        setIsRunningScript( false );

        return true;
    }

    return false;
}

bool runCompiledFunction(compiledScript_t &compiled, bool previewOnly, void *codeEditorWindow)
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

        asIScriptContext* ctx = createScriptContext(compiled.func);

        isScriptRunThreadComplete = false;
        setIsRunningScript( ! previewOnly );

        int ret = pthread_create(&scriptRunThread, NULL, scriptRunThreadFunc, ctx);
        if ( ret ) {
            g_log.log(LL_FATAL, "Script run thread creation failed!");
            return false;
        }

        return true; // for threaded case, return 'still running' status
    }

    asIScriptContext* ctx = createScriptContext(compiled.func);

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

    asIScriptContext* ctx = createScriptContext(compiled.func);
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


bool isScriptPaused = false;

bool currentlyPausingScriptThread()
{
    return isScriptPaused;
}

void pauseScript()
{
    if ( currentlyRunningScriptThread() ) {
        asIScriptContext *ctx = getCurrentScriptContext();
        if ( ctx ) {

            g_log.log(LL_DEBUG, "pauseScript");

            ctx->Suspend();
            isScriptPaused = true;

            std::chrono::steady_clock::time_point t1 =   std::chrono::steady_clock::now();
            long long timeTaken = std::chrono::duration_cast<std::chrono::microseconds>(t1 - scriptStartTime).count();
            g_log.log(LL_INFO, "Script run took %lld us", timeTaken);
        }
    }
}



bool resumeScript()
{
    if ( isScriptPaused ) {
        asIScriptContext *ctx = getCurrentScriptContext();
        if ( ctx ) {

            g_log.log(LL_DEBUG, "resumeScript");

            scriptStartTime = std::chrono::steady_clock::now();

            int ret = pthread_create(&scriptRunThread, NULL, scriptRunThreadFunc, ctx);
            if ( ret ) {
                g_log.log(LL_FATAL, "Script run thread creation failed!");
                return false;
            }

            isScriptPaused = false;
        }
    }

    return false;
}



void abortScript()
{
    if ( currentlyRunningScriptThread() ) {
        asIScriptContext *ctx = getCurrentScriptContext();
        if ( ctx ) {

            g_log.log(LL_DEBUG, "abortScript");

            isScriptPaused = false;
            setIsRunningScript( false );

            ctx->Abort();

            //cleanupScriptContext(ctx);  don't do any cleanup here, let executeScriptContext do it
        }
    }
}





































