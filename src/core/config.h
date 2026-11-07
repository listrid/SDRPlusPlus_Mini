#pragma once
#include <SDRpp_api.h>
#include <atomic>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>


namespace config_detail
{
    struct ConfigManagerTestAccess;
}

class tdJSON;

class ConfigManagerImpl: public ConfigManager
{
    friend class ConfigManager::EditSection;
    friend class ConfigManager::ReadSection;
    friend class ConfigManager::ReadFields;
public:
    ConfigManagerImpl();
    ~ConfigManagerImpl();
    virtual void setFile(const char* fileName, bool full = false);
    virtual bool load();
    virtual bool save();
    virtual void enableAutoSave();
    virtual void disableAutoSave();
    // Synchronously saves every dirty manager currently registered for
    // autosave, without changing their lifecycle state. Intended for platforms
    // that may resume after a save-state event. It deliberately makes one save
    // attempt per manager, leaving background retry to the autosaver. Continues
    // after individual failures and returns true only if every attempted save
    // succeeded.
    static bool flushAll();
    virtual bool Clear();
    virtual bool shutdown();

    virtual ConfigManager::ReadSection read();
    virtual ConfigManager::EditSection edit();

private:
    tdJSON* m_json;
    bool   m_modifed = false;
    size_t m_section = 0;
    void add_section() noexcept;
    void dec_section() noexcept;
    void set_modifed();
    tdJSON& getUpdate()
    {
        m_modifed = true;
        return *m_json;
    };

    bool anyAccessHeldByCurrentThread() const noexcept;
    void disableAutoSaveImpl();

    enum class SaveRetryMode
    {
        SingleAttempt,
        TransientWindowsLocks
    };

    // Called only by the one process-wide ConfigSaver. The dirty flag is guarded
    // by mtx and is cleared only after a successful disk commit.
    bool saveIfDirty();
    bool saveImpl(bool onlyIfDirty, bool allowClosing = false, SaveRetryMode retryMode = SaveRetryMode::SingleAttempt);

    friend class ConfigSaver;
    friend struct config_detail::ConfigManagerTestAccess;

    std::string m_path = "";
    enum class LifecycleState
    {
        Open, Closing, Closed
    };
    LifecycleState m_lifecycleState = LifecycleState::Open;

    bool m_dirty = false;
    bool m_autoSaveEnabled = false;
    std::mutex m_mtx;
    std::mutex m_persistenceMtx;
};

