#pragma once

#define use_muFFT 1
//#define use_PFFFT 1  // 2.167 ms
//#define use_fftwf 1    //14.218 ms


struct fft_complex
{
    float real;
    float imag;
};


#ifdef use_muFFT

#include <fft_internal.h>

typedef mufft_plan_1d* fft_plan;

inline fft_complex* fft_alloc(size_t size)
{
    return (fft_complex*)mufft_alloc(size*sizeof(cfloat));
}

inline void fft_free(fft_complex* ptr)
{
    mufft_free(ptr);
}

inline fft_plan fft_create_plan_1d(size_t size, fft_complex *in, fft_complex* out, bool isForward)
{
    return mufft_create_plan_1d_c2c((unsigned)size, (isForward)?MUFFT_FORWARD:MUFFT_INVERSE, MUFFT_FLAG_CPU_ANY);
}

inline void fft_exec_1d(fft_plan plan, fft_complex* in, fft_complex* out)
{
    mufft_execute_plan_1d(plan, out, in);
}

inline fft_plan fft_create_plan_r1d(size_t size, float* in, fft_complex* out)
{
    return mufft_create_plan_1d_r2c((unsigned)size, MUFFT_FLAG_CPU_ANY);
}

inline void fft_exec_r1d(fft_plan plan, float* in, fft_complex* out)
{
    mufft_execute_plan_1d(plan, out, in);
}

inline void fft_destroy_plan(fft_plan plan)
{
    mufft_free_plan_1d(plan);
}

#endif

#ifdef use_PFFFT

extern "C" {
#include <pffft/pffft.h>
};


typedef struct fft_plan
{
    PFFFT_Setup* m_plan;
    float* m_work;
    bool   m_isForward;
    size_t m_size;
    explicit operator bool() const
    {
        return m_plan != nullptr;
    }
    
}fft_plan;


inline fft_complex* fft_alloc(size_t size)
{
    return (fft_complex*)pffft_aligned_malloc(size * 2 * sizeof(float));
}

inline void fft_free(fft_complex* ptr)
{
    pffft_aligned_free(ptr);
}

inline fft_plan fft_create_plan_1d(size_t size, fft_complex* in, fft_complex* out, bool isForward)
{
    fft_plan ret;
    ret.m_plan = pffft_new_setup((int)size, PFFFT_COMPLEX);
    ret.m_work = (float*)fft_alloc(size);
    ret.m_isForward = isForward;
    ret.m_size = size;
    return ret;
}

inline void fft_exec_1d(fft_plan& plan, fft_complex* in, fft_complex* out)
{
    float* in_f  = reinterpret_cast<float*>(in);
    float* out_f = reinterpret_cast<float*>(out);
    pffft_transform_ordered( plan.m_plan, (float*)in, (float*)out, plan.m_work, plan.m_isForward ? PFFFT_FORWARD : PFFFT_BACKWARD);
}

inline fft_plan fft_create_plan_r1d(size_t size, float* in, fft_complex* out)
{
    fft_plan ret;
    ret.m_plan = pffft_new_setup((int)size, PFFFT_REAL);
    ret.m_work = (float*)fft_alloc(size);
    ret.m_isForward = true;
    ret.m_size = size;
    return ret;
}

inline void fft_exec_r1d(fft_plan&plan, float* in, fft_complex* out)
{
    pffft_transform_ordered(plan.m_plan, in, (float*)out, plan.m_work, PFFFT_FORWARD);
    size_t size = plan.m_size;
    float* out_f = (float*)out;
    float nyquist_val = out[0].imag; 
    out[0].imag = 0.0f;
    out[size].real = nyquist_val;
    out[size].imag = 0.0f;
}

inline void fft_destroy_plan(fft_plan& plan)
{
    if(!plan.m_plan)
        return;
    pffft_destroy_setup(plan.m_plan);
    fft_free((fft_complex*)plan.m_work);
    plan.m_plan = NULL;
}
   

#endif

#ifdef use_fftwf

#include <fftw3.h>

typedef fftwf_plan fft_plan;

inline fft_complex* fft_alloc(size_t size)
{
    return (fft_complex*)fftwf_malloc(size*sizeof(fftwf_complex));
}

inline void fft_free(fft_complex* ptr)
{
    fftwf_free(ptr);
}

inline fft_plan fft_create_plan_1d(size_t size, fft_complex* in, fft_complex* out, bool isForward)
{
    return fftwf_plan_dft_1d((int)size, (fftwf_complex*)in, (fftwf_complex*)out, (isForward)?FFTW_FORWARD:FFTW_BACKWARD, FFTW_ESTIMATE);
}

inline void fft_exec_1d(fft_plan plan, fft_complex* in, fft_complex* out)
{
    fftwf_execute(plan);
}

inline fft_plan fft_create_plan_r1d(size_t size, float* in, fft_complex* out)
{
    return fftwf_plan_dft_r2c_1d((int)size, in, (fftwf_complex*)out, FFTW_ESTIMATE);
}

inline void fft_exec_r1d(fft_plan plan, float*  in, fft_complex* out)
{
    fftwf_execute(plan);
}

inline void fft_destroy_plan(fft_plan plan)
{
    fftwf_destroy_plan(plan);
}

#endif

