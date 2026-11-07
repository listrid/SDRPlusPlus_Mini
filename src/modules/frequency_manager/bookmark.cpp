#include "bookmark.h"
#include <radio_interface.h>
#include <algorithm>
#include <cctype>
#include <cstdint>

// Bookmarks are not all self-written: importBookmarks() feeds this an arbitrary
// user-supplied file, having checked only that a "bookmarks" object exists. So
// every field is both presence- and type-checked. Note that a bare
// `j["frequency"]` would not merely be lax -- operator[] on a *const* json with
// a missing key is undefined behaviour rather than a throw, so the try/catch
// around the import loop would not have caught it.
void bookmarkFromJson(const ConfigManager::ReadSection& sec, FrequencyBookmark& bm)
{

    bm.frequency = sec.value("frequency",  0.0);
    bm.bandwidth = sec.value("bandwidth", 0.0);
    // -1 for an unusable value, never RAW: RAW is a real demodulator and a
    // bookmark asking for it must be one the user actually chose. -1 says "no
    // usable mode was recorded", and applyBookmark() then leaves the
    // demodulator alone -- the same convention the band stack registers use.
    bm.mode = -1;
    if(sec.contains("mode"))
        bm.mode = radioModeFromName(sec.value("mode", "WFM").data());

    bm.startTime = sec.value("startTime", int64_t(0));
    bm.endTime = sec.value("endTime", int64_t(0));
    for (int i = 0; i < 7; i++)
        bm.days[i] = true;
    
    if (sec.contains("days"))
    {
        int i = 0;
        for(auto& o: sec.section("days").peek())
        {
            bool v = true;
            o.tryGet(v);
            bm.days[i++] = v;
            if(i > 7)
                break;
        }
    }
    bm.geoinfo = sec.value("geoinfo", "");
    bm.notes = sec.value("notes", "");
    bm.selected = false;
}

void bookmarkToJson(const FrequencyBookmark& bm, ConfigManager::EditSection& sec)
{
    sec["frequency"] = bm.frequency;
    sec["bandwidth"] = bm.bandwidth;
    if (radioModeValid(bm.mode))
        sec["mode"] = radioModeName(bm.mode);
    sec["startTime"] = bm.startTime;
    sec["endTime"] = bm.endTime;
    ConfigManager::EditSection& days = sec.array("days");

    for(size_t i = 0; i < 7; i++)
        days[i] = bm.days[i];
    sec["geoinfo"] = bm.geoinfo.c_str();
    sec["notes"] = bm.notes.c_str();

}

void bookmarkFromExternalJson(const ConfigManager::ReadSection& sec, ExternalBookmarkParseResult& result)
{
    result.modeStatus = ExternalBookmarkModeStatus::Unrecorded;
    bookmarkFromJson(sec, result.bookmark);
    if(!sec.contains("mode"))
        return;
    std::string out;
    if(sec.tryGet("mode", out))
    {
        result.modeStatus = radioModeValid(result.bookmark.mode) ? ExternalBookmarkModeStatus::Resolved : ExternalBookmarkModeStatus::Invalid;
        return;
    }
    int mode = -1;
    sec.tryGet("mode", mode);
    if (radioModeValid(mode))
    {
        result.bookmark.mode = mode;
        result.modeStatus = ExternalBookmarkModeStatus::Resolved;
    }else{
        result.modeStatus = ExternalBookmarkModeStatus::Invalid;
    }
}

bool compareWaterfallBookmarks(const WaterfallBookmark& wbm1, const WaterfallBookmark& wbm2)
{
    return (wbm1.bookmark.frequency < wbm2.bookmark.frequency);
}

bool compareBookmarksFreqAsc(const std::pair<std::string, FrequencyBookmark>& a, const std::pair<std::string, FrequencyBookmark>& b)
{
    return a.second.frequency < b.second.frequency;
}

bool compareBookmarksFreqDesc(const std::pair<std::string, FrequencyBookmark>& a, const std::pair<std::string, FrequencyBookmark>& b)
{
    return a.second.frequency > b.second.frequency;
}

ImU32 hexStrToColor(const std::string& col)
{
    if (col.length() == 7 && col[0] == '#' && std::all_of(col.begin() + 1, col.end(), [](unsigned char c) { return std::isxdigit(c) != 0; }))
    {
        int r = std::stoi(col.substr(1, 2), NULL, 16);
        int g = std::stoi(col.substr(3, 2), NULL, 16);
        int b = std::stoi(col.substr(5, 2), NULL, 16);
        return IM_COL32(r, g, b, 255);
    }
    return IM_COL32(255, 255, 0, 255);
}

ImVec4 color32ToVec4(ImU32 col)
{
    ImVec4 val;
    float sc = 1.0f / 255.0f;
    val.x = (float)((col >> IM_COL32_R_SHIFT) & 0xFF) * sc;
    val.y = (float)((col >> IM_COL32_G_SHIFT) & 0xFF) * sc;
    val.z = (float)((col >> IM_COL32_B_SHIFT) & 0xFF) * sc;
    val.w = (float)((col >> IM_COL32_A_SHIFT) & 0xFF) * sc;
    return val;
}
