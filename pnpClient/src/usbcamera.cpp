
#ifndef __APPLE__
// Rather annoyingly, this needs to be included before libuvc.h even though
// the lib was configured with JPEG support?!? Seems like on MacOS it figures
// things out better.
    #define LIBUVC_HAS_JPEG
#endif

#include <unistd.h>
#include <atomic>
#include <chrono>

#include "libuvc/libuvc.h"
#include "usbcamera.h"
#include "log.h"
#include "videoView.h"
#include "script_vision.h"
#include "workspace.h"
#include "notify.h"

using namespace std;

#define USBCAMERA_WINDOW_TITLE "USB camera control"

//uvc_context_t *ctx = NULL;
//uvc_device_handle_t *devh = NULL;
//uvc_frame_format currentFrameFormat = UVC_FRAME_FORMAT_YUYV; // YUYV will probably be selected first
//uvc_frame_t *rgbFrame = NULL;
//uint8_t* imgData = NULL;
//uint8_t grayscale[640*480*3];

static int controlDifferentiatorIndex = 0;
static char controlDifferentiatorBuffer[128];

// ignore annoying GCC warning about not checking result of snprintf
#ifndef __APPLE__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

static char* cd(const char* str) {
    snprintf(controlDifferentiatorBuffer, sizeof(controlDifferentiatorBuffer), "%s##%d", str, controlDifferentiatorIndex++);
    return controlDifferentiatorBuffer;
}

#ifndef __APPLE__
#pragma GCC diagnostic pop
#endif

vector<usbCameraInfo_t*> usbCameraInfos;

bool getUSBFrameBufferLock(usbCameraInfo_t *info) {
    if ( info->frameBufferLocked->load(std::memory_order_acquire) )
        return false;
    info->frameBufferLocked->store(true, std::memory_order_release);
    return true;
}

void releaseUSBFrameBufferLock(usbCameraInfo_t *info) {
    info->frameBufferLocked->store(false, std::memory_order_release);
}

void usbCameraFrameCallback(uvc_frame_t *frame, void *ptr) {

    usbCameraInfo_t* info = (usbCameraInfo_t*)ptr;
    if ( ! info )
        return;

    if ( info->uvcAllocateFrameAlreadyFailed )
        return;

    if ( ! info->rgbFrame ) {
        info->rgbFrame = uvc_allocate_frame(frame->width * frame->height * 3);
        if ( ! info->rgbFrame ) {
            info->uvcAllocateFrameAlreadyFailed = true; // prevent further attempts
            printf("unable to allocate rgbFrame frame!");
            return;
        }
    }

    // This is called by a separate thread at any time while the camera is open.
    // Need to make sure none of our other code is currently using the frame data.
    // If we can't get this lock, just skip updating the image data for this frame.
    if ( ! getUSBFrameBufferLock(info) ) {
        return;
    }

    //printf("callback! length = %u, ptr = %d\n", frame->data_bytes, (void*) ptr);

    uvc_error_t ret = UVC_SUCCESS;
    switch ( info->currentFrameFormat ) {
    case UVC_FRAME_FORMAT_YUYV:
        ret = uvc_any2rgb(frame, info->rgbFrame);
        break;
    case UVC_FRAME_FORMAT_MJPEG:
        ret = uvc_mjpeg2rgb(frame, info->rgbFrame);
        break; // if this function doesn't link, you need to build libuvc AFTER installing libjpeg-devel
    default:;
    }

    if (ret) {
        uvc_perror(ret, "uvc_any2bgr");
        releaseUSBFrameBufferLock(info);
        return;
    }

    info->frameBuffers.width = frame->width;
    info->frameBuffers.height = frame->height;
    info->frameBuffers.originalData = (uint8_t*)info->rgbFrame->data;

    if ( ! info->frameBuffers.rgbData ) {
        info->frameBuffers.rgbData = new uint8_t[frame->width * frame->height * 3];
    }
    memcpy( info->frameBuffers.rgbData, info->frameBuffers.originalData, frame->width * frame->height * 3 );

    // setActiveScriptFrameBuffers(&info->frameBuffers);

    // info->visionVideoView.runVision();

    // setActiveScriptFrameBuffers(NULL);

    info->frameCounter++;

    releaseUSBFrameBufferLock(info);
}

bool enumerateUSBCameras(vector<usbCameraInfo_t*> &infos)
{
    uvc_context_t *ctx;
    uvc_error_t res = uvc_init(&ctx, NULL);
    if (res < 0) {
        g_log.log(LL_ERROR, "Could not enumerate USB cameras (uvc_init): %s", uvc_strerror(res));
        return false;
    }

    uvc_device_t** devices;
    res = uvc_get_device_list(ctx, &devices);
    if (res < 0) {
        g_log.log(LL_ERROR, "Could not enumerate USB cameras (uvc_get_device_list): %s", uvc_strerror(res));
        uvc_exit(ctx);
        return false;
    }

    int count = 0;
    while ( devices[count] ) {

        uvc_device_descriptor_t* desc;
        res = uvc_get_device_descriptor(devices[count], &desc);
        if ( res < 0 ) {
            g_log.log(LL_ERROR, "Could not enumerate USB cameras (uvc_find_devices): %s", uvc_strerror(res));
        }
        else {
            usbCameraInfo_t* info = new usbCameraInfo_t();
            info->index = count;            
            info->descriptor = *desc;
            info->continuousUpdate = true;
            info->preferMJPG = false;
            info->visionVideoView.setCameraInfo( info );
            char buf[64];
            sprintf(buf, "0x%04X_0x%04X_%s", desc->idVendor, desc->idProduct, desc->serialNumber);
            info->idHash = string(buf);
            infos.push_back(info);

            uvc_free_device_descriptor( desc );
        }

        count++;
    }

    uvc_free_device_list(devices, true);

    uvc_exit(ctx);

    return true;
}

bool fetchUSBCameraFormats(int index) {

    if ( index >= (int)usbCameraInfos.size() ) {
        g_log.log(LL_ERROR, "Invalid index for USB camera: %d", index);
        return false;
    }

    uvc_context_t *ctx;
    uvc_error_t res = uvc_init(&ctx, NULL);
    if (res < 0) {
        g_log.log(LL_ERROR, "Could not open USB camera %d (uvc_init): %s", index, uvc_strerror(res));
        return false;
    }

    uvc_device_t** devices;
    res = uvc_get_device_list(ctx, &devices);
    if (res < 0) {
        g_log.log(LL_ERROR, "Could not open USB camera %d (uvc_get_device_list): %s", index, uvc_strerror(res));
        uvc_exit(ctx);
        return false;
    }

    bool ret = true;

    int count = 0;
    while ( devices[count] ) {
        if ( count == index ) {
            g_log.log(LL_DEBUG, "Inspecting device %d", count);

            uvc_device_handle_t *devh;
            res = uvc_open(devices[count], &devh);
            if ( res < 0 ) {
                g_log.log(LL_ERROR, "Could not open USB camera %d (uvc_open): %s", index, uvc_strerror(res));
                break;
            }

            const uvc_format_desc_t *formatDescOrig = uvc_get_format_descs(devh);

            const uvc_format_desc_t *formatDesc = formatDescOrig;
            while (formatDesc) {
                g_log.log(LL_DEBUG, "Format: %s", formatDesc->guidFormat);

                const uvc_frame_desc_t *frameDesc = formatDesc->frame_descs;
                while ( frameDesc ) {
                    uint32_t* interval = frameDesc->intervals;
                    while (*interval) {
                        float fps = 10000000 / (float)*interval;
                        g_log.log(LL_DEBUG, "    Frame: %d x %d, %.2f fps", frameDesc->wWidth, frameDesc->wHeight, fps);
                        interval++;
                    }
                    frameDesc = frameDesc->next;
                }
                formatDesc = formatDesc->next;
            }

            uvc_close(devh);

            break;
        }
        count++;
    }

    uvc_free_device_list(devices, true);

    uvc_exit(ctx);

    return ret;
}

const char* getFourCCText(uvc_frame_format format) {
    switch( format ) {
    case UVC_FRAME_FORMAT_MJPEG: return "MJPG";
    case UVC_FRAME_FORMAT_YUYV: return "YUYV";
    case UVC_FRAME_FORMAT_H264: return "H264";
    default: return "?";
    }
}

bool openUSBCamera(int index) {

    if ( index >= (int)usbCameraInfos.size() ) {
        g_log.log(LL_ERROR, "Invalid index for USB camera: %d", index);
        return false;
    }

    usbCameraInfo_t *info = usbCameraInfos[index];

    //uvc_context_t *ctx;
    uvc_error_t res = uvc_init(&info->ctx, NULL);
    if (res < 0) {
        g_log.log(LL_ERROR, "Could not open USB camera %d (uvc_init): %s", index, uvc_strerror(res));
        return false;
    }

    uvc_device_t** devices;
    res = uvc_get_device_list(info->ctx, &devices);
    if (res < 0) {
        g_log.log(LL_ERROR, "Could not open USB camera %d (uvc_get_device_list): %s", index, uvc_strerror(res));
        uvc_exit(info->ctx);
        info->ctx = NULL;
        return false;
    }


    bool ret = true;

    int count = 0;
    while ( devices[count] ) {
        if ( count == index ) {
            g_log.log(LL_DEBUG, "Opening device %d", count);

            //uvc_device_handle_t *devh;
            res = uvc_open(devices[count], &info->devh);
            if ( res < 0 ) {
                g_log.log(LL_ERROR, "Could not open USB camera %d (uvc_open): %s", index, uvc_strerror(res));
                break;
            }

            const uvc_format_desc_t *formatDesc = uvc_get_format_descs(info->devh);
            //const uvc_frame_desc_t *frameDesc = formatDesc->frame_descs;

            enum uvc_frame_format frame_format = UVC_FRAME_FORMAT_YUYV;
            /*switch (formatDesc->bDescriptorSubtype) {
            case UVC_VS_FORMAT_MJPEG:
                frame_format = UVC_FRAME_FORMAT_MJPEG;
                break;
            case UVC_VS_FORMAT_FRAME_BASED:
                frame_format = UVC_FRAME_FORMAT_H264;
                break;
            default:
                frame_format = UVC_FRAME_FORMAT_YUYV;
                break;
            }*/

            int width = 640;
            int height = 480;
            int fps = 30;

            // if (frameDesc) {
            //     width = frameDesc->wWidth;
            //     height = frameDesc->wHeight;
            //     fps = 10000000 / frameDesc->dwDefaultFrameInterval;
            // }

            if ( info->preferMJPG )
                frame_format = UVC_FRAME_FORMAT_MJPEG;

            info->currentFrameFormat = frame_format;

            g_log.log(LL_DEBUG, "Trying format: (%4s) %dx%d %dfps", formatDesc->fourccFormat, width, height, fps);

            uvc_stream_ctrl_t ctrl;
            res = uvc_get_stream_ctrl_format_size( info->devh, &ctrl, frame_format, width, height, fps );
            if (res < 0) {
                g_log.log(LL_ERROR, "Could not open USB camera %d (uvc_get_stream_ctrl_format_size): %s", index, uvc_strerror(res));
                uvc_exit(info->ctx);
                info->ctx = NULL;
                info->devh = NULL;
                ret = false;
                break;
            }

            uvc_print_stream_ctrl(&ctrl, stdout);
            fflush(stdout);

            res = uvc_start_streaming(info->devh, &ctrl, usbCameraFrameCallback, info, 0);
            if (res < 0) {
                g_log.log(LL_ERROR, "Could not open USB camera %d (uvc_start_streaming): %s", index, uvc_strerror(res));
                uvc_exit(info->ctx);
                info->ctx = NULL;
                info->devh = NULL;
                ret = false;
                break;
            }

            snprintf(info->mode.fourcc, 5, "%s", getFourCCText(info->currentFrameFormat));
            info->mode.width = width;
            info->mode.height = height;
            info->mode.fps = fps;

            break;
        }
        count++;
    }

    uvc_free_device_list(devices, true);

    if ( ! ret ) {
        //uvc_exit(ctx);
        //ctx = NULL;
        //info->devh = NULL;
    }

    return ret;
}

void handleError(uvc_error_t errorCode, const char *message)
{
    if (errorCode < 0)
    {
        fprintf(stderr, "%s", message);
    }
}

// TODO: the functions below are so similar, try to generalize them

void showUSBCameraZoomSettings(usbCameraInfo_t *info)
{
    if ( info->zoom.notSupported )
        return;

    if ( ! info->zoom.alreadyInited ) {
        uvc_error errorCode = uvc_get_zoom_abs(info->devh, &info->zoom.current, UVC_GET_CUR);
        if ( errorCode < 0 ) {
            g_log.log(LL_ERROR, "libuvc %d: failed to read zoom current - possibly unsupported by this camera?", errorCode);
            info->zoom.notSupported = true;
            return;
        }

        errorCode = uvc_get_zoom_abs(info->devh, &info->zoom.step, UVC_GET_RES);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read zoom step - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_zoom_abs(info->devh, &info->zoom.min, UVC_GET_MIN);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read zoom min - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_zoom_abs(info->devh, &info->zoom.max, UVC_GET_MAX);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read zoom max - possibly unsupported by this camera?", errorCode);

        info->zoom.alreadyInited = true;
    }

    //ImGui::Text("Zoom: min=%d, max=%d, step=%d, current=%d", camerainfo->zoom.min, camerainfo->zoom.max, camerainfo->zoom.step, camerainfo->zoom.current );

    int z = info->zoom.current;
    ImGui::PushItemWidth(240);
    ImGui::SliderInt(cd("Zoom"), &z, info->zoom.min, info->zoom.max);
    if ( z != info->zoom.current ) {
        uvc_set_zoom_abs(info->devh, z);
        uvc_get_zoom_abs(info->devh, &info->zoom.current, UVC_GET_CUR);
    }

    if ( info->zoom.dirty ) {
        uvc_get_zoom_abs(info->devh, &info->zoom.current, UVC_GET_CUR);
        info->zoom.dirty = false;
    }
}

void showUSBCameraFocusSettings(usbCameraInfo_t *info)
{
    if ( info->focus.notSupported )
        return;

    if ( ! info->focus.alreadyInited ) {
        uvc_error errorCode = uvc_get_focus_abs(info->devh, &info->focus.current, UVC_GET_CUR);
        if ( errorCode < 0 ) {
            g_log.log(LL_ERROR, "libuvc %d: failed to read focus current - possibly unsupported by this camera?", errorCode);
            info->focus.notSupported = true;
            return;
        }

        errorCode = uvc_get_focus_abs(info->devh, &info->focus.step, UVC_GET_RES);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read focus step - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_focus_abs(info->devh, &info->focus.min, UVC_GET_MIN);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read focus min - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_focus_abs(info->devh, &info->focus.max, UVC_GET_MAX);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read focus max - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_focus_auto(info->devh, &info->focus.isAuto, UVC_GET_CUR);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read focus auto - possibly unsupported by this camera?", errorCode);

        info->focus.alreadyInited = true;
    }

    //ImGui::Text("Focus: min=%d, max=%d, step=%d, current=%d, auto=%d", camerainfo->focus.min, camerainfo->focus.max, camerainfo->focus.step, camerainfo->focus.current, camerainfo->focus.isAuto );

    int z = info->focus.current;
    ImGui::PushItemWidth(240);
    ImGui::SliderInt(cd("Focus"), &z, info->focus.min, info->focus.max);
    if ( z != info->focus.current ) {
        uvc_set_focus_abs(info->devh, z);
        uvc_get_focus_abs(info->devh, &info->focus.current, UVC_GET_CUR);
    }

    bool af = info->focus.isAuto;
    ImGui::SameLine();
    ImGui::Checkbox(cd("Auto"), &af);
    if ( af != info->focus.isAuto ) {
        uvc_set_focus_auto(info->devh, af);
        uvc_get_focus_auto(info->devh, &info->focus.isAuto, UVC_GET_CUR);
    }

    if ( info->continuousUpdate && info->focus.isAuto )
        uvc_get_focus_abs(info->devh, &info->focus.current, UVC_GET_CUR);

    if ( info->focus.dirty ) {
        uvc_get_focus_auto(info->devh, &info->focus.isAuto, UVC_GET_CUR);
        uvc_get_focus_abs(info->devh, &info->focus.current, UVC_GET_CUR);
        info->focus.dirty = false;
    }

}

void showUSBCameraExposureSettings(usbCameraInfo_t *info)
{
    if ( info->exposure.notSupported )
        return;

    if ( ! info->exposure.alreadyInited ) {
        uvc_error errorCode = uvc_get_exposure_abs(info->devh, &info->exposure.current, UVC_GET_CUR);
        if ( errorCode < 0 ) {
            g_log.log(LL_ERROR, "libuvc %d: failed to read exposure current - possibly unsupported by this camera?", errorCode);
            info->exposure.notSupported = true;
            return;
        }

        errorCode = uvc_get_exposure_abs(info->devh, &info->exposure.step, UVC_GET_RES);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read exposure step - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_exposure_abs(info->devh, &info->exposure.min, UVC_GET_MIN);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read exposure min - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_exposure_abs(info->devh, &info->exposure.max, UVC_GET_MAX);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read exposure max - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_ae_mode(info->devh, &info->exposure.isAuto, UVC_GET_CUR);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read exposure auto - possibly unsupported by this camera?", errorCode);

        info->exposure.alreadyInited = true;
    }

    //ImGui::Text("Exposure: min=%d, max=%d, step=%d, current=%d, auto=%d", camerainfo->exposure.min, camerainfo->exposure.max, camerainfo->exposure.step, camerainfo->exposure.current, camerainfo->exposure.isAuto );

    int e = (int)info->exposure.current;
    ImGui::PushItemWidth(240);
    ImGui::SliderInt(cd("Exposure"), &e, info->exposure.min, info->exposure.max);
    if ( e != (int)info->exposure.current ) {
        uvc_error errorCode = uvc_set_exposure_abs(info->devh, e);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to set exposure - possibly unsupported by this camera?", errorCode);
        uvc_get_exposure_abs(info->devh, &info->exposure.current, UVC_GET_CUR);
    }

    uint8_t ae = info->exposure.isAuto;
    uint8_t aeBefore = ae;

    ImGui::SameLine();
    if ( ImGui::RadioButton(cd("1"), ae == 1) )
        ae = 1;
    ImGui::SetItemTooltip("Full manual");

    ImGui::SameLine();
    if ( ImGui::RadioButton(cd("2"), ae == 2) )
        ae = 2;
    ImGui::SetItemTooltip("Full auto");

    ImGui::SameLine();
    if ( ImGui::RadioButton(cd("4"), ae == 4) )
        ae = 4;
    ImGui::SetItemTooltip("Shutter priority");

    ImGui::SameLine();
    if ( ImGui::RadioButton(cd("8"), ae == 8) )
        ae = 8;
    ImGui::SetItemTooltip("Aperture prority");

    if ( ae != aeBefore ) {
        uvc_error errorCode = uvc_set_ae_mode(info->devh, ae);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to set auto exposure mode - possibly unsupported by this camera?", errorCode);
        uvc_get_ae_mode(info->devh, &info->exposure.isAuto, UVC_GET_CUR);
    }

    if ( info->continuousUpdate && info->exposure.isAuto )
        uvc_get_exposure_abs(info->devh, &info->exposure.current, UVC_GET_CUR);

    if ( info->exposure.dirty ) {
        uvc_get_ae_mode(info->devh, &info->exposure.isAuto, UVC_GET_CUR);
        uvc_get_exposure_abs(info->devh, &info->exposure.current, UVC_GET_CUR);
        info->exposure.dirty = false;
    }
}

void showUSBCameraWhiteBalanceSettings(usbCameraInfo_t *info)
{
    if ( info->whiteBalance.notSupported )
        return;

    if ( ! info->whiteBalance.alreadyInited ) {
        uvc_error errorCode = uvc_get_white_balance_temperature(info->devh, &info->whiteBalance.current, UVC_GET_CUR);
        if ( errorCode < 0 ) {
            g_log.log(LL_ERROR, "libuvc %d: failed to read white balance temp current - possibly unsupported by this camera?", errorCode);
            info->whiteBalance.notSupported = true;
            return;
        }

        errorCode = uvc_get_white_balance_temperature(info->devh, &info->whiteBalance.step, UVC_GET_RES);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read white balance temp step - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_white_balance_temperature(info->devh, &info->whiteBalance.min, UVC_GET_MIN);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read white balance temp min - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_white_balance_temperature(info->devh, &info->whiteBalance.max, UVC_GET_MAX);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read white balance temp max - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_white_balance_temperature_auto(info->devh, &info->whiteBalance.isAuto, UVC_GET_CUR);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read white balance auto - possibly unsupported by this camera?", errorCode);

        info->whiteBalance.alreadyInited = true;
    }

    //ImGui::Text("White balance temperature: min=%d, max=%d, step=%d, current=%d, auto=%d", camerainfo->whiteBalance.min, camerainfo->whiteBalance.max, camerainfo->whiteBalance.step, camerainfo->whiteBalance.current, camerainfo->whiteBalance.isAuto );

    int e = (int)info->whiteBalance.current;
    ImGui::PushItemWidth(240);
    ImGui::SliderInt(cd("White balance temperature"), &e, info->whiteBalance.min, info->whiteBalance.max);
    if ( e != (int)info->whiteBalance.current ) {
        uvc_error errorCode = uvc_set_white_balance_temperature(info->devh, e);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to set white balance temperature - possibly unsupported by this camera?", errorCode);
        uvc_get_white_balance_temperature(info->devh, &info->whiteBalance.current, UVC_GET_CUR);
    }

    bool af = info->whiteBalance.isAuto;
    ImGui::SameLine();
    ImGui::Checkbox(cd("Auto"), &af);
    if ( af != info->whiteBalance.isAuto ) {
        uvc_set_white_balance_temperature_auto(info->devh, af);
        uvc_get_white_balance_temperature_auto(info->devh, &info->whiteBalance.isAuto, UVC_GET_CUR);
    }

    if ( info->continuousUpdate && info->whiteBalance.isAuto )
        uvc_get_white_balance_temperature(info->devh, &info->whiteBalance.current, UVC_GET_CUR);

    if ( info->whiteBalance.dirty ) {
        uvc_get_white_balance_temperature_auto(info->devh, &info->whiteBalance.isAuto, UVC_GET_CUR);
        uvc_get_white_balance_temperature(info->devh, &info->whiteBalance.current, UVC_GET_CUR);
        info->whiteBalance.dirty = false;
    }
}

void showUSBCameraSaturationSettings(usbCameraInfo_t *info)
{
    if ( info->saturation.notSupported )
        return;

    if ( ! info->saturation.alreadyInited ) {
        uvc_error errorCode = uvc_get_saturation(info->devh, &info->saturation.current, UVC_GET_CUR);
        if ( errorCode < 0 ) {
            g_log.log(LL_ERROR, "libuvc %d: failed to read saturation current - possibly unsupported by this camera?", errorCode);
            info->saturation.notSupported = true;
            return;
        }

        errorCode = uvc_get_saturation(info->devh, &info->saturation.step, UVC_GET_RES);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read saturation step - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_saturation(info->devh, &info->saturation.min, UVC_GET_MIN);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read saturation min - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_saturation(info->devh, &info->saturation.max, UVC_GET_MAX);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read saturation max - possibly unsupported by this camera?", errorCode);

        info->saturation.alreadyInited = true;
    }

    //ImGui::Text("Saturation: min=%d, max=%d, step=%d, current=%d", camerainfo->saturation.min, camerainfo->saturation.max, camerainfo->saturation.step, camerainfo->saturation.current );

    int z = info->saturation.current;
    ImGui::PushItemWidth(240);
    ImGui::SliderInt(cd("Saturation"), &z, info->saturation.min, info->saturation.max);
    if ( z != info->saturation.current ) {
        uvc_set_saturation(info->devh, z);
        uvc_get_saturation(info->devh, &info->saturation.current, UVC_GET_CUR);
    }

    if ( info->saturation.dirty ) {
        uvc_get_saturation(info->devh, &info->saturation.current, UVC_GET_CUR);
        info->saturation.dirty = false;
    }
}

void showUSBCameraSharpnessSettings(usbCameraInfo_t *info)
{
    if ( info->sharpness.notSupported )
        return;

    if ( ! info->sharpness.alreadyInited ) {
        uvc_error errorCode = uvc_get_sharpness(info->devh, &info->sharpness.current, UVC_GET_CUR);
        if ( errorCode < 0 ) {
            g_log.log(LL_ERROR, "libuvc %d: failed to read sharpness current - possibly unsupported by this camera?", errorCode);
            info->sharpness.notSupported = true;
            return;
        }

        errorCode = uvc_get_sharpness(info->devh, &info->sharpness.step, UVC_GET_RES);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read sharpness step - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_sharpness(info->devh, &info->sharpness.min, UVC_GET_MIN);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read sharpness min - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_sharpness(info->devh, &info->sharpness.max, UVC_GET_MAX);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read sharpness max - possibly unsupported by this camera?", errorCode);

        info->sharpness.alreadyInited = true;
    }

    //ImGui::Text("Saturation: min=%d, max=%d, step=%d, current=%d", camerainfo->saturation.min, camerainfo->saturation.max, camerainfo->saturation.step, camerainfo->saturation.current );

    int z = info->sharpness.current;
    ImGui::PushItemWidth(240);
    ImGui::SliderInt(cd("Sharpness"), &z, info->sharpness.min, info->sharpness.max);
    if ( z != info->sharpness.current ) {
        uvc_set_sharpness(info->devh, z);
        uvc_get_sharpness(info->devh, &info->sharpness.current, UVC_GET_CUR);
    }

    if ( info->sharpness.dirty ) {
        uvc_get_sharpness(info->devh, &info->sharpness.current, UVC_GET_CUR);
        info->sharpness.dirty = false;
    }
}

void showUSBCameraBrightnessSettings(usbCameraInfo_t *info)
{
    if ( info->brightness.notSupported )
        return;

    if ( ! info->brightness.alreadyInited ) {
        uvc_error errorCode = uvc_get_brightness(info->devh, &info->brightness.current, UVC_GET_CUR);
        if ( errorCode < 0 ) {
            g_log.log(LL_ERROR, "libuvc %d: failed to read brightness current - possibly unsupported by this camera?", errorCode);
            info->brightness.notSupported = true;
            return;
        }

        errorCode = uvc_get_brightness(info->devh, &info->brightness.step, UVC_GET_RES);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read brightness step - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_brightness(info->devh, &info->brightness.min, UVC_GET_MIN);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read brightness min - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_brightness(info->devh, &info->brightness.max, UVC_GET_MAX);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read brightness max - possibly unsupported by this camera?", errorCode);

        info->brightness.alreadyInited = true;
    }

    //ImGui::Text("Saturation: min=%d, max=%d, step=%d, current=%d", camerainfo->saturation.min, camerainfo->saturation.max, camerainfo->saturation.step, camerainfo->saturation.current );

    int z = info->brightness.current;
    ImGui::PushItemWidth(240);
    ImGui::SliderInt(cd("Brightness"), &z, info->brightness.min, info->brightness.max);
    if ( z != info->brightness.current ) {
        uvc_set_brightness(info->devh, z);
        uvc_get_brightness(info->devh, &info->brightness.current, UVC_GET_CUR);
    }

    if ( info->brightness.dirty ) {
        uvc_get_brightness(info->devh, &info->brightness.current, UVC_GET_CUR);
        info->brightness.dirty = false;
    }
}

void showUSBCameraContrastSettings(usbCameraInfo_t *info)
{
    if ( info->contrast.notSupported )
        return;

    if ( ! info->contrast.alreadyInited ) {
        uvc_error errorCode = uvc_get_contrast(info->devh, &info->contrast.current, UVC_GET_CUR);
        if ( errorCode < 0 ) {
            g_log.log(LL_ERROR, "libuvc %d: failed to read contrast current - possibly unsupported by this camera?", errorCode);
            info->contrast.notSupported = true;
            return;
        }

        errorCode = uvc_get_contrast(info->devh, &info->contrast.step, UVC_GET_RES);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read contrast step - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_contrast(info->devh, &info->contrast.min, UVC_GET_MIN);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read contrast min - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_contrast(info->devh, &info->contrast.max, UVC_GET_MAX);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read contrast max - possibly unsupported by this camera?", errorCode);

        info->contrast.alreadyInited = true;
    }

    //ImGui::Text("Saturation: min=%d, max=%d, step=%d, current=%d", camerainfo->saturation.min, camerainfo->saturation.max, camerainfo->saturation.step, camerainfo->saturation.current );

    int z = info->contrast.current;
    ImGui::PushItemWidth(240);
    ImGui::SliderInt(cd("Contrast"), &z, info->contrast.min, info->contrast.max);
    if ( z != info->contrast.current ) {
        uvc_set_contrast(info->devh, z);
        uvc_get_contrast(info->devh, &info->contrast.current, UVC_GET_CUR);
    }

    bool af = info->contrast.isAuto;
    ImGui::SameLine();
    ImGui::Checkbox(cd("Auto"), &af);
    if ( af != info->contrast.isAuto ) {
        uvc_set_contrast_auto(info->devh, af);
        uvc_get_contrast_auto(info->devh, &info->contrast.isAuto, UVC_GET_CUR);
    }

    if ( info->continuousUpdate && info->contrast.isAuto )
        uvc_get_contrast(info->devh, &info->contrast.current, UVC_GET_CUR);

    if ( info->contrast.dirty ) {
        uvc_get_contrast(info->devh, &info->contrast.current, UVC_GET_CUR);
        info->contrast.dirty = false;
    }
}

void showUSBCameraGammaSettings(usbCameraInfo_t *info)
{
    if ( info->gamma.notSupported )
        return;

    if ( ! info->gamma.alreadyInited ) {
        uvc_error errorCode = uvc_get_gamma(info->devh, &info->gamma.current, UVC_GET_CUR);
        if ( errorCode < 0 ) {
            g_log.log(LL_ERROR, "libuvc %d: failed to read gamma current - possibly unsupported by this camera?", errorCode);
            info->gamma.notSupported = true;
            return;
        }

        errorCode = uvc_get_gamma(info->devh, &info->gamma.step, UVC_GET_RES);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read gamma step - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_gamma(info->devh, &info->gamma.min, UVC_GET_MIN);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read gamma min - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_gamma(info->devh, &info->gamma.max, UVC_GET_MAX);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read gamma max - possibly unsupported by this camera?", errorCode);

        info->gamma.alreadyInited = true;
    }

    //ImGui::Text("Saturation: min=%d, max=%d, step=%d, current=%d", camerainfo->saturation.min, camerainfo->saturation.max, camerainfo->saturation.step, camerainfo->saturation.current );

    int z = info->gamma.current;
    ImGui::PushItemWidth(240);
    ImGui::SliderInt(cd("Gamma"), &z, info->gamma.min, info->gamma.max);
    if ( z != info->gamma.current ) {
        uvc_set_gamma(info->devh, z);
        uvc_get_gamma(info->devh, &info->gamma.current, UVC_GET_CUR);
    }

    if ( info->gamma.dirty ) {
        uvc_get_gamma(info->devh, &info->gamma.current, UVC_GET_CUR);
        info->gamma.dirty = false;
    }
}

void showUSBCameraHueSettings(usbCameraInfo_t *info)
{
    if ( info->hue.notSupported )
        return;

    if ( ! info->hue.alreadyInited ) {
        uvc_error errorCode = uvc_get_hue(info->devh, &info->hue.current, UVC_GET_CUR);
        if ( errorCode < 0 ) {
            g_log.log(LL_ERROR, "libuvc %d: failed to read hue current - possibly unsupported by this camera?", errorCode);
            info->hue.notSupported = true;
            return;
        }

        errorCode = uvc_get_hue(info->devh, &info->hue.step, UVC_GET_RES);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read hue step - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_hue(info->devh, &info->hue.min, UVC_GET_MIN);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read hue min - possibly unsupported by this camera?", errorCode);

        errorCode = uvc_get_hue(info->devh, &info->hue.max, UVC_GET_MAX);
        if ( errorCode < 0 )
            g_log.log(LL_ERROR, "libuvc %d: failed to read hue max - possibly unsupported by this camera?", errorCode);

        info->hue.alreadyInited = true;
    }

    //ImGui::Text("Saturation: min=%d, max=%d, step=%d, current=%d", camerainfo->saturation.min, camerainfo->saturation.max, camerainfo->saturation.step, camerainfo->saturation.current );

    int z = info->hue.current;
    ImGui::PushItemWidth(240);
    ImGui::SliderInt(cd("Hue"), &z, info->hue.min, info->hue.max);
    if ( z != info->hue.current ) {
        uvc_set_hue(info->devh, z);
        uvc_get_hue(info->devh, &info->hue.current, UVC_GET_CUR);
    }

    bool af = info->hue.isAuto;
    ImGui::SameLine();
    ImGui::Checkbox(cd("Auto"), &af);
    if ( af != info->hue.isAuto ) {
        uvc_set_hue_auto(info->devh, af);
        uvc_get_hue_auto(info->devh, &info->hue.isAuto, UVC_GET_CUR);
    }

    if ( info->continuousUpdate && info->hue.isAuto )
        uvc_get_hue(info->devh, &info->hue.current, UVC_GET_CUR);

    if ( info->hue.dirty ) {
        uvc_get_hue(info->devh, &info->hue.current, UVC_GET_CUR);
        info->hue.dirty = false;
    }
}

void closeUSBCamera(usbCameraInfo_t* info);

void showUSBCameraControl(bool* p_open)
{
    controlDifferentiatorIndex = 0;

    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);

    doLayoutLoad(USBCAMERA_WINDOW_TITLE);

    ImGui::Begin(USBCAMERA_WINDOW_TITLE, p_open);
    {
        if ( ImGui::Button("Detect cameras") ) {

            bool anyCameraOpen = false;
            for ( usbCameraInfo_t* info : usbCameraInfos ) {
                if ( info->devh )
                    anyCameraOpen = true;
            }

            if ( anyCameraOpen ) {
                notify( "Please close any open cameras first", NT_WARNING, 4000 );
            }
            else {
                usbCameraInfos.clear();
                enumerateUSBCameras(usbCameraInfos);
            }
        }

        //ImGui::Text("Detected cameras:");

        for (int i = 0; i < (int)usbCameraInfos.size(); i++) {

            char buf[64];
            bool shouldCloseCamera = false;

            usbCameraInfo_t *info = usbCameraInfos[i];
            //ImGui::Text("Index: %d, Vendor ID: 0x%04X, Product ID: 0x%04X", info->index, info->descriptor.idVendor, info->descriptor.idProduct);
            ImGui::Text("Index: %d, hash: %s", info->index, info->idHash.c_str());

            ImGui::SameLine();
            sprintf(buf, "Fetch formats##%d", i);
            if ( ImGui::Button(buf) ) {
                fetchUSBCameraFormats(info->index);
            }

            ImGui::Indent();
            {
                sprintf(buf, "Prefer MJPG##%d", i);
                ImGui::Checkbox(buf, &info->preferMJPG);

                ImGui::SameLine();
                sprintf(buf, "Open##%d", i);
                if ( ImGui::Button(buf) ) {
                    openUSBCamera(info->index);
                }


                if ( info->devh ) {
                    ImGui::SameLine();
                    sprintf(buf, "Close##%d", i);
                    if ( ImGui::Button(buf) ) {
                        shouldCloseCamera = true;
                    }

                    ImGui::SameLine();
                    ImGui::Checkbox(cd("Continuous update"), &info->continuousUpdate);

                    //ImGui::Text("%s %dx%d %.2f fps", info->mode.fourcc, info->mode.width, info->mode.height, info->mode.fps);

                    showUSBCameraZoomSettings(info);
                    showUSBCameraFocusSettings(info);
                    showUSBCameraExposureSettings(info);
                    showUSBCameraWhiteBalanceSettings(info);
                    showUSBCameraSaturationSettings(info);

                    showUSBCameraSharpnessSettings(info);
                    showUSBCameraBrightnessSettings(info);
                    showUSBCameraContrastSettings(info);
                    showUSBCameraGammaSettings(info);
                    showUSBCameraHueSettings(info);


                    bool didProcessFrame = false;
                    std::chrono::steady_clock::time_point t0;
                    std::chrono::steady_clock::time_point t1;
                    if ( info->lastProcessedFrameCounter != info->frameCounter ) // ideally we would get the lock before checking info->frameCounter, but we only want to check if it is different, not particularly interested in the actual value
                    {
                        // usbCameraFrameCallback() might be updating the rgbData buffer in a separate thread.
                        // If we can't obtain the buffer lock, just skip updating the image texture in this frame.
                        if ( getUSBFrameBufferLock(info) ) // once we get this lock, give it back asap
                        {
                            t0 = std::chrono::steady_clock::now();
                            info->visionContext.buffers = &info->frameBuffers;
                            setMainThreadVisionContext(&info->visionContext);
                            info->visionVideoView.runVision();
                            //setActiveScriptFrameBuffers(NULL);
                            t1 = std::chrono::steady_clock::now();
                            info->lastProcessedFrameCounter = info->frameCounter;
                            releaseUSBFrameBufferLock(info);
                            didProcessFrame = true;
                        }
                    }
                    if ( didProcessFrame ) {
                        info->frameProcesstime = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
                        info->updateMovingAverage(info->frameProcesstime);
                        info->visionVideoView.updateImageData( info->frameBuffers.rgbData );
                    }

                    //sprintf(buf, "USB camera %d", i);
                    //info->visionVideoView.show(buf, info);

                    //ImGui::Text("Processing time: (%ld us)", info->frameProcesstime);
                }

                ImGui::Unindent();
            }

            if ( shouldCloseCamera ) {
                closeUSBCamera(info);
            }

            //            ImGui::Text("    UVC compliance level: %d", info->descriptor.bcdUVC);
            //            ImGui::Text("    Serial number: %s", info->descriptor.serialNumber);
            //            ImGui::Text("    Manufacturer: %s", info->descriptor.manufacturer);
            //            ImGui::Text("    Product: %s", info->descriptor.product);
        }
    }

    doLayoutSave(USBCAMERA_WINDOW_TITLE);

    ImGui::End();
}

void showOpenUSBCameraViews()
{
    for (int i = 0; i < (int)usbCameraInfos.size(); i++) {
        usbCameraInfo_t *info = usbCameraInfos[i];
        if ( info->devh ) {
            char buf[64];
            sprintf(buf, "USB camera %d", i);
            info->visionVideoView.show(buf, info);
        }
    }
}

void closeUSBCamera(usbCameraInfo_t* info)
{
    if ( ! info )
        return;

    if ( info->devh ) {
        g_log.log(LL_DEBUG, "Stopping USB camera stream");
        uvc_stop_streaming(info->devh); // Blocks until last callback is serviced
        uvc_close(info->devh);
    }

    info->visionVideoView.cleanup();

    if ( info->ctx ) {
        try {
            uvc_exit(info->ctx);
        }
        catch(...) {
            g_log.log(LL_ERROR, "Caught exception closing USB camera %d", info->index);
        }
    }

    info->devh = NULL;
    info->ctx = NULL;
    info->frameBuffers.rgbData = NULL;

    if ( info->frameBuffers.grayData )
        delete[] info->frameBuffers.grayData;
    info->frameBuffers.grayData = NULL;

    if ( info->rgbFrame ) {
        uvc_free_frame(info->rgbFrame);
        info->rgbFrame = NULL;
    }
}

void closeAllUSBCameras() {

    for ( usbCameraInfo_t *info : usbCameraInfos ) {
        closeUSBCamera(info);
        delete info;
    }

    usbCameraInfos.clear();
}

bool grabUSBCameraFrame(int index, videoFrameBuffers_t* buffers) {

    if ( index >= (int)usbCameraInfos.size() ) {
        //g_log.log(LL_ERROR, "Invalid index for USB camera: %d", index);
        return false;
    }

    usbCameraInfo_t *info = usbCameraInfos[index];
    if ( ! info->devh )
        return false;

    if ( ! buffers->rgbData ) {
        initFrameBuffers( buffers, info->frameBuffers.width, info->frameBuffers.height );
    }

    int srcSize = info->frameBuffers.width * info->frameBuffers.height * 3;
    int dstSize = buffers->width * buffers->height * 3;

    if ( dstSize != srcSize )
        return false;

    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();

    int retries = 1000;
    while ( retries > 0 ) {
        if ( getUSBFrameBufferLock(info) ) // once we get this lock, give it back asap
            break;
        std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
        long long waited = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        if ( waited > 200 ) {
            g_log.log(LL_WARN, "grabFrame giving up after 200 milliseconds!");
            return false; // no grab after 200 millseconds
        }
        retries--;
    }
    if ( retries < 1 ) {
        g_log.log(LL_WARN, "grabFrame giving up after 1000 attempts!");
        return false;
    }

    //if ( getUSBFrameBufferLock(info) ) // once we get this lock, give it back asap
    {
        memcpy(buffers->rgbData, info->frameBuffers.originalData, dstSize);
        releaseUSBFrameBufferLock(info);
        return true;
    }

    return false;
}

int script_getUSBCameraIndexByHash(string fragment) {

    for ( usbCameraInfo_t* info : usbCameraInfos ) {
        if ( info->idHash.find(fragment) != string::npos ) {
            return info->index;
        }
    }

    return -1;
}


































