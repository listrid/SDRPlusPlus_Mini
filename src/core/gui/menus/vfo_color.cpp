#include <gui/menus/vfo_color.h>
#include <gui/gui.h>
#include <gui/widgets/waterfall.h>
#include <signal_path/signal_path.h>
#include <string>
#include <core.h>
#include <map>
#include <vector>
#include <utils/hex_color.h>

namespace vfo_color_menu
{
    std::map<std::string, ImVec4> g_vfoColors;
    std::string g_openName = "";
    EventHandler<VFOManager::VFO*> g_vfoAddHndl;

    void vfoAddHandler(VFOManager::VFO* vfo, void* ctx)
    {
        std::string name = vfo->getName();
        if (g_vfoColors.find(name) != g_vfoColors.end())
        {
            ImVec4 col = g_vfoColors[name];
            vfo->setColor(IM_COL32((int)roundf(col.x * 255), (int)roundf(col.y * 255), (int)roundf(col.z * 255), 50));
            return;
        }
        vfo->setColor(IM_COL32(255, 255, 255, 50));
        g_vfoColors[name] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    void saveColors(const std::vector<std::pair<std::string, std::string>>& updates)
    {
        if (updates.empty())
            return;

        auto configAccess = core::configManager.edit();
        ConfigManager::EditSection colors = configAccess.section("vfoColors");
        for (const auto& [name, color] : updates)
            colors[name] = color;
    }

    void init()
    {
        // Snapshot the subtree so parsing and VFO callbacks do not hold the config mutex.
        auto configAccess = core::configManager.read().section("vfoColors");
        auto conf = configAccess.peek();

        for (auto& o : conf)
        {
            std::string name = o.GetName();
            std::string col;
            // If not a string, repair with default
            if(!o.tryGet(col))
            {
                core::configManager.edit().section("vfoColors")[name] = "#FFFFFF";
                g_vfoColors[name] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                if (sigpath::vfoManager.vfoExists(name))
                    sigpath::vfoManager.setColor(name, IM_COL32(255, 255, 255, 50));
                continue;
            }

            // If not a valid hex color, repair with default
            const auto color = color_utils::parseHex<3>(col);
            if (!color)
            {
                core::configManager.edit().section("vfoColors")[name] = "#FFFFFF";
                g_vfoColors[name] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                if (sigpath::vfoManager.vfoExists(name)) {
                    sigpath::vfoManager.setColor(name, IM_COL32(255, 255, 255, 50));
                }
                continue;
            }

            // Since the color is valid, decode it and set the vfo's color
            const auto [red, green, blue] = *color;
            const float r = static_cast<float>(red);
            const float g = static_cast<float>(green);
            const float b = static_cast<float>(blue);
            g_vfoColors[name] = ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
            if (sigpath::vfoManager.vfoExists(name))
                sigpath::vfoManager.setColor(name, IM_COL32(red, green, blue, 50));
        }

        // Iterate existing VFOs and set their color if in the config, if not set to
        // default. In memory only -- the entry is written when the user picks a colour.
        for (auto& [name, vfo] : gui::waterfall.m_map_vfos)
        {
            if (g_vfoColors.find(name) == g_vfoColors.end())
            {
                g_vfoColors[name] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                vfo->m_color = IM_COL32(255, 255, 255, 50);
            }
        }

        g_vfoAddHndl.handler = vfoAddHandler;
        sigpath::vfoManager.onVfoCreated.bindHandler(&g_vfoAddHndl);
    }

    void draw(void* ctx)
    {
        ImGui::BeginTable("VFO Color Buttons Table", 2);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::Button("Auto Color##vfo_color", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        {
            float delta = 1.0f / (float)gui::waterfall.m_map_vfos.size();
            float hue = 0;
            std::vector<std::pair<std::string, std::string>> updates;
            updates.reserve(gui::waterfall.m_map_vfos.size());
            for (auto& [name, vfo] : gui::waterfall.m_map_vfos)
            {
                float r, g, b;
                ImGui::ColorConvertHSVtoRGB(hue, 0.5f, 1.0f, r, g, b);
                g_vfoColors[name] = ImVec4(r, g, b, 1.0f);
                vfo->m_color = IM_COL32((int)roundf(r * 255), (int)roundf(g * 255), (int)roundf(b * 255), 50);
                hue += delta;
                char buf[16];
                sprintf(buf, "#%02X%02X%02X", (int)roundf(r * 255), (int)roundf(g * 255), (int)roundf(b * 255));
                updates.emplace_back(name, buf);
            }
            saveColors(updates);
        }

        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button("Clear All##vfo_color", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        {
            std::vector<std::pair<std::string, std::string>> updates;
            updates.reserve(gui::waterfall.m_map_vfos.size());
            for (auto& [name, vfo] : gui::waterfall.m_map_vfos)
            {
                g_vfoColors[name] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                vfo->m_color = IM_COL32(255, 255, 255, 50);
                updates.emplace_back(name, "#FFFFFF");
            }
            saveColors(updates);
        }

        ImGui::EndTable();

        ImGui::BeginTable("VFO Color table", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders);
        for (auto& [name, vfo] : gui::waterfall.m_map_vfos)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImVec4 col(1.0f, 1.0f, 1.0f, 1.0f);
            if (g_vfoColors.find(name) != g_vfoColors.end())
                col = g_vfoColors[name];
            if (ImGui::ColorEdit3(("##vfo_color_" + name).c_str(), (float*)&col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
            {
                g_vfoColors[name] = col;
                vfo->m_color = IM_COL32((int)roundf(col.x * 255), (int)roundf(col.y * 255), (int)roundf(col.z * 255), 50);
                char buf[16];
                sprintf(buf, "#%02X%02X%02X", (int)roundf(col.x * 255), (int)roundf(col.y * 255), (int)roundf(col.z * 255));
                core::configManager.edit().section("vfoColors")[name] = buf;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(name.c_str());
        }
        ImGui::EndTable();
    }
}
