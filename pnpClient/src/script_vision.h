#ifndef SCRIPT_VISION_H
#define SCRIPT_VISION_H

#include <stdint.h>
#include "script/api.h"

extern int script_FC_ALL;
extern int script_FC_ROW;

extern int script_FF_VERT;
extern int script_FF_HORZ;
extern int script_FF_BOTH;

extern int script_NT_BOTH;

extern int script_VP_ALL;
extern int script_VP_HUE;
extern int script_VP_SAT;
extern int script_VP_VAR;

extern int script_VP_RED;
extern int script_VP_GRN;
extern int script_VP_BLU;

struct videoFrameBuffers_t {
    int width;
    int height;
    uint8_t* originalData; // this is owned by libuvc and will always be present (do not free after use!)
    uint8_t* rgbData;   // this is copied from originalData in each frame
    uint8_t* rgbData2;  // this is only set up when required
    uint8_t* grayData;  // this is only set up when required
    uint8_t* grayData2; // this is only set up when required
    uint32_t* voteData;  // this is only set up when required

    videoFrameBuffers_t() {
        width = 0;
        height = 0;
        rgbData = NULL;
        rgbData2 = NULL;
        grayData = NULL;
        grayData2 = NULL;
        voteData = NULL;
    }
};

// Each camera holds its own frame buffers. A 'live' camera view script will
// directly access the buffer of the relevant camera on the main UI thread.
// These scripts run synchronously with the UI and only access the camera of
// the window they are running from.
//
// Other scripts will run in a separate thread and need to make a copy of the
// frame buffer instead of working directly with the buffer held by the camera.
// They can access any camera, so will need to specify which camera to use:
//
//     grabFrame( 1 ); // camera index
//     blur(...);
//     etc...

// A script works with a vision context which contains data used by various
// functions, eg. the frame buffer itself, current draw color
struct visionContext_t {
    videoFrameBuffers_t* buffers;
    std::vector<script_renderText> renderTexts;
    uint8_t colred;
    uint8_t colgrn;
    uint8_t colblu;
    int windowSize;

    visionContext_t() {
        buffers = NULL;
        colred = 255;
        colgrn = 255;
        colblu = 255;
        windowSize = 9999;
    }
};

visionContext_t* getVisionContextForThread();

void setMainThreadVisionContext(visionContext_t *ctx);
//videoFrameBuffers_t* getActiveScriptFrameBuffers();

void initFrameBuffers(videoFrameBuffers_t* vfb, int width, int height);
bool haveGrayData(videoFrameBuffers_t* vfb);
void ensureGrayData(videoFrameBuffers_t* vfb);
void ensureVoteData(videoFrameBuffers_t* vfb);
void cleanupVideoFrameBuffers(videoFrameBuffers_t* vfb);

void script_setDrawColor(int r, int g, int b);
void script_drawWindow();
void script_drawRect(int x1, int x2, int y1, int y2);
void script_drawLine(int x1, int x2, int y1, int y2);
void script_drawCross(int x, int y, int size, float angle);
void script_drawCircle(int x, int y, float radius);

void script_drawRectF(float x1, float x2, float y1, float y2);
void script_drawLineF(float x1, float x2, float y1, float y2);
void script_drawCrossF(float x, float y, float size, float angle);
void script_drawCircleF(float x, float y, float radius);

void script_drawRotatedRect(script_rotatedRect& r);

void script_copyRGB();
void script_overlayRGB(float opacity);
void script_RGB2BGR();
void script_RGB2HSV(int planeForVisual);
void script_RGB2RGB(int planeForVisual);

void script_RGB2RGB_F(float planeForVisual);
void script_RGB2HSV_F(float planeForVisual);

void script_selectCamera(int index);
int script_getCamera();
bool script_grabFrame(int cameraIndex);
void freeAsyncFrameBuffer();
bool script_saveImage(std::string filename);
bool script_loadImage(std::string filename);

void script_setVisionWindowSize(int windowSize);
void script_setVisionWindowSizeF(float windowSize);

class CScriptArray* script_quickblob_default();
class CScriptArray* script_quickblob(int color, int minpixels, int maxpixels, int minwidth, int maxwidth);
void script_blur(int kernelSize);
void script_rgbThreshold(int lr, int ur, int lg, int ug, int lb, int ub);
void script_hsvThreshold(int mh, int hRange, int ls, int us, int lv, int uv);
void script_findContour(int method);
void script_convexHull(bool drawLines);
void script_flipFrame(int method);
//void script_rotateContour(float radians);
script_rotatedRect* script_minAreaRect_default();
script_rotatedRect* script_minAreaRectF(float lx, float ux, float ly, float uy);
script_rotatedRect* script_minAreaRect(int lx, int ux, int ly, int uy);

class CScriptArray* script_findCircles(float diameter);

class CScriptArray* script_findQRCodes(int howMany);
void script_drawQRCode(script_qrcode& q);

void script_drawText(std::string msg, float x, float y);

void script_blurF(float kernelSize);
void script_hsvThresholdF(float mh, float hRange, float ls, float us, float lv, float uv);

#endif // SCRIPT_VISION_H
