#include <tdFile.hpp>
#include <tdJSON_Lite.hpp>

#include <config.h>
#include <utils/flog.h>
#include <core.h>

#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace
{
    std::atomic<uint64_t> g_temporaryFileSequence{ 0 };

    enum class ReadResult
    {
        OK,
        MISSING,
        INVALID
    };

    ReadResult readJsonFile(const std::filesystem::path& path, tdJSON& out)
    {
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (ec)
            return ReadResult::INVALID;
        if (!exists)
            return ReadResult::MISSING;
        if (ec || !std::filesystem::is_regular_file(path, ec) || ec)
            return ReadResult::INVALID;

        size_t data_len = 0;
        char* data = NULL;
        {
            tdFile file;
            if(!file.Open(path.string().c_str(), tdFile::r))
                return ReadResult::INVALID;
            size_t data_len = file.Size();
            data = (char*)malloc(data_len+1);
            file.Read(data, data_len);
            data[data_len] = 0;
        }
        bool rez = out.Init(data, data_len);
        free(data);
        return rez ? ReadResult::OK : ReadResult::INVALID;
    }

    std::filesystem::path temporaryPathFor(const std::filesystem::path& path)
    {
#ifdef _WIN32
        const unsigned long processId = GetCurrentProcessId();
#else
        const long processId = static_cast<long>(getpid());
#endif
        const uint64_t sequence = g_temporaryFileSequence.fetch_add(1, std::memory_order_relaxed);
        std::filesystem::path temporary = path;
        temporary += ".tmp." + std::to_string(processId) + "." + std::to_string(sequence);
        return temporary;
    }

#ifdef _WIN32
    bool accessDenialMayBeTransient(const std::filesystem::path& destination)
    {
        const DWORD attributes = GetFileAttributesW(destination.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            // The destination may not exist yet; the access denial can still
            // come from a scanner holding the freshly written source file.
            // A delete-pending destination can also report access denied here,
            // but so can an ACL denial. Treat that ambiguous case as permanent
            // to keep a structural failure from delaying every config at exit.
            return GetLastError() == ERROR_FILE_NOT_FOUND;
        }
        return (attributes & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY)) == 0;
    }
#endif

    bool replaceFile(const std::filesystem::path& source, const std::filesystem::path& destination, bool retryTransientLocks)
    {
#ifdef _WIN32
        constexpr auto retryBudget = std::chrono::seconds(2);
        constexpr auto maximumDelay = std::chrono::milliseconds(500);
        auto delay = std::chrono::milliseconds(20);
        const auto deadline = std::chrono::steady_clock::now() + retryBudget;
        unsigned int attempts = 0;
        DWORD error = ERROR_SUCCESS;

        for (;;)
        {
            ++attempts;
            if (MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                return true;

            error = GetLastError(); // Capture before the probe below changes last-error.
            // Keep this allowlist to local handle interference. Network recovery
            // and resource-exhaustion errors have different time scales and fail
            // immediately rather than multiplying the shutdown delay.
            const bool transient = (error == ERROR_SHARING_VIOLATION) || (error == ERROR_LOCK_VIOLATION) || (error == ERROR_USER_MAPPED_FILE) ||
                                   ((error == ERROR_ACCESS_DENIED) && accessDenialMayBeTransient(destination));
            const auto now = std::chrono::steady_clock::now();
            if (!retryTransientLocks || !transient || now >= deadline)
                break;

            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            std::this_thread::sleep_for((std::min)(delay, remaining));
            delay = (std::min)(delay * 2, maximumDelay);
        }

        flog::error("Could not replace config file '{}' after {} attempt(s): Windows error {}", destination.string(), attempts, error);
        return false;
#else
        (void)retryTransientLocks;
        std::error_code ec;
        std::filesystem::rename(source, destination, ec);
        if (!ec)
            return true;
        flog::error("Could not replace config file '{}': {}", destination.string(), ec.message());
        return false;
#endif
    }

    bool writeJsonAtomically(const std::filesystem::path& path, const tdJSON& json, bool retryTransientLocks = false)
    {
        const std::filesystem::path temporary = temporaryPathFor(path);
        std::error_code ignored;
        if(!json.Save(temporary.string().c_str(), true))
        {
            flog::error("Could not write temporary config file '{}'", temporary.string());
            std::filesystem::remove(temporary, ignored);
            return false;
        }
        if(replaceFile(temporary, path, retryTransientLocks))
            return true;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
}

class ConfigSaver
{
public:
    static ConfigSaver& instance()
    {
        static ConfigSaver saver;
        return saver;
    }

    void enable(ConfigManagerImpl* manager)
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        if(manager->m_lifecycleState != ConfigManagerImpl::LifecycleState::Open)
            throw std::logic_error("ConfigManager::enableAutoSave called after shutdown began");
        if(manager->m_auto_save)
             return;
        m_managers.push_back(manager);
        manager->m_auto_save = true;
    }

    void disable(ConfigManagerImpl* manager)
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        if (!manager->m_auto_save)
            return;
        const auto it = std::find(m_managers.begin(), m_managers.end(), manager);
        if(it != m_managers.end())
            m_managers.erase(it);
        manager->m_auto_save = false;
    }

    void notifyDirty(ConfigManagerImpl* manager)
    {
        if (!manager->m_auto_save)
            return;
        std::lock_guard<std::mutex> guard(m_mtx);
        m_pending = true;
    }

    bool flushAll()
    {
        std::lock_guard<std::mutex> m_guard(m_mtx);
        if(!m_pending)
            return true;

        bool allSaved = true;
        for(const auto& manager : m_managers)
        {
            if(!manager)
                continue;
            if(!manager->m_dirty)
                continue;
            if(!manager->m_sec_mtx.try_lock())
            {
                allSaved = false;
                continue;
            }
            if(manager->m_section != 0 || !manager->saveImpl(true, true))
                allSaved = false;
            manager->m_sec_mtx.unlock();
        }
        if(allSaved)
            m_pending = false;
        return allSaved;
    }

    ~ConfigSaver()
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        for(const auto& manager : m_managers)
        {
            if(manager)
                manager->m_auto_save = false;
        }
    }

private:
    std::mutex m_mtx;
    std::vector<ConfigManagerImpl*> m_managers;
    bool m_pending = false;
};



ConfigManagerImpl::ConfigManagerImpl()
{
    m_json = new tdJSON;
}

ConfigManagerImpl::~ConfigManagerImpl()
{
    disableAutoSave();
    bool unsaved = false;
    std::string unsavedPath;
    {
        std::lock_guard<std::recursive_mutex> guard(m_sec_mtx);
        unsaved = m_dirty;
        if(unsaved)
            unsavedPath = m_path;
    }
    if (unsaved)
        flog::error("Config manager for '{}' was destroyed with unsaved changes", unsavedPath);
    delete m_json;
}

void ConfigManagerImpl::setFile(const char* fileName, bool full)
{
    std::string file;
    if(full)
    {
        file = fileName;
    }else{
        file = core::args["config"].s() + "/" + fileName;
    }
    const std::string newPath = std::filesystem::absolute(std::move(file)).lexically_normal().string();

    std::lock_guard<std::recursive_mutex> persistenceGuard(m_sec_mtx);
    if(m_lifecycleState != LifecycleState::Open)
        throw std::logic_error("ConfigManager::setPath called after shutdown began");
    m_path = newPath;
    if (m_dirty && m_auto_save)
        ConfigSaver::instance().notifyDirty(this);
}

bool ConfigManagerImpl::load()
{
    std::lock_guard<std::recursive_mutex> persistenceGuard(m_sec_mtx);
    if(m_section)
        throw std::logic_error("ConfigManager::load already being edited");

    if (m_lifecycleState != LifecycleState::Open)
        throw std::logic_error("ConfigManager::load called after shutdown began");

    if (m_path.empty())
    {
        flog::error("Config manager tried to load file with no path specified");
        return false;
    }
    bool load_json = false;
    {
        const ReadResult result = readJsonFile(m_path, *m_json);
        if (result == ReadResult::OK)
        {
            load_json = true;
        }else{
            if (result == ReadResult::MISSING)
            {
                flog::warn("Config file '{}' does not exist, creating it", m_path);
            }else{
                flog::error("Config file '{}' is not a valid JSON file, resetting it", m_path);
            }
        }
        m_dirty =!load_json;
    }
    if (m_dirty && m_auto_save)
        ConfigSaver::instance().notifyDirty(this);
    return load_json;
}

bool ConfigManagerImpl::save()
{
    return saveImpl(false);
}

bool ConfigManagerImpl::flushAll()
{
    return ConfigSaver::instance().flushAll();
}

bool ConfigManagerImpl::saveImpl(bool onlyIfDirty, bool allowClosing, SaveRetryMode retryMode)
{
    std::lock_guard<std::recursive_mutex> persistenceGuard(m_sec_mtx);
    if(m_lifecycleState != LifecycleState::Open)
    {
        if(!allowClosing)
            throw std::logic_error("ConfigManager::save called after shutdown began");
        if(m_lifecycleState == LifecycleState::Closed)
            return !m_dirty;
    }
    if(onlyIfDirty && !m_dirty)
        return true;
    if(!writeJsonAtomically(m_path, *m_json, retryMode == SaveRetryMode::TransientWindowsLocks))
        return false;
    m_dirty = false;
    return true;
}


void ConfigManagerImpl::autoSave(bool enable)
{
    std::lock_guard<std::recursive_mutex> guard(m_sec_mtx);
    if(enable)
    {
        ConfigSaver::instance().enable(this);
        return;
    }
    if(m_lifecycleState != LifecycleState::Open)
        throw std::logic_error("ConfigManager::disableAutoSave called after shutdown began");
    disableAutoSave();
}

void ConfigManagerImpl::disableAutoSave()
{
    if (!m_auto_save)
        return;
    ConfigSaver::instance().disable(this);
}

bool ConfigManagerImpl::shutdown()
{
    std::lock_guard<std::recursive_mutex> persistenceGuard(m_sec_mtx);
    if (m_lifecycleState == LifecycleState::Closed)
        return true;
    m_lifecycleState = LifecycleState::Closing;

    disableAutoSave();
    if (!saveImpl(true, true, SaveRetryMode::TransientWindowsLocks))
    {
        flog::error("Could not save config '{}' during shutdown", m_path);
        return false;
    }
    if (m_dirty)
    {
        flog::error("Config '{}' remained dirty after its shutdown save", m_path);
        return false;
    }else{
        m_lifecycleState = LifecycleState::Closed;
    }
    return true;
}

bool ConfigManagerImpl::clear()
{
    std::lock_guard<std::recursive_mutex> persistenceGuard(m_sec_mtx);
    if(m_section)
        return false;
    m_json->Init("{}", 2);
    set_modifed();
    return true;
}

void ConfigManagerImpl::add_section() noexcept
{
    m_section++;
}

void ConfigManagerImpl::dec_section(bool isRoot) noexcept
{
    if(isRoot)
        m_sec_mtx.unlock();
    m_section--;
    if(!m_section)
    {
        if(m_modifed)
            m_dirty = true;
        m_modifed = false;
    }
    if(m_dirty && m_auto_save)
    {
        try {
            ConfigSaver::instance().notifyDirty(this);
        } catch(...) {}
    }
}

void ConfigManagerImpl::set_modifed()
{
    m_modifed = true;
}


ConfigManager::ReadFields::ReadFields(ConfigManagerImpl& mng, tdJSON_NODE& node) : m_mng(mng), m_node(node){};

const std::string ConfigManager::ReadFields::GetName() const
{
    const char* str;
    size_t len;
    m_node.GetName(str, len);
    return std::string(str, len);
}

bool ConfigManager::ReadFields::isNULL() const { return m_node.isNULL(); }
bool ConfigManager::ReadFields::isNumber() const { return m_node.isDouble() || m_node.isInt64(); }
bool ConfigManager::ReadFields::isBool() const { return m_node.isBool(); }
bool ConfigManager::ReadFields::isString() const { return m_node.isString(); }
bool ConfigManager::ReadFields::isSection() const { return m_node.isObject() || m_node.isArray(); }

ConfigManager::ReadSection ConfigManager::ReadFields::toSection() const
{
    if(m_node.isObject() || m_node.isArray())
        return ConfigManager::ReadSection(m_mng, m_node, false);
    return ConfigManager::ReadSection(m_mng, m_node[7], false);//to NULL;
}
bool ConfigManager::ReadFields::tryGet(std::string& out) const
{
    const char* str;
    size_t len;
    if(!m_node.GetString(str, len))
        return false;
    out.assign(str, len);
    return true;
}
bool ConfigManager::ReadFields::tryGet(int& out) const
{
    int64_t v;
    if(!m_node.GetInt64(v))
        return false;
    out = int(v);
    return true;
}
bool ConfigManager::ReadFields::tryGet(unsigned int& out) const
{
    int64_t v;
    if(!m_node.GetInt64(v))
        return false;
    out = (unsigned int)(v);
    return true;
}
bool ConfigManager::ReadFields::tryGet(int64_t& out) const { return m_node.GetInt64(out); }
bool ConfigManager::ReadFields::tryGet(bool& out) const { return m_node.GetBool(out); }
bool ConfigManager::ReadFields::tryGet(double& out) const { return m_node.GetDouble(out); }
bool ConfigManager::ReadFields::tryGet(float& out) const
{
    double v;
    if(!m_node.GetDouble(v))
        return false;
    out = float(v);
        return true;
}

size_t ConfigManager::ReadFields::Size() const { return m_node.Size(); }
const ConfigManager::ReadFields ConfigManager::ReadFields::operator[](size_t index) const { return ConfigManager::ReadFields(m_mng, m_node[index]); }
const ConfigManager::ReadFields ConfigManager::ReadFields::operator[](const std::string_view& key) const{ return ConfigManager::ReadFields(m_mng, m_node[key.data()]); }


const ConfigManager::ReadSection ConfigManager::ReadSection::section(std::string_view name) const
{
    return ReadSection(m_mng, m_node[name.data()], false);
}

const ConfigManager::ReadSection ConfigManager::ReadSection::section(std::string_view name1, std::string_view name2) const
{
    tdJSON_NODE& node1 = m_node[name1.data()];
    tdJSON_NODE& node2 = node1[name2.data()];
    return ReadSection(m_mng, node2, false);
}

const ConfigManager::ReadFields ConfigManager::ReadSection::operator[](size_t index) const
{
    return ConfigManager::ReadFields(m_mng, m_node[index]);
}

const ConfigManager::ReadFields ConfigManager::ReadSection::operator[](const std::string_view& key) const
{
    return ConfigManager::ReadFields(m_mng, m_node[key.data()]);
}

bool ConfigManager::ReadSection::isArray() const { return m_node.isArray(); }
bool ConfigManager::ReadSection::isNULL() const { return m_node.isNULL(); }
size_t ConfigManager::ReadSection::Size() const { return m_node.Size(); }


bool ConfigManager::ReadSection::tryGet(const char* key, std::string& out) const
{
    tdJSON_NODE& node = m_node[key];
    if(!node.isString())
        return false;
    size_t len;
    const char* val;
    node.GetString(val, len);
    out = val;
    return true;
}
bool ConfigManager::ReadSection::tryGet(const char* key, int& out) const
{
    tdJSON_NODE& node = m_node[key];
    int64_t val;
    if(!node.GetInt64(val))
        return false;
    out = (int)val;
    return true;
}
bool ConfigManager::ReadSection::tryGet(const char* key, unsigned int& out) const
{
    tdJSON_NODE& node = m_node[key];
    int64_t val;
    if(!node.GetInt64(val))
        return false;
    out = (unsigned int)val;
    return true;
}
bool ConfigManager::ReadSection::tryGet(const char* key, int64_t& out) const
{
    tdJSON_NODE& node = m_node[key];
    int64_t val;
    if(!node.GetInt64(val))
        return false;
    out = (unsigned int)val;
    return true;
}
bool ConfigManager::ReadSection::tryGet(const char* key, bool& out) const
{
    tdJSON_NODE& node = m_node[key];
    if(!node.GetBool(out))
        return false;
    return true;
}
bool ConfigManager::ReadSection::tryGet(const char* key, std::vector<std::string>& out) const
{
    tdJSON_NODE& node = m_node[key];
    if(!node.isArray())
        return false;
    size_t size = node.Size();
    out.clear();
    for(size_t i = 0; i < size; i++)
    {
        tdJSON_NODE& p = node[i];
        size_t len;
        const char* str;
        if(!p.GetString(str, len))
            continue;
        out.push_back(std::string(str));
    }
    return true;
}
bool ConfigManager::ReadSection::tryGet(const char* key, double& out) const
{
    tdJSON_NODE& node = m_node[key];
    return node.GetDouble(out);
}
bool ConfigManager::ReadSection::tryGet(const char* key, float& out) const
{
    tdJSON_NODE& node = m_node[key];
    double v;
    if(!node.GetDouble(v))
        return false;
    out = (float)v;
    return true;
}


bool ConfigManager::ReadSection::value(const char* key, bool def) const
{
    tdJSON_NODE& node = m_node[key];
    node.GetBool(def);
    return def;
}

double ConfigManager::ReadSection::value(const char* key, double def) const
{
    tdJSON_NODE& node = m_node[key];
    node.GetDouble(def);
    return def;
}

int64_t ConfigManager::ReadSection::value(const char* key, int64_t def) const
{
    tdJSON_NODE& node = m_node[key];
    node.GetInt64(def);
    return def;
}

const std::string_view ConfigManager::ReadSection::value(const char* key, const char* def) const
{
    tdJSON_NODE& node = m_node[key];
    if(node.isString())
    {
        size_t len;
        node.GetString(def, len);
        return std::string_view(def, len);
    }
    return std::string_view(def);
}

bool ConfigManager::ReadSection::contains(const char* key) const
{
    return !m_node[key].isNULL();
}

const ConfigManager::ReadFields_proxy ConfigManager::ReadSection::peek() const
{
    return ConfigManager::ReadFields_proxy(m_mng, m_node, (uint32_t)m_node.Size());
}
ConfigManager::ReadSection::~ReadSection()
{
    m_mng.dec_section(m_isRoot);
}

ConfigManager::ReadSection::ReadSection(ConfigManagerImpl& mng, tdJSON_NODE& node, bool isRoot) : m_mng(mng), m_node(node), m_isRoot(isRoot)
{
    m_mng.add_section();
}


const ConfigManager::ReadFields ConfigManager::ReadFields_iterator::operator*() const
{ 
    return ConfigManager::ReadFields(m_mng, m_node[m_index]);
};



void ConfigManager::EditFields::set_int64(int64_t value)
{
    tdJSON_NODE& node = m_parent_node[m_key.data()];
    if(node.isNULL())
        m_json.InsertInt64(m_parent_node, value, m_key.data(), (uint8_t)m_key.size());
    else
        m_json.UpdateInt64(node, value);
}

void ConfigManager::EditFields::operator=(unsigned int value){ set_int64(value); }
void ConfigManager::EditFields::operator=(int value) { set_int64(value); }
void ConfigManager::EditFields::operator=(int64_t value) { set_int64(value); }
void ConfigManager::EditFields::operator=(double value)
{
    tdJSON_NODE& node = m_parent_node[m_key.data()];
    if(node.isNULL())
        m_json.InsertDouble(m_parent_node, value, m_key.data(), (uint8_t)m_key.size());
    else
        m_json.UpdateDouble(node, value);
}
void ConfigManager::EditFields::operator=(bool value)
{
    tdJSON_NODE& node = m_parent_node[m_key.data()];
    if(node.isNULL())
        m_json.InsertBool(m_parent_node, value, m_key.data(), (uint8_t)m_key.size());
    else
        m_json.UpdateBool(node, value);
}
void ConfigManager::EditFields::operator=(const std::string_view value)
{
    tdJSON_NODE& node = m_parent_node[m_key.data()];
    if(node.isNULL())
        m_json.InsertString(m_parent_node, value.data(), (uint16_t)value.size(), m_key.data(), (uint8_t)m_key.size());
    else
        m_json.UpdateString(node, value.data(), (uint16_t)value.size());
}
void ConfigManager::EditFields::operator=(const char* value)
{
    tdJSON_NODE& node = m_parent_node[m_key.data()];
    uint16_t len = (uint16_t)strlen(value);
    if(node.isNULL())
        m_json.InsertString(m_parent_node, value, len, m_key.data(), (uint8_t)m_key.size());
    else
        m_json.UpdateString(node, value, len);
}



ConfigManager::EditArray::EditArray(tdJSON& json, tdJSON_NODE& node) : m_json(json), m_node(node){};
void ConfigManager::EditArray::ensure_int64(std::string_view key, int64_t def)
{
    tdJSON_NODE& test_node = m_node[key.data()];
    if(test_node.isNULL())
    {
        m_json.InsertInt64(m_node, def, key.data(), (uint8_t)key.size());
        return;
    }
    if(!test_node.isInt64())
        m_json.UpdateInt64(test_node, def);
}
void ConfigManager::EditArray::operator=(unsigned int value){ m_json.UpdateInt64(m_node, value); }
void ConfigManager::EditArray::operator=(int value)    { m_json.UpdateInt64(m_node, value); }
void ConfigManager::EditArray::operator=(int64_t value){ m_json.UpdateInt64(m_node, value); }
void ConfigManager::EditArray::operator=(double value) { m_json.UpdateDouble(m_node, value); }
void ConfigManager::EditArray::operator=(bool value)   { m_json.UpdateBool(m_node, value); }
void ConfigManager::EditArray::operator=(const std::string_view value){ m_json.UpdateString(m_node, value.data(), (uint16_t)value.size()); }
void ConfigManager::EditArray::operator=(const char* value){ m_json.UpdateString(m_node, value, (uint16_t)strlen(value)); }
ConfigManager::EditFields ConfigManager::EditArray::operator[](const std::string_view& key)
{
    m_json.UpdateObject(m_node);
    return EditFields(m_json, m_node, key);
}

void ConfigManager::EditArray::ensure(std::string_view key, bool def)
{
    tdJSON_NODE& test_node = m_node[key.data()];
    if(test_node.isNULL())
    {
        m_json.InsertBool(m_node, def, key.data(), (uint8_t)key.size());
        return;
    }
    if(!test_node.isBool())
        m_json.UpdateBool(test_node, def);
}
void ConfigManager::EditArray::ensure(std::string_view key, double def)
{
    tdJSON_NODE& test_node = m_node[key.data()];
    if(test_node.isNULL())
    {
        m_json.InsertDouble(m_node, def, key.data(), (uint8_t)key.size());
        return;
    }
    if(!test_node.isDouble())
        m_json.UpdateDouble(test_node, def);
}
void ConfigManager::EditArray::ensure(std::string_view key, int64_t def){ ensure_int64(key, def); }
void ConfigManager::EditArray::ensure(std::string_view key, unsigned int def){ ensure_int64(key, def); }
void ConfigManager::EditArray::ensure(std::string_view key, int def){ ensure_int64(key, def); }
void ConfigManager::EditArray::ensure(std::string_view key, const char* def)
{
    tdJSON_NODE& test_node = m_node[key.data()];
    if(test_node.isNULL())
    {
        m_json.InsertString(m_node, def, (uint16_t)strlen(def), key.data(), (uint8_t)key.size());
        return;
    }
    if(!test_node.isString())
        m_json.UpdateString(test_node, def, (uint16_t)strlen(def));
}



ConfigManager::EditSection::EditSection(ConfigManagerImpl& mng, tdJSON_NODE& node, bool isRoot) : ConfigManager::ReadSection(mng, node, isRoot) {}

bool ConfigManager::EditSection::ReName(std::string_view name, std::string_view newName)
{
    return m_mng.getUpdate().ReName(m_node, name.data(), newName.data());
}
void ConfigManager::EditSection::ensure(std::string_view key, bool def)
{
    if(!m_node[key.data()].isBool())
        (*this)[key] = def;
}
void ConfigManager::EditSection::ensure(std::string_view key, double def)
{
    tdJSON_NODE& node = m_node[key.data()];
    if(!node.isDouble() && !node.isInt64())
        (*this)[key] = def;
}
void ConfigManager::EditSection::ensure(std::string_view key, int64_t def)
{
    tdJSON_NODE& node = m_node[key.data()];
    if(!node.isDouble() && !node.isInt64())
        (*this)[key] = def;
}
void ConfigManager::EditSection::ensure(std::string_view key, unsigned int def)
{
    tdJSON_NODE& node = m_node[key.data()];
    if(!node.isDouble() && !node.isInt64())
        (*this)[key] = def;
}
void ConfigManager::EditSection::ensure(std::string_view key, int def)
{
    tdJSON_NODE& node = m_node[key.data()];
    if(!node.isDouble() && !node.isInt64())
        (*this)[key] = def;
}
void ConfigManager::EditSection::ensure(std::string_view key, const char* def)
{
    if(!m_node[key.data()].isString())
        (*this)[key] = def;
}
bool ConfigManager::EditSection::erase(std::string_view key)
{
    return m_mng.getUpdate().Delete(m_node, key.data(), (uint8_t)key.size());
}

ConfigManager::EditSection ConfigManager::EditSection::section(std::string_view name)
{
    tdJSON_NODE* node = &m_node[name.data()];
    if(!node->isObject() && !node->isArray())
        node = &m_mng.getUpdate().InsertObject(m_node, name.data(), (uint8_t)name.size());
    return EditSection(m_mng, *node, false);
}

ConfigManager::EditSection ConfigManager::EditSection::section(std::string_view name1, std::string_view name2)
{
    tdJSON_NODE* node1 = &m_node[name1.data()];
    if(!node1->isObject())
        node1 = &m_mng.getUpdate().InsertObject(m_node, name1.data(), (uint8_t)name1.size());
    tdJSON_NODE* node2 = &((*node1)[name2.data()]);
    if(!node2->isObject())
        node2 = &m_mng.getUpdate().InsertObject(*node1, name2.data(), (uint8_t)name2.size());
    return EditSection(m_mng, *node2, false);
}

ConfigManager::EditSection ConfigManager::EditSection::array(std::string_view name)
{
    tdJSON_NODE* node = &m_node[name.data()];
    if(!node->isArray())
        node = &m_mng.getUpdate().InsertArray(m_node, name.data(), (uint8_t)name.size());
    return EditSection(m_mng, *node, false);
}

ConfigManager::EditArray ConfigManager::EditSection::operator[](size_t arr_id)
{
    if(m_node.Size() <= arr_id)
    {
        size_t i = m_node.Size();
        for(; i <= arr_id; i++)
            m_mng.getUpdate().InsertObject(m_node);
    }
    return ConfigManager::EditArray(m_mng.getUpdate(), m_node[arr_id]);
}

ConfigManager::EditFields ConfigManager::EditSection::operator[](const std::string_view& key) { return ConfigManager::EditFields(m_mng.getUpdate(), m_node, key); }


ConfigManager::ReadSection ConfigManagerImpl::read()
{
    m_sec_mtx.lock();//unlock in release
    return ConfigManager::ReadSection(*this, m_json->GetRoot(), true);
}

ConfigManager::EditSection ConfigManagerImpl::edit()
{
    m_sec_mtx.lock();//unlock in release
    return ConfigManager::EditSection(*this, m_json->GetRoot(), true);
}


SDRPP_API ConfigManager* ConfigManager_Create()
{
    return new ConfigManagerImpl();
}


SDRPP_API void ConfigManager_Destroy(ConfigManager*& mng)
{
    if(!mng)
        return;
    delete (ConfigManagerImpl*)(mng);
    mng = NULL;
}

