#ifndef CODE_EDITOR_WINDOW_H
#define CODE_EDITOR_WINDOW_H

#include <string>
#include <vector>

#include "codeEditorDocument.h"
#include "scriptlog.h"

struct codeEditorButtonFeedback_t {
    CodeEditorDocument* document;
    bool shouldPreview;
    bool shouldRun;
    codeEditorButtonFeedback_t() {
        document = NULL;
        shouldPreview = false;
        shouldRun = false;
    }
};

class CodeEditorWindow {
protected:
    ImFont* currentFont;
    std::vector<CodeEditorDocument*> *documents;
    CodeEditorDocument* currentDocument;
    CodeEditorDocument* pendingSelectDocument;
public:
    ScriptLog log;
    std::string windowId; // more like a category? scope?
    int windowIndex; // unique id among all code editor windows
    bool shouldRemainOpen;
    codeEditorButtonFeedback_t buttonFeedback;

    CodeEditorWindow(std::vector<CodeEditorDocument*> *documents, std::string id, int index);
    virtual ~CodeEditorWindow() {}
    void init();
    void setFont(ImFont* font);
    void cleanup();

    std::vector<CodeEditorDocument*> * getDocuments() { return documents; }

    virtual std::string getDBFileType() = 0;
    virtual std::string getWindowTitle();
    virtual void renderCustomSection() {}
    virtual void preRenderDoc(CodeEditorDocument* doc) {}
    virtual int pushStyleColors() { return 0; }
    virtual bool shouldShowOutputPane() { return false; }
    virtual void showOutputPane();
    virtual std::string getOpenDialogTitle() = 0;

    void doImport();
    void doExport();
    void doDelete();

    //bool saveDBFile(std::string path, bool allowOverwriteExisting, std::string &errMsg);
    //bool loadDBFile(std::string path, std::string &errMsg);

    void showMenuBar(bool &doOpen, bool &doSave, bool &doSaveAs, bool &doDelete);
    void showOpenDialogPopup(bool openingNow);
    void showSaveAsDialogPopup(bool openingNow);
    void showTabBar(/*CodeEditorDocument *doc,*/ bool &doConfirmClose, CodeEditorDocument *&docToClose);
    void showConfirmCloseDialog(CodeEditorDocument* doc);
    void showConfirmDeleteDialog(CodeEditorDocument* doc);

    void showCompileErrorInfos();
    virtual void setupErrorMarkers(CodeEditorDocument *whichDoc);


    void setSelectedDocument(CodeEditorDocument* doc);
    void onDocumentClosed(CodeEditorDocument* doc);

    void render();
    int getText(std::vector<std::string> &lines);

    void gotoError(errorGotoInfo &info);

};

bool loadDiskFile(CodeEditorDocument* doc, std::string path);
bool saveDiskFile(CodeEditorDocument* doc, std::string path);

bool saveDBFile(CodeEditorDocument* doc, std::string path, bool allowOverwriteExisting, std::string &errMsg);
void gotoCodeCompileError(CodeEditorWindow* w, errorGotoInfo &info);

bool deleteDBFile(CodeEditorDocument* doc);

#endif // CODE_EDITOR_WINDOW_H
