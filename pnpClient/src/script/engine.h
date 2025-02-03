#ifndef SCRIPTENGINE_H
#define SCRIPTENGINE_H

#include <vector>
#include <string>

#include <angelscript.h>

bool setupScriptEngine();
void cleanupScriptEngine();

void generateScriptDocs();

asIScriptModule *createScriptModule(std::string moduleName);
void discardScriptModule(asIScriptModule *mod);
bool addScriptSectionFromFile(std::string moduleName, std::string sectionName, std::string file);
bool addScriptSection(std::string moduleName, std::string sectionName, std::string code);
bool buildScriptModule(asIScriptModule *mod);

asITypeInfo* GetScriptTypeIdByDecl(const char *decl);

void setActiveScriptLog(void* sl);
void* getActiveScriptLog();
void removeActiveScriptLog(void* sl);

void setActiveCommandListPath(std::string sl);
std::string getActiveCommandListPath();

void setActivePreviewOnly(bool b);
bool getActivePreviewOnly();

asIScriptContext* createScriptContext(asIScriptFunction *func);
asIScriptContext* getCurrentScriptContext();
void cleanupScriptContext(asIScriptContext* ctx);
bool executeScriptContext(asIScriptContext *ctx);

void setIsScriptPaused(bool tf);
bool currentlyPausingScript();

#endif // SCRIPTENGINE_H
