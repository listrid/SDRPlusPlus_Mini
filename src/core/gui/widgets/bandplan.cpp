#include <gui/widgets/bandplan.h>
#include <fstream>
#include <utils/flog.h>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <radio_interface.h>
#include <utils/hex_color.h>

#include "../res/bandplan_internal.h"

namespace bandplan
{
    namespace
    {
        uint64_t nextBandPlanRevision = 1;
    }

    std::map<std::string, BandPlan_t> g_bandplans;
    std::vector<std::string> g_bandplanNames;
    std::string g_bandplanNameTxt;
    std::map<std::string, BandPlanColor_t> g_colorTable;

    bool Band_t::hasValidFrequencySpan() const noexcept
    {
        assert(std::isfinite(start) && std::isfinite(end));
        return start > 0.0 && start <= end;
    }

    bool Band_t::containsFrequency(double frequency) const noexcept
    {
        assert(std::isfinite(frequency));
        return hasValidFrequencySpan() && frequency >= start && frequency <= end;
    }

    const Band_t* BandPlan_t::findMappedSegmentAtFrequency(const freq_input::BandMapping& mapping, double frequency) const
    {
        for (const Band_t& candidate : bands)
        {
            if (candidate.resolved.mapping == &mapping && candidate.resolved.isBandOrSegment() && candidate.containsFrequency(frequency))
                return &candidate;
        }
        return nullptr;
    }

    void generateTxt()
    {
        g_bandplanNameTxt.clear();
        for (const std::string& name : g_bandplanNames)
        {
            g_bandplanNameTxt += name;
            g_bandplanNameTxt += '\0';
        }
    }

    namespace
    {
        bool loadBandPlanFile(const std::string& path, bool updateNameText)
        {
            ConfigManagerImpl json;
            json.setFile(path.c_str(), true);
            if(!json.load())
            {
                flog::error("band plan file incorrect: {0}", path);
                return false;
            }
            auto data = json.read();

            BandPlan_t plan;
            plan.revision = nextBandPlanRevision++;

            if(!data.tryGet("name", plan.name))
            {
                flog::error("band plan {0} is missing the name parameter", path);
               return false;
            }

            data.tryGet("country_name", plan.countryName);
            data.tryGet("country_code", plan.countryCode);
            data.tryGet("author_name", plan.authorName);
            data.tryGet("author_url", plan.authorURL);

            const auto sourceBands = data.section("bands");
            size_t bandsSize = sourceBands.Size();

            
            for (std::size_t bandIndex = 0; bandIndex < bandsSize;  bandIndex++)
            {
                const auto band_data = sourceBands[bandIndex].toSection();
                Band_t band;

                band_data.tryGet("name", band.name);
                band_data.tryGet("type", band.type);

                band_data.tryGet("start", band.start);
                band_data.tryGet("end", band.end);

                band_data.tryGet("def_freq", band.defFreq);
                band_data.tryGet("chan", band.chan);

                band_data.tryGet("def_mode", band.defMode);

                if (!band.defMode.empty() && !radioModeValid(radioModeFromName(band.defMode.c_str())))
                {
                    flog::warn("Band '{}' in plan '{}' has unknown def_mode '{}'; the runtime convention will be used", band.name, plan.name, band.defMode);
                }
                const freq_input::LegacyBandClassification classification = freq_input::classifyLegacyBand(band.type, band.name, band.start, band.end);

                band.resolved.legacy = classification;

                const std::string suppliedBandId(band_data.value("band_id", ""));
                if (!suppliedBandId.empty())
                {
                    band.resolved.mapping = freq_input::findBandMappingById(suppliedBandId);
                    if (!band.resolved.mapping)
                        flog::warn("Band '{}' in plan '{}' has unknown band_id '{}'; stable band features are disabled for this row", band.name, plan.name, suppliedBandId);
                    continue;
                }

                band.resolved.mapping = freq_input::findLegacyBandMapping( classification, band.start, band.end);

                plan.bands.push_back(std::move(band));
            }
            if(g_bandplans.find(plan.name) != g_bandplans.end())
            {
                g_bandplans[plan.name] = std::move(plan);
            }else{
                const std::string planName = plan.name;
                g_bandplans.emplace(planName, std::move(plan));
                g_bandplanNames.push_back(planName);
            }
            if (updateNameText)
                generateTxt();
            return true;
        }
    }




    void loadInternalBand(const BandPlan_internal& plan_internal)
    {
        BandPlan_t plan;

        plan.name = plan_internal.name;
        plan.countryName = plan_internal.countryName;
        plan.countryCode =  plan_internal.countryCode;
        size_t plan_band_size = 0;
        for(; plan_internal.bands[plan_band_size].name != NULL; plan_band_size++) {}

        plan.bands.reserve(plan_band_size);

        for(std::size_t bandIndex = 0; bandIndex < plan_band_size; bandIndex++)
        {
            const Band_internal& band_internal = plan_internal.bands[bandIndex];
            Band_t band;
            band.name = band_internal.name;
            band.type = band_internal.type;
            band.start = (double)band_internal.start;
            band.end = (double)band_internal.end;
            const freq_input::LegacyBandClassification classification = freq_input::classifyLegacyBand(band.type, band.name, band.start, band.end);

            band.resolved.legacy = classification;
            band.resolved.mapping = freq_input::findLegacyBandMapping( classification, band.start, band.end);

            plan.bands.push_back(band);
        }
        if(g_bandplans.find(plan.name) != g_bandplans.end())
            return;
        const std::string planName = plan.name;
        g_bandplans.emplace(planName, std::move(plan));
        g_bandplanNames.push_back(planName);
    }

    void loadInternal()
    {
        g_bandplans.clear();
        g_bandplanNames.clear();
        g_bandplanNameTxt.clear();

        for(size_t i = 0; gBandPlan[i].name != NULL; i++)
            loadInternalBand(gBandPlan[i]);
        generateTxt();
    }

    void loadBandPlan(const std::string& path)
    {
        loadBandPlanFile(path, true);
    }

    void loadFromDir(const std::string& path)
    {
        if (!std::filesystem::exists(path))
        {
            flog::error("Band Plan directory does not exist");
            return;
        }
        if (!std::filesystem::is_directory(path))
        {
            flog::error("Band Plan directory isn't a directory...");
            return;
        }
        for (const auto& file : std::filesystem::directory_iterator(path))
        {
            const std::string filePath = file.path().generic_string();
            if (file.path().extension().generic_string() != ".json")
                continue;
            try
            {
                loadBandPlanFile(filePath, false);
            }catch (const std::exception& e)
            {
                flog::error("Could not load band plan '{}': {}", filePath, e.what());
            }catch (...)
            {
                flog::error("Could not load band plan '{}': unknown error", filePath);
            }
        }
        generateTxt();
    }

    void loadColorTable(const ConfigManager::ReadSection& table)
    {
        std::map<std::string, BandPlanColor_t> replacement;
        if (table.isNULL())
        {
            flog::error("Band plan color table isn't an object, ignoring it");
            g_colorTable.swap(replacement);
            return;
        }
        size_t count = table.Size();
        for (size_t i = 0; i < count; i++)
        {
            auto el = table[i];
            std::string type = el.GetName();
            std::string col;
            BandPlanColor_t ct;
            try
            {

                if(!el.tryGet(col))
                    throw std::runtime_error("expected a \"#RRGGBBAA\" string");
                const auto color = color_utils::parseHex<4>(col);
                if(!color)
                    throw std::runtime_error("malformed color '" + col + "', expected \"#RRGGBBAA\"");
                const auto [r, g, b, a] = *color;
                ct.colorValue = IM_COL32(r, g, b, a);
                ct.transColorValue = IM_COL32(r, g, b, 100);
                replacement.insert_or_assign(type, ct);
            }catch (const std::exception& e)
            {
                flog::error("Ignoring band plan color for '{}': {}", type, e.what());
            }
        }
        g_colorTable.swap(replacement);
    }
};
