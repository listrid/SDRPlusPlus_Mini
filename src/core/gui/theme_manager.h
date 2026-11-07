#pragma once
#include <map>
#include <mutex>
#include <imgui.h>
#include <string>
#include <string_view>
#include <vector>

enum ImGuiCol_Ex
{
    ImGuiCol_FFTHoldColor = ImGuiCol_COUNT,
    ImGuiCol_ClearColor,
    ImGuiCol_WaterfallBackground,
    ImGuiCol_COUNT_Ex
};


struct Theme
{
    std::string author;
    unsigned int data[(int)ImGuiCol_COUNT_Ex];
};

class ThemeManager
{
public:
    bool loadThemesFromDir(const std::string& path);
    bool loadTheme(const std::string& path);
    bool applyTheme(const std::string& name);

    std::vector<std::string> getThemeNames() const;

    ImVec4 m_waterfallBg  = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    ImVec4 m_fftHoldColor = ImVec4(0.0f, 1.0f, 0.75f, 1.0f);
    ImVec4 m_clearColor   = ImVec4(0.0666f, 0.0666f, 0.0666f, 1.0f);

private:
    bool loadInternal(const std::string& name);
    void internalGetNames(std::vector<std::string>& names) const;

    static std::map<std::string, int> m_IMGUI_COL_IDS;

    std::map<std::string, Theme> m_themes;
};
