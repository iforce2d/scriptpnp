
#include "imgui.h"
#include "log.h"
#include "workspace.h"
#include "serialPortInfo.h"

#include "serial_view.h"

using namespace std;

#define SERIAL_WINDOW_TITLE "Serial"

void showSerialView(bool* p_open)
{
    ImGui::SetNextWindowSize(ImVec2(480, 480), ImGuiCond_FirstUseEver);

    doLayoutLoad(SERIAL_WINDOW_TITLE);

    ImGui::Begin(SERIAL_WINDOW_TITLE, p_open);
    {
        if ( ImGui::Button("Refresh ports list") ) {
            updateSerialPortList();
        }

        vector<serialPortInfo*> ports = getSerialPortInfos();

        int d = 0;

        for (serialPortInfo* p : ports) {

            ImGui::SeparatorText(p->name.c_str());

            char buf[128];
            sprintf(buf, "portDetails%d", d++);

            if (ImGui::BeginTable(buf, 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableNextRow();

                sp_port* port = findConnectedPort( p->name );

                if ( port ) {

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("Connected:");
                    ImGui::TableSetColumnIndex(1);

                    if ( ImGui::Button("Disconnect") ) {
                        closePort( port );
                    }

                    ImGui::TableNextRow();
                }

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("VID PID:");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("0x%04X 0x%04X (%s)", p->vid, p->pid, p->type.c_str());

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Type:");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(p->type.c_str());

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Manufacturer:");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(p->manufacturer.c_str());

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Product:");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(p->product.c_str());

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("Description:");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text(p->description.c_str());

                ImGui::SameLine();

                if ( ImGui::Button("Copy") ) {
                    ImGui::LogToClipboard();
                    ImGui::LogText("%s", p->description.c_str());
                    ImGui::LogFinish();
                }

                ImGui::EndTable();
            }
        }
    }

    doLayoutSave(SERIAL_WINDOW_TITLE);

    ImGui::End();
}
