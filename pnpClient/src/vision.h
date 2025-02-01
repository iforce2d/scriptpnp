#ifndef VISION_H
#define VISION_H

#include <stdint.h>
#include <vector>

#include "script/api.h"

#define VISION_DEFAULT_COLOR            255
//#define VISION_DEFAULT_THRESHOLD        64
//#define VISION_DEFAULT_MASKSIZE         480
#define VISION_DEFAULT_MINPIXELS        50
#define VISION_DEFAULT_MAXPIXELS        100000
#define VISION_DEFAULT_MINWIDTH         10
#define VISION_DEFAULT_MAXWIDTH         200

typedef struct {
    uint8_t color;
    //uint8_t threshold;
    //int maskSize;
    int minPixels;
    int maxPixels;
    int minWidth;
    int maxWidth;
} blobParams_t;

/*typedef struct {
    int size;
    float x;
    float y;
    int bb_x1, bb_y1, bb_x2, bb_y2;
} blob_t;*/

typedef struct {
    uint8_t* bytes;
    int width;
    int height;

    blobParams_t params;

    int frame;
    std::vector<script_blob> blobs;
    //script_blob* bestblob;
    //float bbdx;
    //float bbdy;
} blobRun_t;

//extern blobrun_t br;
//extern visionParams_t visionParams;

void initVision();
bool runQuickblob(blobRun_t* blobrun);

#endif // VISION_H
