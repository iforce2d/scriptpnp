#include "codeEditorDocument.h"
#include "codeEditorPalettes.h"
#include "db.h"
using namespace std;

CodeEditorDocument::CodeEditorDocument()
{
    hasOwnFile = false;
    dirty = false;
    shouldClose = false;
    //editor = TextEditor();
    editor.SetPalette(myPalette);
}


bool loadDBFile(vector<CodeEditorDocument*> *documents, string dbFileType, string path, string &errMsg)
{
    if ( path.empty() ) {
        errMsg = "Can't loadDBFile from empty path";
        g_log.log(LL_ERROR, "%s", errMsg.c_str());
        return false;
    }

    bool alreadyOpened = false;
    for (CodeEditorDocument* d : *documents) {
        if ( d->filename == path ) {
            alreadyOpened = true;
            break;
        }
    }

    if ( alreadyOpened ) {
        errMsg = "Can't loadDBFile, already opened: " + path;
        g_log.log(LL_ERROR, "%s", errMsg.c_str());
        return false;
    }

    string text;
    if( ! loadTextFromDBFile(text, dbFileType, path, errMsg) ) {
        g_log.log(LL_ERROR, "Failed to load DB file '%s' : %s", path.c_str(), errMsg.c_str());
        return false;
    }

    g_log.log(LL_INFO, "Loaded DB file: '%s'", path.c_str());
    newCodeDocument(documents, dbFileType);
    CodeEditorDocument* doc = documents->back();
    doc->editor.SetText(text);
    doc->editor.ClearTextChangedStatus();
    doc->filename = path;
    doc->dirty = false;
    doc->hasOwnFile = true;

    return true;
}

//static int newDocCounter = 0;

int getNextUntitledPathOfTypeFromTabs(vector<CodeEditorDocument*> *docs)
{
    int highest = 1;
    for ( int v = 0; v < (int)docs->size(); v++ ) {
        CodeEditorDocument* existingDoc = docs->at(v);
        if ( existingDoc->filename.find("untitled") == 0 ) {
            string numStr = existingDoc->filename.substr(8);
            int num = atoi(numStr.c_str());
            highest = max(num, highest);
        }
    }
    return highest + 1;
}

CodeEditorDocument* newCodeDocument(vector<CodeEditorDocument*> *docs, string type) {

    string errMsg;

    int nextByDB = getNextUntitledPathOfTypeFromDB(type, errMsg);
    int nextByTabs = getNextUntitledPathOfTypeFromTabs(docs);
    int nextNum = max( nextByDB, nextByTabs );

    CodeEditorDocument* doc = new CodeEditorDocument();
    doc->fileType = type;
    doc->filename = "untitled" + to_string(nextNum);
    docs->push_back(doc);

    //g_log.log(LL_DEBUG, "newCodeDocument: %s", doc->filename.c_str());

    return doc;
}


CodeEditorDocument *getDocumentByPath(std::vector<CodeEditorDocument *> *docs, std::string path)
{
    for (CodeEditorDocument* d : *docs) {
        if ( d->filename == path )
            return d;
    }
    return NULL;
}
