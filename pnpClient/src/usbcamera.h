#ifndef USBCAMERA_H
#define USBCAMERA_H

#include <string>
#include <vector>
#include <atomic>

#include "libuvc/libuvc.h"

#include "visionvideoview.h"
#include "script_vision.h"

#define PROCESS_TIME_MA_COUNT   60

struct usbCameraFeature_u16_t {
    bool alreadyInited;
    bool notSupported;
    uint16_t current;
    uint16_t step;
    uint16_t min;
    uint16_t max;
    uint8_t isAuto;
    bool dirty;
    usbCameraFeature_u16_t() {
        isAuto = 0;
        min = 0;
        max = 0;
        step = 0;
        current = 0;
        alreadyInited = false; // min/max/steps etc have been fetched
        notSupported = false;
        dirty = true; // script changed something, UI needs update
    }
};

struct usbCameraFeature_i16_t {
    bool alreadyInited;
    bool notSupported;
    int16_t current;
    int16_t step;
    int16_t min;
    int16_t max;
    uint8_t isAuto;
    bool dirty;
    usbCameraFeature_i16_t() {
        isAuto = 0;
        min = 0;
        max = 0;
        step = 0;
        current = 0;
        alreadyInited = false; // min/max/steps etc have been fetched
        notSupported = false;
        dirty = true; // script changed something, UI needs update
    }
};

struct usbCameraFeature_u32_t {
    bool alreadyInited = false;
    bool notSupported;
    uint32_t current;
    uint32_t step;
    uint32_t min;
    uint32_t max;
    uint8_t isAuto;
    bool dirty;
    usbCameraFeature_u32_t() {
        isAuto = 0;
        min = 0;
        max = 0;
        step = 0;
        current = 0;
        alreadyInited = false;
        notSupported = false;
        dirty = true;
    }
};

struct usbCameraFrameMode_t {
    char fourcc[5];
    int width;
    int height;
    float fps;
};

// struct videoFrameBuffers_t {
//     uint8_t* rgbData;   // owned by libuvc
//     uint8_t* grayData;  // owned by app
// };

struct usbCameraInfo_t
{
    int index;
    uvc_device_descriptor_t descriptor;
    uvc_context_t *ctx;
    uvc_device_handle_t *devh;
    usbCameraFrameMode_t mode;    
    bool continuousUpdate;
    bool preferMJPG;
    uvc_frame_format currentFrameFormat;

    std::string idHash;

    bool uvcAllocateFrameAlreadyFailed;
    uvc_frame_t *rgbFrame;
    videoFrameBuffers_t frameBuffers;
    visionContext_t visionContext;
    std::atomic<bool> * frameBufferLocked;
    uint8_t frameCounter;
    uint8_t lastProcessedFrameCounter;
    long long frameProcesstime;

    int maCounts[PROCESS_TIME_MA_COUNT];
    int maIndex;
    int maTotal;

    VisionVideoView visionVideoView;
    usbCameraFeature_u16_t zoom;
    usbCameraFeature_u16_t focus;
    usbCameraFeature_u32_t exposure;
    usbCameraFeature_u16_t whiteBalance;
    usbCameraFeature_u16_t saturation;
    usbCameraFeature_u16_t sharpness;
    usbCameraFeature_i16_t brightness;
    usbCameraFeature_u16_t contrast;
    usbCameraFeature_u16_t gamma;
    usbCameraFeature_i16_t hue;
    usbCameraInfo_t() {
        index = -1;
        ctx = NULL;
        devh = NULL;
        uvcAllocateFrameAlreadyFailed = false;
        rgbFrame = NULL;
        frameBuffers.rgbData = NULL;
        frameBuffers.grayData = NULL;
        frameBufferLocked = new std::atomic<bool>(false);
        frameCounter = 0;
        lastProcessedFrameCounter = 0;

        frameProcesstime = 0;
        memset(maCounts, 0, sizeof(maCounts));
        maIndex = 0;
        maTotal = 0;

        //frameBufferLocked->store(false);
        currentFrameFormat = UVC_FRAME_FORMAT_YUYV;
        continuousUpdate = true;
        preferMJPG = false;
    }
    void updateMovingAverage(long int n) {
        maTotal -= maCounts[maIndex];
        maCounts[maIndex] = n;
        maTotal += n;
        maIndex = (maIndex + 1) % PROCESS_TIME_MA_COUNT;
    }
    ~usbCameraInfo_t() {
        delete frameBufferLocked;
    }
};

bool enumerateUSBCameras(std::vector<usbCameraInfo_t*> &infos);
void showUSBCameraControl(bool* p_open);
void showOpenUSBCameraViews();
void closeAllUSBCameras();
int script_getUSBCameraIndexByHash(std::string fragment);
bool grabUSBCameraFrame(int index, videoFrameBuffers_t* buffers);

#endif
