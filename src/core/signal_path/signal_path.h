#pragma once
#include "iq_frontend.h"
#include "vfo_manager.h"
#include "source.h"
#include "sink.h"
#include <module.h>
#include <utils/event.h>

namespace sigpath {
    extern IQFrontEnd iqFrontEnd;
    extern VFOManager vfoManager;
    extern SourceManager sourceManager;
    extern SinkManager sinkManager;

    // Emitted on transmit state changes (true = TX active). Consumed e.g. by
    // noise reduction to freeze its noise profile while transmitting.
    extern Event<bool> txState;


};

void SourceManager_registerSource(const char* name, SourceManager::SourceHandler* handler);
void SourceManager_unregisterSource(const char* name);
void SourceManager_setTuningLimits(double minFreq, double maxFreq);
void SourceManager_clearTuningLimits();


