
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <vector>
#include <pthread.h>
#include <map>
#include <float.h>

#include <scriptarray/scriptarray.h>
#include "script/engine.h"

#include <ZXing/ReadBarcode.h>
#include <ZXing/BarcodeFormat.h>

#include "script_vision.h"
#include "vision.h"
#include "image.h"
#include "usbcamera.h"

#define PACKED __attribute__((__packed__))

#ifndef DEGTORAD
#define DEGTORAD 0.01745329252
#define RADTODEG 57.2957795131
#endif

using namespace std;
using namespace ZXing;


// There can only be two threads running vision scripts at the same time.
// A single vision context is held for each thread type (main or async).
// The context values persist between script calls, so for example a call
// to setDrawColor(...) will change the draw color for other scripts of
// the same thread type.
visionContext_t* mainThreadVisionContext;
visionContext_t asyncThreadVisionContext;


void setMainThreadVisionContext(visionContext_t* ctx)
{
    //mainThreadVisionContext.buffers = b;
    mainThreadVisionContext = ctx;
}

extern pthread_t mainThreadId;

visionContext_t* getVisionContextForThread()
{
    if ( pthread_self() == mainThreadId )
        return mainThreadVisionContext;
    else
        return &asyncThreadVisionContext;
}

bool script_grabFrame(int cameraIndex)
{
    if ( pthread_self() == mainThreadId )
        return true; // will use mainThreadVisionContext
    visionContext_t* ctx = getVisionContextForThread();
    if ( ! ctx->buffers ) {
        ctx->buffers = new videoFrameBuffers_t();
    }
    bool ok = grabUSBCameraFrame(cameraIndex, ctx->buffers);
    return ok;
}

void freeAsyncFrameBuffer() {
    if ( asyncThreadVisionContext.buffers ) {
        delete asyncThreadVisionContext.buffers;
    }
}


#define GET_THREAD_CONTEXT_ELSE \
    visionContext_t* ctx = getVisionContextForThread();\
    videoFrameBuffers_t* b = ctx->buffers;\
    if ( ! b || ! b->rgbData )



bool script_saveImage(string filename)
{
    GET_THREAD_CONTEXT_ELSE
        return false;

    return savePNG(filename, b->width, b->height, 3, b->rgbData);
}

bool lastLoadedImageResult = false;
string lastLoadedImageFilename = "";
uint8_t* lastLoadedImageBuffer = NULL;

bool script_loadImage(string filename)
{
    visionContext_t* ctx = getVisionContextForThread();
    if ( ! ctx->buffers ) { // ie. for async context
        ctx->buffers = new videoFrameBuffers_t();
    }

    if ( filename == lastLoadedImageFilename ) {
        if ( lastLoadedImageResult ) {
            memcpy( ctx->buffers->rgbData, lastLoadedImageBuffer, ctx->buffers->width * ctx->buffers->height * 3 );
            return true;
        }
        // if failed last time, allow a retry
    }

    if ( ! lastLoadedImageBuffer ) {
        lastLoadedImageBuffer = new uint8_t[ ctx->buffers->width * ctx->buffers->height * 3 ];
        memset( lastLoadedImageBuffer, 0, ctx->buffers->width * ctx->buffers->height * 3 );
    }

    lastLoadedImageResult = loadPNG(filename, ctx->buffers->width, ctx->buffers->height, 3, lastLoadedImageBuffer);

    if ( lastLoadedImageResult )
        lastLoadedImageFilename = filename;
    else
        lastLoadedImageFilename = "";

    return lastLoadedImageResult;
}







void initFrameBuffers(videoFrameBuffers_t* vfb, int width, int height) {
    vfb->width = width;
    vfb->height = height;
    vfb->rgbData = new uint8_t[width * height * 3];
}

bool haveGrayData(videoFrameBuffers_t* vfb) {
    return vfb->grayData != NULL;
}

void ensureGrayData(videoFrameBuffers_t* vfb) {
    if ( vfb->grayData )
        return;
    vfb->grayData = new uint8_t[vfb->width * vfb->height];
}

void ensureGrayData2(videoFrameBuffers_t* vfb) {
    if ( vfb->grayData2 )
        return;
    vfb->grayData2 = new uint8_t[vfb->width * vfb->height];
}

void ensureRGBData2(videoFrameBuffers_t* vfb) {
    if ( vfb->rgbData2 )
        return;
    vfb->rgbData2 = new uint8_t[vfb->width * vfb->height * 3];
}

void ensureVoteData(videoFrameBuffers_t* vfb) {
    if ( vfb->voteData )
        return;
    vfb->voteData = new uint32_t[vfb->width * vfb->height];
}

void cleanupVideoFrameBuffers(videoFrameBuffers_t* vfb)
{
    if ( vfb->rgbData )
        delete[] vfb->rgbData;
    vfb->rgbData = NULL;

    if ( vfb->rgbData2 )
        delete[] vfb->rgbData2;
    vfb->rgbData2 = NULL;

    if ( vfb->grayData )
        delete[] vfb->grayData;
    vfb->grayData = NULL;

    if ( vfb->grayData2 )
        delete[] vfb->grayData2;
    vfb->grayData2 = NULL;

    if ( vfb->voteData )
        delete[] vfb->voteData;
    vfb->voteData = NULL;
}



inline void setPixelColor(videoFrameBuffers_t* bf, int x, int y, int r, int g, int b, int size = 1) {
    for (int xx = x-size+1; xx < x+size; xx++) {
        for (int yy = y-size+1; yy < y+size; yy++) {
            if ( xx >= 0 && xx < bf->width && yy >= 0 && yy < bf->height ) {
                int i = (yy * bf->width + xx) * 3;
                bf->rgbData[i++] = r;
                bf->rgbData[i++] = g;
                bf->rgbData[i] = b;
            }
        }
    }
}

#define FRAMECLAMPX(x)\
    if ( x < 0 ) x = 0; else if ( x >= b->width ) x = b->width-1;

#define FRAMECLAMPY(y)\
    if ( y < 0 ) y = 0; else if ( y >= b->height ) y = b->height-1;

void drawRect(videoFrameBuffers_t* b, int x1, int x2, int y1, int y2, uint8_t red, uint8_t grn, uint8_t blu) {

    if ( y1 >= 0 && y1 < b->height )
        for (int x = std::max(0,x1); x <= std::min(b->width,x2); x++)
            setPixelColor( b, x, y1, red, grn, blu);

    if ( y2 >= 0 && y2 < b->height )
        for (int x = std::max(0,x1); x <= std::min(b->width,x2); x++)
            setPixelColor( b, x, y2, red, grn, blu);

    if ( x1 >= 0 && x1 < b->width )
        for (int y = std::max(0,y1); y <= std::min(b->height,y2); y++)
            setPixelColor( b, x1, y, red, grn, blu);

    if ( x2 >= 0 && x2 < b->width )
        for (int y = std::max(0,y1); y <= std::min(b->height,y2); y++)
            setPixelColor( b, x2, y, red, grn, blu);
}

void drawLine(videoFrameBuffers_t* b, int x1, int x2, int y1, int y2, uint8_t red, uint8_t grn, uint8_t blu)
{
    if ( (y1 == y2) && (y1 < b->height) ) { // horizontal line special case, optimize
        int xs = max(0, min(x1, x2));
        int xe = min(max(x1, x2), b->width - 1);
        int base = (y1 * b->width + xs) * 3;
        for (int i = xs; i < xe; i++) {
            b->rgbData[base++] = red;
            b->rgbData[base++] = grn;
            b->rgbData[base++] = blu;
        }
        return;
    }

    float dx = (x2 - x1);
    float dy = (y2 - y1);

    float step;

    if (abs(dx) >= abs(dy))
        step = abs(dx);
    else
        step = abs(dy);

    dx = dx / step;
    dy = dy / step;
    float x = x1;
    float y = y1;
    int i = 0;

    while (i <= step) {
        if ( x >= 0 && x < b->width && y >= 0 && y < b->height )
            setPixelColor( b, x, y, red, grn, blu);
        x = x + dx;
        y = y + dy;
        i = i + 1;
    }
}

void rotatePixel(videoFrameBuffers_t* b, int x, int y, int &xout, int &yout, float degrees)
{
    float radians = degrees * DEGTORAD;

    int midw = b->width/2;
    int midh = b->height/2;

    float c = cos( radians );
    float s = sin( radians );

    x -= midw;
    y -= midh;

    xout = c * x - s * y;
    yout = s * x + c * y;

    xout += midw;
    yout += midh;
}

void drawCross(videoFrameBuffers_t* b, int x, int y, int size, float angle, uint8_t red, uint8_t grn, uint8_t blu)
{
    int midw = b->width/2;
    int midh = b->height/2;

    int x1 = midw - size;
    int y1 = midh;
    int x2 = midw + size;
    int y2 = midh;

    int rx1, rx2, ry1, ry2;
    rotatePixel(b, x1, y1, rx1, ry1, angle);
    rotatePixel(b, x2, y2, rx2, ry2, angle);

    rx1 += x - midw;
    rx2 += x - midw;
    ry1 += y - midh;
    ry2 += y - midh;

    drawLine(b, rx1, rx2, ry1, ry2, red, grn, blu);

    x1 = midw;
    y1 = midh - size;
    x2 = midw;
    y2 = midh + size;

    rotatePixel(b, x1, y1, rx1, ry1, angle);
    rotatePixel(b, x2, y2, rx2, ry2, angle);

    rx1 += x - midw;
    rx2 += x - midw;
    ry1 += y - midh;
    ry2 += y - midh;

    drawLine(b, rx1, rx2, ry1, ry2, red, grn, blu);
}

void drawCircle(videoFrameBuffers_t* b, int x, int y, float radius, uint8_t red, uint8_t grn, uint8_t blu)
{
    float r = radius * 0.7071;

    int x1 = x - r;
    int x2 = x + r;
    FRAMECLAMPX(x1)
    FRAMECLAMPX(x2)

    for (int xx = x1; xx <= x2; xx++) {
        float dx = xx - x;
        float a = asin(dx / radius);
        float dy = radius * cos(a);
        int yy = y + dy;
        if ( yy >= 0 && yy < b->height )
            setPixelColor( b, xx, yy, red, grn, blu);
        yy = y - dy;
        if ( yy >= 0 && yy < b->height )
            setPixelColor( b, xx, yy, red, grn, blu);
    }

    int y1 = y - r;
    int y2 = y + r;
    FRAMECLAMPY(y1)
    FRAMECLAMPY(y2)

    for (int yy = y1; yy <= y2; yy++) {
        float dy = yy - y;
        float a = asin(dy / radius);
        float dx = radius * cos(a);
        int xx = x + dx;
        if ( xx >= 0 && xx < b->width )
            setPixelColor( b, xx, yy, red, grn, blu );
        xx = x - dx;
        if ( xx >= 0 && xx < b->width )
            setPixelColor( b, xx, yy, red, grn, blu);
    }
}


void script_setDrawColor(int r, int g, int bb)
{
    GET_THREAD_CONTEXT_ELSE
        return;

    ctx->colred = r;
    ctx->colgrn = g;
    ctx->colblu = bb;
}


#define GETWINDOW \
    int windowSize = ctx->windowSize;\
    if ( windowSize < 16 )\
        windowSize = 16;\
    else {\
        if ( windowSize > b->width )\
            windowSize = b->width;\
        if ( windowSize > b->height )\
            windowSize = b->height;\
    }\
    \
    int midw = b->width/2;\
    int midh = b->height/2;\
    \
    int hw = windowSize/2;\
    int lx = midw - hw;\
    int ux = midw + hw;\
    int ly = midh - hw;\
    int uy = midh + hw;

void script_drawWindow()
{
    GET_THREAD_CONTEXT_ELSE
        return;

    GETWINDOW;

    drawRect(b, lx, ux, ly, uy, ctx->colred, ctx->colgrn, ctx->colblu);
}

void script_drawRect(int x1, int x2, int y1, int y2)
{
    GET_THREAD_CONTEXT_ELSE
        return;
    drawRect(b, x1, x2, y1, y2, ctx->colred, ctx->colgrn, ctx->colblu);
}

void script_drawRectF(float x1, float x2, float y1, float y2)
{
    script_drawRect(x1, x2, y1, y2);
}

void script_drawRotatedRect(script_rotatedRect& r)
{
    GET_THREAD_CONTEXT_ELSE
        return;

    float c = cos( r.angle );
    float s = sin( r.angle );

    float hw = r.w * 0.5;
    float hh = r.h * 0.5;

    float p0x = c * -hw - s * -hh;
    float p0y = s * -hw + c * -hh;

    float p1x = c * hw - s * -hh;
    float p1y = s * hw + c * -hh;

    float p2x = c * hw - s * hh;
    float p2y = s * hw + c * hh;

    float p3x = c * -hw - s * hh;
    float p3y = s * -hw + c * hh;

    p0x += r.x;
    p0y += r.y;
    p1x += r.x;
    p1y += r.y;
    p2x += r.x;
    p2y += r.y;
    p3x += r.x;
    p3y += r.y;

    drawLine(b, p0x, p1x, p0y, p1y, ctx->colred, ctx->colgrn, ctx->colblu);
    drawLine(b, p1x, p2x, p1y, p2y, ctx->colred, ctx->colgrn, ctx->colblu);
    drawLine(b, p2x, p3x, p2y, p3y, ctx->colred, ctx->colgrn, ctx->colblu);
    drawLine(b, p3x, p0x, p3y, p0y, ctx->colred, ctx->colgrn, ctx->colblu);
}

void script_drawLine(int x1, int x2, int y1, int y2)
{
    GET_THREAD_CONTEXT_ELSE
        return;
    drawLine(b, x1, x2, y1, y2, ctx->colred, ctx->colgrn, ctx->colblu);
}

void script_drawLineF(float x1, float x2, float y1, float y2)
{
    script_drawLine(x1, x2, y1, y2);
}

void script_drawCross(int x, int y, int size, float angle)
{
    GET_THREAD_CONTEXT_ELSE
        return;
    drawCross(b, x, y, size, angle, ctx->colred, ctx->colgrn, ctx->colblu);
}

void script_drawCrossF(float x, float y, float size, float angle)
{
    script_drawCross(x, y, size, angle);
}

void script_drawCircle(int x, int y, float radius)
{
    GET_THREAD_CONTEXT_ELSE
        return;
    drawCircle(b, x, y, radius, ctx->colred, ctx->colgrn, ctx->colblu);
}

void script_drawCircleF(float x, float y, float radius)
{
    script_drawCircle(x, y, radius);
}

struct rgb_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} PACKED;

struct hsv_t {
    uint8_t h;
    uint8_t s;
    uint8_t v;
} PACKED;



void rgb2hsv(rgb_t *in, hsv_t *out)
{
    int min, max, delta;

    int r = in->r;
    int g = in->g;
    int b = in->b;

    min = r < g ? r : g;
    min = min < b ? min : b;

    max = r > g ? r : g;
    max = max  > b ? max : b;

    out->v = max;
    delta = max - min;
    if (delta < 1)
    {
        out->s = 0;
        out->h = 0; // undefined, maybe nan?
        return;
    }

    if ( max > 0.0 ) { // NOTE: if Max is == 0, this divide would cause a crash
        out->s = (delta / (float)max) * 255;
    } else {
        // if max is 0, then r = g = b = 0
        // s = 0, h is undefined
        out->s = 0.0;
        out->h = 0;                            // its now undefined
        return;
    }

    float h = 0;
    if( r >= max )                           // > is bogus, just keeps compilor happy
        h = ( g - b ) / (float)delta;        // between yellow & magenta
    else if ( g >= max )
        h = 2 + ( b - r ) / (float)delta;  // between cyan & yellow
    else
        h = 4 + ( r - g ) / (float)delta;  // between magenta & cyan

    //out.h *= 60.0;                              // degrees

    if ( h < 0.0 )
        h += 6;

    out->h = h * 42.4;
}


void script_copyRGB()
{
    GET_THREAD_CONTEXT_ELSE
        return;

    ensureRGBData2(b);
    memcpy(b->rgbData2, b->rgbData, b->width*b->height*3);
}

void script_overlayRGB(float opacity)
{
    GET_THREAD_CONTEXT_ELSE
        return;

    if ( ! b->rgbData2 )
        return;

    GETWINDOW;

    for (int y = ly; y < uy; y++) {
        for (int x = lx; x < ux; x++) {
            int i = (y*b->width+x) * 3;
            if ( b->rgbData[i+0] || b->rgbData[i+1] || b->rgbData[i+2] )
                continue;

            b->rgbData[i+0] = b->rgbData2[i+0] * opacity;
            b->rgbData[i+1] = b->rgbData2[i+1] * opacity;
            b->rgbData[i+2] = b->rgbData2[i+2] * opacity;
        }
    }
}

void script_RGB2BGR()
{
    GET_THREAD_CONTEXT_ELSE
        return;

    for (int i = 0; i < b->width * b->height; i++) {
        uint8_t tmp = b->rgbData[i*3];
        b->rgbData[i*3] = b->rgbData[i*3+2];
        b->rgbData[i*3+2] = tmp;
    }
}

int script_VP_ALL = -1;
int script_VP_HUE = 0;
int script_VP_SAT = 1;
int script_VP_VAR = 2;

void script_RGB2HSV(int planeForVisual)
{
    GET_THREAD_CONTEXT_ELSE
        return;

    if ( planeForVisual < -1 || planeForVisual > 2 )
        return;

    hsv_t hsv;

    for (int i = 0; i < b->width * b->height; i++) {
        rgb_t* rgb = (rgb_t*)&b->rgbData[i*3];
        rgb2hsv(rgb, &hsv);
        if ( planeForVisual == -1 ) {
            rgb->r = hsv.h;
            rgb->g = hsv.s;
            rgb->b = hsv.v;
        }
        else {
            uint8_t val = hsv.h;
            if ( planeForVisual == 1 )
                val = hsv.s;
            if ( planeForVisual == 2 )
                val = hsv.v;
            rgb->r = val;
            rgb->g = val;
            rgb->b = val;
        }
    }
}

int script_VP_RED = 0;
int script_VP_GRN = 1;
int script_VP_BLU = 2;

void script_RGB2RGB(int planeForVisual)
{
    GET_THREAD_CONTEXT_ELSE
        return;

    if ( planeForVisual < 0 || planeForVisual > 2 )
        return;

    for (int i = 0; i < b->width * b->height; i++) {
        rgb_t* rgb = (rgb_t*)&b->rgbData[i*3];
        uint8_t val = rgb->r;
        if ( planeForVisual == 1 )
            val = rgb->g;
        if ( planeForVisual == 2 )
            val = rgb->b;
        rgb->r = val;
        rgb->g = val;
        rgb->b = val;
    }
}

void script_RGB2HSV_F(float planeForVisual)
{
    script_RGB2HSV(planeForVisual);
}

void script_RGB2RGB_F(float planeForVisual)
{
    script_RGB2RGB(planeForVisual);
}

void script_setVisionWindowSize(int windowSize)
{
    GET_THREAD_CONTEXT_ELSE
        return;
    ctx->windowSize = windowSize;
}

void script_setVisionWindowSizeF(float windowSize)
{
    script_setVisionWindowSize(windowSize);
}

// CScriptArray* script_quickblob_default() {
//     return script_quickblob(-1,-1,-1,-1,-1);
// }

CScriptArray* script_quickblob(int color, int minpixels, int maxpixels, int minwidth, int maxwidth) {

    asITypeInfo* t = GetScriptTypeIdByDecl("array<blob>");

    GET_THREAD_CONTEXT_ELSE
    {
        CScriptArray* arr = CScriptArray::Create(t, (asUINT)0);
        return arr;
    }

    ensureGrayData(b);

    GETWINDOW;

    int gi = 0;
    for (int y = ly; y < uy; y++) {
        for (int x = lx; x < ux; x++) {
            int i = y*b->width+x;
            rgb_t* rgb = (rgb_t*)&b->rgbData[i*3];
            b->grayData[gi++] = (rgb->r + rgb->g + rgb->b) > 0 ? 255 : 0;
        }
    }

    if ( color < 0 )
        color = VISION_DEFAULT_COLOR;
    if ( minpixels < 0 )
        minpixels = VISION_DEFAULT_MINPIXELS;
    if ( maxpixels < 0 )
        maxpixels = VISION_DEFAULT_MAXPIXELS;
    if ( minwidth < 0 )
        minwidth = VISION_DEFAULT_MINWIDTH;
    if ( maxwidth < 0 )
        maxwidth = VISION_DEFAULT_MAXWIDTH;

    // br.params.color = VISION_DEFAULT_COLOR;
    // //br.params.threshold = VISION_DEFAULT_THRESHOLD;
    // //br.params.maskSize = VISION_DEFAULT_MASKSIZE;
    // br.params.minPixels = VISION_DEFAULT_MINPIXELS;
    // br.params.maxPixels = VISION_DEFAULT_MAXPIXELS;
    // br.params.minWidth = VISION_DEFAULT_MINWIDTH;
    // br.params.maxWidth = VISION_DEFAULT_MAXWIDTH;

    blobRun_t br;
    br.params.color = color;
    br.params.minPixels = minpixels;
    br.params.maxPixels = maxpixels;
    br.params.minWidth = minwidth;
    br.params.maxWidth = maxwidth;

    br.bytes = b->grayData;
    br.frame = 0;
    br.width = ux - lx;
    br.height = uy - ly;

    bool ok = runQuickblob(&br);
    if ( ! ok ) {
        savePNG("blobfail.png", br.width, br.height, 1, br.bytes);
    }

    CScriptArray* arr = CScriptArray::Create(t, br.blobs.size());

    if ( ok ) {
        int i = 0;
        for (script_blob& blob : br.blobs) {
            blob.ax += lx;
            blob.ay += ly;
            blob.bb_x1 += lx;
            blob.bb_x2 += lx;
            blob.bb_y1 += ly;
            blob.bb_y2 += ly;

            blob.w = blob.bb_x2 - blob.bb_x1;
            blob.h = blob.bb_y2 - blob.bb_y1;
            blob.cx = (float)(blob.bb_x1 + blob.bb_x2) * 0.5;
            blob.cy = (float)(blob.bb_y1 + blob.bb_y2) * 0.5;

            script_blob * p = static_cast<script_blob *>(arr->At(i++));
            *p = blob;
        }
    }

    return arr;
}

// https://blog.ivank.net/fastest-gaussian-blur.html
void boxesForGauss(int sigma, int n, vector<int> &boxSizes)  // standard deviation, number of boxes
{
    float wIdeal = sqrt((12*sigma*sigma/n)+1);  // Ideal averaging filter width
    int wl = floor(wIdeal);
    if( wl % 2 == 0 )
        wl--;
    int wu = wl+2;

    float mIdeal = (12*sigma*sigma - n*wl*wl - 4*n*wl - 3*n)/(-4*wl - 4);
    int m = round(mIdeal);

    for (int i = 0; i < n; i++)
        boxSizes.push_back( i < m ? wl : wu );
}

void boxBlurH_4 (uint8_t* scl, uint8_t* tcl, int w, int h, int r)
{
    float iarr = 1 / (float)(r+r+1);
    for(int i=0; i<h; i++) {
        int ti = i*w, li = ti, ri = ti+r;
        float fv = scl[ti], lv = scl[ti+w-1], val = (r+1)*fv;
        for(int j=0; j<r; j++) val += scl[ti+j];
        for(int j=0  ; j<=r ; j++) { val += scl[ri++] - fv       ;   tcl[ti++] = (val*iarr); }
        for(int j=r+1; j<w-r; j++) { val += scl[ri++] - scl[li++];   tcl[ti++] = (val*iarr); }
        for(int j=w-r; j<w  ; j++) { val += lv        - scl[li++];   tcl[ti++] = (val*iarr); }
    }
}

void boxBlurT_4 (uint8_t* scl, uint8_t* tcl, int w, int h, int r)
{
    float iarr = 1 / (float)(r+r+1);
    for(int i=0; i<w; i++) {
        int ti = i, li = ti, ri = ti+r*w;
        float fv = scl[ti], lv = scl[ti+w*(h-1)], val = (r+1)*fv;
        for(int j=0; j<r; j++) val += scl[ti+j*w];
        for(int j=0  ; j<=r ; j++) { val += scl[ri] - fv     ;  tcl[ti] = (val*iarr);  ri+=w; ti+=w; }
        for(int j=r+1; j<h-r; j++) { val += scl[ri] - scl[li];  tcl[ti] = (val*iarr);  li+=w; ri+=w; ti+=w; }
        for(int j=h-r; j<h  ; j++) { val += lv      - scl[li];  tcl[ti] = (val*iarr);  li+=w; ti+=w; }
    }
}

void boxBlur_4 (uint8_t* scl, uint8_t* tcl, int w, int h, int r)
{
    memcpy(tcl, scl, w*h);
    boxBlurH_4(tcl, scl, w, h, r);
    boxBlurT_4(scl, tcl, w, h, r);
}

void gaussBlur_4 (uint8_t* scl, uint8_t* tcl, int w, int h, int r)
{
    vector<int> bxs;
    boxesForGauss(r, 3, bxs);
    boxBlur_4 (scl, tcl, w, h, (bxs[0]-1)/2);
    boxBlur_4 (tcl, scl, w, h, (bxs[1]-1)/2);
    boxBlur_4 (scl, tcl, w, h, (bxs[2]-1)/2);
}

void script_blur(int kernelSize)
{
    if ( kernelSize < 1 )
        return;

    GET_THREAD_CONTEXT_ELSE
        return;

    ensureGrayData(b);
    ensureGrayData2(b); // actually used for one color plane here

    if ( kernelSize > 24 )
        kernelSize = 24;

    GETWINDOW;

    for (int c = 0; c < 3; c++) {

        int gi = 0;
        for (int x = lx; x < ux; x++) {
            for (int y = ly; y < uy; y++) {
                int i = y*b->width+x;
                b->grayData[gi++] = b->rgbData[3*i+c];
            }
        }

        gaussBlur_4(b->grayData, b->grayData2, ux-lx, uy-ly, kernelSize);

        gi = 0;
        for (int x = lx; x < ux; x++) {
            for (int y = ly; y < uy; y++) {
                int i = y*b->width+x;
                b->rgbData[3*i+c] = b->grayData2[gi++];
            }
        }
    }
}

void script_blurF(float kernelSize)
{
    script_blur(kernelSize);
}

void script_rgbThreshold(int lr, int ur, int lg, int ug, int lb, int ub)
{
    GET_THREAD_CONTEXT_ELSE
        return;

    GETWINDOW;

    for (int x = lx; x < ux; x++) {
        for (int y = ly; y < uy; y++) {

            int i = (y*b->width+x) * 3;

            rgb_t* rgb = (rgb_t*)&b->rgbData[i];
            if ( rgb->r < lr || rgb->r > ur ||
                 rgb->g < lg || rgb->g > ug ||
                 rgb->b < lb || rgb->b > ub ) {
                b->rgbData[i+0] = 0;
                b->rgbData[i+1] = 0;
                b->rgbData[i+2] = 0;
            }
            else
            {
                b->rgbData[i+0] = 255;
                b->rgbData[i+1] = 255;//
                b->rgbData[i+2] = 255;
            }
        }
    }
}

void normalize255( int& angle )
{
    while ( angle < -127 ) angle += 255;
    while ( angle >  128 ) angle -= 255;
}

bool isWithinRange255( int testAngle, int a, int b )
{
    normalize255( testAngle );
    a -= testAngle;
    b -= testAngle;
    normalize255( a );
    normalize255( b );
    if ( a * b >= 0 )
        return false;
    return abs( a - b ) < 127;
}

bool isHueWithinRange( int testHue, int middle, int range )
{
    int d = testHue - middle;
    normalize255( d );
    return d <= range;
}

void script_hsvThreshold(int mh, int hRange, int ls, int us, int lv, int uv)
{
    GET_THREAD_CONTEXT_ELSE
        return;

    GETWINDOW;

    hsv_t hsv;

    for (int x = lx; x < ux; x++) {
        for (int y = ly; y < uy; y++) {

            int i = (y*b->width+x) * 3;

            rgb_t* rgb = (rgb_t*)&b->rgbData[i];
            rgb2hsv(rgb, &hsv);

            //int lh = mh - hRange/2;
            //int uh = mh + hRange/2;

            if ( hsv.s < ls || hsv.s > us ||
                 hsv.v < lv || hsv.v > uv ||
                 // ! isWithinRange255( hsv.h, lh, uh ) )
                 ! isHueWithinRange( hsv.h, mh, hRange ))
            {
                b->rgbData[i+0] = 0;
                b->rgbData[i+1] = 0;
                b->rgbData[i+2] = 0;
            }
            /*else
            {
                b->rgbData[i+0] = sdc_red;
                b->rgbData[i+1] = sdc_grn;
                b->rgbData[i+2] = sdc_blu;
            }*/
        }
    }
}

void script_hsvThresholdF(float mh, float hRange, float ls, float us, float lv, float uv)
{
    script_hsvThreshold(mh, hRange, ls, us, lv, uv);
}


int script_FC_ALL = 0;
int script_FC_ROW = 1;

inline bool isBlack( uint8_t* rgbData, int indexOfRedPixel ) {
    uint32_t i32 = *(uint32_t*)(rgbData + indexOfRedPixel);
    i32 <<= 8;
    return i32 == 0;
}

// intended for already binary image
void script_findContour(int method)
{
    GET_THREAD_CONTEXT_ELSE
        return;

    GETWINDOW;

    if ( method == script_FC_ROW ) {
        // keep only outermost pixels of each row
        for (int y = ly; y < uy; y++) {
            int i = y*b->width+lx;
            int lwx = -1;
            int uwx = -1;
            for (int x = lx; x < ux; x++) {
                //if ( b->rgbData[i*3] ) {
                if ( ! isBlack(b->rgbData, i*3) ) {
                    if ( lwx == -1 ) {
                        lwx = i;
                        b->rgbData[i*3] = 255;
                        b->rgbData[i*3+1] = 255;
                        b->rgbData[i*3+2] = 255;
                    }
                    else {
                        if ( uwx != -1 ) {
                            b->rgbData[uwx*3] = 0;
                            b->rgbData[uwx*3+1] = 0;
                            b->rgbData[uwx*3+2] = 0;
                        }
                        uwx = i;
                        b->rgbData[uwx*3] = 255;
                        b->rgbData[uwx*3+1] = 255;
                        b->rgbData[uwx*3+2] = 255;
                    }
                }
                i++;
            }
        }
    }
    else {
        // keep all outlines
        ensureGrayData(b);

        for (int x = lx; x < ux; x++) {
            for (int y = ly; y < uy; y++) {
                int i = y*b->width+x;
                b->grayData[i] = ( isBlack(b->rgbData,i*3) ? 0 : 255);
            }
        }

        for (int x = lx; x < ux; x++) {
            for (int y = ly; y < uy; y++) {
                int i = y*b->width+x;
                bool anyBlack =
                    ( ! b->grayData[i] ) ||
                    ( ! b->grayData[ i - 1 ] ) ||
                    ( ! b->grayData[ i + 1 ] ) ||
                    ( ! b->grayData[ i - b->width ] ) ||
                    ( ! b->grayData[ i + b->width ] );

                if ( anyBlack ) {
                    if ( ! isBlack(b->rgbData,i*3) ) {
                        b->rgbData[ i*3 ] = 255;
                        b->rgbData[ i*3+1 ] = 255;
                        b->rgbData[ i*3+2 ] = 255;
                    }
                }
                else {
                    b->rgbData[ i*3 ] = 0;
                    b->rgbData[ i*3+1 ] = 0;
                    b->rgbData[ i*3+2 ] = 0;
                }
            }
        }

        /*for (int x = lx; x < ux; x++) {
            for (int y = ly; y < uy; y++) {
                int i = y*b->width+x;
                if ( ! isBlack(b->rgbData,i*3) ) {
                    b->rgbData[ i*3 ] = 255;
                    b->rgbData[ i*3+1 ] = 255;
                    b->rgbData[ i*3+2 ] = 255;
                }
            }
        }*/
    }
}

struct chp {
    float x;
    float y;
};

float cross(const chp &O, const chp &A, const chp &B)
{
    return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
}

void convexHull(vector<chp> &P, vector<chp> &H)
{
    size_t n = P.size(), k = 0;
    if (n <= 3) {
        H = P;
        return;
    }
    H.resize(2*n);

    // Sort points lexicographically
    //sort(P.begin(), P.end());

    // Build lower hull
    for (size_t i = 0; i < n; ++i) {
        while (k >= 2 && cross(H[k-2], H[k-1], P[i]) <= 0) k--;
        H[k++] = P[i];
    }

    // Build upper hull
    for (size_t i = n-1, t = k+1; i > 0; --i) {
        while (k >= t && cross(H[k-2], H[k-1], P[i-1]) <= 0) k--;
        H[k++] = P[i-1];
    }

    H.resize(k-1);
}

// intended for already binary image
void script_convexHull(bool drawLines)
{
    GET_THREAD_CONTEXT_ELSE
        return;

    GETWINDOW;

    vector<chp> points;

    for (int x = lx; x < ux; x++) {
        int lwy = -1;
        int uwy = -1;
        for (int y = ly; y < uy; y++) {
            int i = y*b->width+x;
            if ( b->rgbData[i*3] || b->rgbData[i*3+1] || b->rgbData[i*3+2] ) {
                if ( lwy == -1 )
                    lwy = y;
                uwy = y;
            }
            //b->rgbData[i*3+0] = 0;
            //b->rgbData[i*3+1] = 0;
            //b->rgbData[i*3+2] = 0;
        }
        if ( lwy != -1 ) {
            chp p;
            p.x = x;
            p.y = lwy;
            points.push_back(p);
            if ( lwy != uwy ) {
                p.y = uwy;
                points.push_back(p);
            }
        }
    }

    vector<chp> hull;
    convexHull(points, hull);

    if ( drawLines ) {
        for (int i = 1; i < (int)hull.size(); i++) {
            chp &p0 = hull[i-1];
            chp &p1 = hull[i];
            drawLine(b, p0.x, p1.x, p0.y, p1.y, ctx->colred, ctx->colgrn, ctx->colblu);
        }
    }
    else {
        for (chp &p : hull)
            setPixelColor(b, p.x, p.y, 255, 0, 0);
    }
}

int script_FF_VERT = 0;
int script_FF_HORZ = 1;
int script_FF_BOTH = 2;

void verticalFlip(videoFrameBuffers_t* b) {
    for (int y = 0; y < b->height/2; y++) {
        int otherY = b->height - y - 1;
        int ind0 = (y * b->width) * 3;
        int ind1 = (otherY * b->width) * 3;
        memcpy(b->grayData, &b->rgbData[ind0], b->width*3);
        memcpy(&b->rgbData[ind0], &b->rgbData[ind1], b->width*3);
        memcpy(&b->rgbData[ind1], b->grayData, b->width*3);
    }
}

void horizontalFlip(videoFrameBuffers_t* b) {
    for (int y = 0; y < b->height; y++) {
        for (int x = 0; x < b->width/2; x++) {
            int otherX = b->width - x - 1;
            int ind0 = (y * b->width + x) * 3;
            int ind1 = (y * b->width + otherX) * 3;
            rgb_t *p0 = (rgb_t*)&b->rgbData[ind0];
            rgb_t *p1 = (rgb_t*)&b->rgbData[ind1];
            rgb_t t = *p0;
            *p0 = *p1;
            *p1 = t;
        }
    }
}

void script_flipFrame(int method)
{
    GET_THREAD_CONTEXT_ELSE
        return;

    ensureGrayData(b);

    if ( method == script_FF_VERT ) {
        verticalFlip(b);
    }
    else if ( method == script_FF_HORZ ) {
        horizontalFlip(b);
    }
    else if ( method == script_FF_BOTH ) {
        verticalFlip(b);
        horizontalFlip(b);
    }
}

script_rotatedRect rr;

script_rotatedRect* script_minAreaRectF(float lx, float ux, float ly, float uy) {
    return script_minAreaRect(lx, ux, ly, uy);
}

script_rotatedRect* script_minAreaRect_default() {
    return script_minAreaRect(-1, -1, -1, -1);
}

script_rotatedRect* script_minAreaRect(int _lx, int _ux, int _ly, int _uy)
{
    rr.angle = 0;
    rr.x = 0;
    rr.y = 0;
    rr.w = 0;
    rr.h = 0;

    GET_THREAD_CONTEXT_ELSE
    {
        return &rr;
    }

    GETWINDOW;

    // window override can only become smaller
    if ( _lx != -1 && _lx > lx )
        lx = _lx;
    if ( _ly != -1 && _ly > ly )
        ly = _ly;
    if ( _ux != -1 && _ux < ux )
        ux = _ux;
    if ( _uy != -1 && _uy < uy )
        uy = _uy;

    if ( ((ux - lx) < 1) || ((uy - ly) < 1) )
        return &rr;

    vector<chp> points;

    // get initial input for convex hull algorithm, by throwing away
    // all pixels that are not the leftmost or rightmost in their row

    for (int x = lx; x < ux; x++) {
        int lwy = -1;
        int uwy = -1;
        for (int y = ly; y < uy; y++) {
            int i = y*b->width+x;
            if ( b->rgbData[i*3] || b->rgbData[i*3+1] || b->rgbData[i*3+2] ) {
                if ( lwy == -1 )
                    lwy = y;
                uwy = y;
            }
        }
        if ( lwy != -1 ) {
            chp p;
            p.x = x;
            p.y = lwy;
            points.push_back(p);
            if ( lwy != uwy ) {
                p.y = uwy;
                points.push_back(p);
            }
        }
    }

    // get actual convex hull points, this is usually 20 - 30 points

    vector<chp> hull;
    convexHull(points, hull);

    // show convex hull points in rgb frame
    //for (chp &p : hull)
    //    setPixelColor(b, p.x, p.y, 255, 0, 255, 2);

    // naive min-area-rect first step: rotate all convex hull points and measure
    // their maximum extents at 20 different angles. Range is over +/- 47 degrees.

    std::map<float, int> areas;

    float minAngle = -0.26f;
    float maxAngle =  0.26f;
    float angleStep = (maxAngle - minAngle) / 20.0f;

    float bestRadians = 0;
    float bestArea = FLT_MAX;
    float bestCenterX = 0;
    float bestCenterY = 0;
    float bestWidth = 0;
    float bestHeight = 0;

    float bestSplitArea = FLT_MAX;
    float splitLeftRadians = 0;
    float splitRightRadians = 0;

    int i = 0;
    float lastArea = 0;
    float lastRadians = 0;
    for (float angle = minAngle; angle < maxAngle; angle += angleStep) {

        float minrx = 99999;
        float maxrx = -99999;
        float minry = 99999;
        float maxry = -99999;

        float radians = angle * M_PI;
        float c = cos( radians );
        float s = sin( radians );

        for (chp &p : hull) {

            float px = p.x + 0.5; // add 0.5 to be at pixel center
            float py = p.y + 0.5;

            float rx = c * px - s * py;
            float ry = s * px + c * py;

            minrx = min(minrx, rx);
            maxrx = max(maxrx, rx);
            minry = min(minry, ry);
            maxry = max(maxry, ry);
        }

        float area = (maxrx - minrx) * (maxry - minry);
        areas[radians] = area;

        if ( area < bestArea ) {
            bestRadians = radians;
            bestArea = area;
            bestCenterX = (minrx + maxrx) * 0.5f;
            bestCenterY = (minry + maxry) * 0.5f;
            bestWidth = maxrx - minrx;
            bestHeight = maxry - minry;
        }

        if ( i > 0 ) {
            float splitArea = (lastArea + area) * 0.5f;
            if ( splitArea < bestSplitArea ) {
                bestSplitArea = splitArea;
                splitLeftRadians = lastRadians;
                splitRightRadians = radians;
            }
        }

        lastArea = area;
        lastRadians = radians;

        i++;
    }

    // naive min-area-rect second step: same as first step but limited to
    // the angle range between the best two adjacent angles from above.

    angleStep = (splitRightRadians - splitLeftRadians) / 20.0f;
    for (float radians = splitLeftRadians; radians < splitRightRadians; radians += angleStep) {

        float minrx = 99999;
        float maxrx = -99999;
        float minry = 99999;
        float maxry = -99999;

        float c = cos( radians );
        float s = sin( radians );

        for (chp &p : hull) {

            float px = p.x + 0.5; // add 0.5 to be at pixel center
            float py = p.y + 0.5;

            float rx = c * px - s * py;
            float ry = s * px + c * py;

            minrx = min(minrx, rx);
            maxrx = max(maxrx, rx);
            minry = min(minry, ry);
            maxry = max(maxry, ry);
        }

        float area = (maxrx - minrx) * (maxry - minry);
        areas[radians] = area;

        if ( area < bestArea ) {
            bestRadians = radians;
            bestArea = area;
            bestCenterX = (minrx + maxrx) * 0.5f;
            bestCenterY = (minry + maxry) * 0.5f;
            bestWidth = maxrx - minrx;
            bestHeight = maxry - minry;
        }
    }


    rr.angle = -bestRadians;

    float c = cos( -bestRadians );
    float s = sin( -bestRadians );

    float mrx = bestCenterX;
    float mry = bestCenterY;

    rr.x = c * mrx - s * mry;
    rr.y = s * mrx + c * mry;
    rr.w = bestWidth;
    rr.h = bestHeight;

    return &rr;
}


bool display = true;
CScriptArray* script_findCircles(float diameter) {

    asITypeInfo* t = GetScriptTypeIdByDecl("array<float>");

    GET_THREAD_CONTEXT_ELSE
    {
        CScriptArray* arr = CScriptArray::Create(t, (asUINT)0);
        return arr;
    }

    //bool display = true;

    ensureGrayData(b);
    ensureGrayData2(b);
    ensureVoteData(b);
    memset(b->voteData, 0, sizeof(uint32_t)*b->width*b->height);

    GETWINDOW;

    float radius = 0.5 * diameter;

    for (int x = lx; x < ux; x++) {
        for (int y = ly; y < uy; y++) {
            int i = y*b->width+x;
            b->grayData[i] = (b->rgbData[i*3] + b->rgbData[i*3+1] + b->rgbData[i*3+2]) / 3.0f;
            if ( display ) {
                b->rgbData[i*3+0] = 0;
                b->rgbData[i*3+1] = 0;
                b->rgbData[i*3+2] = 0;
            }
        }
    }

    //memset(b->rgbData, 0, 3*sizeof(uint8_t)*b->width*b->height);

    uint32_t highestVote = 0;

    uint32_t totalCirclePixels = 0;
    int samplesPerCircle = 0;

    for (int y = ly; y < uy; y++) {
        for (int x = lx; x < ux; x++) {

            int i = y * b->width + x;

            if ( ! b->grayData[i] )
                continue;

            samplesPerCircle = 0;

            int lastCx = -1;
            int lastCy = -1;

            // draw a circle centered on xx,yy
            for (float a = 0; a < 2*M_PI; a+=0.5) {
                int cx = x + sin(a) * radius;
                int cy = y + cos(a) * radius;

                if ( cx == lastCx && cy == lastCy )
                    continue;

                samplesPerCircle++;

                if ( cx < 0 || cx > b->width-1 || cy < 0 || cy > b->height-1 )
                    continue;

                totalCirclePixels++;

                int ci = cy*b->width+cx;

                b->voteData[ci] += 1;
                if ( b->voteData[ci] > highestVote )
                    highestVote = b->voteData[ci];

                if ( display ) {
                    b->rgbData[ci*3+0] = max(b->grayData[i], b->rgbData[ci*3+0]);
                    b->rgbData[ci*3+1] = max(b->grayData[i], b->rgbData[ci*3+1]);
                    b->rgbData[ci*3+2] = max(b->grayData[i], b->rgbData[ci*3+2]);
                }

                lastCx = cx;
                lastCy = cy;
            }

        }
    }

    // make normalized grayscale
    int gi = 0;
    for (int y = ly; y < uy; y++) {
        for (int x = lx; x < ux; x++) {
            int i = y * b->width + x;
            float f = b->voteData[i] / (float)highestVote;
            b->grayData[gi++] = f * 255;
        }
    }

    // blur a bit
    gaussBlur_4(b->grayData, b->grayData2, ux-lx, uy-ly, 1);

    gi = 0;
    for (int y = ly; y < uy; y++) {
        for (int x = lx; x < ux; x++) {
            int i = y * b->width + x;
            b->grayData[i] = b->grayData2[gi++];
        }
    }

    vector<pair<float,float> > centers;

    while ( true ) {
        int bestX = -1;
        int bestY = -1;
        int bestGray = 0;
        for (int y = ly; y < uy; y++) {
            for (int x = lx; x < ux; x++) {
                int i = y * b->width + x;
                int v = b->grayData[i];
                if ( v > bestGray ) {
                    bestGray = v;
                    bestX = x;
                    bestY = y;
                }
            }
        }
        if ( bestGray < 128 )
            break;

        int avgCount = 0;
        float avgX = 0;
        float avgY = 0;

        for (int y = bestY-radius; y <= bestY+radius; y++) {
            if ( y < ly || y >= uy )
                continue;
            for (int x = bestX-radius; x <= bestX+radius; x++) {
                if ( x < lx || x >= ux )
                    continue;
                int i = y * b->width + x;
                int v = b->grayData[i];
                b->grayData[i] = 0;
                if ( v > 127 ) {
                    avgX += x;
                    avgY += y;
                    avgCount++;
                    if ( display ) {
                        b->rgbData[i*3+0] = v;
                        b->rgbData[i*3+1] = v;
                        b->rgbData[i*3+2] = v;
                    }
                }
                else {
                    if ( display ) {
                        b->rgbData[i*3+0] = 0;
                        b->rgbData[i*3+1] = 0;
                        b->rgbData[i*3+2] = 0;
                    }
                }
            }
        }

        if ( avgCount > 0 ) {
            avgX /= (float)avgCount;
            avgY /= (float)avgCount;
            centers.push_back( make_pair(avgX,avgY));
        }
    }

    CScriptArray* arr = CScriptArray::Create(t, 2*centers.size());

    int arrInd = 0;
    for (auto c : centers) {
        float * p = static_cast<float*>(arr->At(arrInd++));
        *p = c.first;
        p = static_cast<float*>(arr->At(arrInd++));
        *p = c.second;
    }

    // if ( ok ) {
    //     int i = 0;
    //     for (script_blob& blob : br.blobs) {

    //         script_blob * p = static_cast<script_blob *>(arr->At(i++));
    //         *p = blob;
    //     }
    // }

    return arr;
}

CScriptArray* script_findQRCodes(int howMany) {

    asITypeInfo* t = GetScriptTypeIdByDecl("array<qrcode>");

    GET_THREAD_CONTEXT_ELSE
    {
        CScriptArray* arr = CScriptArray::Create(t, (asUINT)0);
        return arr;
    }

    if ( howMany < 1 )
        howMany = 1;

    auto image = ZXing::ImageView(b->rgbData, b->width, b->height, ZXing::ImageFormat::RGB);
    auto options = ZXing::ReaderOptions().setFormats(ZXing::BarcodeFormat::Any);
    options.setTryHarder( false );
    options.setTryInvert( false );
    options.setMaxNumberOfSymbols( howMany );
    //options.setFormats( BarcodeFormat::MicroQRCode | BarcodeFormat::QRCode );
    auto barcodes = ZXing::ReadBarcodes(image, options);

    CScriptArray* arr = CScriptArray::Create(t, barcodes.size());

    int arrInd = 0;
    for (auto&& barcode : barcodes) {

        script_qrcode* p = static_cast<script_qrcode*>(arr->At(arrInd++));
        snprintf(p->value, 32, "%s", barcode.text().c_str());
        //p->outline.angle = barcode.orientation() * DEGTORAD;

        int ind = 0;
        for (auto bp : barcode.position()) {
            p->outlinePoints[ind++] = bp.x;
            p->outlinePoints[ind++] = bp.y;
            if ( ind >= 8 )
                break;
        }
        p->numPoints = ind / 2;

        // int bpInd = 0;
        // int lastX = 0;
        // int lastY = 0;
        // float avgX = 0;
        // float avgY = 0;
        // for (auto bp : barcode.position()) {
        //     if ( bpInd > 0 ) {
        //         int dx = bp.x - lastX;
        //         int dy = bp.y - lastY;
        //         float d = sqrt( dx*dx + dy*dy );
        //         p->outline.w = d;
        //         p->outline.h = d;
        //     }
        //     avgX += bp.x;
        //     avgY += bp.y;
        //     lastX = bp.x;
        //     lastY = bp.y;
        //     bpInd++;
        // }
        // avgX /= 4.0f;
        // avgY /= 4.0f;
        // p->outline.x = avgX;
        // p->outline.y = avgY;
    }

    return arr;
}













void script_drawText(string msg, float x, float y)
{
    if ( pthread_self() != mainThreadId )
        return; // only webcam views can actually do anything

    GET_THREAD_CONTEXT_ELSE
        return;

    script_renderText t;
    t.text = msg;
    t.x = x;
    t.y = y;
    t.r = ctx->colred;
    t.g = ctx->colgrn;
    t.b = ctx->colblu;
    ctx->renderTexts.push_back(t);
}

void script_drawQRCode(script_qrcode& q)
{
    if ( pthread_self() != mainThreadId )
        return; // only webcam views can actually do anything

    GET_THREAD_CONTEXT_ELSE
        return;

    //script_drawRotatedRect(q.outline);
    float avgX = 0;
    float avgY = 0;
    for (int i = 0; i < q.numPoints; i++) {
        int ind0 = (i) % q.numPoints;
        int ind1 = (i+1) % q.numPoints;
        int x1 = q.outlinePoints[ 2*ind0 ];
        int y1 = q.outlinePoints[ 2*ind0+1 ];
        int x2 = q.outlinePoints[ 2*ind1 ];
        int y2 = q.outlinePoints[ 2*ind1+1 ];
        script_drawLine( x1, x2, y1, y2 );
        avgX += x1;
        avgY += y1;
    }
    avgX /= (float)q.numPoints;
    avgY /= (float)q.numPoints;

    script_renderText t;
    t.text = q.value;
    t.x = avgX;
    t.y = avgY;
    t.r = ctx->colred;
    t.g = ctx->colgrn;
    t.b = ctx->colblu;
    ctx->renderTexts.push_back(t);
}







