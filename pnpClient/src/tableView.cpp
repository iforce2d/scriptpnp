
#include <algorithm>
#include <regex>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_notify/font_awesome_5.h"
#include "imgui_notify/fa_solid_900.h"
#include "tableView.h"
#include "db.h"
#include "workspace.h"
#include "log.h"
#include "util.h"
#include "notify.h"

using namespace std;

#define TABLESLIST_WINDOW_TITLE "DB tables"

vector<string> tableNames;
vector<string> openTableNames;
vector<std::string> tablesToRefresh;
vector<TableData> tableDatas;
bool tableSelectionChanged = false;

void setDesiredOpenTables( vector<string> which ) {
    openTableNames = which;
    tableSelectionChanged = true;
}

vector<string> & getOpenTableNames() {
    return openTableNames;
}

void fetchTableNames() {
    tableNames.clear();

    string errMsg;
    vector< vector<string> > tmp;
    executeDatabaseStatement_generic("select name from sqlite_master where type = 'table' and not name like 'internal_%' and not name like 'sqlite_%' order by name", &tmp, errMsg);

    if ( ! tmp.empty() )
        tmp.erase( tmp.begin() );

    for (vector<string> &row : tmp) {
        tableNames.push_back( row[0] );
    }

}

void fetchTableData(TableData& td)
{
    string errMsg;
    vector< vector<string> > tableInfo;
    executeDatabaseStatement_generic(string("PRAGMA table_info(") + td.name + ")", &tableInfo, errMsg);

    td.primaryKeyColumnIndex = -1;
    td.colNames.clear();
    td.relations.clear();
    td.dirty = false;

    // find index of the column called 'pk'
    int nameColIndex = -1;
    int typeColIndex = -1;
    int pkColIndex = -1;
    if ( ! tableInfo.empty() ) {
        vector<string>& colNamesRow = tableInfo[0];
        for ( int i = 0; i < (int)colNamesRow.size(); i++ ) {
            if ( colNamesRow[i] == "name" )
                nameColIndex = i;
            if ( colNamesRow[i] == "type" )
                typeColIndex = i;
            if ( colNamesRow[i] == "pk" )
                pkColIndex = i;
        }
        tableInfo.erase( tableInfo.begin() );
    }

    // find which row has a value of 1 for primary key
    if ( nameColIndex > -1 && pkColIndex > -1 ) {
        for ( int i = 0; i < (int)tableInfo.size(); i++ ) {
            vector<string> row = tableInfo[i];
            if ( row[pkColIndex] == "1" ) {
                td.primaryKeyColumnIndex = i;
            }
            td.colNames.push_back( row[nameColIndex] );
        }
    }

    if ( typeColIndex > -1 ) {
        for ( int i = 0; i < (int)tableInfo.size(); i++ ) {
            vector<string> row = tableInfo[i];
            string typeStr = row[typeColIndex];
            if ( typeStr == "INTEGER" ) {
                td.colTypes.push_back( CDT_INTEGER );
            }
            else if ( typeStr == "REAL" ) {
                td.colTypes.push_back( CDT_REAL );
            }
            else if ( typeStr == "BLOB" ) {
                td.colTypes.push_back( CDT_BLOB );
            }
            else
                td.colTypes.push_back( CDT_TEXT );
        }
    }

    int missingFilters = tableInfo.size() - td.filters.size();
    for ( int i = 0; i < missingFilters; i++ ) {
        td.filters.push_back("");
        td.badRegex.push_back(0);
    }

    vector<vector<string> > grid;
    executeDatabaseStatement_generic(string("select * from ") + td.name, &grid, errMsg);
    if ( ! grid.empty() ) {
        //td.colNames = grid[0];
        grid.erase( grid.begin() ); // skip names row
    }

    td.grid.clear();
    for (vector<string>& rowStrs : grid) {
        vector<TableCell> rowCells;
        for (string& text : rowStrs) {
            TableCell cell;
            cell.text = text;
            rowCells.push_back(cell);
        }
        td.grid.push_back( rowCells );
    }

    // find relation columns
    for (int i = 0; i < (int)td.colNames.size(); i++) {
        TableRelation tr;
        string colName = td.colNames[i];
        if ( stringStartsWith( colName, "relation_") ) {
            vector<string> parts;
            if ( 3 == splitStringVec(parts, colName, '_' ) ) {
                string tableName = parts[1];
                string columnName = parts[2];
                if ( tableWithColumnExists(tableName, columnName) ) {
                    tr.otherTableName = tableName;
                    tr.otherTableColumn = columnName;
                }
            }
        }
        td.relations.push_back( tr );
    }

    // find bool columns
    for (int i = 0; i < (int)td.colNames.size(); i++) {
        TableBool tb;
        string colName = td.colNames[i];
        if ( stringStartsWith( colName, "bool_") ) {
            vector<string> parts;
            if ( 2 == splitStringVec(parts, colName, '_' ) ) {
                tb.shortName = parts[1];
            }
        }
        td.bools.push_back( tb );
    }

    // find button columns
    if ( td.primaryKeyColumnIndex > -1 ) {
        for (int i = 0; i < (int)td.colNames.size(); i++) {
            TableButton tb;
            string colName = td.colNames[i];
            if ( stringStartsWith( colName, "button_") ) {
                vector<string> parts;
                if ( 2 == splitStringVec(parts, colName, '_' ) ) {
                    tb.shortName = parts[1];
                }
            }
            td.buttons.push_back( tb );
        }
    }

    //for (int i = 0; i < td.relations.size(); i++) {
    for (TableRelation& tr : td.relations) {
        if ( tr.otherTableColumn.empty() )
            continue;
        grid.clear();
        executeDatabaseStatement_generic(string("select id,")+tr.otherTableColumn+" from "+tr.otherTableName, &grid, errMsg);
        if ( grid.empty() )
            continue; // nothing in the table being referenced
        grid.erase( grid.begin() ); // skip names row
        for (vector<string>& rowStrs : grid) {
            TableRelationRow trr;
            trr.id = atoi( rowStrs[0].c_str() );
            trr.value = rowStrs[1];
            tr.otherTableEntries.push_back( trr );
        }
    }
}

void saveTableData(TableData* td) {

    if ( td->grid.empty() )
        return;

    string errMsg;

    vector<string> &colNames = td->colNames;

    for (int rowNum = 0; rowNum < (int)td->grid.size(); rowNum++)
    {
        vector<TableCell> &cols = td->grid[rowNum];

        bool haveChangedCell = false;
        for ( TableCell& cell : cols ) {
            if ( cell.dirty ) {
                haveChangedCell = true;
                break;
            }
        }

        if ( ! haveChangedCell )
            continue;

        string sql = string("update ") + td->name + " set ";

        for (int colNum = 0; colNum < (int)cols.size(); colNum++) {
            string &col = cols[colNum].text;
            if ( colNum > 0 )
                sql += ", ";
            sql += "'" + colNames[colNum] + "' = ";
            sql += ( col == "NULL" ) ? "null" : "'" + col + "'";
        }

        sql += " where " + colNames[td->primaryKeyColumnIndex] + " = '" + cols[td->primaryKeyColumnIndex].text + "'";

        printf("%s\n", sql.c_str()); fflush(stdout);

        if ( executeDatabaseStatement_generic(sql, NULL, errMsg) ) {
            for ( TableCell& cell : cols ) {
                cell.dirty = false;
                td->dirty = false; // assume if one row is written successfully they all will be
            }
        }
    }
}

void addNewTableRow(string tableName) {
    string errMsg;
    if ( ! executeDatabaseStatement_generic(string("insert into ") + tableName + "(id) values (null)", NULL, errMsg) ) {
        notify("Error adding row: "+errMsg+"\n(Table must have auto-increment row called 'id' and no 'not null' columns)", NT_ERROR, 5000);
    }
}

void deleteTableRow(string tableName, string pkColumnName, string pkValToDelete) {
    string errMsg;
    if ( ! executeDatabaseStatement_generic(string("delete from ") + tableName + " where "+ pkColumnName +" = '"+pkValToDelete+"'", NULL, errMsg) ) {
        notify("Error deleting row: "+errMsg, NT_ERROR, 5000);
    }
}

void queueCloseTableView(string name)
{
    auto it = find( openTableNames.begin(), openTableNames.end(), name);
    if ( it != openTableNames.end()) {
        openTableNames.erase( it );
        tableSelectionChanged = true;
    }
}

void openTableView(string name)
{
    TableData td;
    td.name = name;
    fetchTableData(td);
    tableDatas.push_back(td);
}

bool isTableOpen(string name)
{
    for ( TableData &td : tableDatas ) {
        if ( td.name == name )
            return true;
    }
    return false;
}

void closeTableView(string name)
{
    for ( int i = 0; i < (int)tableDatas.size(); i++ ) {
        TableData &td = tableDatas[i];
        if ( td.name == name ) {
            tableDatas.erase( tableDatas.begin() + i );
            return;
        }
    }
}

void showTableViewSelection(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(320, 480), ImGuiCond_FirstUseEver);

    string windowPrefix = TABLESLIST_WINDOW_TITLE;

    doLayoutLoad(windowPrefix);

    ImGui::Begin(windowPrefix.c_str(), p_open);
    {
        if ( ImGui::Button("Refresh") ) {
            fetchTableNames();
        }

        for (string name : tableNames) {
            bool checked = false;
            auto it = find( openTableNames.begin(), openTableNames.end(), name);
            if ( it != openTableNames.end() )
                checked = true;
            bool wasChecked = checked;
            ImGui::Checkbox( name.c_str() , &checked);
            if ( ! wasChecked && checked ) {
                openTableNames.push_back( name );
                tableSelectionChanged = true;
            }
            else if ( wasChecked && ! checked ) {
                openTableNames.erase( it );
                tableSelectionChanged = true;
            }
        }

        doLayoutSave(windowPrefix);

        ImGui::End();
    }
}

//int padx = 0;
//int pady = 0;

TableData* current_sort_tabledata = NULL;
ImGuiTableSortSpecs* current_sort_specs = NULL;

bool compareWithSortSpecs(vector<TableCell>& a, vector<TableCell>& b)
{
    for (int n = 0; n < current_sort_specs->SpecsCount; n++)
    {
        // Here we identify columns using the ColumnUserID value that we ourselves passed to TableSetupColumn()
        // We could also choose to identify columns based on their index (sort_spec->ColumnIndex), which is simpler!
        const ImGuiTableColumnSortSpecs* sort_spec = &current_sort_specs->Specs[n];

        int colType = current_sort_tabledata->colTypes[sort_spec->ColumnIndex];

        string& strA = a[sort_spec->ColumnIndex].text;
        string& strB = b[sort_spec->ColumnIndex].text;

        int delta = 0;
        switch( colType )
        {
        case CDT_INTEGER: delta = atoi(strA.c_str()) - atoi(strB.c_str()); break;
        case CDT_REAL: delta = atof(strA.c_str()) - atof(strB.c_str()); break;
        default: delta = strcmp( strA.c_str(), strB.c_str() ); break;
        }

        if (delta > 0)
            return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? false : true;
        if (delta < 0)
            return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? true : false;

        // switch ( sort_spec->ColumnIndex )
        // {
        // case MyItemColumnID_ID:             delta = (a->ID - b->ID);                break;
        // case MyItemColumnID_Name:           delta = (strcmp(a->Name, b->Name));     break;
        // case MyItemColumnID_Quantity:       delta = (a->Quantity - b->Quantity);    break;
        // case MyItemColumnID_Description:    delta = (strcmp(a->Name, b->Name));     break;
        // default: IM_ASSERT(0); break;
        // }
        // if (delta > 0)
        //     return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? +1 : -1;
        // if (delta < 0)
        //     return (sort_spec->SortDirection == ImGuiSortDirection_Ascending) ? -1 : +1;
    }

    return false;
}

void sortWithSortSpecs(ImGuiTableSortSpecs* sort_specs, TableData& td)
{
    current_sort_tabledata = &td;
    current_sort_specs = sort_specs; // Store in variable accessible by the sort function.
    std::sort ( td.grid.begin(), td.grid.end(), compareWithSortSpecs);
    current_sort_tabledata = NULL;
    current_sort_specs = NULL;
}

bool shouldDoRefresh = false;

void showConfirmFetchDialog() {
    if (ImGui::BeginPopupModal("Confirm refresh", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("There are unsaved changes - refresh anyway?");

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
        if ( ImGui::Button("Refresh", ImVec2(120, 0))) {
            shouldDoRefresh = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::EndPopup();
    }
}

void HelpMarker2(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

string pressedTableButtonTable;
string pressedTableButtonFunc;

int showTableViews()
{
    int pressedButtonId = 0;

    if ( tableSelectionChanged ) {
        vector<string> tablesToOpen;
        vector<string> tablesToClose;

        for ( TableData &td : tableDatas ) {
            if ( find( openTableNames.begin(), openTableNames.end(), td.name ) == openTableNames.end() ) {
                tablesToClose.push_back(td.name);
            }
        }
        for ( string name : openTableNames ) {
            if ( ! isTableOpen(name) )
                tablesToOpen.push_back( name );
        }

        for ( string name : tablesToClose ) {
            closeTableView( name );
        }
        for ( string name : tablesToOpen ) {
            openTableView( name );
        }
    }


    for ( TableData &td : tableDatas ) {
        auto thisTableIt = find( tablesToRefresh.begin(), tablesToRefresh.end(), td.name );
        if ( thisTableIt != tablesToRefresh.end() ) {
            tablesToRefresh.erase( thisTableIt );
            fetchTableData( td );            
        }
    }
    //tablesToRefresh.clear();


    for ( TableData &td : tableDatas ) {

        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

        string windowPrefix = "Table: ";
        windowPrefix += td.name;

        doLayoutLoad(windowPrefix);

        bool p_open = true;
        ImGui::Begin(windowPrefix.c_str(), &p_open);
        {
            if ( ! p_open )
                queueCloseTableView( td.name );

            bool didRefresh = false;

            shouldDoRefresh = false;
            if ( ImGui::Button("Refresh") ) {
                if ( td.dirty ) {
                    ImGui::OpenPopup("Confirm refresh");
                }
                else {
                    shouldDoRefresh = true;
                }
            }

            showConfirmFetchDialog();

            if ( shouldDoRefresh ) {
                fetchTableData( td );
                didRefresh = true;
            }

            ImGui::SameLine();

            bool canSave = td.primaryKeyColumnIndex >= 0;
            bool prohibitSave = !td.dirty || !canSave;

            if ( prohibitSave )
                ImGui::BeginDisabled();

            if ( ImGui::Button("Save") ) {
                saveTableData(&td);
            }

            if ( prohibitSave ) {
                ImGui::EndDisabled();
            }

            if ( ! canSave ) {
                ImGui::SameLine();
                HelpMarker2("This table cannot be saved because it does not have a primary key column");
            }

            //ImGui::InputInt( "Pad x", &padx );
            //ImGui::InputInt( "Pad y", &pady );

            vector<string> &firstRow = td.colNames;
            int numCols = firstRow.size();
            if (ImGui::BeginTable("userTable", numCols, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoPadOuterX))
            {
                for (int colNum = 0; colNum < (int)firstRow.size(); colNum++) {
                    string headerTitle = firstRow[colNum];
                    TableRelation& tr = td.relations[colNum];
                    TableBool& tBool = td.bools[colNum];
                    TableButton& tButton = td.buttons[colNum];
                    if ( ! tBool.shortName.empty() ) {
                        headerTitle = tBool.shortName;
                    }
                    else if ( ! tButton.shortName.empty() ) {
                        headerTitle = tButton.shortName;
                    }
                    else if ( ! tr.otherTableName.empty() ) {
                        headerTitle = tr.otherTableName + " (rel)";
                    }
                    ImGui::TableSetupColumn( headerTitle.c_str(), ImGuiTableColumnFlags_DefaultSort );
                }

                ImGui::TableHeadersRow();

                int numRows = td.grid.size();
                if ( numRows > 0 ) {

                    ImGui::TableNextRow(0, 26);

                    // Setup ItemWidth once (instead of setting up every time, which is also possible but less efficient)
                    for (int colNum = 0; colNum < (int)firstRow.size(); colNum++) {
                        ImGui::TableSetColumnIndex(colNum);
                        ImGui::PushItemWidth(0);
                    }

                    for (int colNum = 0; colNum < (int)firstRow.size(); colNum++) {
                        ImGui::TableSetColumnIndex(colNum);
                        string &filterVal = td.filters[colNum];
                        char c[128];
                        sprintf(c, filterVal.c_str());
                        if ( td.badRegex[colNum] )
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0xFF3030b0, colNum);
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 3));
                        ImGui::PushID(colNum);
                        ImGui::InputText( "filter", c, sizeof(c) );
                        ImGui::PopID();
                        ImGui::PopStyleVar();
                        filterVal = c;
                    }


                    if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
                        if ( didRefresh || sort_specs->SpecsDirty ) {
                            sortWithSortSpecs(sort_specs, td );
                            sort_specs->SpecsDirty = false;
                        }
                    }

                    ImGuiTable* table = ImGui::GetCurrentTable();

                    //ImGuiStyle& style = ImGui::GetStyle();
                    //ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(style.CellPadding.x, 20.0f));

                    for (int colNum = 0; colNum < (int)firstRow.size(); colNum++) {
                        td.badRegex[colNum] = false;
                    }

                    int numCols = 0;

                    string rowPKIDForDelete;

                    for (int rowNum = 0; rowNum < (int)td.grid.size(); rowNum++)
                    {
                        vector<TableCell> &cols = td.grid[rowNum];
                        numCols = cols.size();

                        bool filterOk = true;
                        for (int colNum = 0; colNum < numCols; colNum++) {
                            string &filterVal = td.filters[colNum];
                            if ( filterVal.empty() )
                                continue;
                            try {
                                string &colVal = cols[colNum].text;
                                std::regex regexPattern( filterVal );
                                std::smatch matches;
                                if ( ! std::regex_search(colVal, matches, regexPattern) ) {
                                    filterOk = false;
                                    break;
                                }
                            }
                            catch (...) {
                                td.badRegex[colNum] = true;
                            }
                        }

                        if ( ! filterOk )
                            continue;

                        ImGui::TableNextRow(0, 26);

                        // Draw our contents
                        ImGui::PushID(rowNum);

                        for (int colNum = 0; colNum < numCols; colNum++) {
                            bool isPrimaryKey = td.primaryKeyColumnIndex == colNum;
                            string &colVal = cols[colNum].text;
                            string colName = td.colNames[colNum];
                            TableBool& tBool = td.bools[colNum];
                            TableButton& tButton = td.buttons[colNum];
                            TableRelation& tr = td.relations[colNum];
                            ImGui::TableSetColumnIndex(colNum);
                            if ( ! tBool.shortName.empty() ) {
                                bool pushedStyleColor = false;
                                if ( cols[colNum].dirty ) {
                                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.45f, 0.25f, 0.1f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.55f, 0.35f, 0.15f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.9f, 0.5f, 0.2f, 1.0f));
                                    pushedStyleColor = true;
                                }

                                bool checked = colVal == "1";
                                bool oldVal = checked;
                                ImGui::Checkbox("##cb", &checked);
                                cols[colNum].text = checked ? "1" : "0";
                                bool dirty = checked != oldVal;
                                cols[colNum].dirty |= dirty;
                                td.dirty |= dirty;

                                if ( pushedStyleColor )
                                    ImGui::PopStyleColor(3);
                            }
                            else if ( ! tButton.shortName.empty() ) {

                                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(25, 105, 0, 220));
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(40, 163, 0, 220));

                                char buf[64];
                                sprintf( buf, "%s##cbtn", tButton.shortName.c_str() );
                                if ( ImGui::Button(buf) ) {
                                    //g_log.log(LL_DEBUG, "%s %s %d %d %s", tButton.shortName.c_str(), td.name.c_str(), rowNum, colNum, td.grid[rowNum][td.primaryKeyColumnIndex].text.c_str());
                                    pressedButtonId = atoi( td.grid[rowNum][td.primaryKeyColumnIndex].text.c_str() );
                                    pressedTableButtonTable = td.name;
                                    pressedTableButtonFunc = tButton.shortName.c_str();
                                }

                                ImGui::PopStyleColor(2);
                            }
                            else if ( ! tr.otherTableName.empty() ) {
                                ImGui::PushID(colNum);

                                int entryId = atoi(colVal.c_str());

                                bool pushedStyleColor = false;
                                if ( cols[colNum].dirty ) {
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.5f, 0.2f, 1.0f));
                                    pushedStyleColor = true;
                                }

                                if (ImGui::Button( tr.getSelectedDisplayValue(entryId).c_str() ))
                                    ImGui::OpenPopup("relationPopup");

                                if ( pushedStyleColor )
                                    ImGui::PopStyleColor(1);

                                if (ImGui::BeginPopup("relationPopup")) {

                                    string oldVal = cols[colNum].text;

                                    if (ImGui::Selectable("##0")) {
                                        cols[colNum].text = "0";
                                        bool dirty = cols[colNum].text != oldVal;
                                        cols[colNum].dirty |= dirty;
                                        td.dirty |= dirty;
                                    }

                                    for (int entryNum = 0; entryNum < (int)tr.otherTableEntries.size(); entryNum++) {
                                        TableRelationRow& trr = tr.otherTableEntries[entryNum];
                                        if (ImGui::Selectable(trr.value.c_str())) {
                                            cols[colNum].text = to_string(trr.id);
                                            bool dirty = cols[colNum].text != oldVal;
                                            cols[colNum].dirty |= dirty;
                                            td.dirty |= dirty;
                                        }
                                    }

                                    ImGui::EndPopup();
                                }

                                ImGui::PopID();
                            }
                            else if ( colVal.size() > 65535 ) {
                                ImGui::Text( "(too large)" );
                            }
                            else {
                                bool hasContent = true;

                                if ( colVal == "NULL" ) {
                                    hasContent = false;
                                    ImGui::PushID(colNum);
                                    if ( ImGui::Button( ICON_FA_PLUS_SQUARE ) ) {
                                        colVal = "";
                                        hasContent = true;
                                    }
                                    ImGui::PopID();
                                }

                                if ( hasContent ) {

                                    ImRect rect = ImGui::TableGetCellBgRect( table, colNum );

                                    ImGui::PushID(colNum);
                                    if ( isPrimaryKey ) {
                                        ImGui::BeginDisabled();
                                        //ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.9f, 0.2f, 1.0f));
                                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0xFF303030, colNum);
                                        ImGui::AlignTextToFramePadding();
                                        ImGui::Text( colVal.c_str() );

                                        if ( /*ImGui::IsWindowHovered(ImGuiHoveredFlags_None) &&*/ ImGui::IsMouseHoveringRect(rect.Min, rect.Max, false) ) {
                                            rowPKIDForDelete = colVal;
                                        }

                                        //ImGui::PopStyleColor();
                                        ImGui::EndDisabled();
                                    }
                                    else {


                                        if ( ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && ImGui::IsMouseHoveringRect(rect.Min, rect.Max, false) ) {
                                            cols[colNum].active = true;
                                        }

                                        if ( ! cols[colNum].active )
                                            ImGui::BeginDisabled();

                                        if ( cols[colNum].dirty )
                                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.5f, 0.2f, 1.0f));

                                        int flags = 0;
                                        if ( td.colTypes[colNum] == CDT_INTEGER || td.colTypes[colNum] == CDT_REAL )
                                            flags |= ImGuiInputTextFlags_CharsDecimal ;

                                        if ( cols[colNum].active ) {
                                            char c[65536];
                                            sprintf(c, colVal.c_str());
                                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 3));
                                            ImGui::InputText( "", c, IM_ARRAYSIZE(c), flags);
                                            ImGui::PopStyleVar(1);
                                            colVal = c;
                                        }
                                        else {
                                            ImGui::AlignTextToFramePadding();
                                            ImGui::Text( colVal.c_str() );
                                        }

                                        if ( cols[colNum].dirty )
                                            ImGui::PopStyleColor(1);

                                        if ( ! cols[colNum].active )
                                            ImGui::EndDisabled();

                                        if ( ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && ImGui::IsMouseHoveringRect(rect.Min, rect.Max, false) ) {
                                            cols[colNum].active = true;
                                        }
                                        else {
                                            if ( ! ImGui::IsItemActive() )
                                                cols[colNum].active = false;
                                        }

                                        if ( ImGui::IsItemActivated() ) {
                                            cols[colNum].active = true;
                                        }
                                        else if ( ImGui::IsItemDeactivated() ) {
                                            cols[colNum].active = false;
                                            if ( ImGui::IsItemDeactivatedAfterEdit() ) {
                                                cols[colNum].dirty |= true;
                                                td.dirty |= true;
                                            }
                                        }
                                    }
                                    ImGui::PopID();
                                }
                            }

                            // if (ImGui::TableGetColumnFlags(rowNum) & ImGuiTableColumnFlags_IsHovered)
                            //     rowPKIDForDelete = rowPKID;
                        }

                        ImGui::PopID();
                    }

                    if ( td.primaryKeyColumnIndex >= 0 ) {
                        string pkColumnName = td.colNames[td.primaryKeyColumnIndex];
                        ImGui::PushID( td.primaryKeyColumnIndex );
                        static string pkidToDelete; // static so it is preserved for the duration of the popup
                        if ( (ImGui::TableGetColumnFlags(td.primaryKeyColumnIndex) & ImGuiTableColumnFlags_IsHovered) && !ImGui::IsAnyItemHovered() && ImGui::IsMouseReleased(1)) {
                            pkidToDelete = rowPKIDForDelete;
                            ImGui::OpenPopup("MyPopup");
                        }
                        if (ImGui::BeginPopup("MyPopup")) {
                            //ImGui::Text("This is a custom popup for Column %d", column);
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.4f, 1.0f));
                            if (ImGui::Button("Delete row")) {
                                deleteTableRow( td.name, pkColumnName, pkidToDelete );
                                tablesToRefresh.push_back( td.name );
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::PopStyleColor(2);
                            ImGui::EndPopup();
                        }
                        ImGui::PopID();
                    }

                }

                ImGui::EndTable();
            }
        }

        if ( ImGui::Button("New row") ) {
            addNewTableRow( td.name );
            tablesToRefresh.push_back( td.name );
        }

        doLayoutSave(windowPrefix);

        ImGui::End();

    }

    return pressedButtonId;
}

void script_refreshTableView(string which) {
    tablesToRefresh.push_back( which );
}




























