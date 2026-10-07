#pragma once

namespace displaymenu {
    void init();
    void checkKeybinds();
    void draw(void* ctx);
    
    // MPX Analysis Default Constants - Single Source of Truth
    const int MPX_DEFAULT_REFRESH_RATE = 10;
    const float MPX_DEFAULT_LINE_WIDTH = 2.5f;
    const int MPX_DEFAULT_SMOOTHING_FACTOR = 3;
    
    // Global MPX analysis settings - Cross-platform export/import
    extern int mpxRefreshRate;
    extern float mpxLineWidth;
    extern int mpxSmoothingFactor;
}