#ifndef EVENTHOOKS_H
#define EVENTHOOKS_H

#include <vector>
#include <string>

#define NUM_FUNCTION_KEY_HOOKS          12
#define MAX_CUSTOM_BUTTON_LABEL_LEN     128
#define MAX_ENTRY_FUNCTION_LEN          128
#define MAX_TWEAK_LABEL_LEN             128
#define MAX_CUSTOM_BUTTON_GROUP_LEN     128

struct functionKeyHookInfo_t {
    int id;
    bool dirty;
    char entryFunction[MAX_ENTRY_FUNCTION_LEN];
    bool preview;
    functionKeyHookInfo_t() {
        id = 0;
        dirty = true;
        entryFunction[0] = 0;
        preview = true;
    }
};

struct customButtonHookInfo_t {
    int id;
    bool dirty;
    char label[MAX_CUSTOM_BUTTON_LABEL_LEN];
    char entryFunction[MAX_ENTRY_FUNCTION_LEN];
    bool preview;
    char tabGroup[MAX_CUSTOM_BUTTON_GROUP_LEN];
    int displayOrder;
    customButtonHookInfo_t() {
        id = 0;
        dirty = true;
        label[0] = 0;
        entryFunction[0] = 0;
        preview = true;
        tabGroup[0] = 0;
        displayOrder = 0;
    }
};

struct tweakInfo_t {
    int id;
    bool dirty;
    char name[MAX_TWEAK_LABEL_LEN];
    float floatval;
    float minval;
    float maxval;
    char tabGroup[MAX_CUSTOM_BUTTON_GROUP_LEN];
    int displayOrder;
    tweakInfo_t() {
        id = 0;
        dirty = true;
        name[0] = 0;
        floatval = 0;
        minval = 0;
        maxval = 10;
        tabGroup[0] = 0;
        displayOrder = 0;
    }
};

void saveEventHooksToDB();
void loadEventHooksFromDB();

void saveTweakToDB(std::string name);

void showHooksView(bool* p_open, float dt);
void executeFunctionKeyHook(int index);
std::vector<customButtonHookInfo_t> & getCustomButtonInfos();
std::vector<tweakInfo_t> & getTweakInfos();
tweakInfo_t *getTweakByName(std::string name);
void deleteTweak( int id );
void deleteCustomButton( int id );

#endif // EVENTHOOKS_H
