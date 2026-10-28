#include <signal_path/signal_path.h>

namespace sigpath {
    IQFrontEnd iqFrontEnd;
    VFOManager vfoManager;
    SourceManager sourceManager;
    SinkManager sinkManager;
    Event<bool> txState;
   
};

void SourceManager_registerSource(const char* name, SourceManager::SourceHandler* handler)
{
    sigpath::sourceManager.registerSource(name, handler);
}


void SourceManager_unregisterSource(const char* name)
{
    sigpath::sourceManager.unregisterSource(name);
}


void Volk_8i_s32f_convert_32f(float* a0, const int8_t* a1, const float a2, unsigned int a3)
{ 
    volk_8i_s32f_convert_32f(a0, a1, a2, a3);
}


void SourceManager_setTuningLimits(double minFreq, double maxFreq)
{
    sigpath::sourceManager.setTuningLimits(minFreq, maxFreq);
}

void SourceManager_clearTuningLimits()
{
    sigpath::sourceManager.clearTuningLimits();
}

