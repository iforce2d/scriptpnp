#ifndef VIDEOVIEW_H
#define VIDEOVIEW_H

#include <GL/gl.h>
#include "imgui.h"

class VideoView {
    GLuint textureId;
    bool initDone;
    bool doneFirstUpload;
    float zoom;
    uint8_t* pixels;
public:
    VideoView();
    ~VideoView();
    void setup();
    void cleanup();
    void updateImageData(uint8_t* imgData);

    virtual int getLeadingSpace() { return 0; }
    void show(const char *title, struct usbCameraInfo_t* info);
    virtual void showLeadingItems(struct usbCameraInfo_t* info) {}
    virtual void drawOtherStuff(ImVec2 imgPos, float scale, usbCameraInfo_t* info) {}
};

#endif

