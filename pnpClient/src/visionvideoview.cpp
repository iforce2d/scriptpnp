
#include "imgui.h"
#include "script/engine.h"
#include "visionvideoview.h"
#include "usbcamera.h"

VisionVideoView::VisionVideoView()
{
    cameraInfo = NULL;
    continuousUpdate = false;;
    entryFunction[0] = 0;
    sprintf(entryFunction, "circleSym");
    shouldTryImageLoad = false;
}

void VisionVideoView::setCameraInfo(usbCameraInfo_t *info)
{
    cameraInfo = info;
}

int VisionVideoView::getLeadingSpace()
{
    int a = 54;
    return a;
}

void VisionVideoView::showLeadingItems(usbCameraInfo_t* info)
{
    ImGui::SameLine();
    ImGui::Text("Entry function:");
    ImGui::SameLine();
    ImGui::PushItemWidth(-140);
    ImGui::InputText("##entryFunction", entryFunction, sizeof(entryFunction), ImGuiInputTextFlags_CharsNoBlank);

    ImGui::SameLine();
    if ( ImGui::Button("Set") ) {
        prepareFunction();
    }

    ImGui::SameLine();
    ImGui::Checkbox("Update", &continuousUpdate);

    int movingAverage = info->maTotal / (float)PROCESS_TIME_MA_COUNT;
    ImGui::Text("%s %dx%d %.1f fps, processing time: %lld us (avg: %d us)", info->mode.fourcc, info->mode.width, info->mode.height, info->mode.fps, info->frameProcesstime, movingAverage);


    ImGui::SetCursorPosY( ImGui::GetCursorPosY() + 4 );
}

void VisionVideoView::prepareFunction()
{
    discardCompiledFunction(compiled);

    if ( ! entryFunction[0] )
        return;

    shouldTryImageLoad = true;

    char buf[64];
    sprintf(buf, "visionVideoModule%d", cameraInfo->index);
    compileScript(buf, entryFunction, compiled);
}

extern bool script_shouldTryImageLoad;

void VisionVideoView::runVision()
{
    visionContext_t* ctx = getVisionContextForThread();
    ctx->renderTexts.clear();

    if ( ! continuousUpdate )
        return;

    if ( compiled.func ) {
        ctx->shouldTryImageLoad = shouldTryImageLoad;
        shouldTryImageLoad = false;
        runCompiledFunction_simple(compiled);
    }
}

extern ImFont* font_visionOverlay;
void VisionVideoView::drawOtherStuff(ImVec2 imgPos, float scale, usbCameraInfo_t* info)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    for (script_renderText& t : info->visionContext.renderTexts) {
        draw_list->AddText(font_visionOverlay, t.fontSize, ImVec2(imgPos.x+scale*t.x,imgPos.y+scale*t.y), ImColor(t.r/255.0f,t.g/255.0f,t.b/255.0f,1.0f), t.text.c_str());
    }
}

























