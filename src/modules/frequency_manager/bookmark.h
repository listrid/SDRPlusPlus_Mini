#pragma once
#include <imgui.h>
#include <config.h>
#include <string>
#include <utility>


struct FrequencyBookmark
{
    double frequency;
    double bandwidth;
    int mode;
    bool selected;
    int startTime;
    int endTime;
    bool days[7];
    std::string notes;
    std::string geoinfo;
};

struct WaterfallBookmark
{
    std::string listName;
    std::string bookmarkName;
    ImU32 color;
    FrequencyBookmark bookmark;
    // Label rectangle cached during FFT redraw, reused for mouse hit-testing.
    // Set to an empty rect when the label is not drawn.
    ImVec2 clampedRectMin;
    ImVec2 clampedRectMax;
};

struct BookmarkRectangle
{
    double min;
    double max;
};

void bookmarkFromJson(const ConfigManager::ReadSection& sec, FrequencyBookmark& bm);
void bookmarkToJson(const FrequencyBookmark& bm, ConfigManager::EditSection& sec);

enum class ExternalBookmarkModeStatus
{
    Unrecorded,
    Resolved,
    Invalid
};

struct ExternalBookmarkParseResult
{
    FrequencyBookmark bookmark;
    ExternalBookmarkModeStatus modeStatus;
};

// External files may use this fork's canonical string or upstream SDR++'s
// numeric mode. Internal config loading deliberately uses bookmarkFromJson()
// and remains string-only.
void bookmarkFromExternalJson(const ConfigManager::ReadSection& sec, ExternalBookmarkParseResult& result);

bool compareWaterfallBookmarks(const WaterfallBookmark& wbm1, const WaterfallBookmark& wbm2);
bool compareBookmarksFreqAsc(const std::pair<std::string, FrequencyBookmark>& a, const std::pair<std::string, FrequencyBookmark>& b);
bool compareBookmarksFreqDesc(const std::pair<std::string, FrequencyBookmark>& a, const std::pair<std::string, FrequencyBookmark>& b);

ImU32 hexStrToColor(const std::string& col);
ImVec4 color32ToVec4(ImU32 col);
