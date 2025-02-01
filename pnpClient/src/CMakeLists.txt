cmake_minimum_required(VERSION 2.6)

project(pnpClient)

set(IMGUI_DIR ../imgui)
set(IMPLOT_DIR ../implot)
set(COMMON_DIR ../../common)
set(SCV_DIR ../../common/scv)
set(SCRIPT_DIR script)
set(AS_DIR ../angelscript/sdk/angelscript/include)
set(AS_ADDON_DIR ../angelscript/sdk/add_on)
set(NFD_DIR ../nativefiledialog/src/include)

include_directories (
    .
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${IMPLOT_DIR}
    ${AS_DIR}
    ${AS_ADDON_DIR}
    ${IGFD_DIR}
    ${NFD_DIR}
    ${COMMON_DIR}
    /usr/local/include
)

add_executable(pnpClient
    main.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp
    ${IMPLOT_DIR}/implot_demo.cpp
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
    ${AS_ADDON_DIR}/scriptstdstring/scriptstdstring.cpp
    ${AS_ADDON_DIR}/scriptstdstring/scriptstdstring_utils.cpp
    ${AS_ADDON_DIR}/scriptarray/scriptarray.cpp
    ${AS_ADDON_DIR}/scriptmath/scriptmath.cpp
    ${AS_ADDON_DIR}/scriptdictionary/scriptdictionary.cpp
    ${SCV_DIR}/planner.cpp
    ${SCV_DIR}/vec3.cpp
    net_subscriber.cpp
    net_requester.cpp
    TextEditor.cpp
    codeEditorWindow.cpp
    commandEditorWindow.cpp
    scriptEditorWindow.cpp
    ${COMMON_DIR}/commandlist.cpp
    commandlist_parse.cpp
    ${COMMON_DIR}/commands.cpp
    model.cpp
    log_client.cpp
    ${COMMON_DIR}/pnpMessages.cpp
    usbcamera.cpp
    videoView.cpp
    ${COMMON_DIR}/overrides.cpp
    db.cpp
    ${SCRIPT_DIR}/api.cpp
    ${SCRIPT_DIR}/engine.cpp
    codeEditorDocument.cpp
    codeEditorPalettes.cpp
    scriptlog.cpp
    preview.cpp
    ${COMMON_DIR}/machinelimits.cpp
    plangroup.cpp
    compilestatus.cpp
    eventhooks.cpp
    scriptexecution.cpp
    custompanel.cpp
    script_usbcamera.cpp
    quickblob.cpp
    vision.cpp
    tweakspanel.cpp
    script_tweak.cpp
    visionvideoview.cpp
    script_vision.cpp
    workspace.cpp
    script_globals.cpp
    overrides_view.cpp
    server_view.cpp
    serialPortInfo.cpp
    serial_view.cpp
    script_serial.cpp
    jsoncpp.cpp
    tableView.cpp
    util.cpp
    run.h run.cpp
    scopelock.h scopelock.cpp
    image.h image.cpp
    notify.cpp
    script_probing.h script_probing.cpp
    feedback.h feedback.cpp
)

add_compile_definitions(CLIENT)

# C++14 is only for the convenience of std::chrono_literals, could easily use C++11 if you want to.
# ah never mind... we need C++17 for ZXing (QR codes)
SET(CMAKE_CXX_FLAGS  "-Wall -std=c++17")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)
pkg_check_modules(GTK REQUIRED gtk+-3.0)


if(APPLE)
    target_link_libraries(pnpClient -I/usr/local/include -I/opt/local/include -I/opt/homebrew/include "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo" -L/usr/local/lib -L/opt/local/lib -L/opt/homebrew/lib -lglfw -lzmq /usr/local/lib/libuvc.a -lsqlite3 -ljpeg -lpng -lserialport -lZXing -lusb-1.0 libangelscript.a /usr/local/lib/libassimp.a ${CMAKE_SOURCE_DIR}/../nativefiledialog/build/lib/Release/x64/libnfd.a -lz )
else()
    target_link_libraries(pnpClient ${GLFW3_LIBRARIES} libangelscript.a -lGL -lGLU -lzmq -lpthread -lassimp -luvc -lsqlite3 -lpng -lserialport -lZXing ${CMAKE_SOURCE_DIR}/../nativefiledialog/build/lib/Release/x64/libnfd.a ${GTK_LIBRARIES} )
endif()


