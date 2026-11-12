#include <server.h>
#include "imgui.h"
#include <stdio.h>
#include <gui/main_window.h>
#include <gui/style.h>
#include <gui/gui.h>
#include <gui/icons.h>
#include <version.h>
#include <utils/flog.h>
#include <gui/widgets/bandplan.h>
#include <gui/widgets/band_stack.h>
#include <gui/widgets/freq_memory.h>
#include <stb_image.h>
#include <config.h>
#include <core.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <gui/menus/theme.h>
#include <gui/menus/display.h>
#include <backend.h>

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>

#ifdef _WIN32
#include <Windows.h>
#include <initguid.h>
#include <shlobj.h>
#include <filesystem> 
#endif

#include <utils/executable_path.h>

#ifndef INSTALL_PREFIX
#ifdef __APPLE__
#define INSTALL_PREFIX "/usr/local"
#else
#define INSTALL_PREFIX "/usr"
#endif
#endif

namespace core {
    ConfigManagerImpl configManager;
    ModuleManager moduleManager;
    ModuleComManager modComManager;
    CommandArgsParser args;

    bool saveState()
    {
        freq_memory::commitCurrent();
        return ConfigManagerImpl::flushAll();
    }

    void setInputSampleRate(double samplerate)
    {
        // Forward this to the server
        if (args["server"].b())
        {
            server::setInputSampleRate(samplerate);
            return;
        }
        
        // Update IQ frontend input samplerate and get effective samplerate
        sigpath::iqFrontEnd.setSampleRate(samplerate);
        double effectiveSr  = sigpath::iqFrontEnd.getEffectiveSamplerate();
        
        // Reset zoom
        gui::waterfall.setBandwidth(effectiveSr);
        gui::waterfall.setViewOffset(0);
        gui::waterfall.setViewBandwidth(effectiveSr);
        gui::mainWindow.setViewBandwidthSlider(1.0);

        // Debug logs
        flog::info("New DSP samplerate: {0} (source samplerate is {1})", effectiveSr, samplerate);
    }

    // Relative paths from the config (e.g. "./modules" on Windows, "../Plugins"
    // in the MacOS bundle) are interpreted relative to the executable's
    // directory, not the working directory, so the app behaves the same no
    // matter where it is launched from.
    std::string resolveConfigPath(const std::string& path) {
        std::filesystem::path p(path);
        if (!p.is_absolute())
        { 
            p =  (std::string)core::args["root"] / p;
        }
        return p.lexically_normal().string();
    }

    std::string getModulesDirectory() {
#if defined(__linux__) && defined(BUILD_APPIMAGE)
        if (const char* appdir = getenv("APPDIR")) {
            return std::string(appdir) + "/usr/lib/sdrpp_mini/plugins";
        }
#endif
        std::string path(core::configManager.read().value("modulesDirectory", ""));
        std::filesystem::path p(path);
        if(!p.is_absolute()) {
            path = getExecutableDirectory().string() + path;
        }
        return resolveConfigPath(path);
    }

    std::string getResourcesDirectory() {
#if defined(__linux__) && defined(BUILD_APPIMAGE)
        if (const char* appdir = getenv("APPDIR")) {
            return std::string(appdir) + "/usr/share/sdrpp_mini";
        }
#endif

        std::string path(core::configManager.read().value("resourcesDirectory", ""));
        return resolveConfigPath(path);
    }
};

static void DefConfig(ConfigManager &configMng)
{
    auto conf = configMng.edit();
   
    auto bandColors = conf.section("bandColors");

    bandColors.ensure("amateur", "#FF0000FF");
    bandColors.ensure("aviation", "#00FF00FF");
    bandColors.ensure("broadcast", "#0000FFFF");
    bandColors.ensure("marine", "#00FFFFFF");
    bandColors.ensure("military", "#FFFF00FF");
    conf.ensure("bandPlan", "General");
    conf.ensure("bandPlanEnabled", true);
    conf.ensure("bandPlanPos", 0);
    conf.ensure("centerTuning", false);
    conf.ensure("colorMap", "Classic");
    conf.ensure("fftHold", false);
    conf.ensure("fftHoldSpeed", 60);
    conf.ensure("fftSmoothing", false);
    conf.ensure("fftSmoothingSpeed", 100);
    conf.ensure("fastFFT", false);
    conf.ensure("fftHeight", 300);
    conf.ensure("fftHeightLogical", true);
    conf.ensure("fftRate", 20);
    conf.ensure("fftSize", 65536);
    conf.ensure("fftWindow", "Nuttall");
    conf.ensure("mpxLineWidth", displaymenu::MPX_DEFAULT_LINE_WIDTH);
    conf.ensure("mpxRefreshRate", displaymenu::MPX_DEFAULT_REFRESH_RATE);
    conf.ensure("mpxSmoothingFactor", displaymenu::MPX_DEFAULT_SMOOTHING_FACTOR);
    conf.ensure("frequency", 100000000.0);
    {//freq_memory defaults
        auto root = conf.section(freq_memory::ROOT);
        root.ensure(freq_memory::VERSION, freq_memory::CURRENT_VERSION);
        root.ensure(freq_memory::PAGE, "keypad");
        root.ensure(freq_memory::SELECTOR, freq_memory::SELECTOR_BAND);

        auto band = root.section(freq_memory::SELECTOR_BAND);
        band.ensure(freq_memory::GROUP, "ham");
        band.ensure(freq_memory::PREFERRED_SERVICE, "other");
        band.section(freq_memory::STACKING_REGISTERS);

        auto spectrum =  root.section(freq_memory::SELECTOR_SPECTRUM);
        spectrum.ensure(freq_memory::ACTIVE_RANGE, "");
        spectrum.section(freq_memory::LAST_FREQUENCY);
    }
    conf.ensure("fullWaterfallUpdate", false);
    conf.ensure("max", 0.0);
    conf.ensure("maximized", false);
    conf.ensure("fullscreen", false);

    // Menu
    {
        auto menu = conf.array("menuElements");
        menu[0].ensure("name", "Source");
        menu[0].ensure("open", true);

        menu[1].ensure("name", "Radio");
        menu[1].ensure("open",  true);

        menu[2].ensure("name", "Frequency Manager");
        menu[2].ensure("open", true);

        menu[3].ensure("name", "Sinks");
        menu[3].ensure("open", false);

        menu[4].ensure("name", "Recorder");
        menu[4].ensure("open", false);

        menu[5].ensure("name", "VFO Color");
        menu[5].ensure("open", false);

        menu[6].ensure("name", "Band Plan");
        menu[6].ensure("open", false);

        menu[7].ensure("name", "Noise Reduction");
        menu[7].ensure("open", false);

        menu[8].ensure("name", "Display");
        menu[8].ensure("open", false);

        menu[9].ensure("name", "Module Manager");
        menu[9].ensure("open", false);

        menu[10].ensure("name", "Scanner");
        menu[10].ensure("open", false);
    }
    conf.ensure("menuWidth", 400);
    conf.ensure("menuWidthLogical", true);
    conf.ensure("min", -70.0);

    // Module instances
    auto moduleInstances =conf.section("moduleInstances");
    moduleInstances.section("Audio Source").ensure("module", "audio_source");
    moduleInstances.section("Audio Source").ensure("enabled", true);
    moduleInstances.section("File Source").ensure("module", "file_source");
    moduleInstances.section("File Source").ensure("enabled", true);
    moduleInstances.section("Network Source").ensure("module", "network_source");
    moduleInstances.section("Network Source").ensure("enabled", true);
    moduleInstances.section("SDRPP_MINI Server Source").ensure("module", "sdrpp_mini_server_source");
    moduleInstances.section("SDRPP_MINI Server Source").ensure("enabled", true);
    moduleInstances.section("HackRF Source").ensure("module", "hackrf_source");
    moduleInstances.section("HackRF Source").ensure("enabled", true);
    moduleInstances.section("PlutoSDR Source").ensure("module", "plutosdr_source");
    moduleInstances.section("PlutoSDR Source").ensure("enabled", true);

    moduleInstances.section("Radio").ensure("module", "radio");
    moduleInstances.section("Radio").ensure("enabled", true);

    moduleInstances.section("Frequency Manager").ensure("module", "frequency_manager");
    moduleInstances.section("Frequency Manager").ensure("enabled", true);

    moduleInstances.section("Audio Sink").ensure("module", "audio_sink");
    moduleInstances.section("Audio Sink").ensure("enabled", false);
    moduleInstances.section("Network Sink").ensure("module", "network_sink");
    moduleInstances.section("Network Sink").ensure("enabled", false);

//    moduleInstances.section("Airspy Source").ensure("module", "airspy_source");
//    moduleInstances.section("Airspy Source").ensure("enabled", true);
    moduleInstances.section("AirspyHF+ Source").ensure("module", "airspyhf_source");
    moduleInstances.section("AirspyHF+ Source").ensure("enabled", true);
//    moduleInstances.section("BladeRF Source").ensure("module", "bladerf_source");
//    moduleInstances.section("BladeRF Source").ensure("enabled", true);
//    moduleInstances.section("Dragon Labs Source").ensure("module", "dragonlabs_source");
//    moduleInstances.section("Dragon Labs Source").ensure("enabled", true);
//    moduleInstances.section("FobosSDR Source").ensure("module", "fobossdr_source");
//    moduleInstances.section("FobosSDR Source").ensure("enabled", true);
//    moduleInstances.section("Harogic Source").ensure("module", "harogic_source");
//    moduleInstances.section("Harogic Source").ensure("enabled", true);
//    moduleInstances.section("Hermes Source").ensure("module", "hermes_source");
//    moduleInstances.section("Hermes Source").ensure("enabled", true);
//    moduleInstances.section("HydraSDR Source").ensure("module", "hydrasdr_source");
//    moduleInstances.section("HydraSDR Source").ensure("enabled", true);
//    moduleInstances.section("KiwiSDR Source").ensure("module", "kiwisdr_source");
//    moduleInstances.section("KiwiSDR Source").ensure("enabled", true);
//    moduleInstances.section("LimeSDR Source").ensure("module", "limesdr_source");
//    moduleInstances.section("LimeSDR Source").ensure("enabled", true);
//    moduleInstances.section("PerseusSDR Source").ensure("module", "perseus_source");
//    moduleInstances.section("PerseusSDR Source").ensure("enabled", true);
//    moduleInstances.section("QMX Source").ensure("module", "qmx_source");
//    moduleInstances.section("QMX Source").ensure("enabled", true);
//    moduleInstances.section("QMX Server Source").ensure("module", "qmxserver_source");
//    moduleInstances.section("QMX Server Source").ensure("enabled", true);
//    moduleInstances.section("RFNM Source").ensure("module", "rfnm_source");
//    moduleInstances.section("RFNM Source").ensure("enabled", true);
//    moduleInstances.section("RFspace Source").ensure("module", "rfspace_source");
//    moduleInstances.section("RFspace Source").ensure("enabled", true);
//    moduleInstances.section("RTL-SDR Source").ensure("module", "rtl_sdr_source");
//    moduleInstances.section("RTL-SDR Source").ensure("enabled", true);
//    moduleInstances.section("RTL-TCP Source").ensure("module", "rtl_tcp_source");
//    moduleInstances.section("RTL-TCP Source").ensure("enabled", true);
//    moduleInstances.section("SDRplay Source").ensure("module", "sdrplay_source");
//    moduleInstances.section("SDRplay Source").ensure("enabled", true);
//    moduleInstances.section("Spectran HTTP Source").ensure("module", "spectran_http_source");
//    moduleInstances.section("Spectran HTTP Source").ensure("enabled", true);
//    moduleInstances.section("SpyServer Source").ensure("module", "spyserver_source");
//    moduleInstances.section("SpyServer Source").ensure("enabled", true);
    moduleInstances.section("USRP Source").ensure("module", "usrp_source");
    moduleInstances.section("USRP Source").ensure("enabled", true);


//    moduleInstances.section("Radiosonde Decoder").ensure("module", "radiosonde_decoder");
//    moduleInstances.section("Radiosonde Decoder").ensure("enabled", false);

    moduleInstances.section("Recorder").ensure("module", "recorder");
    moduleInstances.section("Recorder").ensure("enabled", false);
    moduleInstances.section("Rigctl Server").ensure("module", "rigctl_server");
    moduleInstances.section("Rigctl Server").ensure("enabled", false);

    moduleInstances.section("Noise Reduction").ensure("module", "noise_reduction_logmmse");
    moduleInstances.section("Noise Reduction").ensure("enabled", false);
//    moduleInstances.section("Spots").ensure("module", "spots");
//    moduleInstances.section("Spots").ensure("enabled", false);
//    moduleInstances.section("WebSDR View").ensure("module", "websdr_view");
//    moduleInstances.section("WebSDR View").ensure("enabled", false);
    moduleInstances.section("Rigctl Client").ensure("module", "rigctl_client");
    moduleInstances.section("Rigctl Client").ensure("enabled", false);

    moduleInstances.section("Scanner").ensure("module", "scanner");
    moduleInstances.section("Scanner").ensure("enabled", false);

    // Themes
    conf.ensure("theme", "Dark");
    conf.ensure("uiScaleFactor", 1.0f);
    conf.ensure("touchStyle", style::touchStyle);

    conf.array("modules");
    {
        auto offsets = conf.section("offsets");
        offsets.ensure("SpyVerter", 120000000.0);
        offsets.ensure("Ham-It-Up", 125000000.0);
        offsets.ensure("MMDS S-band (1998MHz)", -1998000000.0);
        offsets.ensure("DK5AV X-Band", -6800000000.0);
        offsets.ensure("Ku LNB (9750MHz)", -9750000000.0);
        offsets.ensure("Ku LNB (10700MHz)", -10700000000.0);
    }

    conf.ensure("selectedOffset", "None");
    conf.ensure("manualOffset", 0.0);
    conf.ensure("showMenu", true);
    conf.ensure("showWaterfall", true);
    conf.ensure("waterfallAutoRange", false);
    conf.ensure("source", "PlutoSDR");
    conf.ensure("decimation", 1);
    conf.ensure("iqCorrection", false);
    conf.ensure("invertIQ", false);

    conf.ensure("bandPlanPos", 1);
    {
        auto radio = conf.section("streams", "Radio");
        radio.ensure("muted", false);
        radio.ensure("sink", "Audio");
        radio.ensure("volume", 1.0f);
    }
    {
        auto size = conf.section("windowSize");
        size.ensure("h", 720);
        size.ensure("w", 1280);
    }
    conf.section("vfoColors").ensure("Radio", "#00FF00");

#ifdef __ANDROID__
    conf.ensure("lockMenuOrder", true);
#else
    conf.ensure("lockMenuOrder", false);
#endif

#if defined(_WIN32)
    conf.ensure("modulesDirectory", "./modules");
    conf.ensure("resourcesDirectory", "./res");
    conf.ensure("configDirectory", "./conf");

#elif defined(IS_MACOS_BUNDLE)
    conf.ensure("modulesDirectory", "../Plugins");
    conf.ensure("resourcesDirectory", "../Resources");
    conf.ensure("configDirectory",  "../Config");
#elif defined(__ANDROID__)
    conf.ensure("modulesDirectory", root + "/modules");
    conf.ensure("resourcesDirectory", root + "/res");
    conf.ensure("configDirectory", root + "/conf");

#else
    // Linux, BSD, etc.
    conf.ensure("modulesDirectory", INSTALL_PREFIX "/lib/sdrpp_mini/plugins");
    conf.ensure("resourcesDirectory", INSTALL_PREFIX "/share/sdrpp_mini");
#endif
}



// main
int sdrpp_main(int argc, char* argv[]) {
#ifdef _WIN32
    // The UTF-8 activeCodePage manifest makes narrow strings UTF-8 process-wide;
    // switch the console codepage so log output renders correctly on legacy conhost.
    SetConsoleOutputCP(CP_UTF8);
#endif

    flog::info("SDRPP_MINI v" VERSION_STR);

    // Define command line options and parse arguments
    core::args.defineAll();
    if (core::args.parse(argc, argv) < 0) { return -1; } 

    // Show help and exit if requested
    if (core::args["help"].b()) {
        core::args.showHelp();
        return 0;
    }

    // Initialize libcurl process-wide before anything spawns threads or makes
    // HTTP/WS calls. Plugins must not call curl_global_init themselves.

    bool serverMode = (bool)core::args["server"];

#ifdef _WIN32
    // Free console if the user hasn't asked for a console and not in server mode
    if (!core::args["con"].b() && !serverMode)
        FreeConsole();

    // Set error mode to avoid abnoxious popups
    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
#endif

    // Check root directory
    std::string root = (std::string)core::args["root"];
    if (!std::filesystem::exists(root))
    {
        flog::warn("Root directory {0} does not exist, creating it", root);
        if (!std::filesystem::create_directories(root))
        {
            flog::error("Could not create root directory {0}", root);
            return -1;
        }
    }
#if defined(_WIN32)
    if(!std::filesystem::is_directory(root + "\\res"))
    {
        char path[MAX_PATH];
        if(SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, path)))
        {
            root = std::string(path) + "\\SDRPP_mini";
            core::args.set("root", root);
            if(!std::filesystem::exists(root))
            {

                std::filesystem::create_directories(root);
                std::filesystem::create_directories(root+"/recordings");
                std::filesystem::create_directories(root+"/res");
                std::filesystem::create_directories(root+"/res/bandplans");
                std::filesystem::create_directories(root+"/res/colormaps");
                std::filesystem::create_directories(root+"/res/themes");
            }
        }
    }
#endif

    // Check that the path actually is a directory
    if (!std::filesystem::is_directory(root))
    {
        flog::error("{0} is not a directory", root);
        return -1;
    }

    // ======== DEFAULT CONFIG ========
    constexpr int CURRENT_CONFIG_VERSION = 1;

    // Load config
    flog::info("Loading config");
    core::configManager.setFile((root + "/config.json").c_str(), true);
    bool firstStart = !std::filesystem::exists(root + "/config.json");
    if(!core::configManager.load())
        core::configManager.edit().ensure("configVersion", CURRENT_CONFIG_VERSION);
    DefConfig(core::configManager);
    core::configManager.autoSave(true);

    {//init config dir
        std::string confDir;
        core::configManager.read().tryGet("configDirectory", confDir);
        confDir =  root + confDir;
        if(!std::filesystem::exists(confDir))
            std::filesystem::create_directories(confDir);
        core::args.set("config", confDir);
    }
    float uiScaleFactor = 1.0f;
    {
        auto configAccess = core::configManager.edit();

    // Android can't load just any .so file. This means we have to hardcode the name of the modules
#ifdef __ANDROID__
        configAccess.set("modules", json::array({
            "airspy_source.so",
            "airspyhf_source.so",
            "hackrf_source.so",
            "hermes_source.so",
            "hydrasdr_source.so",
            "plutosdr_source.so",
            "qmx_source.so",
            "qmxserver_source.so",
            "rfspace_source.so",
            "rtl_sdr_source.so",
            "rtl_tcp_source.so",
            "sdrpp_mini_server_source.so",
            "spyserver_source.so",
            "kiwisdr_source.so",

            "network_sink.so",
            "audio_sink.so",

            "m17_decoder.so",
            "meteor_demodulator.so",
            "radio.so",
            "radiosonde_decoder.so",

            "frequency_manager.so",
            "noise_reduction_logmmse.so",
            "recorder.so",
            "rigctl_server.so",
            "scanner.so",
            "spots.so",
            "websdr_view.so",
        }));
#endif

        int storedConfigVersion = 0;
        const bool validConfigVersion = configAccess.tryGet("configVersion", storedConfigVersion);
        if (!validConfigVersion || storedConfigVersion < CURRENT_CONFIG_VERSION)
        {
            flog::info("Migrating core config schema from version {0} to {1}", storedConfigVersion, CURRENT_CONFIG_VERSION);
            // Version 1 establishes the explicit schema marker. Future migrations
            // run immediately before advancing this value.
            configAccess["configVersion"] = CURRENT_CONFIG_VERSION;
        }else if (storedConfigVersion > CURRENT_CONFIG_VERSION)
        {
            flog::warn("Core config schema version {0} is newer than supported version {1}; unknown keys will be preserved", storedConfigVersion, CURRENT_CONFIG_VERSION);
        }


        // Subtrees evolve independently of the flat core settings. Seed their
        // schema marker explicitly because the top-level repair above does not
        // recurse into an already existing object.
        ConfigManager::EditSection frequencyMemory = freq_memory::root(configAccess);
        int frequencyMemoryVersion = 0;
        if (!frequencyMemory.tryGet(freq_memory::VERSION, frequencyMemoryVersion) || frequencyMemoryVersion < freq_memory::CURRENT_VERSION)
        {
            frequencyMemory[freq_memory::VERSION] = freq_memory::CURRENT_VERSION;
        }else if (frequencyMemoryVersion > freq_memory::CURRENT_VERSION)
        {
            flog::warn("Frequency-memory config schema version {0} is newer than supported version {1}; unknown keys will be preserved", frequencyMemoryVersion, freq_memory::CURRENT_VERSION);
        }

        // Preserve unknown keys. They may belong to a newer application version;
        // deleting them here would make a temporary downgrade destructive.

        // Update to new module representation in config if needed
        {
            ConfigManager::EditSection instances = configAccess.section("moduleInstances");
            std::vector<std::pair<std::string, std::string>> legacyInstances;
            {
                auto insts = instances.peek();
                for(auto const& obj : insts)
                {
                     std::string _name = obj.GetName();
                     std::string inst;
                     if(obj.tryGet(inst))
                         legacyInstances.emplace_back(_name, inst);
                }
            }
            for(auto const& [_name, mod] : legacyInstances)
            {
                auto newMod  = instances.section(_name);
                newMod["module"] = mod;
                newMod["enabled"] = true;
            }
        }

        // Load UI scale factor; detected scale is not known yet (backend not initialized),
        // so set a temporary scale using the factor alone. The correct effective scale is
        // applied after backend::init() below, before any font loading.
        uiScaleFactor = configAccess.value("uiScaleFactor", 1.0f);
        style::setUIScale(uiScaleFactor);

        // Must be set before thememenu::init() applies the first scaled style.
        // The fallback keeps the per-platform default for configs predating the key.
        style::touchStyle = configAccess.value("touchStyle", style::touchStyle);

        style::migrateLogicalDimension(configAccess, "menuWidth", "menuWidthLogical", 250.0f, [](float value) {
            return style::uiScale > 1.0f && value > 300.0f;
        });
        style::migrateLogicalDimension(configAccess, "fftHeight", "fftHeightLogical", 150.0f, [](float value) {
            return style::uiScale > 1.0f && value >= 300.0f * style::uiScale;
        });
    }

    if (serverMode)
    {
        int rc = server::main();
        return rc;
    }

    std::string resDir = core::getResourcesDirectory();

    // Initialize backend
    int biRes = backend::init(resDir);
    if (biRes < 0)
        return biRes;

    // Apply the correct effective scale now that the backend (GLFW) can report the actual
    // OS display scale. detectedScale is kept in memory only — not persisted to config.
    {
        float detected  = backend::getContentScale();
        float effective = std::clamp(detected * uiScaleFactor, 1.0f, 4.0f);
        style::setUIScale(effective);
        flog::info("UI scale: detected={:.2f}, factor={:.2f}, effective={:.2f}", detected, uiScaleFactor, effective);
    }

    if (firstStart)
        core::configManager.edit()["menuWidth"] = 400;

    // Initialize SmGui in normal mode
    SmGui::init(false);

    if (!style::loadFonts(resDir))
        return -1;
    thememenu::init(resDir);
    LoadingScreen::init();

    LoadingScreen::show("Loading icons");
    flog::info("Loading icons");
    if (!icons::load(resDir))
        return -1;

    LoadingScreen::show("Loading band plans");
    flog::info("Loading band plans");
    bandplan::loadInternal();
    bandplan::loadFromDir(resDir + "/bandplans");


    LoadingScreen::show("Loading band plan colors");
    flog::info("Loading band plans color table");
    {
        auto bandColors = core::configManager.read().section("bandColors");
        bandplan::loadColorTable(bandColors);
    }
    gui::mainWindow.init();

    flog::info("Ready.");

    // Run render loop (TODO: CHECK RETURN VALUE)
    backend::renderLoop();

    bool coreConfigSaved = true;

    // On android, none of this shutdown should happen due to the way the UI works
#ifndef __ANDROID__
    // The render loop has ended but modules are still alive, so the selected
    // radio mode can still be sampled for the active band's stack.
    freq_memory::commitCurrent();

    // Module instances are deliberately NOT destroyed here. Bulk teardown runs
    // largely unaudited destructors, and the source menu's reselect fallback
    // turns any source that never unregisters into a use-after-free. The OS
    // reclaims the instances at process exit instead.
    //
    // The cost is that each module's _END_ below closes its ConfigManager while
    // that module's instances are still alive, and config access after such a
    // shutdown throws. Module config access is confined to the GUI thread,
    // which the render loop has already retired, so the exposure is bounded.
    //
    // FIXME: restore the loop only together with the quiescent shutdown phase
    // described in doc/bugs/desktop-module-teardown.md.
    // while (!core::moduleManager.instances.empty()) {
    //     core::moduleManager.deleteInstance(core::moduleManager.instances.begin()->first);
    // }

    // Shut down all modules
    for (auto& [name, mod] : core::moduleManager.modules)
        mod.end();

    // Terminate backend (TODO: CHECK RETURN VALUE)
    backend::end();

    sigpath::iqFrontEnd.stop();

    coreConfigSaved = core::configManager.shutdown();
#endif

    if (coreConfigSaved)
    {
        flog::info("Exiting successfully");
        return 0;
    }
    flog::error("Exiting after failing to save the core config");
    return -1;
}
