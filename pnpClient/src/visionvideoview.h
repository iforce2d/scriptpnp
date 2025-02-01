#ifndef VISIONVIDEOVIEW_H
#define VISIONVIDEOVIEW_H

#include "videoView.h"
#include "scriptexecution.h"

class VisionVideoView : public VideoView
{
    class usbCameraInfo_t* cameraInfo;
    bool continuousUpdate;
    char entryFunction[128];
    compiledScript_t compiled;
public:
    VisionVideoView();
    void setCameraInfo(class usbCameraInfo_t* info);

    int getLeadingSpace();
    void showLeadingItems(class usbCameraInfo_t* info);

    void prepareFunction();
    void runVision();

    void drawOtherStuff(ImVec2 imgPos, float scale, usbCameraInfo_t *info);
};

#endif // VISIONVIDEOVIEW_H
