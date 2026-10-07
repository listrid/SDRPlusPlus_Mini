#pragma once

#ifdef _WIN32
  #ifdef SDRPP_IS_CORE
    #define SDRPP_API __declspec(dllexport)
    #define IMGUI_API //__declspec(dllexport)
  #else
    #define SDRPP_API __declspec(dllimport)
    #define IMGUI_API //__declspec(dllimport)
  #endif
#else
  #ifdef SDRPP_IS_CORE
    #define SDRPP_API __attribute__((visibility("default")))
    #define IMGUI_API //__attribute__((visibility("default")))
  #else
    #define SDRPP_API
    #define IMGUI_API
  #endif
#endif
