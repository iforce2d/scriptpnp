
#include <stdio.h>
#include <vector>
#include "imgui.h"
#include "videoView.h"
#include "usbcamera.h"
#include "workspace.h"
#include "script/api.h"

//extern void* imgData;

using namespace std;

VideoView::VideoView()
{
    textureId = 0;
    zoom = 1;
    initDone = false;
    doneFirstUpload = false;

    pixels = new uint8_t[640*480*3];
}

VideoView::~VideoView() {
    delete pixels;
}

void VideoView::setup() {
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same
    initDone = true;
}

void VideoView::cleanup() {
    glDeleteTextures(1, &textureId);
    initDone = false;
    doneFirstUpload = false;
}

void VideoView::updateImageData(uint8_t* imgData) {

    if ( imgData ) {
        memcpy( pixels, imgData, 640*480*3 );
    }

    if ( ! initDone )
        setup();

    if ( imgData ) {
        glBindTexture(GL_TEXTURE_2D, textureId);
        if ( ! doneFirstUpload )
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 640, 480, 0, GL_RGB, GL_UNSIGNED_BYTE, imgData);
        else
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 640, 480, GL_RGB, GL_UNSIGNED_BYTE, imgData);
        doneFirstUpload = true;
    }

}

struct aspectControlInfo_t {
    float aspectRatio;
    int leadingSpace;
};

void aspectRatioCallback(ImGuiSizeCallbackData* data)
{
    aspectControlInfo_t* aspectInfo = (aspectControlInfo_t*)data->UserData;
    data->DesiredSize.y = ImGui::GetFrameHeight() +  // window title bar
                          aspectInfo->leadingSpace + // space for UI items at top
            (float)(int)((data->DesiredSize.x + ImGui::GetStyle().WindowPadding.x) / aspectInfo->aspectRatio);
}

void VideoView::show(const char* title, usbCameraInfo_t* info)
{
    ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);

    aspectControlInfo_t aspectInfo;
    aspectInfo.aspectRatio = 640.0f / 480.0f;
    aspectInfo.leadingSpace = getLeadingSpace();

    ImGui::SetNextWindowSizeConstraints( ImVec2(128, 96), ImVec2(FLT_MAX, FLT_MAX), aspectRatioCallback, (void*)&aspectInfo);

    doLayoutLoad(title);

    ImGui::Begin(title);
    {
        showLeadingItems(info);

        if ( textureId ) {

            if (ImGui::IsWindowHovered()) {
                ImGuiIO& io = ImGui::GetIO();
                if ( io.MouseWheel > 0 )
                    zoom /= 1.07f;
                else if ( io.MouseWheel < 0 )
                    zoom *= 1.07f;
                if ( zoom > 1 )
                    zoom = 1;
            }

            ImVec2 uv0;
            ImVec2 uv1;

            uv0.x = 0.5 - 0.5 * zoom;
            uv0.y = 0.5 - 0.5 * zoom;
            uv1.x = 0.5 + 0.5 * zoom;
            uv1.y = 0.5 + 0.5 * zoom;

            ImVec2 s = ImGui::GetWindowSize();
            s.x -= 2*ImGui::GetStyle().WindowPadding.x; // subtract width of paddings
            s.y = s.x / aspectInfo.aspectRatio;
            ImVec2 imgPos = ImGui::GetCursorScreenPos();

            //ImGui::Image((void*)(intptr_t)textureId, s, uv0, uv1);
            ImGui::Image(textureId, s, uv0, uv1);

            float scale = s.x / 640;

            drawOtherStuff(imgPos, scale, info);

            /*ImDrawList* draw_list = ImGui::GetWindowDrawList();
            for (script_renderText& t : info->visionContext.renderTexts) {
                draw_list->AddText(ImVec2(imgPos.x+scale*t.x,imgPos.y+scale*t.y), ImColor(t.r/255.0f,t.g/255.0f,t.b/255.0f,1.0f), t.text.c_str());
            }*/
        }

    }

    doLayoutSave(title);

    ImGui::End();
}





