/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#pragma once

#include "tdMemLite.hpp"
#include <stdio.h>

#define TD_XML_PARSE_ATTRIBYTES
#define TD_XML_SAVE_FILE
#define TD_XML_PARSE_STRING

/*
 
  bool  tdXML::Init(const char* xml, size_t xmlLen = 0)
  const tdXML::NODE* find(const tdXML::NODE* node, const char* name, size_t nameLen = 0, size_t index = 0)
  const tdXML::NODE* find(const char* path)
  bool  tdXML::Save(const char* fileName, bool format)
*/

class tdXML
{
    static const int XML_MAX_LEVEL = 128;
public:
#ifdef TD_XML_PARSE_ATTRIBYTES
    struct NODE_ATTRIBYTES
    {
        const char* m_name;
        const char* m_value;
        size_t      m_name_len;
        size_t      m_value_len;
        char        m_separator;
    };
#endif
    struct NODE
    {
        const char* m_name;
#ifdef TD_XML_PARSE_ATTRIBYTES
        const NODE_ATTRIBYTES* m_attributes;
        size_t      m_attributes_count;
#else
        const char* m_attributes_data;
        size_t      m_attributes_len;
#endif
        size_t      m_name_len;

        enum
        {
            NODE_EMPTY,  // пустой узел (возможно данные в атрибутах)
            NODE_CHILDS, // нода содержит узлы
            NODE_DATA,   // нода содержит данные
        }m_type;
        union
        {
            const char*  m_data;
            tdXML::NODE* m_childs;
        };
        union
        {
            size_t m_childs_count;
            size_t m_data_size;
        };
    };
private:
    struct xml_parser
    {
        char   m_cur_char;
        size_t m_cur_index;
        char   m_next_char;

        const char* m_contents;
        size_t      m_length;
        void next()
        {
            if(m_cur_char != '\0' && m_cur_index+1 < m_length)
            {
                m_cur_index ++;
                m_cur_char  = m_next_char;
                m_next_char = m_contents[m_cur_index+1];
            }else{
                m_next_char = m_cur_char = '\0';
            }
        }
        void skip_space()
        {
            while(m_cur_char == ' ' || m_cur_char == '\r' || m_cur_char == '\n' || m_cur_char == '\t')
                next();
        }
        void skip_name()
        {
            while(m_cur_char != '\0' && m_cur_char != ' ' && m_cur_char != '/' && m_cur_char != '>')
                next();
        }
        void find_end_block()
        {
            while(m_cur_char != '\0' && m_cur_char != '>' && !(m_cur_char == '/' && m_next_char == '>'))
            {
                if(m_cur_char == '\'' || m_cur_char == '\"')
                {
                    char end = m_cur_char;
                    next();
                    while(m_cur_char != '\0' && m_cur_char != end )
                          next();
                }
                next();
            }
        }
        xml_parser(const char* xml, size_t xmlLen)
        {
            m_contents  = xml;
            m_length    = xmlLen;
            m_cur_index = 0;
            m_cur_char  = m_contents[0];
            m_next_char = m_contents[1];
            if(xmlLen < 2)
                m_next_char = 0;
        }
    };

    struct NODE_LEVEL
    {
        tdXML::NODE* m_nodes;
        size_t       m_count;
        size_t       m_count_max;
    };

#ifdef TD_XML_PARSE_STRING
    size_t ParseString(const char* str, size_t len)
    {
        char* str_ = (char*)str;
        size_t new_len = 0;
        for(size_t i = 0; i < len; i++)
        {
            if(str[i] == '&')
            {
                if(memcmp(&str[i], "&lt;", 4) == 0) // '<'
                {
                    str_[new_len++] = '<';
                    i += 3;
                    continue;
                }
                if(memcmp(&str[i], "&gt;", 4) == 0) // '>'
                {
                    str_[new_len++] = '>';
                    i += 3;
                    continue;
                }
                if(memcmp(&str[i], "&quot;", 6) == 0) // '\"'
                {
                    str_[new_len++] = '\"';
                    i += 5;
                    continue;
                }
                if(memcmp(&str[i], "&apos;", 6) == 0) // '\''
                {
                    str_[new_len++] = '\'';
                    i += 5;
                    continue;
                }
                if(memcmp(&str[i], "&amp;", 5) == 0) // '&'
                {
                    str_[new_len++] = '\'';
                    i += 4;
                    continue;
                }
            }
            str_[new_len++] = str[i];
        }
        str_[new_len] = 0;
        return new_len;
    }
#endif

#ifdef TD_XML_PARSE_ATTRIBYTES
    bool ParseAttribute(const char* text, size_t textLen, tdXML::NODE* node)
    {
        xml_parser parser(text, textLen);
        node->m_attributes = NULL;
        node->m_attributes_count = 0;
        parser.skip_space();
        while(parser.m_cur_char != 0)
        {
            tdXML::NODE_ATTRIBYTES* new_attrib = (tdXML::NODE_ATTRIBYTES*)m_mem.Alloc(sizeof(tdXML::NODE_ATTRIBYTES)*(node->m_attributes_count+1));
            if(node->m_attributes_count)
            {
                memcpy(new_attrib, node->m_attributes, sizeof(tdXML::NODE_ATTRIBYTES) * node->m_attributes_count);
                m_mem.Free((tdXML::NODE_ATTRIBYTES*)node->m_attributes, sizeof(tdXML::NODE_ATTRIBYTES) * node->m_attributes_count);
            }
            node->m_attributes = new_attrib;
            new_attrib += node->m_attributes_count;
            node->m_attributes_count++;
            new_attrib->m_name = &parser.m_contents[parser.m_cur_index];
            new_attrib->m_name_len = parser.m_cur_index;
            while(parser.m_cur_char != '\0' && parser.m_cur_char != ' ' && parser.m_cur_char != '=' && parser.m_cur_char != '\t')
                parser.next();
            new_attrib->m_name_len = parser.m_cur_index - new_attrib->m_name_len;
            parser.skip_space();
            if(parser.m_cur_char != '=')
                return false;
            parser.next();
            parser.skip_space();
            if(parser.m_cur_char != '\'' && parser.m_cur_char != '\"')
                return false;
            new_attrib->m_separator = parser.m_cur_char;
            parser.next();
            new_attrib->m_value = &parser.m_contents[parser.m_cur_index];
            new_attrib->m_value_len = parser.m_cur_index;
            while(parser.m_cur_char != '\0' && parser.m_cur_char != new_attrib->m_separator)
                parser.next();
            if(parser.m_cur_char != new_attrib->m_separator)
                return false;
            new_attrib->m_value_len = parser.m_cur_index - new_attrib->m_value_len;
            parser.next();
            parser.skip_space();
        }
#ifdef TD_XML_PARSE_STRING
        for(size_t i = 0; i< node->m_attributes_count; i++)
        {
            ((NODE_ATTRIBYTES*)node->m_attributes)[i].m_name_len  = ParseString(node->m_attributes[i].m_name, node->m_attributes[i].m_name_len);
            ((NODE_ATTRIBYTES*)node->m_attributes)[i].m_value_len = ParseString(node->m_attributes[i].m_value, node->m_attributes[i].m_value_len);
        }
#endif
        return parser.m_cur_char == 0;
    }
#endif
    bool ParseData(const char* xml, size_t xmlLen)
    {
        xml_parser parser(xml, xmlLen);
        m_xml_header_len = 0;
        m_root_count = 0;

        parser.skip_space();
        if(parser.m_cur_char != '<')
            return false;
        tdXML::NODE_LEVEL lvl_noda[XML_MAX_LEVEL];
        memset(lvl_noda, 0, sizeof(lvl_noda));
        int level = 0;

        size_t header_pos = parser.m_cur_index;
        if(parser.m_next_char == '?') //skip <? .... ?>
        {
            parser.next();
            parser.next();
            while(parser.m_cur_char != '\0' && ( parser.m_cur_char != '?' || parser.m_next_char != '>'))
                parser.next();
            parser.next();
            if(parser.m_cur_char != '>')
                return false;
            parser.next();
            m_xml_header = &parser.m_contents[header_pos];
            m_xml_header_len = parser.m_cur_index - header_pos;
        }
        parser.skip_space();
        while(parser.m_cur_char == '<' && parser.m_next_char == '!')//skip <! .... >
        {
            size_t level_D = 1;
            while(level_D && parser.m_cur_char != '\0')
            {
                parser.next();
                if(parser.m_cur_char == '<')
                {
                    level_D++;
                    continue;
                }
                if(parser.m_cur_char == '>')
                {
                    level_D--;
                    continue;
                }
            }
            parser.next();// skip '>'
            parser.skip_space();
        }
        while(parser.m_cur_index < parser.m_length && level >= 0)
        {
            parser.skip_space();
            if(parser.m_cur_char == '\0')
                break;
            if(parser.m_cur_char != '<')
                return false;
            parser.next();
            if(parser.m_cur_char == '!')//'<!'
            {
                parser.next();
                if(parser.m_cur_char == '-' && parser.m_next_char == '-')//comment '<!--'
                {
                    parser.next();
                    while(parser.m_cur_char != '\0' && parser.m_cur_char != '>')
                    {
                        while(parser.m_cur_char != '\0' && !(parser.m_cur_char == '-' && parser.m_next_char == '-'))
                            parser.next();
                        parser.next();
                        parser.next();
                    }
                }else{
                    parser.find_end_block();
                }
                if(parser.m_cur_char == '\0')
                    return false;
                parser.next();
                continue;
            }
            if(parser.m_cur_char == '/')//close
            {
                parser.next();
                parser.skip_space();
                size_t start_name = parser.m_cur_index;
                parser.skip_name();
                const char* name = &parser.m_contents[start_name];
                size_t name_len  = parser.m_cur_index - start_name;
                parser.skip_space();
                if(parser.m_cur_char != '>')
                    return false;
                parser.next();
                tdXML::NODE* childs_nodes = lvl_noda[level].m_nodes;
                size_t    childs_count = lvl_noda[level].m_count;
                memset(&lvl_noda[level], 0, sizeof(tdXML::NODE_LEVEL));
                level--;
                if(level < 0 || lvl_noda[level].m_count == 0)
                    break;
                tdXML::NODE* tmp = &lvl_noda[level].m_nodes[lvl_noda[level].m_count-1];
                tmp->m_childs = childs_nodes;
                tmp->m_childs_count = childs_count;
#ifdef TD_XML_PARSE_STRING
                for(size_t i = 0; i< childs_count; i++)
                {
                    childs_nodes[i].m_name_len = ParseString(childs_nodes[i].m_name, childs_nodes[i].m_name_len);
                }
#endif
                if(name_len != tmp->m_name_len || memcmp(name, tmp->m_name, name_len))
                    return false;
                continue;
            }
            if(lvl_noda[level].m_count == lvl_noda[level].m_count_max)
            {//alloc level node
                tdXML::NODE* tmp = lvl_noda[level].m_nodes;
                lvl_noda[level].m_count_max += lvl_noda[level].m_count_max/2+1;
                lvl_noda[level].m_nodes = (tdXML::NODE*)m_mem.Alloc(sizeof(tdXML::NODE)*lvl_noda[level].m_count_max);
                if(tmp)
                {
                    memcpy(lvl_noda[level].m_nodes, tmp, sizeof(tdXML::NODE)*lvl_noda[level].m_count);
                    m_mem.Free(tmp, sizeof(tdXML::NODE)*lvl_noda[level].m_count);
                }
            }
            tdXML::NODE* node = &lvl_noda[level].m_nodes[lvl_noda[level].m_count];
            lvl_noda[level].m_count++;
            memset(node, 0, sizeof(tdXML::NODE));
            parser.skip_space();
            size_t start_name = parser.m_cur_index;
            parser.skip_name();
            node->m_name     = &parser.m_contents[start_name];
            node->m_name_len = parser.m_cur_index - start_name;
            parser.skip_space();
            if(parser.m_cur_char == '/' && parser.m_contents[parser.m_cur_index+1] == '>')//пустой
            {
                parser.next();
                parser.next();
                node->m_type = tdXML::NODE::NODE_EMPTY;
                continue;
            }
            if(parser.m_cur_char == '\0')
                return false;
            if(parser.m_cur_char != '>')
            {
                size_t start_meta = parser.m_cur_index;
                parser.find_end_block();
#ifdef TD_XML_PARSE_ATTRIBYTES
                if(!ParseAttribute(&parser.m_contents[start_meta], parser.m_cur_index - start_meta, node))
                    return false;
#else
                node->m_attributes_data = &parser.m_contents[start_meta];
                node->m_attributes_len = parser.m_cur_index - start_meta;
                if(node->m_attributes_len && node->m_attributes_data[node->m_attributes_len-1] == ' ')
                    node->m_attributes_len --;
#endif
            }
            if(parser.m_cur_char == '/' && parser.m_contents[parser.m_cur_index+1] == '>')//пустой с мета
            {
                parser.next();
                parser.next();
                node->m_type = tdXML::NODE::NODE_EMPTY;
                continue;
            }
            parser.next(); // чето есть
            size_t start_data = parser.m_cur_index;
            parser.skip_space();
            if(parser.m_cur_char != '<')
            {//данные
                while(parser.m_cur_char != '\0'  && parser.m_cur_char != '<')
                    parser.next();
                if(parser.m_cur_char == '\0')
                    return false;
                size_t stop_data  = parser.m_cur_index;
                parser.next();
                if(parser.m_cur_char != '/')
                    return false;
                parser.next();
                parser.skip_space();
                size_t start_name = parser.m_cur_index;
                parser.skip_name();
                const char* name = &parser.m_contents[start_name];
                size_t name_len = parser.m_cur_index - start_name;
                if(name_len != node->m_name_len || memcmp(name, node->m_name, name_len))
                    return false;
                parser.skip_space();
                if(parser.m_cur_char != '>')
                    return false;
                parser.next();
                node->m_data = &parser.m_contents[start_data];
                node->m_data_size = stop_data - start_data;
                node->m_type = tdXML::NODE::NODE_DATA;
                continue;
            }
            node->m_type = tdXML::NODE::NODE_CHILDS;
            if(parser.m_contents[parser.m_cur_index+1] != '/')
            {
                level++;
                if(level >= XML_MAX_LEVEL)
                    return false;
            }
            continue;
        }
        if(level < 0)
            return false;
        m_root       = lvl_noda[0].m_nodes;
        m_root_count = lvl_noda[0].m_count;
#ifdef TD_XML_PARSE_STRING
        for(size_t i = 0; i< m_root_count; i++)
        {
            m_root[i].m_name_len = ParseString(m_root[i].m_name, m_root[i].m_name_len);
        }
#endif
        return true;
    }
#ifdef TD_XML_SAVE_FILE
    void WriteStop(FILE* f, tdXML::NODE* node, bool format)
    {
        fwrite("</", 1, 2, f);
        fwrite(node->m_name, 1, node->m_name_len, f);
        fwrite(">\r\n", 1, format?3:1, f);
    }
    void WriteStart(FILE* f, tdXML::NODE* node, bool format)
    {
        fwrite("<", 1, 1, f);
        fwrite(node->m_name, 1, node->m_name_len, f);
#ifdef TD_XML_PARSE_ATTRIBYTES
        if(node->m_attributes_count)
        {
            for(size_t i = 0; i < node->m_attributes_count; i++)
            {
                fwrite(" ", 1, 1, f);
                fwrite(node->m_attributes[i].m_name, 1, node->m_attributes[i].m_name_len, f);
                fwrite("=", 1, 1, f);
                fwrite(&node->m_attributes[i].m_separator, 1, 1, f);
                fwrite(node->m_attributes[i].m_value, 1, node->m_attributes[i].m_value_len, f);
                fwrite(&node->m_attributes[i].m_separator, 1, 1, f);
            }
        }
#else
        if(node->m_attributes_len)
        {
            fwrite(" ", 1, 1, f);
            fwrite(node->m_attributes_data, 1, node->m_attributes_len, f);
        }
#endif
        if(node->m_type == tdXML::NODE::NODE_EMPTY)
        {
            fwrite(" />\r\n", 1, format?5:3, f);
        }else if(node->m_type == tdXML::NODE::NODE_DATA && node->m_data_size)
        {
            fwrite(">", 1, 1, f);
            fwrite(node->m_data, 1, node->m_data_size, f);
        }else{
            fwrite(">\r\n", 1, format?3:1, f);
        }
    }
#endif
public:
    tdXML():m_mem(0x1000 - sizeof(void*)) { }
    ~tdXML() { }
    bool Init(const char* xml, size_t xmlLen = 0)
    {
        if(!xmlLen)
            xmlLen = strlen(xml);
        m_mem.Reset(true);
        return ParseData(xml, xmlLen);
    }
    const tdXML::NODE* find(const tdXML::NODE* node, const char* name, size_t nameLen = 0, size_t index = 0)
    {
        if(!nameLen)
            nameLen = strlen(name);
        if(node == NULL)
        {
            for(size_t i = 0; i < m_root_count; i++)
            {
                if(nameLen == m_root[i].m_name_len && memcmp(name, m_root[i].m_name, nameLen) == 0)
                {
                    if(!index)
                        return &m_root[i];
                    index--;
                }
            }
            return NULL;
        }
        if(node->m_type != tdXML::NODE::NODE_CHILDS)
            return NULL;
        for(size_t i = 0; i < node->m_childs_count; i++)
        {
            if(nameLen == node->m_childs[i].m_name_len && memcmp(name, node->m_childs[i].m_name, nameLen) == 0)
            {
                if(!index)
                    return &node->m_childs[i];
                index--;
            }
        }
        return NULL;
    }
    const tdXML::NODE* find(const char* name)
    {
        const tdXML::NODE* n = NULL;
        size_t start = 0, i = 0;
        while(name[i])
        {
            i++;
            if(name[i] == '/' || name[i] == '\\' || name[i] == '\0')
            {
                n = find(n, &name[start], i-start);
                if(!n)
                    break;
                start = i+1;
            }
        }
        return n;
    }
#ifdef TD_XML_SAVE_FILE
    bool Save(const char* fileName, bool format)
    {
        FILE* f = fopen(fileName, "wb+");
        if(!f)
            return false;
        if(m_xml_header_len)
            fwrite(m_xml_header, 1, m_xml_header_len, f);
        if(format)
            fwrite("\r\n", 1, 2, f);
        int lvl_index[XML_MAX_LEVEL];
        tdXML::NODE* lvl_node[XML_MAX_LEVEL];
        char spase[XML_MAX_LEVEL*3];
        memset(spase, ' ', sizeof(spase));
        for(size_t i = 0; i < m_root_count; i++)
        {
            lvl_node [0] = &m_root[i];
            lvl_index[0] = 0;
            int level = 0;
            while(level >= 0)
            {
                tdXML::NODE* node = lvl_node[level];//перейдем на уровень ниже
                if(lvl_index[level] == 0)
                {
                    if(format && level)
                        fwrite(spase, 1, level*3, f);
                    WriteStart(f, node, format);
                }
                if(node->m_type != NODE::NODE_CHILDS)
                {
                    if(node->m_type != NODE::NODE_EMPTY)
                        WriteStop(f, node, format);
                    level--;
                    continue;
                }
                if(lvl_index[level] == node->m_childs_count)
                {
                    if(format && level)
                        fwrite(spase, 1, level*3, f);
                    WriteStop(f, node, format);
                    level--;
                    continue;
                }
                lvl_index[level + 1] = 0;
                lvl_node [level + 1] = &node->m_childs[lvl_index[level]];
                lvl_index[level]++;
                level++;
            }
        }
        fclose(f);
        return true;
    }
#endif
private:
    tdMemLite m_mem;
    tdXML::NODE* m_root;
    size_t    m_root_count;

    const char* m_xml_header;
    size_t      m_xml_header_len;
};

