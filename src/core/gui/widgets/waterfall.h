#pragma once
#include <vector>
#include <mutex>
#include <cmath>
#include <gui/widgets/bandplan.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <utils/event.h>

#include <utils/opengl_include_code.h>

#define WATERFALL_RESOLUTION 1000000

namespace ImGui {
    class WaterfallVFO {
    public:
        void setOffset(double offset);
        void setCenterOffset(double offset);
        void setBandwidth(double bw);
        void setReference(int ref);
        void setSnapInterval(double interval);
        void setNotchOffset(double offset);
        void setNotchVisible(bool visible);
        void updateDrawingVars(double viewBandwidth, float dataWidth, double viewOffset, ImVec2 widgetPos, int fftHeight); // NOTE: Datawidth double???
        void draw(ImGuiWindow* window, bool selected);

        enum {
            REF_LOWER,
            REF_CENTER,
            REF_UPPER,
            _REF_COUNT
        };

        double m_generalOffset; //смещение от центра (что овырезать)
        double m_centerOffset;
        double m_lowerOffset;
        double m_upperOffset;
        double m_bandwidth;
        double m_snapInterval = 5000;
        int m_reference = REF_CENTER;

        double m_notchOffset = 0;
        bool m_notchVisible = false;

        bool m_leftClamped;
        bool m_rightClamped;

        ImVec2 m_rectMin;
        ImVec2 m_rectMax;
        ImVec2 m_lineMin;
        ImVec2 m_lineMax;
        ImVec2 m_wfRectMin;
        ImVec2 m_wfRectMax;
        ImVec2 m_wfLineMin;
        ImVec2 m_wfLineMax;
        ImVec2 m_lbwSelMin;
        ImVec2 m_lbwSelMax;
        ImVec2 m_rbwSelMin;
        ImVec2 m_rbwSelMax;
        ImVec2 m_wfLbwSelMin;
        ImVec2 m_wfLbwSelMax;
        ImVec2 m_wfRbwSelMin;
        ImVec2 m_wfRbwSelMax;
        ImVec2 m_notchMin;
        ImVec2 m_notchMax;

        bool m_centerOffsetChanged = false;
        bool m_lowerOffsetChanged = false;
        bool m_upperOffsetChanged = false;
        bool m_redrawRequired = true;
        bool m_lineVisible = true;
        bool m_bandwidthChanged = false;

        double m_minBandwidth;
        double m_maxBandwidth;
        bool m_bandwidthLocked;

        ImU32 m_color = IM_COL32(255, 255, 255, 50);

        Event<double> m_onUserChangedBandwidth;
        Event<double> m_onUserChangedNotch;
    };

    class WaterFall {
    public:
        WaterFall();

        void init();

        void draw();
        float* getFFTBuffer();
        void pushFFT();

        void updatePallette(float colors[][3], int colorCount);
        void updatePalletteFromArray(const float* colors, int colorCount);

        void setCenterFrequency(double freq);
        double getCenterFrequency();

        void setBandwidth(double bandWidth);
        double getBandwidth();

        void setViewBandwidth(double bandWidth);
        double getViewBandwidth();

        void setViewOffset(double offset);
        double getViewOffset();

        void setFFTMin(float min);
        float getFFTMin();

        void setFFTMax(float max);
        float getFFTMax();

        void setWaterfallMinMax(float min, float max);
        float getWaterfallMin();
        float getWaterfallMax();

        void setZoom(double zoomLevel);
        void setOffset(double zoomOffset);

        bool getAutorangeRef(float& refDb, int lines = 1);
        void freeAutorangeScratch();

        void selectFirstVFO();

        void showWaterfall();
        void hideWaterfall();

        void showBandplan();
        void hideBandplan();

        void setFFTHeight(int height);
        int getFFTHeight();

        void setRawFFTSize(int size);

        void setFullWaterfallUpdate(bool saveHistory, bool fullUpdate);

        void setBandPlanPos(int pos);

        void setFFTHold(bool hold);
        void setFFTHoldSpeed(float speed);

        void setFFTSmoothing(bool enabled);
        void setFFTSmoothingSpeed(float speed);

        float* acquireLatestFFT(int& width);
        void releaseLatestFFT();

        bool m_centerFreqMoved = false;
        bool m_vfoFreqChanged = false;
        bool m_bandplanEnabled = false;
        bandplan::BandPlan_t* m_bandplan = NULL;

        bool m_mouseInFFTResize = false;
        bool m_mouseInFreq = false;
        bool m_mouseInFFT = false;
        bool m_mouseInWaterfall = false;

        float m_selectedVFOSNR = 0.0f;
        float m_selectedVFOLevel = -INFINITY;    // peak in-bandwidth FFT level of the selected VFO (dBFS)
        float m_selectedVFOLevelMax = -INFINITY; // peak hold of selectedVFOLevel over the last few FFT frames

        bool m_centerFrequencyLocked = false;

        std::map<std::string, WaterfallVFO*> m_map_vfos;
        std::string m_selectedVFO = "";
        bool m_selectedVFOChanged = false;

        struct FFTRedrawArgs {
            ImVec2 min;
            ImVec2 max;
            double lowFreq;
            double highFreq;
            double freqToPixelRatio;
            double pixelToFreqRatio;
            ImGuiWindow* window;
        };

        Event<FFTRedrawArgs> m_onFFTRedraw;

        struct InputHandlerArgs {
            ImVec2 fftRectMin;
            ImVec2 fftRectMax;
            ImVec2 freqScaleRectMin;
            ImVec2 freqScaleRectMax;
            ImVec2 waterfallRectMin;
            ImVec2 waterfallRectMax;
            double lowFreq;
            double highFreq;
            double freqToPixelRatio;
            double pixelToFreqRatio;
        };

        bool m_inputHandled = false;
        bool m_VFOMoveSingleClick = false;
        Event<InputHandlerArgs> m_onInputProcess;

        enum {
            REF_LOWER,
            REF_CENTER,
            REF_UPPER,
            _REF_COUNT
        };

        enum {
            BANDPLAN_POS_BOTTOM,
            BANDPLAN_POS_TOP,
            _BANDPLAN_POS_COUNT
        };

        ImVec2 m_fftAreaMin;
        ImVec2 m_fftAreaMax;
        ImVec2 m_freqAreaMin;
        ImVec2 m_freqAreaMax;
        ImVec2 m_wfMin;
        ImVec2 m_wfMax;

    private:
        void drawWaterfall();
        void drawFFT();
        void drawVFOs();
        void drawBandPlan();
        void processInputs(bool fftResizePillVisible, const ImVec2& fftResizePillCenter);
        // Pan the view by viewDelta Hz: positive moves the view up in frequency, sliding the
        // spectrum left on screen. Callers negate as their input device requires.
        void panView(double viewDelta, bool respectCenterFrequencyLock = false);
        void onPositionChange();
        void onResize();
        void updateWaterfallFb();
        void updateWaterfallTexture();
        void updateAllVFOs(bool checkRedrawRequired = false);
        bool calculateVFOSignalInfo(float* fftLine, WaterfallVFO* vfo, float& strength, float& snr);
        bool collectAutorangeBins(std::vector<float>& out, int lines); // pooled visible raw bins minus edges/DC notch

        bool m_waterfallUpdate = false;

        uint32_t m_waterfallPallet[WATERFALL_RESOLUTION];

        ImVec2 m_widgetPos;
        ImVec2 m_widgetEndPos;
        ImVec2 m_widgetSize;

        ImVec2 m_lastWidgetPos;
        ImVec2 m_lastWidgetSize;
        uint64_t m_lastScaleEpoch = 0;

        ImGuiWindow* m_window;

        GLuint m_textureId;

        std::recursive_mutex m_buf_mtx;
        std::recursive_mutex m_latestFFTMtx;
        std::mutex m_texMtx;
        std::mutex m_smoothingBufMtx;

        float m_vRange;

        int m_maxVSteps;
        int m_maxHSteps;

        int m_dataWidth;           // Width of the FFT and waterfall
        int m_fftHeight;           // Height of the fft graph
        int m_waterfallHeight = 0; // Height of the waterfall

        double m_viewBandwidth;
        double m_viewOffset;


        size_t m_flagsFreqMoved = 0;

        // Absolute values
        double m_centerFreq;     //центральная частота
        double m_wholeBandwidth; //окно приема

        double m_lowerFreq; //минимальная частота (левая)
        double m_upperFreq; //максимальная частота (правая)
        double m_range;

        float m_lastDrag;

        int m_vfoRef = REF_CENTER;


        // Ranges
        float m_fftMin;
        float m_fftMax;
        float m_waterfallMin;
        float m_waterfallMax;


        int m_rawFFTWidth;
        int m_rawFFTHeight = 0;
        float* m_rawFFTs = NULL;
        float* m_latestFFT = NULL;
        float* m_latestFFTHold = NULL;
        float* m_smoothingBuf = NULL;
        std::vector<float> m_autorangeVals;    // pooled surviving-bin buffer for the range estimators
        int m_currentFFTLine = 0;
        int m_fftLines = 0;

        uint32_t* m_waterfallFb;

        bool m_draggingFW = false;
        int m_FFTAreaHeight;
        int m_newFFTAreaHeight;

        bool m_waterfallVisible = true;
        bool m_bandplanVisible = false;

        bool m_saveHistory = true;
        bool m_fullUpdate = true;

        int m_bandPlanPos = BANDPLAN_POS_BOTTOM;

        bool m_fftHold = false;
        float m_fftHoldSpeed = 0.3f;

        bool m_fftSmoothing = false;
        float m_fftSmoothingAlpha = 0.5;
        float m_fftSmoothingBeta = 0.5;

        // Shared rolling window over the last few FFT frames for the selected
        // VFO: the peak readout holds the max in-band level, the noise floor is
        // the out-of-VFO-subband average over the same window, and the SNR is
        // the held peak over that averaged noise. Buffers share
        // levelHistoryPos/Count and reset together.
        static constexpr int LEVEL_HOLD_FRAMES = 10;
        float m_levelHistory[LEVEL_HOLD_FRAMES];
        float m_noiseHistory[LEVEL_HOLD_FRAMES];
        int m_levelHistoryPos = 0;
        int m_levelHistoryCount = 0;
        std::string m_levelHistoryVFO = ""; // VFO the level history belongs to

        // UI Select elements
        bool m_fftResizeSelect = false;
        bool m_fftResizePending = false;    // Android: touch near the divider, drag direction not decided yet
        ImVec2 m_fftResizePendingPos;
        float m_fftResizeGrabOffset = 0.0f; // grab-point offset so the divider doesn't jump to the finger/cursor
        bool m_freqScaleSelect = false;
        bool m_vfoSelect = false;           // first press selected a different VFO; suppress input until release
        bool m_vfoBorderSelect = false;
        WaterfallVFO* m_relatedVfo = NULL;
        ImVec2 m_mouseDownPos;

        ImVec2 m_lastMousePos;
    };
};
