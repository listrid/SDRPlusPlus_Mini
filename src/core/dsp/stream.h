#pragma once
#include <string.h>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <volk/volk.h>
#include "buffer/buffer.h"

// 1MSample buffer
#define STREAM_BUFFER_SIZE 1000000 / 2

namespace dsp {
    class untyped_stream {
    public:
        virtual ~untyped_stream() {}
//         virtual bool swap(int size) { return false; }
//         virtual int read() { return -1; }
//         virtual void flush() {}
        virtual void stopWriter() {}
        virtual void clearWriteStop() {}
        virtual void stopReader() {}
        virtual void clearReadStop() {}
    };

    template <class T>
    class stream : public untyped_stream {
    public:
        stream() {
            m_wBuf = m_rBuf = NULL;
            m_wBuf_Size = m_rBuf_Size = 0;
        }

        virtual ~stream() {
            free();
        }

        virtual void setBufferSize(int samples) {
            buffer::free(m_wBuf);
            buffer::free(m_rBuf);
            m_wBuf = buffer::alloc<T>(samples);
            m_rBuf = buffer::alloc<T>(samples);
            m_wBuf_Size = m_rBuf_Size = samples;
        }

        inline bool swap(int size) {
            {
                // Wait to either swap or stop
                std::unique_lock<std::mutex> lck(m_swapMtx);
                m_swapCV.wait(lck, [this] { return (m_canSwap || m_writerStop); });

                // If writer was stopped, abandon operation
                if (m_writerStop) { return false; }

                // Swap buffers
                m_dataSize = size;
                T* temp = m_wBuf;
                int temp_size = m_wBuf_Size;
                m_wBuf = m_rBuf;
                m_wBuf_Size = m_rBuf_Size;
                m_rBuf_Size = temp_size;
                m_rBuf = temp;
                m_canSwap = false;
            }
           
            {// Notify reader that some data is ready
                std::lock_guard<std::mutex> lck(m_rdyMtx);
                m_dataReady = true;
            }
            m_rdyCV.notify_all();

            return true;
        }

        inline int read() {
            // Wait for data to be ready or to be stopped
            std::unique_lock<std::mutex> lck(m_rdyMtx);
            m_rdyCV.wait(lck, [this] { return (m_dataReady || m_readerStop); });

            return (m_readerStop ? -1 : m_dataSize);
        }

        // Like read(), but returns 0 if no data became ready within `timeout`.
        // -1 still means stopReader() was called. Used by realtime audio sink
        // callbacks that must not block past one device period.
        virtual inline int read_for(std::chrono::milliseconds timeout) {
            std::unique_lock<std::mutex> lck(m_rdyMtx);
            if (!m_rdyCV.wait_for(lck, timeout, [this] { return (m_dataReady || m_readerStop); })) {
                return 0;
            }
            return (m_readerStop ? -1 : m_dataSize);
        }

        inline void flush() {
            // Clear data ready
            {
                std::lock_guard<std::mutex> lck(m_rdyMtx);
                m_dataReady = false;
            }

            // Notify writer that buffers can be swapped
            {
                std::lock_guard<std::mutex> lck(m_swapMtx);
                m_canSwap = true;
            }

            m_swapCV.notify_all();
        }

        virtual void stopWriter() {
            {
                std::lock_guard<std::mutex> lck(m_swapMtx);
                m_writerStop = true;
            }
            m_swapCV.notify_all();
        }

        virtual void clearWriteStop() {
            m_writerStop = false;
        }

        virtual void stopReader() {
            {
                std::lock_guard<std::mutex> lck(m_rdyMtx);
                m_readerStop = true;
            }
            m_rdyCV.notify_all();
        }

        virtual void clearReadStop() {
            m_readerStop = false;
        }

        void free() {
            if (m_wBuf) { buffer::free(m_wBuf); }
            if (m_rBuf) { buffer::free(m_rBuf); }
            m_wBuf = NULL;
            m_rBuf = NULL;
        }
        inline T* GetRBuf(int max_size)
        {
            if(m_rBuf_Size < max_size)
            {
                if(max_size > STREAM_BUFFER_SIZE)
                {
                    assert(false);
                    max_size = STREAM_BUFFER_SIZE;
                }
                m_rBuf_Size = (max_size + 15) & ~15;
                if(m_rBuf) { buffer::free(m_rBuf); }
                m_rBuf = buffer::alloc<T>(m_rBuf_Size);
            }
            return m_rBuf;
        }
        inline T* GetRBuf()
        {
            return m_rBuf;
        };
        inline T* GetWBuf(int max_size)
        {
            if(m_wBuf_Size < max_size)
            {
                if(max_size > STREAM_BUFFER_SIZE)
                    max_size = STREAM_BUFFER_SIZE;
                m_wBuf_Size = (max_size + 15) & ~15;
                if(m_wBuf) { buffer::free(m_wBuf); }
                m_wBuf = buffer::alloc<T>(m_wBuf_Size);
            }
            return m_wBuf;
        };

    private:

        T*  m_rBuf;
        T*  m_wBuf;
        int m_rBuf_Size;
        int m_wBuf_Size;

        std::mutex m_swapMtx;
        std::condition_variable m_swapCV;
        bool m_canSwap = true;

        std::mutex m_rdyMtx;
        std::condition_variable m_rdyCV;
        bool m_dataReady = false;

        bool m_readerStop = false;
        bool m_writerStop = false;

        int m_dataSize = 0;
    };


    template <class T>
    class vector
    {
    public:
        vector() {
            m_buf = NULL;
            m_buf_Size = 0;
        }
        ~vector() {
            if(m_buf) { buffer::free(m_buf); }
        }
        inline T* GetBuf(int max_size){
            if(m_buf_Size < max_size)
            {
                if(max_size > STREAM_BUFFER_SIZE * 2) {
                    assert(false);
                    max_size = STREAM_BUFFER_SIZE * 2;
                }
                m_buf_Size = (max_size + 15) & ~15;
                if(m_buf) { buffer::free(m_buf); }
                m_buf = buffer::alloc<T>(m_buf_Size);
            }
            return m_buf;
        };
        int Size() {
            return m_buf_Size;
        }
    private:
        T* m_buf;
        int m_buf_Size;
    };
}

