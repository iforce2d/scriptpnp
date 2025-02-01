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

class TableData {
public:
    std::string name;
    int primaryKeyColumnIndex; // index into colNames
    std::vector<std::string> colNames;
    std::vector<int> colTypes;
    std::vector< std::vector<TableCell> > grid;
    std::vector<std::string> filters;
    std::vector<bool> badRegex;
    bool dirty;

    TableData() {
        primaryKeyColumnIndex = -1;
        dirty = false;
    }
};

extern std::vector<std::string> tablesToRefresh;
extern std::vector<TableData> tableDatas;

void setDesiredOpenTables( std::vector<std::string> which );
std::vector<std::string> & getOpenTableNames();

void fetchTableNames();
void showTableViewSelection(bool *p_open);
void showTableViews();

void script_refreshTableView(std::string which);

#endif
