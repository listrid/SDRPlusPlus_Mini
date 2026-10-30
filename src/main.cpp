#include <core.h>


#include <exception>


namespace std {
    class _ExceptionPtr_static {
    public:
        void* _Data[4];
        _ExceptionPtr_static() noexcept {}
        ~_ExceptionPtr_static() noexcept {}
    };
}
extern "C" {
    void __cdecl _ExceptionPtrCreate(void*) {}
    void __cdecl _ExceptionPtrDestroy(void*) {}
    void __cdecl _ExceptionPtrCopy(void*, const void*) {}
    void __cdecl _ExceptionPtrAssign(void*, const void*) {}
}




#include <fft_config.h>
int sdrpp_main(int argc, char* argv[]);
int main(int argc, char* argv[]) {

//     float out_true[32+16];
//     float out_false[32+16];
// 
//     float out_r1d[190];
//     {
//         float input[32] ={
//             0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f, 0.0f, // 0-3 комплексные
//             0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f, 0.0f, // 4-7 комплексные
//             0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f, 0.0f, // 8-11 комплексные
//             0.0f, 0.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f, 0.0f  // 12-15 комплексные
//         };
//         fft_plan plan = fft_create_plan_1d(16, (fft_complex*)input, (fft_complex*)out_false, false);
//         fft_exec_1d(plan, (fft_complex*)input, (fft_complex*)out_false);
//         fft_destroy_plan(plan);
// 
//         plan = fft_create_plan_1d(16, (fft_complex*)input, (fft_complex*)out_true, true);
//         fft_exec_1d(plan, (fft_complex*)input, (fft_complex*)out_true);
//         fft_destroy_plan(plan);
//         //FFTW_FORWARD(true)   out[9] = -8  out[25] = 8
//         //FFTW_BACKWARD(false) out[9] = 8;  out[25] = -8; 
//     }
// 
// 
// 
// 
//     {
//         float input_real[32] ={
//     4.0f, 1.0f, 1.0f, 1.0f,  4.0f, 1.0f, 1.0f, 1.0f,
//     4.0f, 1.0f, 1.0f, 1.0f,  4.0f, 1.0f, 1.0f, 1.0f,
//     4.0f, 1.0f, 1.0f, 1.0f,  4.0f, 1.0f, 1.0f, 1.0f,
//     4.0f, 1.0f, 1.0f, 1.0f,  4.0f, 1.0f, 1.0f, 1.0f
//         };
//         fft_plan plan = fft_create_plan_r1d(32, input_real, (fft_complex*)out_r1d);
//         fft_exec_r1d(plan, input_real, (fft_complex*)out_r1d);
// 
// 
//         fft_destroy_plan(plan);
// //out_r1d[0] = 56   out_r1d[16] = 24 out_r1d[32] = 24
//     }



    return sdrpp_main(argc, argv);
}



