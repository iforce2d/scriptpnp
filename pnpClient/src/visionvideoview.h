#ifndef VISIONVIDEOVIEW_H
#define VISIONVIDEOVIEW_H

#include "videoView.h"
#include "scriptexecution.h"

class VisionVideoView : public VideoView
{
    struct usbCameraInfo_t* cameraInfo;
    bool continuousUpdate;
    char entryFunction[128];
    compiledScript_t compiled;

    bool shouldTryImageLoad; // image load should only be attempted once per click of the 'set' button

public:
    VisionVideoView();
    void setCameraInfo(struct usbCameraInfo_t* info);

    int getLeadingSpace();
    void showLeadingItems(struct usbCameraInfo_t* info);

    void prepareFunction();
    void runVision();

    void drawOtherStuff(ImVec2 imgPos, float scale, usbCameraInfo_t *info);
};

#endif // VISIONVIDEOVIEW_H
