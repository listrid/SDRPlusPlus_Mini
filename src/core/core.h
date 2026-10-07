#pragma once
#include <config.h>
#include <module.h>
#include <module_com.h>
#include <string>
#include "command_args.h"

namespace core {
    extern SDRPP_API ConfigManager configManager;
    extern SDRPP_API ModuleManager moduleManager;
    extern SDRPP_API ModuleComManager modComManager;
    extern SDRPP_API CommandArgsParser args;

    SDRPP_API void setInputSampleRate(double samplerate);

    // Commit live UI state that is not continuously persisted, then synchronously
    // flush every autosaved core and module config. This is non-terminal because
    // Android may resume after the lifecycle event.
    SDRPP_API bool saveState();

    // Effective resource paths. Returns the value from the config,
    // except under AppImage builds (BUILD_APPIMAGE) on Linux when $APPDIR
    // is set — in which case the bundled paths inside the AppImage mount
    // are returned. The accessor pattern keeps the FUSE mount point out
    // of the persisted config file. Both take the config lock themselves, so
    // they must not be called while this manager has an active access.
    SDRPP_API std::string getModulesDirectory();
    SDRPP_API std::string getResourcesDirectory();

    // Resolve a possibly relative path from the config against the
    // executable's directory (see core.cpp for rationale).
    SDRPP_API std::string resolveConfigPath(const std::string& path);
};

int sdrpp_main(int argc, char* argv[]);
