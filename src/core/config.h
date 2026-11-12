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



class tdJSON;

class ConfigManagerImpl: public ConfigManager
{
    friend class ConfigSaver;
    friend class ConfigManager::EditSection;
    friend class ConfigManager::ReadSection;
    friend class ConfigManager::ReadFields;
public:
    ConfigManagerImpl();
    ~ConfigManagerImpl();
    virtual void setFile(const char* fileName, bool full = false);
    virtual bool load();
    virtual bool save();
    virtual void autoSave(bool enable);

    // Synchronously saves every dirty manager currently registered for
    // autosave, without changing their lifecycle state. Intended for platforms
    // that may resume after a save-state event. It deliberately makes one save
    // attempt per manager, leaving background retry to the autosaver. Continues
    // after individual failures and returns true only if every attempted save
    // succeeded.
    static bool flushAll();
    virtual bool clear();
    virtual bool shutdown();

    virtual ConfigManager::ReadSection read();
    virtual ConfigManager::EditSection edit();

private:
    tdJSON* m_json;
    bool   m_modifed = false;
    size_t m_section = 0;
    void add_section() noexcept;
    void dec_section(bool isRoot) noexcept;
    void set_modifed();
    tdJSON& getUpdate()
    {
        m_modifed = true;
        return *m_json;
    };

    void disableAutoSave();

    enum class SaveRetryMode
    {
        SingleAttempt,
        TransientWindowsLocks
    };

    bool saveImpl(bool onlyIfDirty, bool allowClosing = false, SaveRetryMode retryMode = SaveRetryMode::SingleAttempt);

    std::string m_path = "";
    enum class LifecycleState
    {
        Open, Closing, Closed
    };
    LifecycleState m_lifecycleState = LifecycleState::Open;

    bool m_dirty = false;
    bool m_auto_save = false;
    std::recursive_mutex m_sec_mtx;
};

