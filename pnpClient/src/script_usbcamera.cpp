
#include "libuvc/libuvc.h"
#include "usbcamera.h"
#include "scriptlog.h"
#include "script/engine.h"

#include <vector>

using namespace std;

extern vector<usbCameraInfo_t*> usbCameraInfos;

bool script_setUSBCameraParams(int index, int zoom, int focus, int exposure, int whiteBalance, int saturation)
{
    if ( getActivePreviewOnly() )
        return false;

    ScriptLog* slog = (ScriptLog*)getActiveScriptLog();

    if ( index < 0 || index >= (int)usbCameraInfos.size() ) {
        g_log.log(LL_ERROR, "Invalid USB camera index: %d", index);
        if ( slog )
            slog->log(LL_ERROR, NULL, 0, "Invalid USB camera index: %d", index);
        return false;
    }

    usbCameraInfo_t *info = usbCameraInfos[index];

    if ( ! info->devh ) {
        g_log.log(LL_ERROR, "USB camera state invalid (devh == NULL)");
        if ( slog )
            slog->log(LL_ERROR, NULL, 0, "USB camera state invalid - not opened yet?");
        return false;
    }

    bool allok = true;

    if ( zoom > -1 ) {
        if ( zoom < info->zoom.min || zoom > info->zoom.max ) {
            g_log.log(LL_ERROR, "zoom should be from %d to %d", info->zoom.min, info->zoom.max);
            if ( slog )
                slog->log(LL_ERROR, NULL, 0, "zoom should be from %d to %d", info->zoom.min, info->zoom.max);
            allok = false;
        }
        else {
            allok &= 0 <= uvc_set_zoom_abs(info->devh, zoom);
            uvc_get_zoom_abs(info->devh, &info->zoom.current, UVC_GET_CUR);
            info->zoom.dirty = true;
        }
    }

    if ( focus > -1 ) {
        if ( focus < info->focus.min || focus > info->focus.max ) {
            g_log.log(LL_ERROR, "focus should be from %d to %d", info->focus.min, info->focus.max);
            if ( slog )
                slog->log(LL_ERROR, NULL, 0, "focus should be from %d to %d", info->focus.min, info->focus.max);
            allok = false;
        }
        else {
            allok &= 0 <= uvc_set_focus_auto(info->devh, 0);
            allok &= 0 <= uvc_set_focus_abs(info->devh, focus);
            uvc_get_focus_abs(info->devh, &info->focus.current, UVC_GET_CUR);
            info->focus.dirty = true;
        }
    }

    if ( exposure > -1 ) {
        if ( exposure < (int)info->exposure.min || exposure > (int)info->exposure.max ) {
            g_log.log(LL_ERROR, "exposure should be from %d to %d", info->exposure.min, info->exposure.max);
            if ( slog )
                slog->log(LL_ERROR, NULL, 0, "exposure should be from %d to %d", info->exposure.min, info->exposure.max);
            allok = false;
        }
        else {
            allok &= 0 <= uvc_set_ae_mode(info->devh, 1); // manual
            allok &= 0 <= uvc_set_exposure_abs(info->devh, exposure);
            uvc_get_exposure_abs(info->devh, &info->exposure.current, UVC_GET_CUR);
            info->exposure.dirty = true;
        }
    }

    if ( whiteBalance > -1 ) {
        if ( whiteBalance < info->whiteBalance.min || whiteBalance > info->whiteBalance.max ) {
            g_log.log(LL_ERROR, "whiteBalance should be from %d to %d", info->whiteBalance.min, info->whiteBalance.max);
            if ( slog )
                slog->log(LL_ERROR, NULL, 0, "white balance should be from %d to %d", info->whiteBalance.min, info->whiteBalance.max);
            allok = false;
        }
        else {
            allok &= 0 <= uvc_set_white_balance_temperature_auto(info->devh, 0);
            allok &= 0 <= uvc_set_white_balance_temperature(info->devh, whiteBalance);
            uvc_get_white_balance_temperature(info->devh, &info->whiteBalance.current, UVC_GET_CUR);
            info->whiteBalance.dirty = true;
        }
    }

    if ( saturation > -1 ) {
        if ( saturation < info->saturation.min || saturation > info->saturation.max ) {
            g_log.log(LL_ERROR, "saturation should be from %d to %d", info->saturation.min, info->saturation.max);
            if ( slog )
                slog->log(LL_ERROR, NULL, 0, "saturation should be from %d to %d", info->saturation.min, info->saturation.max);
            allok = false;
        }
        else {
            allok &= 0 <= uvc_set_saturation(info->devh, saturation);
            uvc_get_saturation(info->devh, &info->saturation.current, UVC_GET_CUR);
            info->saturation.dirty = true;
        }
    }

    return allok;
}
