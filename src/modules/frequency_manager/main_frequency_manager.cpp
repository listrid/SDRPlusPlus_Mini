#include <imgui.h>
#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <core.h>
#include <thread>
#include <radio_interface.h>
#include <signal_path/signal_path.h>
#include <vector>
#include <gui/tuner.h>
#include <gui/file_dialogs.h>
#include <utils/freq_formatting.h>
#include <gui/dialogs/dialog_box.h>
#include <gui/widgets/popup_dialog.h>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include "bookmark.h"
#include "schedule.h"

static ModuleManager::ModuleInfo_t frequency_manager_INFO
{
    /* Name:            */ "frequency_manager",
    /* Description:     */ "Frequency manager module",
    /* Author:          */ "Ryzerth;Zimm;Darau Ble;Davide Rovelli",
    /* Version:         */ 0, 4, 0,
    /* Max instances    */ 1
};

// Maximum number of waterfall label rows (bookmarkRows is an index 0..MAX_ROWS-1 meaning 1..MAX_ROWS rows)
constexpr int MAX_ROWS = 10;

static ConfigManagerImpl g_config;

enum
{
    BOOKMARK_DISP_MODE_OFF,
    BOOKMARK_DISP_MODE_TOP,
    BOOKMARK_DISP_MODE_BOTTOM,
    _BOOKMARK_DISP_MODE_COUNT
};

const char* bookmarkDisplayModesTxt = "Off\0Top\0Bottom\0";
const char* bookmarkRowsTxt = "1\0""2\0""3\0""4\0""5\0""6\0""7\0""8\0""9\0""10\0";

class FrequencyManagerModule : public ModuleManager::Instance
{
public:
    FrequencyManagerModule(std::string name)
    {
        this->m_name = name;

        std::string selList;
        {
            auto configAccess = g_config.read();
            configAccess.tryGet("selectedList", selList);
            configAccess.tryGet("bookmarkDisplayMode", m_bookmarkDisplayMode);
            if (configAccess.tryGet("bookmarkRows", m_bookmarkRows))
                m_bookmarkRows = std::clamp<int>(m_bookmarkRows, 0, MAX_ROWS - 1);
            configAccess.tryGet("bookmarkRectangle", m_bookmarkRectangle);
            configAccess.tryGet("bookmarkCentered", m_bookmarkCentered);
            configAccess.tryGet("bookmarkNoClutter", m_bookmarkNoClutter);
        }

        refreshLists();
        loadByName(selList);
        refreshWaterfallBookmarks();

        m_fftRedrawHandler.ctx = this;
        m_fftRedrawHandler.handler = fftRedraw;
        m_inputHandler.ctx = this;
        m_inputHandler.handler = fftInput;

        gui::menu.registerEntry(name, menuHandler, this, NULL);
        gui::waterfall.m_onFFTRedraw.bindHandler(&m_fftRedrawHandler);
        gui::waterfall.m_onInputProcess.bindHandler(&m_inputHandler);
    }

    ~FrequencyManagerModule()
    {
        gui::menu.removeEntry(m_name);
        gui::waterfall.m_onFFTRedraw.unbindHandler(&m_fftRedrawHandler);
        gui::waterfall.m_onInputProcess.unbindHandler(&m_inputHandler);
    }

    void postInit() {}

    void enable()
    {
        m_enabled = true;
    }

    void disable()
    {
        m_enabled = false;
    }

    bool isEnabled()
    {
        return m_enabled;
    }

private:
    static void applyBookmark(FrequencyBookmark bm, std::string vfoName)
    {
        if (vfoName == "")
        {
            // TODO: Replace with proper tune call
            gui::waterfall.m_centerFreqMoved = true;
            gui::waterfall.setCenterFrequency(bm.frequency);
        }else{
            if(core::modComManager.interfaceExists(vfoName))
            {
                if (core::modComManager.getModuleName(vfoName) == "radio")
                {
                    // Unrecorded fields are left alone rather than imposed. The
                    // bandwidth guard is not cosmetic: setBandwidth() clamps a 0
                    // to the demodulator's minimum and *persists* it into
                    // radio.json, so recalling a bookmark taken without a
                    // radio VFO used to destroy the saved bandwidth of whatever
                    // mode it had just switched to.
                    if (bm.mode >= 0)
                    {
                        int mode = bm.mode;
                        core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_MODE, &mode, NULL);
                    }
                    if (bm.bandwidth > 0)
                    {
                        float bandwidth = bm.bandwidth;
                        core::modComManager.callInterface(vfoName, RADIO_IFACE_CMD_SET_BANDWIDTH, &bandwidth, NULL);
                    }
                }
            }
            tuner::tune(tuner::TUNER_MODE_NORMAL, vfoName, bm.frequency);
        }
    }

    // Bandwidth and mode captured from the currently selected VFO, used to seed
    // a freshly created bookmark. Either can come back unrecorded: mode -1 when
    // the VFO is not a radio (or there is none), bandwidth 0 when there is no
    // VFO at all. applyBookmark() then leaves that setting alone, so a bookmark
    // only ever imposes what was actually observed. RAW is deliberately not used
    // as the unknown value -- it is a real demodulator, and a bookmark asking
    // for it must be one the user chose.
    void currentBookmarkBwMode(double& bandwidth, int& mode)
    {
        bandwidth = 0;
        mode = -1;
        if (gui::waterfall.m_selectedVFO == "")
            return;
        bandwidth = sigpath::vfoManager.getBandwidth(gui::waterfall.m_selectedVFO);
        if (core::modComManager.getModuleName(gui::waterfall.m_selectedVFO) == "radio")
        {
            // Seeded rather than merely assigned to: callInterface() invokes no
            // handler at all when the interface is not registered, and the radio
            // ignores the command while it has no demodulator selected. Reading
            // an uninitialized `m` in either case stored stack garbage as the
            // bookmark's mode.
            int m = -1;
            core::modComManager.callInterface(gui::waterfall.m_selectedVFO, RADIO_IFACE_CMD_GET_MODE, NULL, &m);
            mode = m;
        }
    }

    // Seed editedBookmark for a brand new bookmark and open the edit dialog in
    // Create mode. Shared by the "Add" button and the right-click-on-spectrum
    // gesture.
    void beginCreateBookmark(double frequency, double bandwidth, int mode)
    {
        m_editedBookmark.frequency = frequency;
        m_editedBookmark.bandwidth = bandwidth;
        m_editedBookmark.mode = mode;
        m_editedBookmark.startTime = 0;
        m_editedBookmark.endTime = 0;
        for (int i = 0; i < 7; i++)
            m_editedBookmark.days[i] = true;
        m_editedBookmark.geoinfo = "";
        m_editedBookmark.notes = "";
        m_editedBookmark.selected = false;
        m_editedBookmarkListId = m_selectedListId;
        m_firstEditedBookmarkName = "";

        m_bookmarkDialogMode = BookmarkDialogMode::Create;
        m_editDialog.request();

        // Find a unique default name
        if (m_bookmarks.find("New Bookmark") == m_bookmarks.end())
        {
            m_editedBookmarkName = "New Bookmark";
        }else{
            char buf[64];
            for (int i = 1; i < 1000; i++)
            {
                sprintf(buf, "New Bookmark (%d)", i);
                if(m_bookmarks.find(buf) == m_bookmarks.end())
                    break;
            }
            m_editedBookmarkName = buf;
        }
    }

    // Expand this module's menu section so the create dialog is actually drawn:
    // the dialog is only rendered from menuHandler, which the menu skips while
    // the section is collapsed. Used when the dialog is triggered from outside
    // the menu (right-click on the spectrum).
    void ensureMenuExpanded()
    {
        for (auto& opt : gui::menu.order)
        {
            if (opt.name == m_name)
            {
                opt.open = true;
                break;
            }
        }
        gui::mainWindow.setFirstMenuRender();
    }

    void saveBookmarkEdit(const std::string& targetListName)
    {
        if(targetListName != m_selectedListName)
        {
            // Moving to another list: remove from the current one if editing
            if (m_bookmarkDialogMode == BookmarkDialogMode::Edit)
            {
                m_bookmarks.erase(m_firstEditedBookmarkName);
                saveByName(m_selectedListName);
            }

            // Add to the target list and switch to it
            {
               bookmarkToJson(m_editedBookmark, g_config.edit().section("lists").section(targetListName, "bookmarks").section(m_editedBookmarkName));
            }
            refreshWaterfallBookmarks();

            loadByName(targetListName);
            g_config.edit()["selectedList"] = targetListName;
        }else{
            // Same list: normal add or edit
            if(m_bookmarkDialogMode == BookmarkDialogMode::Edit)
                m_bookmarks.erase(m_firstEditedBookmarkName);
            m_bookmarks[m_editedBookmarkName] = m_editedBookmark;
            saveByName(m_selectedListName);
        }
    }

    void bookmarkEditDialog()
    {
        gui::mainWindow.lockWaterfallControls = true;

        std::string id = "Edit##freq_manager_edit_popup_" + m_name;
        if (m_editDialog.begin(id.c_str(), ImGuiWindowFlags_NoResize))
        {
            char nameBuf[1024];
            snprintf(nameBuf, sizeof(nameBuf), "%s", m_editedBookmarkName.c_str());

            char geoinfoBuf[2048];
            snprintf(geoinfoBuf, sizeof(geoinfoBuf), "%s", m_editedBookmark.geoinfo.c_str());

            char notesBuf[4096];
            snprintf(notesBuf, sizeof(notesBuf), "%s", m_editedBookmark.notes.c_str());

            float editWinSize = 250.0f * style::uiScale;
            ImGui::BeginTable(("freq_manager_edit_table" + m_name).c_str(), 2);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::LeftLabel("Name");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(editWinSize);
            if (ImGui::InputText(("##freq_manager_edit_name" + m_name).c_str(), nameBuf, sizeof(nameBuf) - 1))
                m_editedBookmarkName = nameBuf;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::LeftLabel("List");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(editWinSize);
            ImGui::Combo(("##freq_manager_edit_list" + m_name).c_str(), &m_editedBookmarkListId, m_listNamesTxt.c_str());

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::LeftLabel("Frequency");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(editWinSize);
            ImGui::InputDouble(("##freq_manager_edit_freq" + m_name).c_str(), &m_editedBookmark.frequency);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::LeftLabel("Bandwidth");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(editWinSize);
            ImGui::InputDouble(("##freq_manager_edit_bw" + m_name).c_str(), &m_editedBookmark.bandwidth);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::LeftLabel("Mode");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(editWinSize);
            // BeginCombo rather than Combo: the preview is a separate parameter
            // from the item list, so "not set" can be shown as a prompt without
            // being an entry the user could choose. A bookmark reaches mode -1
            // by being taken without a radio VFO, or by carrying an unusable
            // value in the file -- never by selection, and picking a real mode
            // is one-way.
            {
                bool modeNotSet = !radioModeValid(m_editedBookmark.mode);
                const char* preview = modeNotSet ? "(not set)" : RADIO_IFACE_MODE_NAMES[m_editedBookmark.mode];
                if (modeNotSet)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                bool modeOpen = ImGui::BeginCombo(("##freq_manager_edit_mode" + m_name).c_str(), preview);
                if (modeNotSet)
                    ImGui::PopStyleColor();
                if (modeOpen)
                {
                    for (int i = 0; i < _RADIO_IFACE_MODE_COUNT; i++)
                    {
                        bool selected = (m_editedBookmark.mode == i);
                        if (ImGui::Selectable(RADIO_IFACE_MODE_NAMES[i], selected))
                            m_editedBookmark.mode = i;
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::LeftLabel("Start Time");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(editWinSize);
            ImGui::InputScalarN(("##freq_manager_edit_start_time" + m_name).c_str(), ImGuiDataType_S32, &m_editedBookmark.startTime, 1, NULL, NULL, "%04d", 0);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::LeftLabel("End Time");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(editWinSize);
            ImGui::InputScalarN(("##freq_manager_edit_end_time" + m_name).c_str(), ImGuiDataType_S32, &m_editedBookmark.endTime, 1, NULL, NULL, "%04d", 0);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::LeftLabel("Days");
            ImGui::TableSetColumnIndex(1);

            ImGui::BeginGroup();
            ImGui::Columns(4, "FreqManagerBookmarkDays", false);
            ImGui::Checkbox("Su", &m_editedBookmark.days[0]);
            ImGui::Checkbox("Th", &m_editedBookmark.days[4]);
            ImGui::NextColumn();
            ImGui::Checkbox("Mo", &m_editedBookmark.days[1]);
            ImGui::Checkbox("Fr", &m_editedBookmark.days[5]);
            ImGui::NextColumn();
            ImGui::Checkbox("Tu", &m_editedBookmark.days[2]);
            ImGui::Checkbox("Sa", &m_editedBookmark.days[6]);
            ImGui::NextColumn();
            ImGui::Checkbox("We", &m_editedBookmark.days[3]);
            ImGui::Columns(1, "FreqManagerEndBookmarkDays", false);
            ImGui::EndGroup();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::LeftLabel("Geo Info");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(editWinSize);
            if (ImGui::InputText(("##freq_manager_edit_geoinfo" + m_name).c_str(), geoinfoBuf, sizeof(geoinfoBuf) - 1))
                m_editedBookmark.geoinfo = geoinfoBuf;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::LeftLabel("Notes");
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(editWinSize);
            if (ImGui::InputTextMultiline(("##freq_manager_edit_notes" + m_name).c_str(), notesBuf, sizeof(notesBuf) - 1))
                m_editedBookmark.notes = notesBuf;
            bool notesFieldActive = ImGui::IsItemActive();

            ImGui::EndTable();

            std::string targetListName = m_listNames.empty() ? m_selectedListName : m_listNames[std::clamp<int>(m_editedBookmarkListId, 0, (int)m_listNames.size() - 1)];

            // Check for a name conflict in the target list
            bool nameExists;
            if (targetListName != m_selectedListName)
            {
                auto configAccess = g_config.read();
                nameExists = configAccess.section("lists").section(targetListName, "bookmarks").contains(m_editedBookmarkName.c_str());
            }else{
                nameExists = (m_bookmarks.find(m_editedBookmarkName) != m_bookmarks.end()) && (m_editedBookmarkName != m_firstEditedBookmarkName);
            }

            bool applyDisabled = (strlen(nameBuf) == 0) || nameExists || !timeValid(m_editedBookmark.startTime) || !timeValid(m_editedBookmark.endTime);
            if (m_editDialog.applyButton("Apply", applyDisabled, notesFieldActive))
            {
                saveBookmarkEdit(targetListName);
                m_editDialog.close();
            }
            ImGui::SameLine();
            if(m_editDialog.cancelButton())
                m_editDialog.close();
            m_editDialog.end();
        }
    }

    void newListDialog()
    {
        gui::mainWindow.lockWaterfallControls = true;

        float menuWidth = ImGui::GetContentRegionAvail().x;

        std::string id = "New##freq_manager_new_popup_" + m_name;
        if (m_listDialog.begin(id.c_str(), ImGuiWindowFlags_NoResize))
        {
            char nameBuf[1024];
            snprintf(nameBuf, sizeof(nameBuf), "%s", m_editedListName.c_str());

            ImGui::LeftLabel("Name");
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::InputText(("##freq_manager_edit_name" + m_name).c_str(), nameBuf, sizeof(nameBuf) - 1))
                m_editedListName = nameBuf;
            ImGui::LeftLabel("Color");
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            ImGui::ColorEdit3(("##freq_manager_list_color_" + m_name).c_str(), (float*)&m_editedListColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);

            bool nameChanged = (m_listDialogMode != ListDialogMode::Rename) || (m_editedListName != m_firstEditedListName);
            bool alreadyExists = nameChanged && (std::find(m_listNames.begin(), m_listNames.end(), m_editedListName) != m_listNames.end());

            if (m_listDialog.applyButton("Apply", (strlen(nameBuf) == 0) || alreadyExists))
            {
                {
                    auto configAccess = g_config.edit();
                    ConfigManager::EditSection lists = configAccess.section("lists");
                    if (m_listDialogMode == ListDialogMode::Rename)
                    {
                        if (m_editedListName != m_firstEditedListName)
                            lists.ReName(m_firstEditedListName, m_editedListName);
                    }else{
                        ConfigManager::EditSection list = lists.section(m_editedListName);
                        list["showOnWaterfall"] =  true;
                        list.section("bookmarks");
                    }
                    char buf[16];
                    snprintf(buf, sizeof(buf), "#%02X%02X%02X", (int)roundf(m_editedListColor.x * 255), (int)roundf(m_editedListColor.y * 255), (int)roundf(m_editedListColor.z * 255));
                    lists.section(m_editedListName)["color"] = buf;
                }

                refreshWaterfallBookmarks();
                refreshLists();
                loadByName(m_editedListName);
                m_listDialog.close();
            }
            ImGui::SameLine();
            if (m_listDialog.cancelButton())
                m_listDialog.close();
            m_listDialog.end();
        }
    }

    void selectListsDialog()
    {
        gui::mainWindow.lockWaterfallControls = true;

        std::string id = "Select lists##freq_manager_sel_popup_" + m_name;
        if (m_selectDialog.begin(id.c_str(), ImGuiWindowFlags_NoResize))
        {
            // Copied out so the checkboxes below can write without the lock held.
            auto lists = g_config.read().section("lists");
            for (auto& o : lists.peek())
            {
                std::string listName = o.GetName();
                bool shown = lists.section(listName).value("showOnWaterfall", true);
                if (ImGui::Checkbox((std::string(listName) + "##freq_manager_sel_list_").c_str(), &shown))
                {
                    g_config.edit().section("lists", listName)["showOnWaterfall"] = shown;
                    refreshWaterfallBookmarks();
                }
            }

            // Checkbox changes apply immediately, so Escape and Ok both just dismiss
            if(m_selectDialog.applyButton("Ok") || m_selectDialog.cancelRequested())
                m_selectDialog.close();
            m_selectDialog.end();
        }
    }

    void refreshLists()
    {
        m_listNames.clear();
        m_listNamesTxt = "";
        m_sortSpecsDirty = true;

        auto lists = g_config.read().section("lists").peek();
        for (auto& o : lists)
        {
            std::string _name = o.GetName();
            m_listNames.push_back(_name);
            m_listNamesTxt += _name;
            m_listNamesTxt += '\0';
        }
    }

    // Takes the config lock itself, so it must be called with no access open.
    void refreshWaterfallBookmarks()
    {
        auto lists = g_config.read().section("lists");
        m_waterfallBookmarks.clear();
        for (auto& o : lists.peek())
        {
            std::string listName = o.GetName();
            auto list = o.toSection();

            bool showOnWaterfall = false;
            list.tryGet("showOnWaterfall", showOnWaterfall);

            if(!showOnWaterfall)
                continue;
            WaterfallBookmark wbm;
            wbm.listName = listName;
            wbm.color = IM_COL32(255, 255, 0, 255);
            std::string str;
            if(list.tryGet("color", str))
                wbm.color = hexStrToColor(str);
            auto bookmarks = lists.section(listName, "bookmarks");
            for (auto& bm : bookmarks.peek())
            {
                std::string bookmarkName = bm.GetName();
                wbm.bookmarkName = bookmarkName;
                bookmarkFromJson(bookmarks.section(bookmarkName), wbm.bookmark);
                wbm.clampedRectMin = ImVec2(-1, -1);
                wbm.clampedRectMax = ImVec2(-1, -1);
                m_waterfallBookmarks.push_back(wbm);
            }
        }
        std::sort(m_waterfallBookmarks.begin(), m_waterfallBookmarks.end(), compareWaterfallBookmarks);
    }

    void loadFirst()
    {
        if (m_listNames.size() > 0)
        {
            loadByName(m_listNames[0]);
            return;
        }
        m_selectedListName = "";
        m_selectedListId = 0;
    }

    void loadByName(std::string listName)
    {
        m_bookmarks.clear();
        m_sortSpecsDirty = true;
        if (std::find(m_listNames.begin(), m_listNames.end(), listName) == m_listNames.end())
        {
            m_selectedListName = "";
            m_selectedListId = 0;
            loadFirst();
            return;
        }
        m_selectedListId = std::distance(m_listNames.begin(), std::find(m_listNames.begin(), m_listNames.end(), listName));
        m_selectedListName = listName;

        auto storedBookmarks = g_config.read().section("lists", listName).section("bookmarks");

        for (auto& bm : storedBookmarks.peek())
        {
            std::string bmName = bm.GetName();
            FrequencyBookmark newBm;
            bookmarkFromJson(storedBookmarks.section(bmName), newBm);
            m_bookmarks[bmName] = newBm;
        }
    }

    void saveByName(std::string listName)
    {
        auto list = g_config.edit().section("lists", listName);
        list.erase("bookmarks");
        auto bms = list.section("bookmarks");
        for (auto [bmName, bm] : m_bookmarks)
            bookmarkToJson(bm, bms.section(bmName));
        refreshWaterfallBookmarks();
        m_sortSpecsDirty = true;
    }

    static void menuHandler(void* ctx)
    {
        FrequencyManagerModule* _this = (FrequencyManagerModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;

        // TODO: Replace with something that won't iterate every frame
        std::vector<std::string> selectedNames;
        for (auto& [m_name, bm] : _this->m_bookmarks)
        {
            if (bm.selected)
                selectedNames.push_back(m_name);
        }

        ImGuiStyle& imStyle = ImGui::GetStyle();
        float sqBtnSize = ImGui::GetFrameHeight();

        float btnSize = ImGui::CalcTextSize("Rename").x + (imStyle.FramePadding.x * 2.0f);
        ImGui::SetNextItemWidth(menuWidth - btnSize - (2.0f * sqBtnSize) - (3.0f * imStyle.ItemSpacing.x));
        if (ImGui::Combo(("##freq_manager_list_sel" + _this->m_name).c_str(), &_this->m_selectedListId, _this->m_listNamesTxt.c_str()))
        {
            _this->loadByName(_this->m_listNames[_this->m_selectedListId]);
            g_config.edit()["selectedList"] = _this->m_selectedListName;
        }
        ImGui::SameLine();
        if (_this->m_listNames.size() == 0)
            style::beginDisabled();
        if (ImGui::Button(("Rename##_freq_mgr_ren_lst_" + _this->m_name).c_str(), ImVec2(btnSize, 0)))
        {
            _this->m_firstEditedListName = _this->m_listNames[_this->m_selectedListId];
            _this->m_editedListName = _this->m_firstEditedListName;
            _this->m_editedListColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
            {
                auto configAccess = g_config.read();
                std::string color;
                if (configAccess.section("lists", _this->m_firstEditedListName).tryGet("color", color))
                    _this->m_editedListColor = color32ToVec4(hexStrToColor(color));
            }
            _this->m_listDialogMode = ListDialogMode::Rename;
            _this->m_listDialog.request();
        }
        if (_this->m_listNames.size() == 0)
            style::endDisabled();
        ImGui::SameLine();
        if (ImGui::Button(("+##_freq_mgr_add_lst_" + _this->m_name).c_str(), ImVec2(sqBtnSize, 0)))
        {
            // Find new unique default name
            if (std::find(_this->m_listNames.begin(), _this->m_listNames.end(), "New List") == _this->m_listNames.end())
            {
                _this->m_editedListName = "New List";
            }else{
                char buf[64];
                for (int i = 1; i < 1000; i++)
                {
                    sprintf(buf, "New List (%d)", i);
                    if (std::find(_this->m_listNames.begin(), _this->m_listNames.end(), buf) == _this->m_listNames.end())
                        break;
                }
                _this->m_editedListName = buf;
            }
            _this->m_editedListColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
            _this->m_listDialogMode = ListDialogMode::New;
            _this->m_listDialog.request();
        }
        ImGui::SameLine();
        if (_this->m_selectedListName == "")
            style::beginDisabled();
        if (ImGui::Button(("-##_freq_mgr_del_lst_" + _this->m_name).c_str(), ImVec2(sqBtnSize, 0)))
            _this->m_deleteListOpen = true;
        if (_this->m_selectedListName == "")
            style::endDisabled();
        // List delete confirmation
        if (ImGui::GenericDialog(("freq_manager_del_list_confirm" + _this->m_name).c_str(), _this->m_deleteListOpen, GENERIC_DIALOG_BUTTONS_YES_NO, [_this]() {
                                     ImGui::Text("Deleting list named \"%s\". Are you sure?", _this->m_selectedListName.c_str());
                                   }) == GENERIC_DIALOG_BUTTON_YES)
        {
            g_config.edit().section("lists").erase(_this->m_selectedListName);
            _this->refreshWaterfallBookmarks();
            _this->refreshLists();
            if (_this->m_listNames.size() > 0)
            {
                _this->m_selectedListId = std::clamp<int>(_this->m_selectedListId, 0, (int)_this->m_listNames.size() - 1);
                _this->loadByName(_this->m_listNames[_this->m_selectedListId]);
            }else{
                _this->m_selectedListId = 0;
                _this->m_selectedListName = "";
            }
        }

        if (_this->m_selectedListName == "")
            style::beginDisabled();
        //Draw buttons on top of the list
        ImGui::BeginTable(("freq_manager_btn_table" + _this->m_name).c_str(), 3, 0, ImVec2(ImGui::BeginActionRow(), 0));
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::Button(("Add##_freq_mgr_add_" + _this->m_name).c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        {
            // If there's no VFO selected, just save the center freq
            double bw;
            int mode;
            _this->currentBookmarkBwMode(bw, mode);
            double freq = (gui::waterfall.m_selectedVFO == "")
                              ? gui::waterfall.getCenterFrequency()
                              : gui::waterfall.getCenterFrequency() + sigpath::vfoManager.getOffset(gui::waterfall.m_selectedVFO);
            _this->beginCreateBookmark(freq, bw, mode);
        }

        ImGui::TableSetColumnIndex(1);
        if (selectedNames.size() == 0 && _this->m_selectedListName != "")
            style::beginDisabled();
        if (ImGui::Button(("Remove##_freq_mgr_rem_" + _this->m_name).c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0)))
            _this->m_deleteBookmarksOpen = true;
        if (selectedNames.size() == 0 && _this->m_selectedListName != "") 
            style::endDisabled();
        ImGui::TableSetColumnIndex(2);
        if (selectedNames.size() != 1 && _this->m_selectedListName != "")
            style::beginDisabled();
        if (ImGui::Button(("Edit##_freq_mgr_edt_" + _this->m_name).c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0)))
        {
            _this->m_bookmarkDialogMode = BookmarkDialogMode::Edit;
            _this->m_editDialog.request();
            _this->m_editedBookmark = _this->m_bookmarks[selectedNames[0]];
            _this->m_editedBookmarkName = selectedNames[0];
            _this->m_firstEditedBookmarkName = selectedNames[0];
            _this->m_editedBookmarkListId = _this->m_selectedListId;
        }
        if (selectedNames.size() != 1 && _this->m_selectedListName != "")
            style::endDisabled();
        ImGui::EndTable();

        // Bookmark delete confirm dialog
        if (ImGui::GenericDialog(("freq_manager_del_bkm_confirm" + _this->m_name).c_str(), _this->m_deleteBookmarksOpen, GENERIC_DIALOG_BUTTONS_YES_NO, [_this]() {
            ImGui::TextUnformatted("Deleting selected bookmarks. Are you sure?");
            }) == GENERIC_DIALOG_BUTTON_YES)
        {
            for (auto& _name : selectedNames)
                _this->m_bookmarks.erase(_name);
            _this->saveByName(_this->m_selectedListName);
        }

        // Bookmark list
        if (ImGui::BeginTable(("freq_manager_bkm_table" + _this->m_name).c_str(), 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable, ImVec2(0, 200.0f * style::uiScale)))
        {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f, 0);
            ImGui::TableSetupColumn("Bookmark", ImGuiTableColumnFlags_DefaultSort, 0.0f, 1);
            ImGui::TableSetupScrollFreeze(2, 1);
            ImGui::TableHeadersRow();

            // Re-sort when the user clicks a column header or the bookmark list changed
            ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
            if (sortSpecs && (sortSpecs->SpecsDirty || _this->m_sortSpecsDirty))
            {
                _this->m_sortedBookmarks.assign(_this->m_bookmarks.begin(), _this->m_bookmarks.end());
                _this->m_scrollToClickedBookmark = true;
                if (sortSpecs->SpecsCount > 0)
                {
                    ImGuiTableColumnSortSpecs spec = sortSpecs->Specs[0];
                    if (spec.ColumnUserID == 0)
                    {
                        // Sort by name (map iteration is already name-ascending)
                        if (spec.SortDirection == ImGuiSortDirection_Descending)
                            std::reverse(_this->m_sortedBookmarks.begin(), _this->m_sortedBookmarks.end());
                    }else{
                        // Sort by frequency
                        std::sort(_this->m_sortedBookmarks.begin(), _this->m_sortedBookmarks.end(), (spec.SortDirection == ImGuiSortDirection_Descending) ? compareBookmarksFreqDesc : compareBookmarksFreqAsc);
                    }
                }
                sortSpecs->SpecsDirty = false;
                _this->m_sortSpecsDirty = false;
            }

            for (auto& [m_name, bm] : _this->m_sortedBookmarks)
            {
                auto it = _this->m_bookmarks.find(m_name);
                if (it == _this->m_bookmarks.end())
                    continue;
                FrequencyBookmark& cbm = it->second;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                if (ImGui::Selectable((m_name + "##_freq_mgr_bkm_name_" + _this->m_name).c_str(), &cbm.selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_SelectOnClick))
                {
                    // if shift or control isn't pressed, deselect all others
                    if (!ImGui::GetIO().KeyShift && !ImGui::GetIO().KeyCtrl)
                    {
                        for (auto& [_name, _bm] : _this->m_bookmarks)
                        {
                            if(m_name == _name)
                                continue;
                            _bm.selected = false;
                        }
                    }
                }
                if (ImGui::TableGetHoveredColumn() >= 0 && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    applyBookmark(cbm, gui::waterfall.m_selectedVFO);
                    cbm.selected = true;
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s %s", utils::formatFreq(cbm.frequency).c_str(), radioModeName(cbm.mode));

                if (_this->m_scrollToClickedBookmark && cbm.selected)
                {
                    ImGui::SetScrollHereY(0.5f);
                    _this->m_scrollToClickedBookmark = false;
                }
            }
            ImGui::EndTable();
        }


        if (selectedNames.size() != 1 && _this->m_selectedListName != "")
            style::beginDisabled();
        if (ImGui::ActionButton(("Apply##_freq_mgr_apply_" + _this->m_name).c_str()))
        {
            FrequencyBookmark& bm = _this->m_bookmarks[selectedNames[0]];
            applyBookmark(bm, gui::waterfall.m_selectedVFO);
            bm.selected = false;
        }
        if (selectedNames.size() != 1 && _this->m_selectedListName != "")
            style::endDisabled();
        //Draw import and export buttons
        ImGui::BeginTable(("freq_manager_bottom_btn_table" + _this->m_name).c_str(), 2, 0, ImVec2(ImGui::BeginActionRow(), 0));
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::Button(("Import##_freq_mgr_imp_" + _this->m_name).c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0)) && !_this->m_importOpen)
        {
            _this->m_importOpen = true;
            _this->m_importDialog = new pfd::open_file("Import bookmarks", "", { "JSON Files (*.json)", "*.json", "All Files", "*" }, pfd::opt::multiselect);
        }

        ImGui::TableSetColumnIndex(1);
        if (selectedNames.size() == 0 && _this->m_selectedListName != "") { style::beginDisabled(); }
        if (ImGui::Button(("Export##_freq_mgr_exp_" + _this->m_name).c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 0)) && !_this->m_exportOpen)
        {
            _this->m_exportedBookmarks.Clear();
            ConfigManager::ReadSection bms = g_config.read().section("lists").section(_this->m_selectedListName, "bookmarks");
            for (auto& _name : selectedNames)
            {
                auto dst = _this->m_exportedBookmarks.edit().section("bookmarks", _name);
                auto src = bms.section(_name);
                {
                    double frequency = 0.0, bandwidth = 0.0;
                    std::string mode, geoinfo, notes;
                    src.tryGet("frequency", frequency);
                    src.tryGet("bandwidth", bandwidth);
                    src.tryGet("frequency", frequency);
                    src.tryGet("mode", mode);
                    src.tryGet("geoinfo", geoinfo);
                    src.tryGet("notes", notes);
                    dst["frequency"] = frequency;
                    dst["bandwidth"] = bandwidth;
                    dst["mode"] = mode;
                    dst["geoinfo"] = geoinfo;
                    dst["notes"] = notes;
                    int64_t startTime = 0, endTime = 0;
                    src.tryGet("startTime", startTime);
                    src.tryGet("endTime", endTime);
                    dst["startTime"] = startTime;
                    dst["endTime"] = endTime;
                    auto dst_days = dst.array("days");
                    auto src_days = src.section("days");
                    for(size_t i = 0; i < 7; i++)
                    {
                        bool b = true;
                        src_days[i].tryGet(b);
                        dst_days[i] = b;
                    }
                }
            }
            _this->m_exportOpen = true;
            _this->m_exportDialog = new pfd::save_file("Export bookmarks", "", { "JSON Files (*.json)", "*.json", "All Files", "*" });
        }
        if (selectedNames.size() == 0 && _this->m_selectedListName != "")
            style::endDisabled();
        ImGui::EndTable();

        if (ImGui::ActionButton(("Select displayed lists##_freq_mgr_exp_" + _this->m_name).c_str()))
            _this->m_selectDialog.request();

        ImGui::LeftLabel("Bookmark display mode");
        ImGui::FillWidth();
        if (ImGui::Combo(("##_freq_mgr_dms_" + _this->m_name).c_str(), &_this->m_bookmarkDisplayMode, bookmarkDisplayModesTxt))
            g_config.edit()["bookmarkDisplayMode"] = _this->m_bookmarkDisplayMode;

        ImGui::LeftLabel("Rows of bookmarks");
        ImGui::FillWidth();
        if (ImGui::Combo(("##_freq_mgr_rob_" + _this->m_name).c_str(), &_this->m_bookmarkRows, bookmarkRowsTxt))
            g_config.edit()["bookmarkRows"] = _this->m_bookmarkRows;

        if (ImGui::Checkbox(("Rectangles##_freq_mgr_rect_" + _this->m_name).c_str(), &_this->m_bookmarkRectangle))
            g_config.edit()["bookmarkRectangle"] = _this->m_bookmarkRectangle;

        ImGui::SameLine();
        if (ImGui::Checkbox(("Centered##_freq_mgr_cen_" + _this->m_name).c_str(), &_this->m_bookmarkCentered))
            g_config.edit()["bookmarkCentered"] = _this->m_bookmarkCentered;

        if (ImGui::Checkbox(("Avoid clutter on last row##_freq_mgr_noClut_" + _this->m_name).c_str(), &_this->m_bookmarkNoClutter))
            g_config.edit()["bookmarkNoClutter"] = _this->m_bookmarkNoClutter;

        if (_this->m_selectedListName == "")
            style::endDisabled();

        if (_this->m_bookmarkDialogMode != BookmarkDialogMode::None)
        {
            _this->bookmarkEditDialog();
            if (!_this->m_editDialog.isOpen())
                _this->m_bookmarkDialogMode = BookmarkDialogMode::None;
        }

        if (_this->m_listDialogMode != ListDialogMode::None)
        {
            _this->newListDialog();
            if (!_this->m_listDialog.isOpen())
                _this->m_listDialogMode = ListDialogMode::None;
        }

        if (_this->m_selectDialog.isOpen())
            _this->selectListsDialog();

        // Handle import and export
        if (_this->m_importOpen && _this->m_importDialog->ready())
        {
            _this->m_importOpen = false;
            std::vector<std::string> paths = _this->m_importDialog->result();
            if (paths.size() > 0 && _this->m_listNames.size() > 0)
                _this->importBookmarks(paths[0]);
            delete _this->m_importDialog;
        }
        if (_this->m_exportOpen && _this->m_exportDialog->ready())
        {
            _this->m_exportOpen = false;
            std::string path = _this->m_exportDialog->result();
            if (path != "")
                _this->exportBookmarks(path);
            delete _this->m_exportDialog;
        }
    }

    static void fftRedraw(ImGui::WaterFall::FFTRedrawArgs args, void* ctx)
    {
        FrequencyManagerModule* _this = (FrequencyManagerModule*)ctx;
        if (_this->m_bookmarkDisplayMode == BOOKMARK_DISP_MODE_OFF)
            return;

        // Rectangles already placed on each label row, for overlap avoidance
        std::vector<BookmarkRectangle> rowRects[MAX_ROWS];
        int lastRow = std::clamp<int>(_this->m_bookmarkRows, 0, MAX_ROWS - 1);

        int now = getUTCTime();
        int weekDay = getWeekDay();
        bool top = (_this->m_bookmarkDisplayMode == BOOKMARK_DISP_MODE_TOP);

        auto overlapsRow = [](const std::vector<BookmarkRectangle>& row, double minX, double maxX)
        {
            for (const auto& r : row)
            {
                if (minX <= r.max && maxX >= r.min)
                    return true;
            }
            return false;
        };

        for (auto& bm : _this->m_waterfallBookmarks)
        {
            // Invalidate the cached hit-test rectangle; it's set again if the label is drawn
            bm.clampedRectMin = ImVec2(-1, -1);
            bm.clampedRectMax = ImVec2(-1, -1);

            if (bm.bookmark.frequency < args.lowFreq || bm.bookmark.frequency > args.highFreq)
                continue;

            double centerXpos = args.min.x + std::round((bm.bookmark.frequency - args.lowFreq) * args.freqToPixelRatio);
            ImVec2 nameSize = ImGui::CalcTextSize(bm.bookmarkName.c_str());

            double bmMinX, bmMaxX;
            if (_this->m_bookmarkCentered)
            {
                bmMinX = centerXpos - (nameSize.x / 2) - 5;
                bmMaxX = centerXpos + (nameSize.x / 2) + 5;
            }else{
                bmMinX = centerXpos - 5;
                bmMaxX = centerXpos + nameSize.x + 5;
            }

            // Find the first row where the label doesn't overlap an already placed one
            int row = -1;
            for(int i = 0; i <= lastRow; i++)
            {
                if(!overlapsRow(rowRects[i], bmMinX, bmMaxX))
                {
                    row = i;
                    break;
                }
            }
            if (row < 0)
            {
                // All rows collide: either skip the label or pile it up on the last row
                if (_this->m_bookmarkNoClutter)
                    continue;
                row = lastRow;
            }

            // Skip labels that would end up outside of the FFT area
            float rowTop = top ? (args.min.y + (nameSize.y * row)) : (args.max.y - (nameSize.y * (row + 1)));
            float rowBottom = rowTop + nameSize.y;
            if(top && rowBottom >= args.max.y)
                continue;
            if(!top && rowTop <= args.min.y)
                continue;

            bm.clampedRectMin = ImVec2(std::clamp<double>(bmMinX, args.min.x, args.max.x), rowTop);
            bm.clampedRectMax = ImVec2(std::clamp<double>(bmMaxX, args.min.x, args.max.x), rowBottom);

            rowRects[row].push_back(BookmarkRectangle{ bmMinX, bmMaxX });

            ImU32 bookmarkColor = bm.color;
            if(!bookmarkOnline(bm.bookmark, now, weekDay))
                bookmarkColor = IM_COL32(128, 128, 128, 255);
            ImU32 bookmarkTextColor = IM_COL32(0, 0, 0, 255);
            if(_this->m_bookmarkRectangle)
            {
                args.window->DrawList->AddRectFilled(bm.clampedRectMin, bm.clampedRectMax, bookmarkColor);
            }else{
                bookmarkTextColor = bookmarkColor;
            }

            if(top)
            {
                args.window->DrawList->AddLine(ImVec2(centerXpos, rowBottom), ImVec2(centerXpos, args.max.y), bookmarkColor);
            }else{
                args.window->DrawList->AddLine(ImVec2(centerXpos, args.min.y), ImVec2(centerXpos, rowTop), bookmarkColor);
            }

            float textX = _this->m_bookmarkCentered ? (centerXpos - (nameSize.x / 2)) : (bmMinX + 6);
            if(textX >= args.min.x && (textX + nameSize.x) <= args.max.x)
                args.window->DrawList->AddText(ImVec2(textX, rowTop), bookmarkTextColor, bm.bookmarkName.c_str());
        }
    }

    bool m_mouseAlreadyDown = false;
    bool m_mouseClickedInLabel = false;
    static void fftInput(ImGui::WaterFall::InputHandlerArgs args, void* ctx)
    {
        FrequencyManagerModule* _this = (FrequencyManagerModule*)ctx;

        // Right-click on the spectrum or waterfall: create a bookmark at the
        // frequency under the cursor. Independent of the bookmark display mode,
        // but needs a list to save into (matches the disabled "Add" button).
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Right) && _this->m_selectedListName != "")
        {
            ImVec2 mp = ImGui::GetMousePos();
            bool inFFT = mp.x >= args.fftRectMin.x && mp.x < args.fftRectMax.x && mp.y >= args.fftRectMin.y && mp.y < args.fftRectMax.y;
            bool inWaterfall = mp.x >= args.waterfallRectMin.x && mp.x < args.waterfallRectMax.x && mp.y >= args.waterfallRectMin.y && mp.y < args.waterfallRectMax.y;
            if (inFFT || inWaterfall)
            {
                double freq = args.lowFreq + ((double)mp.x - args.fftRectMin.x) * args.pixelToFreqRatio;

                // Snap to the selected VFO's grid when there is one
                if (gui::waterfall.m_selectedVFO != "")
                {
                    double snap = gui::waterfall.m_map_vfos[gui::waterfall.m_selectedVFO]->m_snapInterval;
                    if (snap > 0)
                        freq = std::round(freq / snap) * snap;
                }

                double bw;
                int mode;
                _this->currentBookmarkBwMode(bw, mode);
                _this->beginCreateBookmark(freq, bw, mode);
                _this->ensureMenuExpanded();

                gui::waterfall.m_inputHandled = true;
                return;
            }
        }

        if (_this->m_bookmarkDisplayMode == BOOKMARK_DISP_MODE_OFF)
            return;

        if (_this->m_mouseClickedInLabel)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                _this->m_mouseClickedInLabel = false;
            gui::waterfall.m_inputHandled = true;
            return;
        }

        // Check the label rectangles cached during the FFT redraw for a hovered bookmark
        bool inALabel = false;
        WaterfallBookmark hoveredBookmark;
        std::string hoveredBookmarkName;

        for (auto& bm : _this->m_waterfallBookmarks)
        {
            if (bm.clampedRectMax.x - bm.clampedRectMin.x <= 0)
                continue;
            if (ImGui::IsMouseHoveringRect(bm.clampedRectMin, bm.clampedRectMax))
            {
                inALabel = true;
                hoveredBookmark = bm;
                hoveredBookmarkName = bm.bookmarkName;
            }
        }

        // Check if mouse was already down
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !inALabel)
            _this->m_mouseAlreadyDown = true;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            _this->m_mouseAlreadyDown = false;
            _this->m_mouseClickedInLabel = false;
        }

        // If yes, cancel
        if (_this->m_mouseAlreadyDown || !inALabel)
            return;

        gui::waterfall.m_inputHandled = true;

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            _this->m_mouseClickedInLabel = true;
            applyBookmark(hoveredBookmark.bookmark, gui::waterfall.m_selectedVFO);

            // If the clicked bookmark belongs to another list, switch to it
            if (hoveredBookmark.listName != _this->m_selectedListName)
            {
                _this->loadByName(hoveredBookmark.listName);
                g_config.edit()["selectedList"] = _this->m_selectedListName;
            }

            // Select the clicked bookmark in the manager list
            for (auto& [m_name, bm] : _this->m_bookmarks)
                bm.selected = (m_name == hoveredBookmarkName);
            _this->m_scrollToClickedBookmark = true;
        }

        ImGui::BeginTooltip();
        ImGui::TextUnformatted(hoveredBookmarkName.c_str());
        ImGui::Separator();
        ImGui::Text("List: %s", hoveredBookmark.listName.c_str());
        ImGui::Text("Frequency: %s", utils::formatFreq(hoveredBookmark.bookmark.frequency).c_str());
        ImGui::Text("Bandwidth: %s", utils::formatFreq(hoveredBookmark.bookmark.bandwidth).c_str());
        ImGui::Text("Mode: %s", radioModeName(hoveredBookmark.bookmark.mode));
        if (hoveredBookmark.bookmark.startTime != 0 || hoveredBookmark.bookmark.endTime != 0)
            ImGui::Text("Time: %04d - %04d UTC", hoveredBookmark.bookmark.startTime, hoveredBookmark.bookmark.endTime);
        bool allDays = true;
        for (int i = 0; i < 7; i++)
            allDays &= hoveredBookmark.bookmark.days[i];
        if (!allDays) {
            const char dayLetters[7] = { 'S', 'M', 'T', 'W', 'T', 'F', 'S' };
            char bookmarkDays[8];
            bookmarkDays[7] = 0;
            for (int i = 0; i < 7; i++)
                bookmarkDays[i] = hoveredBookmark.bookmark.days[i] ? dayLetters[i] : '-';
            ImGui::Text("Days: %s", bookmarkDays);
        }
        if (!hoveredBookmark.bookmark.geoinfo.empty())
            ImGui::Text("Geo info: %s", hoveredBookmark.bookmark.geoinfo.c_str());
        if (!hoveredBookmark.bookmark.notes.empty())
            ImGui::Text("Notes: %s", hoveredBookmark.bookmark.notes.c_str());
        ImGui::EndTooltip();
    }

    ConfigManagerImpl m_exportedBookmarks;

    bool m_importOpen = false;
    bool m_exportOpen = false;
    pfd::open_file* m_importDialog;
    pfd::save_file* m_exportDialog;

    void importBookmarks(std::string path)
    {
        ConfigManagerImpl fileBookmarks;
        fileBookmarks.setFile(path.c_str(), true);
        if(!fileBookmarks.load())
        {
            flog::error("Could not parse bookmark file: {0}", path);
            return;
        }
        auto importedBookmarks = fileBookmarks.read();
        if(!importedBookmarks.contains("bookmarks"))
        {
            flog::error("File does not contain any bookmarks");
            return;
        }

        if (!importedBookmarks["bookmarks"].isSection())
        {
            flog::error("Bookmark attribute is invalid");
            return;
        }

        // Load every bookmark
        int importedCount = 0;
        int invalidModeCount = 0;
        auto bookmarks_sec = importedBookmarks.section("bookmarks");
        for (auto const bm : bookmarks_sec.peek())
        {
            std::string _name = bm.GetName();
            if (m_bookmarks.find(_name) != m_bookmarks.end())
            {
                flog::warn("Bookmark with the name '{0}' already exists in list, skipping", _name);
                continue;
            }
            ExternalBookmarkParseResult parsed;
            bookmarkFromExternalJson(bookmarks_sec.section(_name), parsed);
            m_bookmarks[_name] = parsed.bookmark;
            if(parsed.modeStatus == ExternalBookmarkModeStatus::Invalid)
            {
                invalidModeCount++;
                flog::warn("Bookmark '{0}' has an invalid or unsupported mode; importing it without a mode", _name);
            }
            importedCount++;
        }
        saveByName(m_selectedListName);

        flog::info("Imported {0} bookmarks", importedCount);
        if (invalidModeCount)
            flog::warn("{0} imported bookmarks had no usable mode", invalidModeCount);
    }

    void exportBookmarks(std::string& path)
    {
        m_exportedBookmarks.setFile(path.c_str(), true);
        m_exportedBookmarks.save();
    }

    std::string m_name;
    bool m_enabled = true;
    enum class BookmarkDialogMode { None, Create, Edit };
    BookmarkDialogMode m_bookmarkDialogMode = BookmarkDialogMode::None;
    PopupDialog m_editDialog;
    enum class ListDialogMode { None, New, Rename };
    ListDialogMode m_listDialogMode = ListDialogMode::None;
    PopupDialog m_listDialog;
    PopupDialog m_selectDialog;

    bool m_deleteListOpen = false;
    bool m_deleteBookmarksOpen = false;

    EventHandler<ImGui::WaterFall::FFTRedrawArgs> m_fftRedrawHandler;
    EventHandler<ImGui::WaterFall::InputHandlerArgs> m_inputHandler;

    std::map<std::string, FrequencyBookmark> m_bookmarks;
    std::vector<std::pair<std::string, FrequencyBookmark>> m_sortedBookmarks;
    bool m_sortSpecsDirty = true;
    bool m_scrollToClickedBookmark = false;

    std::string m_editedBookmarkName = "";
    std::string m_firstEditedBookmarkName = "";
    FrequencyBookmark m_editedBookmark;
    int m_editedBookmarkListId = 0;

    std::vector<std::string> m_listNames;
    std::string m_listNamesTxt = "";
    std::string m_selectedListName = "";
    int m_selectedListId = 0;

    std::string m_editedListName;
    std::string m_firstEditedListName;
    ImVec4 m_editedListColor;

    std::vector<WaterfallBookmark> m_waterfallBookmarks;

    int m_bookmarkDisplayMode = 0;
    int m_bookmarkRows = 0;
    bool m_bookmarkRectangle = true;
    bool m_bookmarkCentered = true;
    bool m_bookmarkNoClutter = false;
};

static void frequency_manager_INIT()
{
    g_config.setFile("frequency_manager.json");
    if(!g_config.load())
    {//def
        auto def = g_config.edit();
        def["selectedList"] = "General";
        def["bookmarkDisplayMode"] = int(BOOKMARK_DISP_MODE_TOP);
        def["bookmarkRows"] = 4;
        def["bookmarkRectangle"] = true;
        def["bookmarkCentered"] = true;
        def["bookmarkNoClutter"] = false;
        auto sec = def.section("lists", "General");
        sec["showOnWaterfall"] = true;
        sec.section("bookmarks");

    }
    g_config.enableAutoSave();

    // Fill in missing options and convert lists of the old type
    {
        auto configAccess = g_config.edit();
        configAccess.ensure("bookmarkDisplayMode", (unsigned int)BOOKMARK_DISP_MODE_TOP);
        configAccess.ensure("bookmarkRows", 4);
        configAccess.ensure("bookmarkRectangle", true);
        configAccess.ensure("bookmarkCentered", true);
        configAccess.ensure("bookmarkNoClutter", false);
    }
}

static ModuleManager::Instance* frequency_manager_CREATE_INSTANCE(std::string name)
{
    return new FrequencyManagerModule(name);
}

static void frequency_manager_DELETE_INSTANCE(ModuleManager::Instance* instance)
{
    delete (FrequencyManagerModule*)instance;
}

static void frequency_manager_END()
{
    g_config.shutdown();
}

ModuleManager::Module_t frequency_manager_module
{
    NULL, &frequency_manager_INFO, frequency_manager_INIT, frequency_manager_CREATE_INSTANCE, frequency_manager_DELETE_INSTANCE, frequency_manager_END
};
