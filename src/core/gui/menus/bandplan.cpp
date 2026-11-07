#include <gui/menus/bandplan.h>
#include <gui/widgets/bandplan.h>
#include <gui/gui.h>
#include <core.h>
#include <gui/style.h>
#include <algorithm>
#include <iterator>

namespace bandplanmenu {
    int bandplanId;
    bool bandPlanEnabled;
    int bandPlanPos = 0;

    const char* bandPlanPosTxt = "Bottom\0Top\0";

    namespace
    {
        bandplan::BandPlan_t* getPlan(int index)
        {
            if (index < 0 || index >= static_cast<int>(bandplan::g_bandplanNames.size()))
                return nullptr;
            const auto plan = bandplan::g_bandplans.find(bandplan::g_bandplanNames[index]);
            return plan == bandplan::g_bandplans.end() ? nullptr : &plan->second;
        }
    }

    // The band plan is a core menu, not a module instance, but this adapter
    // lets the menu widget draw the standard enable checkbox on its header.
    // It is not registered with the module manager, so it persists through
    // the existing "bandPlanEnabled" config key instead of "moduleInstances".
    class BandPlanToggle : public ModuleManager::Instance {
        void postInit() {}
        void enable() { setEnabled(true); }
        void disable() { setEnabled(false); }
        bool isEnabled() { return bandPlanEnabled; }

        void setEnabled(bool en) {
            bandPlanEnabled = en;
            bandPlanEnabled ? gui::waterfall.showBandplan() : gui::waterfall.hideBandplan();
            core::configManager.edit()["bandPlanEnabled"] = bandPlanEnabled;
        }
    };
    BandPlanToggle toggle;

    ModuleManager::Instance* getInstance() {
        return &toggle;
    }

    void init() {
        if (bandplan::g_bandplanNames.empty()) {
            gui::waterfall.hideBandplan();
            return;
        }

        std::string name;
        {
            auto configAccess = core::configManager.read();
            configAccess.tryGet("bandPlan", name);
            configAccess.tryGet("bandPlanEnabled", bandPlanEnabled);
            configAccess.tryGet("bandPlanPos", bandPlanPos);
        }

        const auto selectedName = std::find(
            bandplan::g_bandplanNames.begin(), bandplan::g_bandplanNames.end(), name);
        const auto selectedPlan = bandplan::g_bandplans.find(name);
        if (selectedName != bandplan::g_bandplanNames.end() &&
            selectedPlan != bandplan::g_bandplans.end())
        {
            bandplanId = static_cast<int>(std::distance(
                bandplan::g_bandplanNames.begin(), selectedName));
            gui::waterfall.m_bandplan = &selectedPlan->second;
        }
        else {
            bandplanId = 0;
            gui::waterfall.m_bandplan = getPlan(bandplanId);
            if (!gui::waterfall.m_bandplan) {
                gui::waterfall.hideBandplan();
                return;
            }
        }

        bandPlanEnabled ? gui::waterfall.showBandplan() : gui::waterfall.hideBandplan();
        gui::waterfall.setBandPlanPos(bandPlanPos);
    }

    void draw(void* ctx) {
        if (bandplan::g_bandplanNames.empty()) {
            ImGui::TextDisabled("No band plans available");
            return;
        }
        bandplanId = std::clamp(
            bandplanId, 0, static_cast<int>(bandplan::g_bandplanNames.size()) - 1);

        float menuColumnWidth = ImGui::GetContentRegionAvail().x;
        ImGui::PushItemWidth(menuColumnWidth);
        if (ImGui::Combo("##_bandplan_name_", &bandplanId, bandplan::g_bandplanNameTxt.c_str())) {
            if (bandplan::BandPlan_t* plan = getPlan(bandplanId)) {
                gui::waterfall.m_bandplan = plan;
                core::configManager.edit()["bandPlan"] = bandplan::g_bandplanNames[bandplanId];
            }
        }
        ImGui::PopItemWidth();

        ImGui::LeftLabel("Position");
        ImGui::SetNextItemWidth(menuColumnWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("##_bandplan_pos_", &bandPlanPos, bandPlanPosTxt)) {
            gui::waterfall.setBandPlanPos(bandPlanPos);
            core::configManager.edit()["bandPlanPos"] = bandPlanPos;
        }

        const bandplan::BandPlan_t* plan = getPlan(bandplanId);
        if (!plan) {
            ImGui::TextDisabled("Selected band plan is unavailable");
            return;
        }
        ImGui::Text("Country: %s (%s)", plan->countryName.c_str(), plan->countryCode.c_str());
        ImGui::Text("Author: %s", plan->authorName.c_str());
        if (!plan->authorURL.empty() && plan->authorURL != "none") {
            ImGui::TextWrapped("Author URL: %s", plan->authorURL.c_str());
        }
    }
};
