#pragma once
#include "../processor.h"
#include "../window/nuttall.h"
#include "../fft_config.h"

namespace dsp::noise_reduction {
    class FMIF : public Processor<complex_t, complex_t> {
        using base_type = Processor<complex_t, complex_t>;
    public:
        FMIF() {}

        FMIF(stream<complex_t>* in, int bins) { init(in, bins); }

        ~FMIF() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            destroyBuffers();
        }

        void init(stream<complex_t>* in, int bins) {
            _bins = bins;
            initBuffers();
            base_type::init(in);
        }

        void setBins(int bins) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _bins = bins;
            destroyBuffers();
            initBuffers();
            base_type::tempStart();
        }

        void reset()
        {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            buffer::clear(buffer, _bins - 1);
            buffer::clear(backFFTIn, _bins);
            base_type::tempStart();
        }

        int process(int count, const complex_t* in, complex_t* out)
        {
            // Write new input data to buffer buffer
            memcpy(bufferStart, in, count * sizeof(complex_t));
            
            // Iterate the FFT
            for (int i = 0; i < count; i++)
            {
                // Apply windows
                volk_32fc_32f_multiply_32fc((lv_32fc_t*)forwFFTIn, (lv_32fc_t*)&buffer[i], fftWin, _bins);

                // Do forward FFT
                fft_exec_1d(forwardPlan, (fft_complex*)forwFFTIn, (fft_complex*)forwFFTOut);
                // Process bins here
                uint32_t idx;
                volk_32fc_magnitude_32f(ampBuf, (lv_32fc_t*)forwFFTOut, _bins);
                volk_32f_index_max_32u(&idx, ampBuf, _bins);

                // Keep only the bin of highest amplitude
                backFFTIn[idx] = forwFFTOut[idx];

                // Do reverse FFT and get first element
                fft_exec_1d(backwardPlan, (fft_complex*)backFFTIn, (fft_complex*)backFFTOut);

                out[i] = backFFTOut[_bins / 2];

                // Reset the input buffer
                backFFTIn[idx] = { 0, 0 };
            }

            // Move buffer buffer
            memmove(buffer, &buffer[count], (_bins - 1) * sizeof(complex_t));

            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            process(count, base_type::_in->GetRBuf(), base_type::out.GetWBuf(count));

            // Swap if some data was generated
            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    protected:
        void initBuffers()
        {
            // Allocate FFT buffers
            forwFFTIn  = (complex_t*)fft_alloc(_bins);
            forwFFTOut = (complex_t*)fft_alloc(_bins);
            backFFTIn  = (complex_t*)fft_alloc(_bins);
            backFFTOut = (complex_t*)fft_alloc(_bins);

            // Allocate and clear delay buffer
            buffer = buffer::alloc<complex_t>(STREAM_BUFFER_SIZE + 64000);
            bufferStart = &buffer[_bins - 1];
            buffer::clear(buffer, _bins - 1);

            // Clear backward FFT input since only one value is changed and reset at a time
            buffer::clear(backFFTIn, _bins);

            // Allocate amplitude buffer
            ampBuf = buffer::alloc<float>(_bins);

            // Allocate and generate Window
            fftWin = buffer::alloc<float>(_bins);
            for (int i = 0; i < _bins; i++)
            {
                fftWin[i] = window::nuttall(i, _bins - 1);
            }

            // Plan FFTs
            forwardPlan  = fft_create_plan_1d(_bins, (fft_complex*)forwFFTIn, (fft_complex*)forwFFTOut, true);
            backwardPlan = fft_create_plan_1d(_bins, (fft_complex*)backFFTIn, (fft_complex*)backFFTOut, true);
        }

        void destroyBuffers() {

            fft_destroy_plan(forwardPlan);
            fft_destroy_plan(backwardPlan);
            fft_free((fft_complex*)forwFFTIn);
            fft_free((fft_complex*)forwFFTOut);
            fft_free((fft_complex*)backFFTIn);
            fft_free((fft_complex*)backFFTOut);

            buffer::free(buffer);
            buffer::free(ampBuf);
            buffer::free(fftWin);
        }

        complex_t* forwFFTIn;
        complex_t* forwFFTOut;
        complex_t* backFFTIn;
        complex_t* backFFTOut;

        fft_plan forwardPlan;
        fft_plan backwardPlan;

        complex_t* buffer;
        complex_t* bufferStart;

        float* fftWin;

        float* ampBuf;

        int _bins;

    }; 
}