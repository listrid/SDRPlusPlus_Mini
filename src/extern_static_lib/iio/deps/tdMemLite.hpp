/**
* @author:  Egorov Sergey <listrid@yandex.ru>
**/
#pragma once

#include <malloc.h>
#include <string.h>

class tdMemLite
{
private:
    void **m_chunk_bottom, **m_chunk_top;
    void **m_list_big;  //список выделенных больших блоков
    void **m_list_free; //список освобожденных кусков

    size_t m_chunk_size;
    size_t m_free_space;

    size_t m_chunk_use;
    size_t m_chunk_count;
    size_t m_pool_size;
    size_t m_alloc_big;
#ifdef _TD_MEM_H_
    tdMemMaster* m_master;
#endif
public:
    tdMemLite(size_t chunk_size = 0)
    {
        m_chunk_bottom = m_list_big = NULL;
        Init(chunk_size);
    }
    ~tdMemLite(){ Reset(true); }
    void Init(size_t chunk_size)
    {
        Reset(true);
        if(!chunk_size)
            chunk_size = ((1<<16) - sizeof(void*));//64kb
        m_chunk_size = (chunk_size + sizeof(void*) + 0xFFF)&(~0xFFF); // кратно 4kb
#ifdef _TD_MEM_H_
        m_master = NULL;
#endif
    }
#ifdef _TD_MEM_H_
    tdMemLite(tdMemMaster* master)
    {
        m_chunk_bottom = m_list_big = NULL;
        Init(master);
    }
    void Init(tdMemMaster* master)
    {
        Reset(true);
        m_master = master;
        if(m_master)
        {
            m_chunk_size = master->BlockSize()&(~15);
        }else{
            m_chunk_size = (1<<16); //64kb
        }
    }
#endif

    void Reset(bool trim)
    {
        m_pool_size = m_alloc_big = 0;
        if(trim)
        {//освободить всю память системе
            while(m_chunk_bottom)
            {
                void** temp = m_chunk_bottom;
                m_chunk_bottom = (void**)(m_chunk_bottom[0]);
#ifdef _TD_MEM_H_
                if(m_master)
                    m_master->Free((void*)temp);
                else
#endif
                    free((void*)temp);
            }
            m_list_free = m_chunk_top = m_chunk_bottom = NULL, m_free_space = m_chunk_use = m_chunk_count = 0;
        }else{//вернуть себе всю выданную память
            m_chunk_use  = 0;
            m_chunk_top  = m_chunk_bottom;
            m_free_space = (m_chunk_bottom != NULL) ? m_chunk_size - sizeof(void*) : 0;
            m_list_free = NULL;
        }
        while(m_list_big)
        {//удалить большие блоки
            void** temp = m_list_big;
            m_list_big = (void**)(m_list_big[0]);
            free((void*)temp);
        }
    }
    void* Alloc(size_t size) //выделить память 
    {
        if(size < sizeof(void*)*2)
            size = sizeof(void*)*2;
        size = (size+7)&(~7);
        if(m_list_free)
        {//поиск в свободных
            void** prev = NULL;
            void** next = m_list_free;
            while(next)
            {
                if(next[1] == (void**)size)
                {
                    if(prev){ prev[0] = next[0]; }else{ m_list_free = (void**)next[0]; }
                    m_pool_size -= size;
                    return next;
                }
                prev = next;
                next = (void**)(next[0]);
            }
        }
        if(size > (m_chunk_size - sizeof(void*)))
        {//выделить большой блок
            void** block = (void**)malloc(size + sizeof(void*));
            block[0] = m_list_big;
            m_list_big = block;
            m_alloc_big += size;
            return &block[1];
        }
        if(m_free_space < size)
        {
            if(m_free_space >= sizeof(void*)*2)
                this->Free(((char*)m_chunk_top) + (m_chunk_size - m_free_space), m_free_space);
            if(m_chunk_top == NULL || m_chunk_top[0] == NULL)
            {
                void **block;
#ifdef _TD_MEM_H_
                if(m_master)
                    block = (void**)m_master->Alloc();
                else
#endif
                    block = (void**)malloc(m_chunk_size);
                block[0] = NULL;
                if(m_chunk_top)
                    m_chunk_top[0] = block;
                else
                    m_chunk_top = m_chunk_bottom = block;
                m_chunk_count++;
            }
            if(m_chunk_top[0])
                m_chunk_top = (void**)(m_chunk_top[0]);
            m_free_space = m_chunk_size - sizeof(void*);
            m_chunk_use++;
        }
        void *ptr = ((char*)m_chunk_top) + (m_chunk_size - m_free_space);
        m_free_space -= size;
        return ptr;
    }
    void Free(void* ptr, size_t size) //вернуть в список свободных
    {
        if(size < sizeof(void*)*2)
            size = sizeof(void*)*2;
        size = (size+7)&(~7);
        ((void**)ptr)[0] = m_list_free;
        ((void**)ptr)[1] = (void*)size;
        m_list_free = (void**)ptr;
        m_pool_size += size;
    }
    void Stat_chunk(size_t* chunk_all, size_t* chunk_use = 0)
    {
        if(chunk_all)
            *chunk_all = m_chunk_count;
        if(chunk_use)
            *chunk_use = m_chunk_use;
    }
    void Stat(size_t* all, size_t* alloc, size_t* pollFree = 0, size_t* useBig = 0) // сколько памяти выделили, размер всей памяти в управлении, размер пула освобожденных, выделенно вне чинков
    {
        if(all)
            *all = m_chunk_count*(m_chunk_size - sizeof(void*));
        if(alloc)
            *alloc = m_chunk_use*(m_chunk_size - sizeof(void*)) - m_free_space - m_pool_size + m_alloc_big;
        if(pollFree)
            *pollFree = m_pool_size + m_free_space;
        if(useBig)
            *useBig = m_alloc_big;
    }
};//tdMemLite


template <class T>
class tdMemLite_allocator
{
public:
    typedef T value_type;

    tdMemLite* m_mem;
    tdMemLite_allocator(tdMemLite* mem)
    {
        m_mem = mem;
    }
    tdMemLite_allocator(const tdMemLite_allocator& base)
    {
        m_mem = base.m_mem;
    }
    T* allocate(size_t n, const void* p = 0)
    {
        return (T*)m_mem->Alloc(n*sizeof(T));
    }
    void deallocate(void* p, size_t size)
    {
        if(p)
            m_mem->Free(p, size*sizeof(T));
    }
    template <class U>
    struct rebind
    {
        typedef tdMemLite_allocator<U> other;
    };
    template <class U>
    tdMemLite_allocator(const tdMemLite_allocator<U>& base)
    {
        m_mem = base.m_mem;
    }
};

