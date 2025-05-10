#ifndef SCRIPTAPI_H
#define SCRIPTAPI_H

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <scriptarray/scriptarray.h>

extern int script_MM_NONE;
extern int script_MM_TRAJECTORY;
extern int script_MM_JOG;
extern int script_MM_HOMING;

extern int script_NT_NONE;
extern int script_NT_SUCCESS;
extern int script_NT_WARNING;
extern int script_NT_ERROR;
extern int script_NT_INFO;

extern int script_MR_NONE;
extern int script_MR_SUCCESS;
extern int script_MR_FAIL_CONFIG;
extern int script_MR_FAIL_NOT_HOMED;
extern int script_MR_FAIL_TIMED_OUT;
extern int script_MR_FAIL_NOT_TRIGGERED;
extern int script_MR_FAIL_OUTSIDE_BOUNDS;
extern int script_MR_FAIL_FOLLOWING_ERROR;
extern int script_MR_FAIL_LIMIT_TRIGGERED;

enum dbResultStatus_e {
    DBRS_FAILED,    // includes not done yet
    DBRS_SUCCEEDED
};

class dbRow {
    int refCount;
    class dbResult* result;
public:
    dbRow(class dbResult* result, int row);
    int IncRef();
    int DecRef();

    dbRow* opAssign(dbRow *other);

    int rowNum;

    std::string name();
    int get_numCols();
    std::string dump();
    std::string col_int(unsigned int i);
    std::string col_string(std::string colName);
};

class dbResult {
    int refCount;
    dbResult* other;
public:
    bool haveNames;
    dbResult();
    ~dbResult();

    int IncRef();
    int DecRef();
    dbResult* opAssign(dbResult *other);

    dbResultStatus_e status;
    std::string query;
    std::string errorMessage;
    long int executionTime;

    std::vector<std::string> columnNames;
    std::vector< std::vector<std::string> > rows;


    int get_status();
    std::string name();
    int get_numRows();
    int get_numCols();
    std::string dump();
    std::string dump_row(int rowNum);
    std::string columnName(unsigned int i);
    dbRow *row(unsigned int i);
    std::string getRowCol(int r, int c);
    int getColumnIndexByName(std::string name);
};

class script_blob {
public:
    int size;
    float ax;
    float ay;
    float cx;
    float cy;
    int bb_x1, bb_y1, bb_x2, bb_y2;
    int w, h;
    int pixels;
};

// class script_point {
// public:
//     float x;
//     float y;
// };

class script_rotatedRect {
public:
    bool valid;
    float x;
    float y;
    float w;
    float h;
    float area;
    float angle;
};

class script_renderText {
public:
    std::string text;
    int x;
    int y;
    float fontSize;
    int r;
    int g;
    int b;
};

void addScriptRenderText(std::string msg, int x, int y, int r, int g, int b);
std::vector<script_renderText>& getScriptRenderTexts();
void resetScriptRenderText();


class script_vec3 {
public:
    float x;
    float y;
    float z;

    script_vec3 operator +(const script_vec3& other);
    script_vec3 operator -(const script_vec3& other);
    script_vec3 operator +=(const script_vec3& other);
    script_vec3 operator -=(const script_vec3& other);

    script_vec3() {x = y = z = 0;}
    script_vec3(float _x, float _y, float _z) { x = _x; y = _y; z = _z; }

    script_vec3& operator=(const script_vec3 &other);
    script_vec3 set_floatfloatfloat(float _x, float _y, float _z);
    float length();
    float normalize();
    script_vec3 normalized();
    float distTo(const script_vec3 other);
    float distToXY(const script_vec3 other);
    script_vec3 rotatedBy(float degrees);
};


void vec3_opAdd_vec3(asIScriptGeneric * gen);
void vec3_opSub_vec3(asIScriptGeneric * gen);
void vec3_opAddAssign_vec3(asIScriptGeneric * gen);
void vec3_opSubAssign_vec3(asIScriptGeneric * gen);

void vec3_opMul_float(asIScriptGeneric * gen);
void vec3_opDiv_float(asIScriptGeneric * gen);
void vec3_opMulAssign_float(asIScriptGeneric * gen);
void vec3_opDivAssign_float(asIScriptGeneric * gen);

void vec3_opNeg(asIScriptGeneric * gen);


class script_qrcode {
public:
    int numPoints;
    int outlinePoints[8];
    char value[64];
    int orientation;
    std::string getValue() { return value; }
    script_vec3 getCenter();
    int getOrientation() { return orientation; }
};


class script_serialReply {
    int refCount;
public:
    bool ok;
    std::string str;
    CScriptArray* vals;
    int IncRef();
    int DecRef();
    script_serialReply *opAssign(script_serialReply *ref);

    script_serialReply();
    bool get_ok();
    CScriptArray* get_vals();
    std::string get_str();
};

class script_affine {
public:
    bool valid;
    float matrix[6][7];
    float rotation;
    float scaleX;
    float scaleY;

    script_affine() {
        valid = false;
        memset(matrix, 0, sizeof(matrix));
        scaleX = 1;
        scaleY = 1;
        rotation = 0;
    }
    script_affine(const script_vec3 a0, const script_vec3 a1, const script_vec3 a2,
                  const script_vec3 b0, const script_vec3 b1, const script_vec3 b2);
    script_affine& operator=(const script_affine &other);
    void setIdentity();
    void gaussEliminate();
    script_vec3 transform(const script_vec3& p);
    std::string str();

};


void script_print(std::string &msg);
void script_printarray_string( void* array );
std::string script_strArray( void* array );
std::string script_strDictionaryKeys( void* dict );
std::string script_strDictionary( void* dict );

dbResult *script_dbQuery(std::string &query);
int script_getLastInsertId();
dbResult *script_dbResultFactory();
dbRow *script_dbRowFactory();
script_serialReply *script_serialReplyFactory();

void script_print_dbRow(dbRow &row);
void script_print_dbResult(dbResult &res);

bool script_runCommand(std::string line);
bool script_runCommandList(std::string filename);
bool script_runCommandList_dict(std::string filename, void* dict );

bool script_setUSBCameraParams(int index, int zoom, int focus, int exposure, int whiteBalance, int saturation);

bool script_isPreview();
void script_wait(int millis);

void script_setDigitalOut(int which, int toWhat);
void script_setPWMOut(float toWhat);

bool script_getDigitalIn(int which);
bool script_getDigitalOut(int which);
script_vec3 script_getActualPos();
float script_getActualRot();
float script_getVacuum();
int script_getLoadcell();
float script_getWeight();
float script_getADC(int i);
int32_t script_getEncoder();

uint64_t script_millis();
int script_getMachineMode();
int script_getTrajectoryResult();
int script_getHomingResult();
int script_getProbingResult();

void script_print_bool(bool b);
void script_print_int(int i);
void script_print_float(float f);
std::string script_str_float(float f);
std::string script_str_vec3(script_vec3 &v);
void script_print_vec3(script_vec3 &v);
void script_print_serialReply(script_serialReply &r);
void script_print_affine(script_affine &a);

void script_exit();

//void script_notify(std::string msg, int type, int timeout);

#endif // SCRIPTAPI_H




