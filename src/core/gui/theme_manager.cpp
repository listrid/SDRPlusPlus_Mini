#include <config.h>
#include <gui/theme_manager.h>
#include <imgui_internal.h>
#include <utils/flog.h>
#include <utils/hex_color.h>
#include <filesystem>
#include <fstream>
#include <utility>

bool ThemeManager::loadThemesFromDir(const std::string& path)
{
    if (!std::filesystem::is_directory(path))
    {
        flog::error("Theme directory doesn't exist: {0}", path);
        return false;
    }
    m_themes.clear();
    for (const auto& file : std::filesystem::directory_iterator(path))
    {
        const std::string themePath = file.path().generic_string();
        if (file.path().extension().generic_string() != ".json")
            continue;
        loadTheme(themePath);
    }
    return true;
}

bool ThemeManager::loadTheme(const std::string& path)
{
    ConfigManagerImpl json;
    json.setFile(path.c_str(), true);
    if(!json.load())
    {
        flog::error("Theme file incorrect: {0}", path);
        return false;
    }
    auto data = json.read();

     // Load theme name
    if(!data.contains("name"))
    {
        flog::error("Theme {0} is missing the name parameter", path);
        return false;
    }

    std::string name;
    if(!data.tryGet("name", name))
    {
        flog::error("A theme named '{0}' already exists", path);
        return false;
    }

    // Load defaults in theme
    Theme thm;
    thm.author = "--";
    for(size_t i = 0; i < sizeof(thm.data)/sizeof(thm.data[0]); i++)
        thm.data[i] = 0xFF000000;

    // Load theme author if available
    if(data.contains("author"))
    {
        if(!data.tryGet("author", thm.author))
        {
            flog::error("Theme {0} contains invalid author field. Expected string", path);
            return false;
        }
    }
    for (const auto& item : data.peek())
    {
        const std::string param = item.GetName();
        if (param == "name" || param == "author") { continue; }
        std::string val;
        if(!item.tryGet(val))
        {
            flog::error("Theme {0} contains invalid {1} field. Expected string", path, param);
            return false;
        }

        std::optional<std::array<uint8_t, 4>> channels = color_utils::parseHex<4>(val);
        if(!channels)
        {
            flog::error("Theme {0} contains invalid {1} field. Expected hex RGBA color", path, param);
            return false;
        }
        const auto [red, green, blue, alfa] = *channels;
        uint32_t color = (red << 24) | (green<<16)|(blue<<8)|alfa;

        // Exception for non-imgu colors
        if (param == "WaterfallBackground")
        {
            thm.data[ImGuiCol_WaterfallBackground] = color;
            continue;
        }
        if(param == "ClearColor")
        {
            thm.data[ImGuiCol_ClearColor] = color;
            continue;
        }
        if(param == "FFTHoldColor")
        {
            thm.data[ImGuiCol_FFTHoldColor] = color;
            continue;
        }
        auto id = m_IMGUI_COL_IDS.find(param);
        if (id == m_IMGUI_COL_IDS.end())
        {
            flog::error("Theme {0} contains unknown {1} field.", path, param);
            return false;
        }
        thm.data[id->second] =color;
    }
    m_themes.emplace(std::move(name), std::move(thm));
   
    return true;
}

bool ThemeManager::applyTheme(const std::string& name)
{
    const auto selected = m_themes.find(name);
    if (selected == m_themes.end())
    {
        if(loadInternal(name))
            return true;
        flog::error("Unknown theme: {0}", name);
        return false;
    }

    ImGui::StyleColorsDark();

    auto& style = ImGui::GetStyle();

    // Subtle desktop rounding, in logical pixels: applyTheme() runs as the
    // resetStyle callback of style::applyScaledStyle(), which scales these by
    // the UI scale afterwards. Windows and children stay square: they run
    // edge-to-edge, so rounding would only clip corners against the viewport.
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.PopupRounding = 5.0f;
    style.TabRounding = 3.0f;
    style.ScrollbarRounding = 9.0f;

    ImVec4* style_colors = style.Colors;
    const Theme& thm = selected->second;

    for(int i = 0; i < ImGuiCol_COUNT_Ex; i++)
    {
        if(thm.data[i] == 0xFF000000)
            continue;
        uint8_t* rgba = (uint8_t*)&thm.data[i];

        ImVec4 v = ImVec4((float)rgba[3] / 255.0f, (float)rgba[2] / 255.0f, (float)rgba[1] / 255.0f, (float)rgba[0] / 255.0f);
        if(i == ImGuiCol_WaterfallBackground)
        {
            ThemeManager::m_waterfallBg = v;
            continue;
        }
        if(i == ImGuiCol_ClearColor)
        {
            ThemeManager::m_clearColor = v;
            continue;
        }
        if(i == ImGuiCol_FFTHoldColor)
        {
            ThemeManager::m_fftHoldColor = v;
            continue;
        }
        if(i < ImGuiCol_COUNT)
            style_colors[i] = v;
    }
    return true;
}

std::vector<std::string> ThemeManager::getThemeNames() const {
    std::vector<std::string> names;
    names.reserve(m_themes.size() + 6);

    internalGetNames(names);

    for (const auto& theme : m_themes) { names.push_back(theme.first); }
    return names;
}

std::map<std::string, int> ThemeManager::m_IMGUI_COL_IDS = {
    { "Text", ImGuiCol_Text },
    { "TextDisabled", ImGuiCol_TextDisabled },
    { "WindowBg", ImGuiCol_WindowBg },
    { "ChildBg", ImGuiCol_ChildBg },
    { "PopupBg", ImGuiCol_PopupBg },
    { "Border", ImGuiCol_Border },
    { "BorderShadow", ImGuiCol_BorderShadow },
    { "FrameBg", ImGuiCol_FrameBg },
    { "FrameBgHovered", ImGuiCol_FrameBgHovered },
    { "FrameBgActive", ImGuiCol_FrameBgActive },
    { "TitleBg", ImGuiCol_TitleBg },
    { "TitleBgActive", ImGuiCol_TitleBgActive },
    { "TitleBgCollapsed", ImGuiCol_TitleBgCollapsed },
    { "MenuBarBg", ImGuiCol_MenuBarBg },
    { "ScrollbarBg", ImGuiCol_ScrollbarBg },
    { "ScrollbarGrab", ImGuiCol_ScrollbarGrab },
    { "ScrollbarGrabHovered", ImGuiCol_ScrollbarGrabHovered },
    { "ScrollbarGrabActive", ImGuiCol_ScrollbarGrabActive },
    { "CheckMark", ImGuiCol_CheckMark },
    { "SliderGrab", ImGuiCol_SliderGrab },
    { "SliderGrabActive", ImGuiCol_SliderGrabActive },
    { "Button", ImGuiCol_Button },
    { "ButtonHovered", ImGuiCol_ButtonHovered },
    { "ButtonActive", ImGuiCol_ButtonActive },
    { "Header", ImGuiCol_Header },
    { "HeaderHovered", ImGuiCol_HeaderHovered },
    { "HeaderActive", ImGuiCol_HeaderActive },
    { "Separator", ImGuiCol_Separator },
    { "SeparatorHovered", ImGuiCol_SeparatorHovered },
    { "SeparatorActive", ImGuiCol_SeparatorActive },
    { "ResizeGrip", ImGuiCol_ResizeGrip },
    { "ResizeGripHovered", ImGuiCol_ResizeGripHovered },
    { "ResizeGripActive", ImGuiCol_ResizeGripActive },
    { "Tab", ImGuiCol_Tab },
    { "TabHovered", ImGuiCol_TabHovered },
    { "TabActive", ImGuiCol_TabActive },
    { "TabUnfocused", ImGuiCol_TabUnfocused },
    { "TabUnfocusedActive", ImGuiCol_TabUnfocusedActive },
    { "PlotLines", ImGuiCol_PlotLines },
    { "PlotLinesHovered", ImGuiCol_PlotLinesHovered },
    { "PlotHistogram", ImGuiCol_PlotHistogram },
    { "PlotHistogramHovered", ImGuiCol_PlotHistogramHovered },
    { "TableHeaderBg", ImGuiCol_TableHeaderBg },
    { "TableBorderStrong", ImGuiCol_TableBorderStrong },
    { "TableBorderLight", ImGuiCol_TableBorderLight },
    { "TableRowBg", ImGuiCol_TableRowBg },
    { "TableRowBgAlt", ImGuiCol_TableRowBgAlt },
    { "TextSelectedBg", ImGuiCol_TextSelectedBg },
    { "DragDropTarget", ImGuiCol_DragDropTarget },
    { "NavHighlight", ImGuiCol_NavHighlight },
    { "NavWindowingHighlight", ImGuiCol_NavWindowingHighlight },
    { "NavWindowingDimBg", ImGuiCol_NavWindowingDimBg },
    { "ModalWindowDimBg", ImGuiCol_ModalWindowDimBg }
};


