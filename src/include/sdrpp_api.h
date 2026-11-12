#pragma once
#include <stdint.h>
#include <string>
#include <vector>
//libusb.h
//

#define IMGUI_API

#ifdef _WIN32
#ifdef SDRPP_IS_CORE
#define SDRPP_API __declspec(dllexport)
#else
#define SDRPP_API __declspec(dllimport)
#endif
#else
#ifdef SDRPP_IS_CORE
#define SDRPP_API __attribute__((visibility("default")))
#else
#define SDRPP_API
#endif
#endif


class ConfigManager
{
public:
    class ReadSection;
    class ReadFields;
    class EditSection;
    class EditFields;
    class EditArray;

    class ReadFields_proxy;
    class ReadFields_iterator;

    virtual void setFile(const char* fileName, bool full = false) = 0;
    virtual bool load() = 0;
    virtual bool save() = 0;
    virtual void autoSave(bool enable) = 0;
    virtual ConfigManager::ReadSection read() = 0;
    virtual ConfigManager::EditSection edit() = 0;

    // Terminal operation: rejects new access, drains the autosaver, and commits
    // the final dirty state. A failed shutdown may be retried, but the manager
    // remains frozen in between so only an external condition can unblock it.
    virtual bool shutdown() = 0;
    virtual bool clear() = 0;
};


SDRPP_API ConfigManager* ConfigManager_Create();
SDRPP_API void ConfigManager_Destroy(ConfigManager*& mng);


class ConfigManagerImpl;
class tdJSON;
class tdJSON_NODE;


class ConfigManager::ReadFields_iterator
{
    friend class ConfigManager::ReadFields_proxy;
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using pointer           = void;
    using reference         = ConfigManager::ReadFields;

    virtual const ConfigManager::ReadFields operator*() const;

    ReadFields_iterator& operator++(){ ++m_index; return *this; }
    ReadFields_iterator operator++(int){ ReadFields_iterator tmp = *this; ++m_index; return tmp; }
    bool operator != (const ConfigManager::ReadFields_iterator& other) const { return m_index != other.m_index; }
    bool operator == (const ConfigManager::ReadFields_iterator& other) const { return m_index == other.m_index; }
private:
    ConfigManager::ReadFields_iterator(ConfigManagerImpl& mng, const tdJSON_NODE& node, size_t index) : m_mng(mng), m_node(node), m_index(index) {}
    ConfigManagerImpl& m_mng;
    const tdJSON_NODE& m_node;
    size_t m_index;
};


class ConfigManager::ReadFields_proxy
{
    friend class ConfigManager::ReadSection;
    explicit ReadFields_proxy(ConfigManagerImpl& mng, const tdJSON_NODE& node, uint32_t size) : m_mng(mng), m_node(node), m_size(size) {}
public:
    ConfigManager::ReadFields_iterator begin() const { return ConfigManager::ReadFields_iterator(m_mng, m_node, 0); }
    ConfigManager::ReadFields_iterator end() const { return ConfigManager::ReadFields_iterator(m_mng, m_node, m_size); }
private:
    ConfigManagerImpl& m_mng;
    const tdJSON_NODE& m_node;
    uint32_t m_size;
};


class ConfigManager::ReadFields
{
    friend class ConfigManager::ReadSection;
    friend class ConfigManager::ReadFields_iterator;

    ConfigManagerImpl& m_mng;
    tdJSON_NODE& m_node;

    ReadFields(ConfigManagerImpl& mng, tdJSON_NODE& node);
public:
    virtual const std::string GetName() const;
    virtual bool isNULL() const;
    virtual bool isNumber() const;
    virtual bool isBool() const;
    virtual bool isString() const;
    virtual bool isSection() const;

    virtual ConfigManager::ReadSection toSection() const;
    virtual bool tryGet(std::string& out) const;
    virtual bool tryGet(int& out) const;
    virtual bool tryGet(unsigned int& out) const;
    virtual bool tryGet(int64_t& out) const;
    virtual bool tryGet(bool& out) const;
    virtual bool tryGet(double& out) const;
    virtual bool tryGet(float& out) const;

    virtual size_t Size() const;
    virtual const ConfigManager::ReadFields operator[](size_t index) const;
    virtual const ConfigManager::ReadFields operator[](const std::string_view& key) const;
};


class ConfigManager::ReadSection
{
    friend class ConfigManagerImpl;
    friend class ConfigManager::EditSection;
    friend class ConfigManager::ReadFields;

    tdJSON_NODE& m_node;
    ConfigManagerImpl& m_mng;
    bool m_isRoot;
    ReadSection(ConfigManagerImpl& mng, tdJSON_NODE& node, bool isRoot);
public:

    virtual bool tryGet(const char* key, std::string& out) const;
    virtual bool tryGet(const char* key, int& out) const;
    virtual bool tryGet(const char* key, unsigned int& out) const;
    virtual bool tryGet(const char* key, int64_t& out) const;
    virtual bool tryGet(const char* key, bool& out) const;
    virtual bool tryGet(const char* key, std::vector<std::string>& out) const;
    virtual bool tryGet(const char* key, double& out) const;
    virtual bool tryGet(const char* key, float& out) const;

    virtual bool value(const char* key, bool def) const;
    virtual double value(const char* key, double def) const;
    virtual int64_t value(const char* key, int64_t def) const;
    virtual const std::string_view value(const char* key, const char* def) const;
    virtual bool contains(const char* key) const;

    virtual const ReadSection section(std::string_view name) const;
    virtual const ReadSection section(std::string_view name1, std::string_view name2) const;
    virtual const ReadFields operator[](size_t index) const;
    virtual const ReadFields operator[](const std::string_view& key) const;

    virtual bool isArray() const;
    virtual bool isNULL() const;
    virtual size_t Size() const;

    virtual const ReadFields_proxy peek() const;
    virtual ~ReadSection();
};


class ConfigManager::EditFields
{
    friend class ConfigManager::EditSection;
    friend class EditArray;

    const std::string_view& m_key;
    tdJSON_NODE& m_parent_node;
    tdJSON& m_json;
    EditFields(tdJSON& json, tdJSON_NODE& node, const std::string_view& key):m_key(key), m_json(json), m_parent_node(node){ };
    void set_int64(int64_t value);
public:
    virtual void operator= (unsigned int value);
    virtual void operator= (int value);
    virtual void operator= (int64_t value);
    virtual void operator= (double value);
    virtual void operator= (bool value);
    virtual void operator= (const std::string_view value);
    virtual void operator= (const char* value);
};


class ConfigManager::EditArray
{
    friend class ConfigManager::EditSection;

    tdJSON_NODE& m_node;
    tdJSON& m_json;
    EditArray(tdJSON& json, tdJSON_NODE& node);
    void ensure_int64(std::string_view key, int64_t def);
public:
    virtual void operator=(unsigned int value);
    virtual void operator=(int value);
    virtual void operator=(int64_t value);
    virtual void operator=(double value);
    virtual void operator=(bool value);
    virtual void operator=(const std::string_view value);
    virtual void operator=(const char* value);
    virtual ConfigManager::EditFields operator[](const std::string_view& key);
    virtual void ensure(std::string_view key, bool def);
    virtual void ensure(std::string_view key, double def);
    virtual void ensure(std::string_view key, int64_t def);
    virtual void ensure(std::string_view key, unsigned int def);
    virtual void ensure(std::string_view key, int def);
    virtual void ensure(std::string_view key, const char* def);
};


class ConfigManager::EditSection : public ConfigManager::ReadSection
{
    friend class ConfigManagerImpl;

    EditSection(ConfigManagerImpl& mng, tdJSON_NODE& node, bool isRoot);
public:
    virtual void ensure(std::string_view key, bool def);
    virtual void ensure(std::string_view key, double def);
    virtual void ensure(std::string_view key, int64_t def);
    virtual void ensure(std::string_view key, unsigned int def);
    virtual void ensure(std::string_view key, int def);
    virtual void ensure(std::string_view key, const char* def);
    virtual bool erase(std::string_view key);

    virtual bool ReName(std::string_view name, std::string_view newName);

    virtual ConfigManager::EditSection array(std::string_view name);
    virtual ConfigManager::EditSection section(std::string_view name);
    virtual ConfigManager::EditSection section(std::string_view name1, std::string_view name2);
    virtual ConfigManager::EditArray  operator[](size_t arr_id);
    virtual ConfigManager::EditFields operator[](const std::string_view& key);
};


