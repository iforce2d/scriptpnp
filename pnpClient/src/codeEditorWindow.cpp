
#include <string>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <iterator>

#include "nfd.h"

#include "log.h"
#include "codeEditorWindow.h"
#include "codeEditorPalettes.h"
#include "db.h"
#include "compilestatus.h"
#include "workspace.h"

using namespace std;

extern ImFont* font_proggy;
extern ImFont* font_ubuntuMono;
extern ImFont* font_sourceCodePro;

//#define OPEN_DIALOG_ID      "Open..."
#define SAVEAS_DIALOG_ID    "Save as..."
#define UNSAVED_DIALOG_ID   "Unsaved document"

// these are for modal dialogs so should be ok as static?
static string loadDbFileErrorMsg;
static string saveDbFileErrorMsg;
static bool okayToOverwriteExisting = false;
static bool doRefocusOnInputAfterFileSelectFail = false;
static CodeEditorDocument* docToClose = NULL;
static vector<string> openableFilenames;
static vector<string> alreadyOpenedFilenames;
static int selectedOpenDocumentIndex = -1;

CodeEditorWindow::CodeEditorWindow(std::vector<CodeEditorDocument *> *docs, string id, int index)
{
    log.setOwner(this);
    shouldRemainOpen = true;
    documents = docs;
    windowId = id;
    windowIndex = index;
    pendingSelectDocument = NULL;
}

void CodeEditorWindow::init()
{
    currentFont = font_proggy;
    currentDocument = NULL;
}

void CodeEditorWindow::setFont(ImFont *font)
{
    currentFont = font;
}

string CodeEditorWindow::getWindowTitle()
{
    char buf[128];
    snprintf(buf, sizeof(buf), "Code editor##%p", this);
    return string(buf);
}

void CodeEditorWindow::doImport() {
    nfdchar_t *openPath = NULL;
    nfdresult_t result = NFD_OpenDialog( "as,pnps;txt", NULL, &openPath );
    if ( result == NFD_OKAY )
    {
        g_log.log(LL_DEBUG, "doImport user selected: %s", openPath);

        CodeEditorDocument* doc = newCodeDocument(documents, getDBFileType());

        loadDiskFile(doc, openPath);

        free(openPath);
    }
    else if ( result == NFD_CANCEL )
    {
        g_log.log(LL_DEBUG, "doImport: user pressed cancel");
    }
    else
    {
        g_log.log(LL_DEBUG, "doImport error: %s", NFD_GetError() );
    }
}

void CodeEditorWindow::doExport() {

    nfdchar_t *savePath = NULL;
    nfdresult_t result = NFD_SaveDialog( "as,pnps;txt", NULL, &savePath );
    if ( result == NFD_OKAY )
    {
        g_log.log(LL_DEBUG, "doExport user selected: %s", savePath);

        saveDiskFile(currentDocument, savePath);

        free(savePath);
    }
    else if ( result == NFD_CANCEL )
    {
        g_log.log(LL_DEBUG, "doExport: user pressed cancel");
    }
    else
    {
        g_log.log(LL_DEBUG, "doExport error: %s", NFD_GetError() );
    }
}

bool saveDiskFile(CodeEditorDocument* doc, std::string path)
{
    if ( path.empty() ) {
        g_log.log(LL_ERROR, "Can't saveDiskFile to empty path");
        return false;
    }

    if ( ! doc ) {
        g_log.log(LL_ERROR, "Can't saveDiskFile for non-existent document");
        return false;
    }

    ofstream t( path );
    if ( ! t.is_open() ) {
        g_log.log(LL_ERROR, "Could not open disk file for writing: %s", path.c_str());
        return false;
    }

    string s = doc->editor.GetText();
    t.write(s.data(), s.length());

    return true;
}

bool loadDiskFile(CodeEditorDocument* doc, string file)
{
    ifstream t(file);
    if (t.good())
    {
        string str((istreambuf_iterator<char>(t)), istreambuf_iterator<char>());
        doc->editor.SetText(str);
        //doc->filename = file;
        return true;
    }
    else {
        g_log.log(LL_ERROR, "Could not open file for editing: %s", file.c_str());
        return false;
    }
}

bool saveDBFile(CodeEditorDocument* doc, string path, bool allowOverwriteExisting, string &errMsg)
{
    if ( path.empty() ) {
        errMsg = "Can't saveDBFile to empty path";
        g_log.log(LL_ERROR, "%s", errMsg.c_str());
        return false;
    }

    if ( ! doc ) {
        errMsg = "Can't saveDBFile for non-existent document";
        g_log.log(LL_ERROR, "%s", errMsg.c_str());
        return false;
    }

    string text = doc->editor.GetText();

    if( ! saveTextToDBFile(text, doc->fileType, path, allowOverwriteExisting, errMsg) ) {
        g_log.log(LL_DEBUG, "Failed to save DB file as '%s' : %s", path.c_str(), errMsg.c_str());
        return false;
    }

    g_log.log(LL_DEBUG, "Saved DB file as: '%s'", path.c_str());
    doc->filename = path;
    doc->dirty = false;
    doc->editor.ClearTextChangedStatus();
    doc->hasOwnFile = true;

    return true;
}

void CodeEditorWindow::showMenuBar(bool &doOpen, bool &doSave, bool &doSaveAs)
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", "Ctrl+O", (bool*)NULL, true))
            {
                doOpen = true;
            }
            if (ImGui::MenuItem("Save", "Ctrl+S", (bool*)NULL, currentDocument && currentDocument->hasOwnFile))
            {
                doSave = true;
            }

            if (ImGui::MenuItem("Save as", NULL, (bool*)NULL, currentDocument != NULL ))
            {
                doSaveAs = true;
            }

            if (ImGui::MenuItem("Import", NULL, (bool*)NULL, true ))
            {
                doImport();
            }

            if (ImGui::MenuItem("Export", NULL, (bool*)NULL, currentDocument != NULL ))
            {
                doExport();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Palette"))
        {
            if (ImGui::MenuItem("iforce2d palette", NULL, (bool*)NULL, currentDocument != NULL))
                currentDocument->editor.SetPalette(myPalette);
            if (ImGui::MenuItem("Dark palette", NULL, (bool*)NULL, currentDocument != NULL))
                currentDocument->editor.SetPalette(TextEditor::GetDarkPalette());
            if (ImGui::MenuItem("Light palette", NULL, (bool*)NULL, currentDocument != NULL))
                currentDocument->editor.SetPalette(TextEditor::GetLightPalette());
            if (ImGui::MenuItem("Retro blue palette", NULL, (bool*)NULL, currentDocument != NULL))
                currentDocument->editor.SetPalette(TextEditor::GetRetroBluePalette());
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Font"))
        {
            if (ImGui::MenuItem("Proggy Vector", NULL, currentFont == font_proggy))
                setFont(font_proggy);
            if (ImGui::MenuItem("Source Code Pro", NULL, currentFont == font_sourceCodePro))
                setFont(font_sourceCodePro);
            if (ImGui::MenuItem("Ubuntu Mono", NULL, currentFont == font_ubuntuMono))
                setFont(font_ubuntuMono);
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

}

void CodeEditorWindow::showOpenDialogPopup(bool openingNow)
{
    ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(getOpenDialogTitle().c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        //static char path[256] = "";

        /*ImGui::Text("Path:");
        ImGui::SameLine();
        if ( openingNow || doRefocusOnInputAfterFileSelectFail )
            ImGui::SetKeyboardFocusHere();
        bool enterWasPressed = ImGui::InputText("##load", path, sizeof(path), ImGuiInputTextFlags_EnterReturnsTrue);

        doRefocusOnInputAfterFileSelectFail = false;
        */

        string pathToOpen;

        if (ImGui::BeginListBox("##openpath"))
        {
            for (int n = 0; n < (int)openableFilenames.size(); n++)
            {
                string thePath = openableFilenames[n];
                const bool isSelected = (selectedOpenDocumentIndex == n);

                bool canOpen = ( std::find(alreadyOpenedFilenames.begin(), alreadyOpenedFilenames.end(), thePath) == alreadyOpenedFilenames.end() );

                if ( ! canOpen )
                    ImGui::BeginDisabled();

                if (ImGui::Selectable(thePath.c_str(), isSelected))
                    selectedOpenDocumentIndex = n;

                if ( ! canOpen )
                    ImGui::EndDisabled();

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                    pathToOpen = thePath;
                }
            }
            ImGui::EndListBox();
        }

        //bool canOpenSelected = selectedOpenDocumentIndex >= 0 && selectedOpenDocumentIndex < (int)openableFilenames.size();

        if ( pathToOpen.empty() )
            ImGui::BeginDisabled();

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
        if ( /*enterWasPressed ||*/ ImGui::Button("OK", ImVec2(120, 0))) {
            if ( loadDBFile(documents, getDBFileType(), pathToOpen, loadDbFileErrorMsg) )
                ImGui::CloseCurrentPopup();
            else
                doRefocusOnInputAfterFileSelectFail = true;
        }
        ImGui::PopStyleColor(3);

        if ( pathToOpen.empty() )
            ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        if ( ! loadDbFileErrorMsg.empty() ) {
            ImGui::TextColored( ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", loadDbFileErrorMsg.c_str());
        }

        ImGui::EndPopup();
    }
}

void CodeEditorWindow::showSaveAsDialogPopup(bool openingNow)
{
    ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(SAVEAS_DIALOG_ID, NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static char path[256] = "";

        ImGui::Text("Path:");
        ImGui::SameLine();
        if ( openingNow || doRefocusOnInputAfterFileSelectFail )
            ImGui::SetKeyboardFocusHere();
        string textBefore = path;
        bool enterWasPressed = ImGui::InputText("##save", path, sizeof(path), ImGuiInputTextFlags_EnterReturnsTrue);
        bool textWasChanged = textBefore != path;

        if ( textWasChanged )
            saveDbFileErrorMsg = "";

        doRefocusOnInputAfterFileSelectFail = false;

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
        if ( enterWasPressed || ImGui::Button("OK", ImVec2(120, 0))) {
            if ( saveDBFile(currentDocument, path, okayToOverwriteExisting, saveDbFileErrorMsg) )
                ImGui::CloseCurrentPopup();
            else
                doRefocusOnInputAfterFileSelectFail = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        if ( ! saveDbFileErrorMsg.empty() ) {
            ImGui::TextColored( ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", saveDbFileErrorMsg.c_str());
            if ( strlen(path) > 0 )
                ImGui::Checkbox("Overwrite existing file", &okayToOverwriteExisting);
        }

        ImGui::EndPopup();
    }
}

void CodeEditorWindow::showTabBar(/*CodeEditorDocument* dogggc,*/ bool &doConfirmClose, CodeEditorDocument* &docToClose)
{
    char tabBarName[128];
    snprintf(tabBarName, sizeof(tabBarName), "TabBar##%d", windowIndex);

        ImGuiTabBarFlags tab_bar_flags =
            ImGuiTabBarFlags_AutoSelectNewTabs |
            ImGuiTabBarFlags_Reorderable |
            ImGuiTabBarFlags_FittingPolicyResizeDown;

        if (ImGui::BeginTabBar(tabBarName, tab_bar_flags))
        {
            for (int i = 0; i < (int)documents->size(); i++) {

                CodeEditorDocument* activeDoc = documents->at(i);

                activeDoc->dirty |= activeDoc->editor.IsTextChanged();

                char tabName[128];
                //snprintf(tabName, sizeof(tabName), "%s###%d_%d", activeDoc->filename.c_str(), windowIndex, i);
                snprintf(tabName, sizeof(tabName), "%s", activeDoc->filename.c_str());

                bool isOpen = true;
                int flags = 0;
                if ( activeDoc->dirty )
                    flags |= ImGuiTabItemFlags_UnsavedDocument;
                if ( pendingSelectDocument == activeDoc ) {
                    flags |= ImGuiTabItemFlags_SetSelected;
                    pendingSelectDocument = NULL;
                }

                if (ImGui::BeginTabItem(tabName, &isOpen, flags))
                {
                    currentDocument = activeDoc;

                    if ( currentFont )
                        ImGui::PushFont(currentFont);
                    preRenderDoc(activeDoc);
                    //string intstr = to_string(i);
                    activeDoc->editor.Render( "asdfasfasdf" );
                    if ( currentFont )
                        ImGui::PopFont();

                    ImGui::EndTabItem();
                }

                if ( ! isOpen ) {
                    if ( activeDoc->dirty ) {
                        doConfirmClose = true;
                        docToClose = activeDoc;
                    }
                    else {
                        //onDocumentClosed(activeDoc);
                        activeDoc->shouldClose = true;
                    }
                }
            }

            if (ImGui::TabItemButton(" + ", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip))
                newCodeDocument(documents, getDBFileType());

            ImGui::EndTabBar();
        }
}

void CodeEditorWindow::showConfirmCloseDialog(CodeEditorDocument *doc)
{
    ImVec2 center = ImGui::GetMainViewport()->GetWorkCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(UNSAVED_DIALOG_ID, NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Document is not saved, close anyway?");

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
        if ( ImGui::Button("Yes", ImVec2(120, 0))) {
            //onDocumentClosed(docToClose);
            docToClose->shouldClose = true;
            docToClose = NULL;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("No", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::EndPopup();
    }
}

void CodeEditorWindow::showCompileErrorInfos()
{
    if ( ! currentDocument )
        return;

    vector<codeCompileErrorInfo> &infos = getCompileErrorInfos();
    for (codeCompileErrorInfo &e : infos) {
        errorMarkerType_t markerType = e.type == LL_WARN ? EMT_WARNING : EMT_ERROR;
        currentDocument->addErrorMarker(e.row, markerType, e.message);
    }
    currentDocument->showErrorMarkers();
}

void CodeEditorWindow::setupErrorMarkers(CodeEditorDocument *whichDoc)
{
    vector<codeCompileErrorInfo> &errors = getCompileErrorInfos();
    for (codeCompileErrorInfo &e : errors) {

        if ( whichDoc && (whichDoc->filename != e.section) )
            continue;

        if ( e.type == LL_INFO )
            continue;

        errorMarkerType_t markerType = EMT_ERROR;
        logLevel_e logLevel = LL_SCRIPT_ERROR;
        if ( e.type == LL_WARN ) {
            markerType = EMT_WARNING;
            logLevel = LL_SCRIPT_WARN;
        }

        char errBuf[256];
        snprintf(errBuf, sizeof(errBuf), "[%s] %s (row %d, col %d) : %s", logPrefixArray[logLevel], e.section.c_str(), e.row, e.col, e.message.c_str());

        // if ( whichDoc ) {
        //     whichDoc->addErrorMarker(e.row, markerType, errBuf );
        //     whichDoc->showErrorMarkers();
        //     return;
        // }

        CodeEditorDocument* docOfOffendingSection = getDocumentByPath(documents, e.section);

        if ( docOfOffendingSection )
            docOfOffendingSection->addErrorMarker(e.row, markerType, errBuf );

        log.log( logLevel, &e, 0, "%s", errBuf);
    }

    for ( CodeEditorDocument* d : *documents) {
        d->showErrorMarkers();
    }
}

void CodeEditorWindow::showOutputPane()
{
    ImGui::Text("Output will show here.");
}

void CodeEditorWindow::setSelectedDocument(CodeEditorDocument *doc)
{
    pendingSelectDocument = doc;
}

void CodeEditorWindow::onDocumentClosed(CodeEditorDocument *doc)
{
    if ( doc == currentDocument )
        currentDocument = NULL;
    /*if ( doc == currentDocument ) {
        // find another document to be the current
        CodeEditorDocument* successorDoc = NULL;
        bool sawCurrent = false;
        for (int i = 0; i < (int)documents->size(); i++) {
            auto d = documents->at(i);
            if ( d != currentDocument ) {
                successorDoc = d;
                if ( sawCurrent )
                    break;
            }
            if ( d == currentDocument ) {
                if ( d && sawCurrent ) {
                    successorDoc = d;
                    break;
                }
                sawCurrent = true;
            }
        }
        currentDocument = successorDoc;
        pendingSelectDocument = successorDoc;
    }
    else {
        pendingSelectDocument = currentDocument;
    }*/
}

extern bool ctrlOJustPressed;
extern bool ctrlSJustPressed;

void CodeEditorWindow::render()
{
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar;

    char windowTitle[128];
    snprintf(windowTitle, sizeof(windowTitle), "%s###%s%d", getWindowTitle().c_str(), windowId.c_str(), windowIndex);

    doLayoutLoad(windowTitle);

    int numStylesToPop = pushStyleColors();

    ImGui::Begin(windowTitle, &shouldRemainOpen, windowFlags);
    {
        bool doOpen = false;
        bool doSave = false;
        bool doSaveAs = false;

        if ( ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) ) {
            if ( ctrlOJustPressed )
                doOpen = true;
            if ( ctrlSJustPressed )
                doSave = true;
        }

        showMenuBar( doOpen, doSave, doSaveAs );

        if ( doOpen ) {
            loadDbFileErrorMsg.clear();

            openableFilenames.clear();
            alreadyOpenedFilenames.clear();
            string errMsg;
            getAllPathsOfTypeFromDBFile(openableFilenames, getDBFileType(), errMsg);
            for (CodeEditorDocument* d : *documents )
                alreadyOpenedFilenames.push_back( d->filename );

            selectedOpenDocumentIndex = -1;
            ImGui::OpenPopup(getOpenDialogTitle().c_str());
        }

        if ( doSave ) {
            string errMsg;
            saveDBFile(currentDocument, currentDocument->filename, true, errMsg);
        }

        if ( doSaveAs ) {
            saveDbFileErrorMsg.clear();
            okayToOverwriteExisting = false;
            ImGui::OpenPopup(SAVEAS_DIALOG_ID);
        }

        showOpenDialogPopup(doOpen);
        showSaveAsDialogPopup(doSaveAs);

        buttonFeedback.document = currentDocument;
        renderCustomSection();

        bool doConfirmClose = false;

        ImVec2 cr = ImGui::GetContentRegionAvail();
        ImVec2 area(0, cr.y - ImGui::GetFontSize() - 5);

        if ( area.y > 0 ) {

            if ( shouldShowOutputPane() )
            {
                ImGui::BeginChild("splitPane", area);
                {
                    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
                    ImGui::BeginChild("codeView", ImVec2(0,area.y*0.66), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeY);
                    {
                        showTabBar(doConfirmClose, docToClose);
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar();

                    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(32, 32, 32, 255));
                    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(110, 110, 128, 128));
                    ImGui::BeginChild("outputView", ImVec2(0,0), ImGuiChildFlags_Border);
                    {
                        showOutputPane();
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor(2);
                }
                ImGui::EndChild();
            }
            else {
                ImGui::BeginChild("pane", area);
                {
                    showTabBar(doConfirmClose, docToClose);
                }
                ImGui::EndChild();
            }
        }

        if ( doConfirmClose ) {
            if ( docToClose->dirty ) {
                ImGui::OpenPopup(UNSAVED_DIALOG_ID);
            }
            else {
                docToClose->shouldClose = true;
                docToClose = NULL;
            }
        }

        showConfirmCloseDialog(docToClose);

        if ( currentDocument ) {
            auto cpos = currentDocument->editor.GetCursorPosition();
            ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s",
                        cpos.mLine + 1,
                        cpos.mColumn + 1,
                        currentDocument->editor.GetTotalLines(),
                        currentDocument->editor.IsOverwrite() ? "Ovr" : "Ins",
                        currentDocument->editor.CanUndo() ? "*" : " ",
                        currentDocument->editor.GetLanguageDefinition().mName.c_str(),
                        currentDocument->filename.c_str()
                        );
        }
        else {
            ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s",
                        0,
                        0,
                        0,
                        "Ins",
                        " ",
                        "",
                        ""
                        );
        }

        doLayoutSave(windowTitle);

        ImGui::End();
    }

    ImGui::PopStyleColor(numStylesToPop);
}

int CodeEditorWindow::getText(std::vector<std::string> &lines)
{
    if (currentDocument)
        return currentDocument->editor.GetTextLines(lines);
    else
        return 0;
}

CodeEditorWindow* ensureEditorOpenForDocument( string type, string path );

void gotoCodeCompileError(CodeEditorWindow* w, errorGotoInfo &info)
{
    bool needHighlight = false;

    if ( info.fileType != w->getDBFileType() ) {
        // w is the window being clicked, but it might be the wrong type of editor. This
        // can happen when a script calls a command list that causes an error output.
        w = ensureEditorOpenForDocument( info.fileType, info.section );
        if ( ! w ) {
            g_log.log(LL_ERROR, "Couldn't ensureEditorOpenForDocument: '%s', '%s'", info.fileType.c_str(), info.section.c_str());
            return;
        }

        needHighlight = true;
    }

    CodeEditorDocument* doc = getDocumentByPath(w->getDocuments(), info.section);

    if ( ! doc ) {
        // document wasn't already open
        string errMsg;
        if ( ! loadDBFile(w->getDocuments(), w->getDBFileType(), info.section, errMsg) ) {
            return;
        }
        doc = getDocumentByPath(w->getDocuments(), info.section);
        if ( ! doc )
            return;

        needHighlight = true;
    }

    if ( needHighlight )
        w->setupErrorMarkers(doc);

    w->setSelectedDocument( doc );

    TextEditor::Coordinates coords;
    coords.mLine = info.row-1;
    coords.mColumn = info.col;
    doc->editor.SetCursorPosition(coords);
}



