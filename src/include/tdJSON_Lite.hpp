/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#pragma once

#include "tdMemLite.hpp"
#include <stdint.h>
#include <stdlib.h>
#include <iterator>

#define TD_JSON_PARSE_EXT
#define TD_JSON_SAVE_FILE


#ifdef TD_JSON_SAVE_FILE
  #include <stdio.h>
#endif

class tdJSON_items_iterator;

class tdJSON_NODE
{
    tdJSON_NODE() = default;
    tdJSON_NODE(const tdJSON_NODE&) = delete;
    tdJSON_NODE& operator=(const tdJSON_NODE&) = delete;
    friend class tdJSON;
    friend class tdJSON_items_proxy;
    friend class tdJSON_items_iterator;
    union
    {
        tdJSON_NODE** m_obj_array;
        char*   m_str;
        int64_t m_int64;
        double  m_double;
    }m_value;
    uint16_t m_valueLen;
    uint16_t m_max_valueLen;
    enum TYPE : uint8_t
    {
        NODE_NULL = 0, NODE_ARRAY, NODE_OBJECT, NODE_STRING, NODE_INT64, NODE_DOUBLE, NODE_BOOL
    }m_type;
    uint8_t m_nameLen;
    char    m_name[1];
    static tdJSON_NODE& GetNULL()
    {
        static tdJSON_NODE _null;// = {0};
        if(!_null.isNULL())
            memset(&_null, 0, sizeof(tdJSON_NODE));
        return _null;
    }
public:
    tdJSON_NODE& operator[](const char* name) const
    {
        if(!isObject())
            return GetNULL();
        size_t len = strlen(name);
        for(size_t i = 0; i < m_valueLen; i++)
        {
            if(m_value.m_obj_array[i]->m_nameLen == len && memcmp(name, m_value.m_obj_array[i]->m_name, len) == 0)
                return *m_value.m_obj_array[i];
        }
        return GetNULL();
    }
    tdJSON_NODE& operator[](size_t id) const
    {
        if(!isObject() && !isArray())
            return GetNULL();
        if(id >= m_valueLen)
            return GetNULL();
        return *m_value.m_obj_array[id];
    }
    bool isArray()  const { return m_type == TYPE::NODE_ARRAY;  }
    bool isObject() const { return m_type == TYPE::NODE_OBJECT; }
    bool isNULL()   const { return m_type == TYPE::NODE_NULL;   }
    bool isString() const { return m_type == TYPE::NODE_STRING; }
    bool isBool()   const { return m_type == TYPE::NODE_BOOL;   }
    bool isInt64()  const { return m_type == TYPE::NODE_INT64;  }
    bool isDouble() const { return m_type == TYPE::NODE_DOUBLE; }
    
    bool GetDouble(double& value) const
    {
        if(isDouble())
        {
            value = m_value.m_double;
            return true;
        }
        if(isInt64() || isBool())
        {
            value = (double)m_value.m_int64;
            return true;
        }
        if(isString())
        {
            char* endStr;
            value = strtod(m_value.m_str, &endStr);
            return true;
        }
        return false;
    }
    bool GetInt64(int64_t& value) const
    {
        if(isInt64() || isBool())
        {
            value = m_value.m_int64;
            return true;
        }
        double t;
        if(GetDouble(t))
        {
            value = (int64_t)t;
            return true;
        }
        return false;
    }
    bool GetBool(bool& value) const
    {
        int64_t t;
        if(!GetInt64(t))
            return false;
        value = (t != 0);
        return true;
    }
    bool GetString(const char*& str, size_t& len) const
    {
        if(!isString())
        {
            len = 0;
            str = "";
            return false;
        }
        str = m_value.m_str;
        len = m_valueLen;
        return true;
    }
    void GetName(const char*& str, size_t& len) const
    {
        str = m_name;
        len = m_nameLen;
    }
    size_t Size() const
    {
        if(isArray() || isObject())
            return m_valueLen;
        return 0;
    }
};


class tdJSON_items_iterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;

    tdJSON_items_iterator(const tdJSON_NODE& node, size_t index) : m_base(node), m_index(index) {}

    tdJSON_NODE& operator*() const { return *m_base.m_value.m_obj_array[m_index]; };

    tdJSON_items_iterator& operator++()
    {
        ++m_index;
        return *this;
    }

    tdJSON_items_iterator operator++(int)
    {
        tdJSON_items_iterator tmp = *this;
        ++m_index;
        return tmp;
    }

    bool operator != (const tdJSON_items_iterator& other) const { return m_index != other.m_index; }
    bool operator == (const tdJSON_items_iterator& other) const { return m_index == other.m_index; }
private:
    const tdJSON_NODE& m_base;
    size_t m_index;
};


class tdJSON_items_proxy
{
public:
    explicit tdJSON_items_proxy(const tdJSON_NODE& data) : m_data(data) {}
    tdJSON_items_iterator begin() const { return tdJSON_items_iterator(m_data, 0); }
    tdJSON_items_iterator end() const { return tdJSON_items_iterator(m_data, m_data.Size()); }
private:
    const tdJSON_NODE& m_data;
};


class tdJSON
{
public:
    struct ERR
    {
        const char* m_str;      // err message
        size_t      m_line;     // number line
        size_t      m_line_pos; // offset begin line
        size_t      m_pos;      // offset error char
    };

private:
    static const int JSON_MAX_LEVEL = 128;
    tdJSON_NODE m_root;
    tdMemLite   m_mem;
    ERR         m_err;

    struct json_parser
    {
        char   m_cur_char;
        char   m_next_char;

        size_t m_cur_index;
        size_t m_line;
        size_t m_line_pos;

        const char* m_contents;
        size_t      m_length;
        inline void next()
        {
            m_cur_char = m_next_char;
            if(m_cur_index+1 < m_length)
            {
                m_cur_index ++;
                m_line_pos  ++;
                m_next_char = m_contents[m_cur_index+1];
                if(m_cur_char == '\n')
                {
                    m_line ++;
                    m_line_pos = 0;
                }
                return;
            }
            if(m_cur_index < m_length)
            {
                m_cur_index ++;
                m_next_char = '\0';
            }
        }
        inline bool test(const char* str, size_t len)
        {
            if(m_cur_index + len > m_length)
                return false;
            return memcmp(str, &m_contents[m_cur_index], len) == 0;
        }
        inline bool isSpace()
        {
            return m_cur_char == ' ' || m_cur_char == '\r' || m_cur_char == '\n' || m_cur_char == '\t';
        }
        inline bool isLexNum()
        {
            if(m_cur_char >= '0' && m_cur_char <= '9')
                return true;
            return ( m_cur_char == '.' || m_cur_char == '-' || m_cur_char == '+' || m_cur_char == 'E' || m_cur_char == 'e');
        }
        inline void skip_space()
        {
#ifdef TD_JSON_PARSE_EXT
            while(true)
            {
                while(isSpace())
                    next();
                if(m_cur_char == '/')
                {
                    if(m_next_char == '/')
                    {// comment //
                        while(m_cur_char != '\0' && m_cur_char != '\n')
                            next();
                        next();// '\n'
                        continue;
                    }
                    if( m_next_char == '*')
                    {// comment /*
                        next();// '/'
                        next();// '*'
                        while(m_cur_char)
                        {
                            if(m_cur_char == '*' && m_next_char == '/')
                            {
                                next();// '*'
                                next();// '/'
                                break;
                            }
                            next();
                        }
                        continue;
                    }
                }
                break;
            }
#else
            while(isSpace())
                next();
#endif
        }
        inline void next_skip_space()
        {
            next();
            skip_space();
        }
        inline bool skip_name(char ch_end)
        {
            if(m_cur_char == ch_end)
            {
                next();
                while(m_cur_char != '\0' && m_cur_char != ch_end)
                {
                    if(m_cur_char == '\\')
                        next();
                    next();
                }
                bool ret = (m_cur_char != '\0');
                next();
                return ret;
            }
#ifdef TD_JSON_PARSE_EXT
            while(m_cur_char != '\0' && m_cur_char != ':' && m_cur_char != '=' && m_cur_char != ' ' && m_cur_char != '\t')
                next();
            return m_cur_char != '\0';
#else
            return false;
#endif
        }
        json_parser(const char* json, size_t jsonLen)
        {
            m_contents  = json;
            m_length    = jsonLen;
            m_cur_index = 0;
            m_cur_char  = m_contents[0];
            m_next_char = m_contents[1];
            if(jsonLen < 2)
                m_next_char = 0;
            m_line = 0;
            m_line_pos = 0;
        }
    };

    tdJSON_NODE* AllocNode(const char* name, uint8_t nameLen, tdJSON_NODE::TYPE type, const char* vStr = NULL, uint16_t vStrLen = 0)
    {
        tdJSON_NODE* node = (tdJSON_NODE*)m_mem.Alloc(sizeof(tdJSON_NODE)+nameLen);
        if(nameLen)
            memcpy(node->m_name, name, nameLen);
        node->m_name[nameLen] = 0;
        node->m_nameLen       = nameLen;
        node->m_type          = type;
        node->m_valueLen      = 0;
        node->m_max_valueLen  = 0;

        if(type == tdJSON_NODE::TYPE::NODE_ARRAY || type == tdJSON_NODE::TYPE::NODE_OBJECT)
        {
            node->m_max_valueLen = 4;
            node->m_value.m_obj_array = (tdJSON_NODE**)m_mem.Alloc(sizeof(tdJSON_NODE*)*node->m_max_valueLen);
        }
        if(type == tdJSON_NODE::TYPE::NODE_STRING)
        {
            node->m_value.m_str = (char*)m_mem.Alloc(vStrLen+1);
            node->m_valueLen = vStrLen;
            node->m_max_valueLen = vStrLen+1;
            memcpy(node->m_value.m_str, vStr, vStrLen);
            node->m_value.m_str[vStrLen] = 0;
        }
        return node;
    }
    bool err(json_parser& parser, const char* str)
    {
        m_err.m_str      = str;
        m_err.m_line     = parser.m_line;
        m_err.m_line_pos = parser.m_line_pos;
        m_err.m_pos      = parser.m_cur_index;
        return false;
    }
    bool ParseNum(json_parser& parser, int64_t& v1, double &v2, bool& isInt64)
    {
        bool isMinus = false;
        while (parser.m_cur_char == '-' || parser.m_cur_char == '+')
        {// '-' '-' '+' '-' ...
            if(parser.m_cur_char == '-')
                isMinus = !isMinus;
            parser.next_skip_space();
        }
        if(parser.m_cur_char == 'N' && parser.test("NaN", 3))
        {
            isInt64 = false;
            *((uint64_t*)&v2) = 0x7ff8000000000000;
            parser.next();
            parser.next();
            parser.next_skip_space();
            return true;
        }
        if(parser.m_cur_char == 'I' && parser.test("Infinity", 8))
        {
            isInt64 = false;
            *((uint64_t*)&v2) = 0x7ff0000000000000;
            if(isMinus)
                *((uint64_t*)&v2) = 0xfff0000000000000;
            for(size_t i = 0 ; i < 7; i++)
                parser.next();
            parser.next_skip_space();
            return true;
        }
        if(parser.m_cur_char == '0' && parser.m_next_char == 'x')
        {//hex
            parser.next();
            parser.next();
            int64_t v = 0;
            size_t i = 0;
            size_t start_num = parser.m_cur_index;
            while(true)
            {
                char ch = parser.m_cur_char;
                if(ch >= '0' && ch <= '9')
                {
                    ch -= '0';
                }else if(ch >= 'a' && ch <= 'f')
                {
                    ch -= 'a' - 10;
                }else if(ch >= 'A' && ch <= 'F')
                {
                    ch -= 'A' - 10;
                }else{
                    break;
                }
                parser.next();
                v = (v << 4) | ch;
                i++;
            }
            if(i > 16 || start_num == parser.m_cur_index)
                return false;
            parser.skip_space();
            if(isMinus)
                v = -v;
            isInt64 = true;
            v1 = v;
            return true;
        }
        size_t start_num = parser.m_cur_index;
        size_t count_num = 0;
        while(parser.isLexNum())
        {
            char ch = parser.m_cur_char;
            if(ch >= '0' && ch <= '9')
                count_num++;
            parser.next();
        }
        if(!count_num)
            return false;
        size_t len_num = parser.m_cur_index - start_num;
        parser.skip_space();
        if(count_num == len_num)
        {//int64
            int64_t v = 0;
            const char* str = &parser.m_contents[start_num];
            for(size_t i = 0; i < count_num;i ++)
                v = v*10 + (str[i] - '0');
            if(v < 0 || count_num > 19)
                return false;
            if(isMinus)
                v = -v;
            isInt64 = true;
            v1 = v;
            return true;
        }
        //double
        char* endStr;
        double result = strtod(&parser.m_contents[start_num], &endStr);
        if(&parser.m_contents[start_num + len_num] != endStr)
            return false;
        if(isMinus)
            result = -result;
        isInt64 = false;
        v2 = result;
        return true;
    }
    void Clear()
    {
        m_mem.Reset(false);
        memset(&m_root, 0, sizeof(m_root));
        m_root.m_type = tdJSON_NODE::TYPE::NODE_OBJECT;
        m_root.m_max_valueLen = 4;
        m_root.m_value.m_obj_array = (tdJSON_NODE**)m_mem.Alloc(sizeof(tdJSON_NODE*)*m_root.m_max_valueLen);
        m_err.m_str      = NULL;
        m_err.m_line     = 0;
        m_err.m_line_pos = 0;
        m_err.m_pos      = 0;
    }
    bool Parse(const char* json, size_t jsonLen)
    {
        Clear();
        json_parser parser(json, jsonLen);
        parser.skip_space();
        if(parser.m_cur_char == '{')
        {
        }else if(parser.m_cur_char == '[')
        {
            m_root.m_type = tdJSON_NODE::TYPE::NODE_ARRAY;
        }else
            return err(parser, "incorrect char");
        parser.next_skip_space();

        tdJSON_NODE* lvl_noda[JSON_MAX_LEVEL+1];
        memset(lvl_noda, 0, sizeof(lvl_noda));
        int level = 0;
        lvl_noda[0] = &m_root;
        while(parser.m_cur_index < parser.m_length && level >= 0)
        {
            if(level >= JSON_MAX_LEVEL)
                return err(parser, "max level");
            if(parser.m_cur_char == '\0')
                break;
            if(parser.m_cur_char == ']' || parser.m_cur_char == '}')
            {
                if(parser.m_cur_char == '}')
                {
                    if(!lvl_noda[level]->isObject())
                        return err(parser, "incorrect close object");
                }else{
                    if(!lvl_noda[level]->isArray())
                        return err(parser, "incorrect close array");
                }
                if(--level < 0)
                    break;
                parser.next_skip_space();
#ifdef TD_JSON_PARSE_EXT
                if(parser.m_cur_char != ',' && parser.m_cur_char != ';')
                    continue;
#else
                if(parser.m_cur_char != ',')
                    continue;
#endif
                parser.next_skip_space(); //есть еще на этом уровне
                if(lvl_noda[level]->isObject())
                    continue;
            }
            size_t      name_len = 0;
            const char* name     = NULL;

            if(lvl_noda[level]->isObject())
            {//выделение имени + разделитель
#ifdef TD_JSON_PARSE_EXT
                if(parser.m_cur_char == ',' || parser.m_cur_char == ';')
                {//лишний разделитель (нет vale=key)
                    parser.next_skip_space();
                    continue;
                }
#endif
                size_t start_name = parser.m_cur_index;
                size_t stop_name;
                if(parser.m_cur_char == '\"' || parser.m_cur_char == '\'')
                {
                    start_name ++;
                    if(!parser.skip_name(parser.m_cur_char))
                        return err(parser, "incorrect name");
                    stop_name = parser.m_cur_index - 1;
                }else{
#ifdef TD_JSON_PARSE_EXT
                    if(!parser.skip_name('\t'))
                        return err(parser, "incorrect name");
                    stop_name = parser.m_cur_index;
#else
                    return err(parser, "incorrect name");
#endif
                }
                name      = &parser.m_contents[start_name];
                name_len  = stop_name - start_name;
                if(/*!name_len ||*/ name_len > 0xFF)
                    return err(parser, "incorrect name");
                parser.skip_space();
#ifdef TD_JSON_PARSE_EXT
                if(parser.m_cur_char != ':' && parser.m_cur_char != '=')
                    return err(parser, "no ':'");
#else
                if(parser.m_cur_char != ':')
                    return err(parser, "no ':'");
#endif
                parser.next_skip_space();
            }
            //======переход на левел
            if(parser.m_cur_char == '[')
            {//массив
                 tdJSON_NODE& noda = InsertArray(*lvl_noda[level], name, uint8_t(name_len));
                 lvl_noda[++level] = &noda;
                 parser.next_skip_space();
                 continue;
            }
            if(parser.m_cur_char == '{')
            {//объект
                tdJSON_NODE& noda = InsertObject(*lvl_noda[level], name, uint8_t(name_len));
                lvl_noda[++level] = &noda;
                parser.next_skip_space();
                continue;
            }

            int64_t v_int;
            double  v_d;
            bool isInt64 = false;
            //======выделение значения
            if(parser.m_cur_char == 't' || parser.m_cur_char == 'f' || parser.m_cur_char == 'n')
            {// 'true' 'false' 'null'
                bool skip4 = false;
                if(parser.test("true", 4) || parser.test("false", 5))
                {
                    InsertBool(*lvl_noda[level], parser.m_cur_char == 't', name, uint8_t(name_len));
                    if(parser.m_cur_char == 'f')
                        parser.next();
                    skip4 = true;
                }else if(parser.test("null", 4))
                {
                    InsertNULL(*lvl_noda[level], name, uint8_t(name_len));
                    skip4 = true;
                }
                if(skip4)
                {
                    parser.next();
                    parser.next();
                    parser.next();
                    parser.next_skip_space();
                }
            }else if(parser.m_cur_char == '\"' || parser.m_cur_char == '\'')
            {// string
                size_t start_str = parser.m_cur_index+1;
                if(!parser.skip_name(parser.m_cur_char))
                    return err(parser, "incorrect string");
                size_t stop_str = parser.m_cur_index - 1;
                if(stop_str - start_str > 0x8000)
                    return err(parser, "big string > 32kb");
                InsertString(*lvl_noda[level], &parser.m_contents[start_str], uint16_t(stop_str - start_str), name, uint8_t(name_len));
                parser.skip_space();
            }else if(ParseNum(parser, v_int, v_d, isInt64))
            {// number
                if(isInt64)
                {// int64
                    InsertInt64(*lvl_noda[level], v_int, name, uint8_t(name_len));
                }else{// double
                    InsertDouble(*lvl_noda[level], v_d, name, uint8_t(name_len));
                }
            }else{
                return err(parser, "incorrect char");
            }
            if(parser.m_cur_char == ']' || parser.m_cur_char == '}')// закрытие делается выше
                continue;
            if(parser.m_cur_char == ',')
            {// есть еще на этом уровне
                parser.next_skip_space();
                continue;
            }
#ifdef TD_JSON_PARSE_EXT
            if(parser.m_cur_char == ';')
            {// есть еще на этом уровне
                parser.next_skip_space();
                continue;
            }
#endif
            return err(parser, "incorrect char");// ошибка, нет разделителя
        }
        if(level >= 0)
            return err(parser, "incorrect level");
        if((m_root.isArray() && parser.m_cur_char != ']') || (m_root.isObject() && parser.m_cur_char != '}'))
            return err(parser, "incorrect close object");
        parser.next_skip_space();
        if(parser.m_length != parser.m_cur_index)
            return err(parser, "incorrect char");
        return true;
    }

    tdJSON_NODE& Insert(tdJSON_NODE& node, const char* name, uint8_t nameLen, tdJSON_NODE::TYPE type, const char* vStr = NULL, uint16_t vStrLen = 0)
    {
        if(!node.isObject() && !node.isArray())
            return tdJSON_NODE::GetNULL();
        if(node.isArray())
        {
            nameLen = 0;
        }else{
            if(!name)
                return tdJSON_NODE::GetNULL();
        }
        if(node.m_valueLen == node.m_max_valueLen)
        {//resize
            if(node.m_max_valueLen > 128)
            {
                node.m_max_valueLen += 128;
            }else{
                node.m_max_valueLen *= 2;
            }
            tdJSON_NODE** new_obj_array = (tdJSON_NODE**)m_mem.Alloc(sizeof(tdJSON_NODE*)*node.m_max_valueLen);
            memcpy(new_obj_array, node.m_value.m_obj_array, node.m_valueLen*sizeof(tdJSON_NODE*));
            m_mem.Free(node.m_value.m_obj_array, node.m_valueLen*sizeof(tdJSON_NODE*));
            node.m_value.m_obj_array = new_obj_array;
        }
        tdJSON_NODE* ret = AllocNode(name, nameLen, type, vStr, vStrLen);
        node.m_value.m_obj_array[node.m_valueLen++] = ret;
        return *ret;
    }

    bool ClearRAW(tdJSON_NODE& node)
    {
        if(&node == &node.GetNULL())
            return false;
        if(node.isString())
        {
            m_mem.Free(node.m_value.m_str, node.m_max_valueLen);
            return true;
        }
        if(node.isObject() || node.isArray())
        {
            for(size_t i = 0; i < node.m_valueLen; i++)
            {
                tdJSON_NODE* del_node = node.m_value.m_obj_array[i];
                ClearRAW(*del_node);
                m_mem.Free(del_node, sizeof(tdJSON_NODE) + del_node->m_nameLen);
            }
            m_mem.Free(node.m_value.m_obj_array, sizeof(tdJSON_NODE*)*node.m_max_valueLen);
        }
        return true;
    }
public:
    tdJSON():m_mem(0x1000) { Clear(); }
#ifdef _TD_MEM_H_
    tdJSON(tdMemMaster* master):m_mem(master) { Clear(); }
#endif
    ~tdJSON() { }
    
    bool Init(const char* json, size_t jsonLen = 0)
    {
        if(!jsonLen)
            jsonLen = strlen(json);
        return Parse(json, jsonLen);
    }
    tdJSON_NODE& GetRoot()
    {
        return m_root;
    }
    tdJSON_NODE& InsertArray(tdJSON_NODE& node, const char* name = NULL, uint8_t nameLen = 0)
    {
        return Insert(node, name, nameLen, tdJSON_NODE::TYPE::NODE_ARRAY);
    }
    tdJSON_NODE& InsertObject(tdJSON_NODE& node, const char* name = NULL, uint8_t nameLen = 0)
    {
        return Insert(node, name, nameLen, tdJSON_NODE::TYPE::NODE_OBJECT);
    }
    bool InsertInt64(tdJSON_NODE& node, int64_t value, const char* name = NULL, uint8_t nameLen = 0)
    {
         tdJSON_NODE& ret = Insert(node, name, nameLen, tdJSON_NODE::TYPE::NODE_INT64);
         if(!ret.isInt64())
             return false;
         ret.m_value.m_int64 = value;
         return true;
    }
    bool InsertDouble(tdJSON_NODE& node, double value, const char* name = NULL, uint8_t nameLen = 0)
    {
        tdJSON_NODE& ret = Insert(node, name, nameLen, tdJSON_NODE::TYPE::NODE_DOUBLE);
        if(!ret.isDouble())
            return false;
        ret.m_value.m_double = value;
        return true;
    }
    bool InsertString(tdJSON_NODE& node, const char* value, uint16_t len, const char* name = NULL, uint8_t nameLen = 0)
    {
        return Insert(node, name, nameLen, tdJSON_NODE::TYPE::NODE_STRING, value, len).isString();
    }
    bool InsertBool(tdJSON_NODE& node, bool value, const char* name = NULL, uint8_t nameLen = 0)
    {
        tdJSON_NODE& ret = Insert(node, name, nameLen, tdJSON_NODE::TYPE::NODE_BOOL);
        if(!ret.isBool())
            return false;
        ret.m_value.m_int64 = value?1:0;
        return true;
    }
    bool InsertNULL(tdJSON_NODE& node, const char* name = NULL, uint8_t nameLen = 0)
    {
        return Insert(node, name, nameLen, tdJSON_NODE::TYPE::NODE_NULL).isNULL();
    }

    bool UpdateBool(tdJSON_NODE& node, bool value)
    {
        if(!ClearRAW(node))
            return false;
        node.m_type = tdJSON_NODE::TYPE::NODE_BOOL;
        node.m_value.m_int64 = value?1:0;
        return true;
    }
    bool UpdateInt64(tdJSON_NODE& node, int64_t value)
    {
        if(!ClearRAW(node))
            return false;
        node.m_type = tdJSON_NODE::TYPE::NODE_INT64;
        node.m_value.m_int64 = value;
        return true;
    }
    bool UpdateDouble(tdJSON_NODE& node, double value)
    {
        if(!ClearRAW(node))
            return false;
        node.m_type = tdJSON_NODE::TYPE::NODE_DOUBLE;
        node.m_value.m_double = value;
        return true;
    }
    bool UpdateString(tdJSON_NODE& node, const char* value, uint16_t len)
    {
        if(!ClearRAW(node))
            return false;
        node.m_type = tdJSON_NODE::TYPE::NODE_STRING;
        node.m_value.m_str = (char*)m_mem.Alloc(len+1);
        node.m_valueLen = len;
        node.m_max_valueLen = len+1;
        memcpy(node.m_value.m_str, value, len);
        node.m_value.m_str[len] = 0;
        return true;
    }
    bool UpdateObject(tdJSON_NODE& node)
    {
        if(node.isObject())
            return true;
        if(!ClearRAW(node))
            return false;
        node.m_valueLen = 0;
        node.m_max_valueLen = 4;
        node.m_value.m_obj_array = (tdJSON_NODE**)m_mem.Alloc(sizeof(tdJSON_NODE*)*node.m_max_valueLen);
        node.m_type = tdJSON_NODE::TYPE::NODE_OBJECT;
        return true;
    }
    bool UpdateArray(tdJSON_NODE& node)
    {
        if(node.isArray())
            return true;
        if(!ClearRAW(node))
            return false;
        node.m_valueLen = 0;
        node.m_max_valueLen = 4;
        node.m_value.m_obj_array = (tdJSON_NODE**)m_mem.Alloc(sizeof(tdJSON_NODE*)*node.m_max_valueLen);
        node.m_type = tdJSON_NODE::TYPE::NODE_ARRAY;
        return true;
    }
    bool ReName(tdJSON_NODE& node, const char* name, const char* newName)
    {
        if(!node.isObject())
            return false;
        size_t index = 0;
        size_t len = strlen(name);
        for(; index < node.m_valueLen; index++)
        {
            if(node.m_value.m_obj_array[index]->m_nameLen == len && memcmp(name, node.m_value.m_obj_array[index]->m_name, len) == 0)
                break;
        }
        if(index == node.m_valueLen)
            return false;
        size_t new_len = strlen(newName);
        tdJSON_NODE* new_node = (tdJSON_NODE*)m_mem.Alloc(sizeof(tdJSON_NODE)+new_len);
        memcpy(new_node, node.m_value.m_obj_array[index], sizeof(tdJSON_NODE));
        new_node->m_nameLen = (uint8_t)new_len;
        memcpy(new_node->m_name, newName, new_len+1);
        m_mem.Free(node.m_value.m_obj_array[index], sizeof(tdJSON_NODE) + len);
        node.m_value.m_obj_array[index] = new_node;
        return true;
    }
    bool Delete(tdJSON_NODE& node, const char* name, uint8_t nameLen = 0)
    {
        if(!node.isObject())
            return false;
        if(!nameLen)
            nameLen = (uint8_t)strlen(name);
        size_t index = 0;
        for(; index < node.m_valueLen; index++)
        {
            if(node.m_value.m_obj_array[index]->m_nameLen == nameLen && memcmp(name, node.m_value.m_obj_array[index]->m_name, nameLen) == 0)
                break;
        }
        if(index == node.m_valueLen)
            return false;
        ClearRAW(*node.m_value.m_obj_array[index]);
        m_mem.Free(node.m_value.m_obj_array[index], sizeof(tdJSON_NODE) + node.m_value.m_obj_array[index]->m_nameLen);
        for(; index < node.m_valueLen - 1; index++)
            node.m_value.m_obj_array[index] = node.m_value.m_obj_array[index + 1];
        node.m_valueLen--;
        return true;
    }
#ifdef TD_JSON_SAVE_FILE
    bool Save(const char* fileName, bool format) const
    {
        FILE* f = fopen(fileName, "wb+");
        if(!f)
            return false;
        int lvl_index[JSON_MAX_LEVEL+1];
        const tdJSON_NODE* lvl_node[JSON_MAX_LEVEL+1];
        char spase[JSON_MAX_LEVEL*2];
        memset(spase, ' ', sizeof(spase));
        fwrite(m_root.isObject() ? "{" : "[", 1, 1, f);
        lvl_node[0]  = &m_root;
        lvl_index[0] = 0;
        int level = 0;
        while(level >= 0)
        {
            int index = lvl_index[level];
            const tdJSON_NODE* node = lvl_node[level];
            while(index < node->m_valueLen)
            {
                if(index)
                    fwrite(",", 1, 1, f);
                if(format)
                    fwrite("\r\n  ", 1, 4, f);
                tdJSON_NODE* obj = node->m_value.m_obj_array[index++];
                if(node->isObject())
                {//name
                    if(format && level)
                        fwrite(spase, 1, level*2, f);
                    fwrite("\"", 1, 1, f);
                    fwrite(obj->m_name, 1, obj->m_nameLen, f);
                    fwrite("\":", 1, 2, f);
                    if(format)
                        fwrite(" ", 1, 1, f);
                }
                if(obj->isObject() || obj->isArray())
                {
                    if(/*node->isObject() ||*/ node->isArray())
                    {
                        if(format)
                            fwrite(spase, 1, level*2, f);
                    }
                    if(obj->isObject())
                    {
                        fwrite("{", 1, 1, f);
                    }else{
                        fwrite("[", 1, 1, f);
                    }
                    if(level == JSON_MAX_LEVEL)
                    {
                        fclose(f);
                        return false;
                    }
                    lvl_index[level] = index;
                    level++;
                    lvl_index[level] = index = 0;
                    lvl_node[level]  = node  = obj;
                    break;
                }
                if(node->isArray())
                {
                    if(format && level)
                        fwrite(spase, 1, level*2, f);
                }
                if(obj->isBool())
                {
                    if(obj->m_value.m_int64 != 0)
                    {
                        fwrite("true", 1, 4, f);
                    }else{
                        fwrite("false", 1, 5, f);
                    }
                    continue;
                }
                if(obj->isDouble())
                {
                    if(obj->m_value.m_int64 == 0x7ff8000000000000)
                    {
                        fwrite("NaN", 1, 3, f);
                        continue;
                    }
                    if(obj->m_value.m_int64 == 0x7ff0000000000000)
                    {
                        fwrite("Infinity", 1, 8, f);
                        continue;
                    }
                    if(obj->m_value.m_int64 == 0xfff0000000000000)
                    {
                        fwrite("-Infinity", 1, 9, f);
                        continue;
                    }
                    if(obj->m_value.m_double == (int64_t)obj->m_value.m_double)
                    {
                        fprintf(f, "%.1f", obj->m_value.m_double);
                    }else{
                        fprintf(f, "%g", obj->m_value.m_double);
                    }
                    continue;
                }
                if(obj->isInt64())
                {
                    fprintf(f, "%lld", obj->m_value.m_int64);
                    continue;
                }
                if(obj->isNULL())
                {
                    fwrite("null", 1, 4, f);
                    continue;
                }
                if(obj->isString())
                {
                    fwrite("\"", 1, 1, f);
                    fwrite(obj->m_value.m_str, 1, obj->m_valueLen, f);
                    fwrite("\"", 1, 1, f);
                }
            }
            if(index == node->m_valueLen)
            {
                if(index && format)
                {
                    fwrite("\r\n", 1, 2, f);
                    if(level)
                        fwrite(spase, 1, (level)*2, f);
                }
                fwrite(node->isObject()?"}":"]", 1, 1, f);
                level--;
                continue;
            }
        }
        fclose(f);
        return true;
    }
#endif
    const tdJSON_NODE& operator[](const char* name)
    {
        return m_root[name];
    }
    const tdJSON_NODE& operator[](size_t id)
    {
        return m_root[id];
    }
    const tdJSON_NODE& find(const char* path)
    {
        char name[256];
        const tdJSON_NODE* node = &m_root;
        size_t pos = 0;
        while(path[pos])
        {
            while(path[pos] && path[pos] == ' ')
                pos++;
            size_t i = 0;
            while(path[pos] && path[pos] != ' ' && path[pos] != '.'&& path[pos] != '[' && i < 250)
                 name[i++] = path[pos++];
            name[i] = 0;
            node = &(*node)[name];
            if(node->isNULL())
                break;
            while(path[pos] && path[pos] == ' ')
                pos++;
            if(path[pos] == 0)
                break;
            if(path[pos] == '[')
            {
                pos ++;
                while(path[pos] && path[pos] == ' ')
                    pos++;
                size_t num = 0;
                while(path[pos])
                {
                    char ch = path[pos];
                    if(ch < '0' || ch > '9')
                        break;
                    pos++;
                    num = num*10 + ch - '0'; 
                }
                node = &(*node)[num];
                if(node->isNULL())
                    break;
                while(path[pos] && path[pos] == ' ')
                    pos++;
                if(path[pos] != ']')
                    return node->GetNULL();
                pos++;
                while(path[pos] && path[pos] == ' ')
                    pos++;
                if(path[pos] == 0)
                    break;
            }
            if(path[pos] != '.')
                return node->GetNULL();
            pos++;
        }
        return *node;
    }

    const ERR* GetError() const
    {
        if(!m_err.m_str)
            return NULL;
        return &m_err;
    }
};

//1. Восемь стандартных сокращений: \", \\, \/, \b, \f, \n, \r, \t
//2. Универсальная группа для управления: \u00xx(для всех тридцати двух символов с кодами от 0x00 до 0x1F).



// \b	Backspace(Возврат на один символ назад)	0x08
// \t	Horizontal Tab(Горизонтальная табуляция)	0x09
// \n	Line Feed(Перенос строки / Новая строка)	0x0A
// \f	Form Feed(Прогон страницы / Конец страницы)	0x0C
// \r	Carriage Return(Возврат каретки в начало строки) 0x0D
