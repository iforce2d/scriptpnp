#ifndef CODE_EDITOR_DOCUMENT_H
#define CODE_EDITOR_DOCUMENT_H

#include <vector>
#include <string>

#include "TextEditor.h"
#include "log.h"

enum codeEditorDocumentType_e {
    CEDT_COMMAND,
    CEDT_SCRIPT,
};

class CodeEditorDocument {
public:
    TextEditor editor;
    std::string fileType; // 'script' etc
    std::string filename; // doesn't mean a file exists on disk!
    bool hasOwnFile;
    bool dirty;
    bool shouldClose;
    bool shouldDelete;
    TextEditor::ErrorMarkers markers;
    TextEditor::Breakpoints breakpoints;

    CodeEditorDocument();

    void clearErrorMarkers() {
        markers.clear();
        editor.SetErrorMarkers(markers);
    }

    void addErrorMarker(int line, errorMarkerType_t type, std::string s)
    {
        TextEditor::ErrorMarker em;
        em.type = type;
        em.msg = s;
        markers[line] = em;
    }

    void showErrorMarkers() {
        editor.SetErrorMarkers(markers);
    }
};

CodeEditorDocument *newCodeDocument(std::vector<CodeEditorDocument*> *docs, std::string type);
CodeEditorDocument* getDocumentByPath(std::vector<CodeEditorDocument*> *docs, std::string path);

bool loadDBFile(std::vector<CodeEditorDocument*> *documents, std::string dbFileType, std::string path, std::string &errMsg);

#endif // CODE_EDITOR_DOCUMENT_H
