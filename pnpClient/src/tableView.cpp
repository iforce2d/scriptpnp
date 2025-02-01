
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
                break;
            }
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
        td.colNames = grid[0];
        grid.erase( grid.begin() );
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

void showTableViews()
{
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
        if ( find( tablesToRefresh.begin(), tablesToRefresh.end(), td.name ) != tablesToRefresh.end() )
            fetchTableData( td );
    }
    tablesToRefresh.clear();


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

            int numRows = td.grid.size();
            if ( numRows > 0 ) {
                vector<string> &firstRow = td.colNames;
                int numCols = firstRow.size();
                if (ImGui::BeginTable("userTable", numCols, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable | ImGuiTableFlags_SortMulti | ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_NoPadOuterX))
                {
                    for (int colNum = 0; colNum < (int)firstRow.size(); colNum++) {
                        ImGui::TableSetupColumn( firstRow[colNum].c_str(), ImGuiTableColumnFlags_DefaultSort );
                    }

                    ImGui::TableHeadersRow();


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

                    for (int rowNum = 0; rowNum < (int)td.grid.size(); rowNum++)
                    {
                        vector<TableCell> &cols = td.grid[rowNum];

                        bool filterOk = true;
                        for (int colNum = 0; colNum < (int)cols.size(); colNum++) {
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

                        for (int colNum = 0; colNum < (int)cols.size(); colNum++) {
                            bool isPrimaryKey = td.primaryKeyColumnIndex == colNum;
                            string &colVal = cols[colNum].text;
                            ImGui::TableSetColumnIndex(colNum);
                            if ( colVal.size() > 65535 ) {
                                ImGui::Text( "(too large)" );
                            }
                            else {
                                bool hasContent = true;
                                if ( colVal == "NULL" ) {
                                    hasContent = false;
                                    if ( ImGui::Button( ICON_FA_PLUS_SQUARE ) ) {
                                        colVal = "";
                                        hasContent = true;
                                    }
                                }

                                if ( hasContent ) {
                                    ImGui::PushID(colNum);
                                    if ( isPrimaryKey ) {
                                        ImGui::BeginDisabled();
                                        //ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.9f, 0.2f, 1.0f));
                                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0xFF303030, colNum);
                                        ImGui::AlignTextToFramePadding();
                                        ImGui::Text( colVal.c_str() );
                                        //ImGui::PopStyleColor();
                                        ImGui::EndDisabled();
                                    }
                                    else {

                                        ImRect rect = ImGui::TableGetCellBgRect( table, colNum );

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
                                                cols[colNum].dirty = true;
                                                td.dirty = true;
                                            }
                                        }
                                    }
                                    ImGui::PopID();
                                }
                            }
                        }

                        ImGui::PopID();
                    }
                    ImGui::EndTable();

                    //ImGui::PopStyleVar(1);
                }
            }
        }

        doLayoutSave(windowPrefix);

        ImGui::End();

    }
}

void script_refreshTableView(string which) {
    tablesToRefresh.push_back( which );
}




























