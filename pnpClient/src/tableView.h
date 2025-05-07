#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include <string>
#include <vector>

enum cellDataType_e {
    CDT_TEXT,
    CDT_INTEGER,
    CDT_REAL,
    CDT_BLOB
};

class TableCell {
public:
    std::string text;
    bool dirty;
    bool active;

    TableCell() {
        dirty = false;
        active = false;
    }
};

class TableRelationRow {
public:
    int id;
    std::string value;
};

class TableRelation {
public:
    std::string fullColumnName;   // eg. relation_part_lcsc
    std::string otherTableName;   // eg. part
    std::string otherTableColumn; // eg. lcsc
    std::vector<TableRelationRow> otherTableEntries;
    std::string getSelectedDisplayValue(int id) {
        for (TableRelationRow& trr : otherTableEntries) {
            if ( trr.id == id )
                return trr.value;
        }
        return "(invalid id: "+std::to_string(id)+")";
    }
};

class TableBool {
public:
    std::string shortName;
};

class TableButton {
public:
    std::string shortName;
};

class TableData {
public:
    std::string name;
    int primaryKeyColumnIndex; // index into colNames
    std::vector<std::string> colNames;
    std::vector<int> colTypes;
    std::vector< std::vector<TableCell> > grid;
    std::vector<std::string> filters;
    std::vector<bool> badRegex;
    std::vector<TableRelation> relations;
    std::vector<TableBool> bools;
    std::vector<TableButton> buttons;
    bool dirty;

    TableData() {
        primaryKeyColumnIndex = -1;
        dirty = false;
    }
};

extern std::vector<std::string> tablesToRefresh;
extern std::vector<TableData> tableDatas;

extern std::string pressedTableButtonTable;
extern std::string pressedTableButtonFunc;

void setDesiredOpenTables( std::vector<std::string> which );
std::vector<std::string> & getOpenTableNames();
std::vector<std::string> & getAutogenScriptTableNames();
std::vector<std::string> & getHideTableNames();

void fetchTableNames();
void fetchTableData_basic(TableData& td);
void showTableViewSelection(bool *p_open);
void showTableSettings(bool *p_open);
int showTableViews();

void script_refreshTableView(std::string which);
void script_addHighlightedButtonKey(std::string key);
void script_clearHighlightedButtonKey(std::string key);
void script_clearAllHighlightedButtonKeys();

#endif
