#ifndef SCRIPTEXECUTION_H
#define SCRIPTEXECUTION_H

#include <vector>
#include <string>

#include "log.h"

extern std::string scriptPrintBuffer;

extern std::vector<class CodeEditorDocument*> commandDocuments;
extern std::vector<class CodeEditorDocument*> scriptDocuments;
extern std::vector<class CodeEditorWindow*> commandEditorWindows;
extern std::vector<class CodeEditorWindow*> scriptEditorWindows;

enum codeType_e {
    CT_COMMAND_LIST,
    CT_SCRIPT,
};

struct codeCompileErrorInfo
{
    codeType_e fileType;
    std::string section;
    int         row;
    int         col;
    logLevel_e  type;
    std::string message;
};

struct compiledScript_t {
    class asIScriptModule* mod;
    class asIScriptFunction* func;
    compiledScript_t() {
        mod = NULL;
        func = NULL;
    }
};

enum scriptParamType_t {
    SPT_INT,
    SPT_STRING
};

struct scriptParam_t {
    scriptParamType_t type;
    std::string stringVal;
    int intVal;
};

struct scriptParams_t {
    std::vector< scriptParam_t > paramList;
};

extern std::vector<std::string> dirtyDocuments;

class CodeEditorWindow* ensureEditorOpenForDocument( std::string type, std::string path );
void saveAllDocuments(std::vector<class CodeEditorDocument*> &documents);
bool isAnyDocumentDirty(bool &allDocsHaveOwnFile);

void openCommandEditorWindow();
void openScriptEditorWindow();

int getNumOpenScriptEditorWindows();
int getNumOpenCommandEditorWindows();
void ensureNScriptEditorWindowsOpen(int n);
void ensureNCommandEditorWindowsOpen(int n);

void addCompileErrorInfo(codeCompileErrorInfo &info);
std::vector<codeCompileErrorInfo> &getCompileErrorInfos();
void clearCompileErrorInfos();

void beforeRunScript();
void afterRunScript();

bool runScript(std::string moduleName, std::string funcName, bool previewOnly, void* codeEditorWindow = NULL, scriptParams_t *params = NULL);

bool compileScript(std::string moduleName, compiledScript_t &compiled, void* codeEditorWindow = NULL);
bool setScriptFunc(compiledScript_t &compiled, std::string funcName, scriptParams_t *params = NULL, void* codeEditorWindow = NULL);
bool runCompiledFunction(compiledScript_t &compiled, bool previewOnly, void *codeEditorWindow, scriptParams_t *params = NULL);
bool runCompiledFunction_simple(compiledScript_t &compiled);
void discardCompiledFunction(compiledScript_t &compiled);

bool currentlyRunningScriptThread();
bool checkScriptRunThreadComplete();

bool currentlyPausingScript();

void pauseScript();
bool resumeScript();
void abortScript();

#endif // SCRIPTEXECUTION_H
