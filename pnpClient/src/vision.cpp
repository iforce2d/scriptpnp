
#include <algorithm>

#include "vision.h"
#include "quickblob.h"

//int visionCount = 0;

//blobParams_t visionParams;
//blobRun_t br;
//blob_t* bestblob = NULL;

int init_pixel_stream_hook(void* user_struct, struct stream_state* stream) {
    blobRun_t* br = (blobRun_t*)user_struct;

    // if ( br->params.maskSize > br->width )
    //     br->params.maskSize = br->width;
    // if ( br->params.maskSize > br->height )
    //     br->params.maskSize = br->height;

    //stream->w = br->params.maskSize;
    //stream->h = br->params.maskSize;

    stream->w = br->width;
    stream->h = br->height;

    return 0;
}

int close_pixel_stream_hook(void* user_struct, struct stream_state* stream) {
    // free up anything you allocated in init_pixel_stream_hook
    return 0;
}

int next_row_hook(void* user_struct, struct stream_state* stream) {
    // load the (grayscale) row at stream->y into the (8 bit) stream->row array
    blobRun_t* br = (blobRun_t*)user_struct;

    //stream->row = br->image->bits() + stream->y * stream->w;

    int startOffsetX = 0;//(br->width - br->params.maskSize) / 2;
    int startOffsetY = 0;//(br->height - br->params.maskSize) / 2;
    stream->row = br->bytes + (stream->y+startOffsetY) * br->width + startOffsetX;

    return 0;
}

int next_frame_hook(void* user_struct, struct stream_state* stream) {
    blobRun_t* br = (blobRun_t*)user_struct;
    return br->frame++;
}

void log_blob_hook(void* user_struct, struct blob* b) {
    blobRun_t* br = (blobRun_t*)user_struct;
    if ( b->color != br->params.color || b->size < br->params.minPixels || b->size > br->params.maxPixels )
        return;
    int w = b->bb_x2 - b->bb_x1;
    if ( w < br->params.minWidth || w > br->params.maxWidth )
        return;
    int h = b->bb_y2 - b->bb_y1;
    if ( h < br->params.minWidth || h > br->params.maxWidth )
        return;
    /*int diff = abs(w - h);
    int maxAspectDiff = ((br->params.maxWidth + br->params.minWidth) / 2) / 10;
    if ( diff > maxAspectDiff )
        return;

    float radius = (w+h) / 4.0f;
    int area = 3.14159265359f * radius * radius;
    int areaDiff = abs(area - b->size);
    if ( areaDiff > area / 4 )
        return;*/

    int startOffsetX = 0;//(br->width - br->params.maskSize) / 2;
    int startOffsetY = 0;//(br->height - br->params.maskSize) / 2;

    script_blob bs;
    bs.size = b->size;
    bs.ax = b->center_x + startOffsetX;
    bs.ay = b->center_y + startOffsetY;
    bs.bb_x1 = b->bb_x1 + startOffsetX;
    bs.bb_x2 = b->bb_x2 + startOffsetX;
    bs.bb_y1 = b->bb_y1 + startOffsetY;
    bs.bb_y2 = b->bb_y2 + startOffsetY;
    bs.pixels = b->size;
    br->blobs.push_back( bs );
}

bool compareBlobSize(script_blob& a, script_blob& b)
{
    return (a.size > b.size);
}


// void initVision() {
//     visionParams.color = VISION_DEFAULT_COLOR;
//     visionParams.threshold = VISION_DEFAULT_THRESHOLD;
//     visionParams.maskSize = VISION_DEFAULT_MASKSIZE;
//     visionParams.minPixels = VISION_DEFAULT_MINPIXELS;
//     visionParams.maxPixels = VISION_DEFAULT_MAXPIXELS;
//     visionParams.minWidth = VISION_DEFAULT_MINWIDTH;
//     visionParams.maxWidth = VISION_DEFAULT_MAXWIDTH;
//     //visionParams.maxAspectDiff = (visionParams.maxWidth + visionParams.minWidth) / 2 / 10;
//     visionParams.saveFile = false;
//     visionParams.displayType = VISION_DEFAULT_DISPLAYTYPE;

//     visionParams.focus = VISION_DEFAULT_FOCUS;
//     visionParams.zoom = VISION_DEFAULT_ZOOM;

//     visionParams.overlayElements = 0xFFFFFFFF;
// }

bool runQuickblob(blobRun_t* br) {

    /*int numBytes = br->width * br->height;
    for ( int i = 0; i < numBytes; i++ ) {
        if (br->bytes[i] > br->params.threshold)
            br->bytes[i] = 255;
        else
            br->bytes[i] = 0;
    }*/

    br->blobs.clear();
    br->blobs.reserve(100);
    if ( extract_image(br) ) {
        return false;
    }


    std::sort(br->blobs.begin(), br->blobs.end(), compareBlobSize);

    //    if ( visionCount++ % 10 == 0 ) {
    //        image->save("threshold.png");
    //    }


    /*br->bestblob = NULL;

    int bestDelta = br->height * br->height;

    int hx = br->width / 2;
    int hy = br->height / 2;

    //std::vector<blob_t>::iterator it = br->blobs.begin();
    //while ( it != br->blobs.end() ) {
    for (script_blob& bs : br->blobs) {

        float dx = bs.ax - hx;
        float dy = bs.ay - hy;
        int delta = dx*dx + dy*dy;
        if ( delta < bestDelta ) {
            bestDelta = delta;
            br->bestblob = &bs;
            br->bbdx = dx;
            br->bbdy = dy;
        }

    }*/

    return true;

}



