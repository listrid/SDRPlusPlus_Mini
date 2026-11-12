#pragma once
#include "../processor.h"
#include "../window/nuttall.h"
#include "../fft_config.h"

namespace dsp::noise_reduction
{
    class FMIF : public Processor<complex_t, complex_t>
    {
        using base_type = Processor<complex_t, complex_t>;
    public:
        FMIF() {}

        FMIF(stream<complex_t>* in, int bins) { init(in, bins); }

        ~FMIF()
        {
            if (!base_type::_block_init)
                return;
            base_type::stop();
            destroyBuffers();
        }

        void init(stream<complex_t>* in, int bins)
        {
            m_bins = bins;
            initBuffers();
            base_type::init(in);
        }

        void setBins(int bins)
        {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            m_bins = bins;
            destroyBuffers();
            initBuffers();
            base_type::tempStart();
        }

        void reset()
        {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            buffer::clear(m_buffer, m_bins - 1);
            buffer::clear(m_backFFTIn, m_bins);
            base_type::tempStart();
        }

        int process(int count, const complex_t* in, complex_t* out)
        {
            // Write new input data to buffer buffer
            memcpy(m_bufferStart, in, count * sizeof(complex_t));
            
            // Iterate the FFT
            for (int i = 0; i < count; i++)
            {
                // Apply windows
                volk_32fc_32f_multiply_32fc((lv_32fc_t*)m_forwFFTIn, (lv_32fc_t*)&m_buffer[i], m_fftWin, m_bins);

                // Do forward FFT
                fft_exec_1d(m_forwardPlan, (fft_complex*)m_forwFFTIn, (fft_complex*)m_forwFFTOut);
                // Process bins here
                uint32_t idx;
                volk_32fc_magnitude_32f(m_ampBuf, (lv_32fc_t*)m_forwFFTOut, m_bins);
                volk_32f_index_max_32u(&idx, m_ampBuf, m_bins);

                // Keep only the bin of highest amplitude
                m_backFFTIn[idx] = m_forwFFTOut[idx];

                // Do reverse FFT and get first element
                fft_exec_1d(m_backwardPlan, (fft_complex*)m_backFFTIn, (fft_complex*)m_backFFTOut);

                out[i] = m_backFFTOut[m_bins / 2];

                // Reset the input buffer
                m_backFFTIn[idx] = { 0, 0 };
            }

            // Move buffer buffer
            memmove(m_buffer, &m_buffer[count], (m_bins - 1) * sizeof(complex_t));

            return count;
        }

        int run()
        {
            int count = base_type::_in->read();
            if (count < 0)
                return -1;

            process(count, base_type::_in->GetRBuf(), base_type::out.GetWBuf(count));

            // Swap if some data was generated
            base_type::_in->flush();
            if (!base_type::out.swap(count))
                return -1;
            return count;
        }

    protected:
        void initBuffers()
        {
            // Allocate FFT buffers
            m_forwFFTIn  = (complex_t*)fft_alloc(m_bins);
            m_forwFFTOut = (complex_t*)fft_alloc(m_bins);
            m_backFFTIn  = (complex_t*)fft_alloc(m_bins);
            m_backFFTOut = (complex_t*)fft_alloc(m_bins);

            // Allocate and clear delay buffer
            m_buffer = buffer::alloc<complex_t>(STREAM_BUFFER_SIZE + 64000);
            m_bufferStart = &m_buffer[m_bins - 1];
            buffer::clear(m_buffer, m_bins - 1);

            // Clear backward FFT input since only one value is changed and reset at a time
            buffer::clear(m_backFFTIn, m_bins);

            // Allocate amplitude buffer
            m_ampBuf = buffer::alloc<float>(m_bins);

            // Allocate and generate Window
            m_fftWin = buffer::alloc<float>(m_bins);
            for (int i = 0; i < m_bins; i++)
            {
                m_fftWin[i] = window::nuttall(i, m_bins - 1);
            }

            // Plan FFTs
            m_forwardPlan  = fft_create_plan_1d(m_bins, (fft_complex*)m_forwFFTIn, (fft_complex*)m_forwFFTOut, true);
            m_backwardPlan = fft_create_plan_1d(m_bins, (fft_complex*)m_backFFTIn, (fft_complex*)m_backFFTOut, true);
        }

        void destroyBuffers()
        {

            fft_destroy_plan(m_forwardPlan);
            fft_destroy_plan(m_backwardPlan);
            fft_free((fft_complex*)m_forwFFTIn);
            fft_free((fft_complex*)m_forwFFTOut);
            fft_free((fft_complex*)m_backFFTIn);
            fft_free((fft_complex*)m_backFFTOut);

            buffer::free(m_buffer);
            buffer::free(m_ampBuf);
            buffer::free(m_fftWin);
        }

        complex_t* m_forwFFTIn;
        complex_t* m_forwFFTOut;
        complex_t* m_backFFTIn;
        complex_t* m_backFFTOut;

        fft_plan m_forwardPlan;
        fft_plan m_backwardPlan;

        complex_t* m_buffer;
        complex_t* m_bufferStart;

        float* m_fftWin;

        float* m_ampBuf;

        int m_bins;

    }; 
}