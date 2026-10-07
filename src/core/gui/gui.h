#pragma once
#include <gui/widgets/waterfall.h>
#include <gui/widgets/frequency_select.h>
#include <gui/widgets/menu.h>
#include <gui/dialogs/loading_screen.h>
#include <module.h>
#include <gui/main_window.h>
#include <gui/theme_manager.h>

namespace gui {
    extern ImGui::WaterFall waterfall;
    extern FrequencySelect freqSelect;
    extern Menu menu;
    extern ThemeManager themeManager;
    extern MainWindow mainWindow;

    void selectSource(std::string name);
};
