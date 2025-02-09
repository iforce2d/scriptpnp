#ifndef DB_H
#define DB_H

#include <vector>
#include <string>

struct dbTextFileInfo {
    std::string path;
    std::string text;
};

typedef int (*dbRowCallback)(void*,int,char**,char**);

bool openDatabase(std::string file);
void closeDatabase();

bool executeDatabaseStatement(std::string statement, dbRowCallback cb, std::string &errMsg);
bool executeDatabaseStatement_generic(std::string statement, std::vector< std::vector<std::string> > * dst, std::string &errMsg);

bool saveTextToDBFile(std::string &text, std::string dbFileType, std::string path, bool allowOverwriteExisting, std::string &errMsg);
bool loadTextFromDBFile(std::string &text, std::string dbFileType, std::string path, std::string &errMsg);

int getAllPathsOfTypeFromDBFile(std::vector<std::string> &paths, std::string dbFileType, std::string &errMsg);
int loadAllTextOfTypeFromDBFile(std::vector<dbTextFileInfo> &entries, std::string dbFileType, std::vector<std::string> &excludePaths, std::string &errMsg);

int getLastInsertId();
int getNextUntitledPathOfTypeFromDB(std::string dbFileType, std::string &errMsg);

#endif // DB_H
