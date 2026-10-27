#include <gui/widgets/waterfall.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imutils.h>
#include <algorithm>
#include <cmath>
#include <volk/volk.h>
#include <utils/flog.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <gui/tuner.h>
#ifdef __ANDROID__
#include <android_backend.h>
#endif

float DEFAULT_COLOR_MAP[][3] = {
    { 0x00, 0x00, 0x20 },
    { 0x00, 0x00, 0x30 },
    { 0x00, 0x00, 0x50 },
    { 0x00, 0x00, 0x91 },
    { 0x1E, 0x90, 0xFF },
    { 0xFF, 0xFF, 0xFF },
    { 0xFF, 0xFF, 0x00 },
    { 0xFE, 0x6D, 0x16 },
    { 0xFF, 0x00, 0x00 },
    { 0xC6, 0x00, 0x00 },
    { 0x9F, 0x00, 0x00 },
    { 0x75, 0x00, 0x00 },
    { 0x4A, 0x00, 0x00 }
};

// TODO: Fix this hacky BS

double freq_ranges[] = {
    1.0, 2.0, 2.5, 5.0,
    10.0, 20.0, 25.0, 50.0,
    100.0, 200.0, 250.0, 500.0,
    1000.0, 2000.0, 2500.0, 5000.0,
    10000.0, 20000.0, 25000.0, 50000.0,
    100000.0, 200000.0, 250000.0, 500000.0,
    1000000.0, 2000000.0, 2500000.0, 5000000.0,
    10000000.0, 20000000.0, 25000000.0, 50000000.0
};

inline double findBestRange(double bandwidth, int maxSteps) {
    for (int i = 0; i < 32; i++) {
        if (bandwidth / freq_ranges[i] < (double)maxSteps) {
            return freq_ranges[i];
        }
    }
    return 50000000.0;
}

inline void printAndScale(double freq, char* buf) {
    double freqAbs = fabs(freq);
    if (freqAbs < 1000) {
        sprintf(buf, "%.6g", freq);
    }
    else if (freqAbs < 1000000) {
        sprintf(buf, "%.6lgK", freq / 1000.0);
    }
    else if (freqAbs < 1000000000) {
        sprintf(buf, "%.6lgM", freq / 1000000.0);
    }
    else if (freqAbs < 1000000000000) {
        sprintf(buf, "%.6lgG", freq / 1000000000.0);
    }
}

inline void doZoom(int offset, int width, int inSize, int outSize, float* in, float* out) {
    // NOTE: REMOVE THAT SHIT, IT'S JUST A HACKY FIX
    if (offset < 0) {
        offset = 0;
    }
    if (width > 524288) {
        width = 524288;
    }

    float factor = (float)width / (float)outSize;
    float sFactor = ceilf(factor);
    float uFactor;
    float id = offset;
    float maxVal;
    int sId;
    for (int i = 0; i < outSize; i++) {
        maxVal = -INFINITY;
        sId = (int)id;
        uFactor = (sId + sFactor > inSize) ? sFactor - ((sId + sFactor) - inSize) : sFactor;
        for (int j = 0; j < uFactor; j++) {
            if (in[sId + j] > maxVal) { maxVal = in[sId + j]; }
        }
        out[i] = maxVal;
        id += factor;
    }
}

namespace ImGui {
    WaterFall::WaterFall() {
        m_fftMin = -70.0;
        m_fftMax = 0.0;
        m_waterfallMin = -70.0;
        m_waterfallMax = 0.0;
        m_FFTAreaHeight = 300;
        m_newFFTAreaHeight = m_FFTAreaHeight;
        m_fftHeight = m_FFTAreaHeight - 50;
        m_dataWidth = 600;
        m_lastWidgetPos.x = 0;
        m_lastWidgetPos.y = 0;
        m_lastWidgetSize.x = 0;
        m_lastWidgetSize.y = 0;
        m_latestFFT = new float[m_dataWidth];
        m_latestFFTHold = new float[m_dataWidth];
        // Fill with the "hide everything" sentinel so the buffers hold a
        // well-defined value before the first FFT frame / onResize() fill.
        for (int i = 0; i < m_dataWidth; i++) {
            m_latestFFT[i] = -1000.0f;
            m_latestFFTHold[i] = -1000.0f;
        }
        m_waterfallFb = new uint32_t[1];

        m_viewBandwidth = 1.0;
        m_wholeBandwidth = 1.0;

        updatePallette(DEFAULT_COLOR_MAP, 13);
    }

    void WaterFall::init() {
        glGenTextures(1, &m_textureId);
    }

    void WaterFall::drawFFT() {
        std::lock_guard<std::recursive_mutex> lck(m_latestFFTMtx);
        // Calculate scaling factor
        float startLine = floorf(m_fftMax / m_vRange) * m_vRange;
        float vertRange = m_fftMax - m_fftMin;
        float scaleFactor = m_fftHeight / vertRange;
        char buf[100];

        ImU32 trace = ImGui::GetColorU32(ImGuiCol_PlotLines);
        ImU32 traceHold = ImGui::ColorConvertFloat4ToU32(gui::themeManager.fftHoldColor);
        ImU32 shadow = ImGui::GetColorU32(ImGuiCol_PlotLines, 0.2);
        ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
        float textVOffset = (float)style::scale(10.0f);

        // Vertical scale
        for (float line = startLine; line > m_fftMin; line -= m_vRange) {
            float yPos = m_fftAreaMax.y - ((line - m_fftMin) * scaleFactor);
            m_window->DrawList->AddLine(ImVec2(m_fftAreaMin.x, roundf(yPos)),
                                      ImVec2(m_fftAreaMax.x, roundf(yPos)),
                                      IM_COL32(50, 50, 50, 255), style::lineWidth());
            sprintf(buf, "%d", (int)line);
            ImVec2 txtSz = ImGui::CalcTextSize(buf);
            m_window->DrawList->AddText(ImVec2(m_fftAreaMin.x - txtSz.x - textVOffset, roundf(yPos - (txtSz.y / 2.0))), text, buf);
        }

        // Horizontal scale
        double startFreq = ceilf(m_lowerFreq / m_range) * m_range;
        double horizScale = (double)m_dataWidth / m_viewBandwidth;
        float scaleVOfsset = (float)style::scale(7.0f);
        for (double freq = startFreq; freq < m_upperFreq; freq += m_range) {
            double xPos = m_fftAreaMin.x + ((freq - m_lowerFreq) * horizScale);
            m_window->DrawList->AddLine(ImVec2(roundf(xPos), m_fftAreaMin.y + 1),
                                      ImVec2(roundf(xPos), m_fftAreaMax.y),
                                      IM_COL32(50, 50, 50, 255), style::lineWidth());
            m_window->DrawList->AddLine(ImVec2(roundf(xPos), m_fftAreaMax.y),
                                      ImVec2(roundf(xPos), m_fftAreaMax.y + scaleVOfsset),
                                      text, style::lineWidth());
            printAndScale(freq, buf);
            ImVec2 txtSz = ImGui::CalcTextSize(buf);
            m_window->DrawList->AddText(ImVec2(roundf(xPos - (txtSz.x / 2.0)), m_fftAreaMax.y + txtSz.y), text, buf);
        }

        // Data
        if (m_latestFFT != NULL && m_fftLines != 0) {
            for (int i = 1; i < m_dataWidth; i++) {
                double aPos = m_fftAreaMax.y - ((m_latestFFT[i - 1] - m_fftMin) * scaleFactor);
                double bPos = m_fftAreaMax.y - ((m_latestFFT[i] - m_fftMin) * scaleFactor);
                aPos = std::clamp<double>(aPos, m_fftAreaMin.y + 1, m_fftAreaMax.y);
                bPos = std::clamp<double>(bPos, m_fftAreaMin.y + 1, m_fftAreaMax.y);
                m_window->DrawList->AddLine(ImVec2(m_fftAreaMin.x + i - 1, roundf(aPos)),
                                          ImVec2(m_fftAreaMin.x + i, roundf(bPos)), trace, 1.0);
                m_window->DrawList->AddLine(ImVec2(m_fftAreaMin.x + i, roundf(bPos)),
                                          ImVec2(m_fftAreaMin.x + i, m_fftAreaMax.y), shadow, 1.0);
            }
        }

        // Hold
        if (m_fftHold && m_latestFFT != NULL && m_latestFFTHold != NULL && m_fftLines != 0) {
            for (int i = 1; i < m_dataWidth; i++) {
                double aPos = m_fftAreaMax.y - ((m_latestFFTHold[i - 1] - m_fftMin) * scaleFactor);
                double bPos = m_fftAreaMax.y - ((m_latestFFTHold[i] - m_fftMin) * scaleFactor);
                aPos = std::clamp<double>(aPos, m_fftAreaMin.y + 1, m_fftAreaMax.y);
                bPos = std::clamp<double>(bPos, m_fftAreaMin.y + 1, m_fftAreaMax.y);
                m_window->DrawList->AddLine(ImVec2(m_fftAreaMin.x + i - 1, roundf(aPos)),
                                          ImVec2(m_fftAreaMin.x + i, roundf(bPos)), traceHold, 1.0);
            }
        }

        FFTRedrawArgs args;
        args.min = m_fftAreaMin;
        args.max = m_fftAreaMax;
        args.lowFreq = m_lowerFreq;
        args.highFreq = m_upperFreq;
        args.freqToPixelRatio = horizScale;
        args.pixelToFreqRatio = m_viewBandwidth / (double)m_dataWidth;
        args.window = m_window;
        m_onFFTRedraw.emit(args);

        // X Axis
        m_window->DrawList->AddLine(ImVec2(m_fftAreaMin.x, m_fftAreaMax.y),
                                  ImVec2(m_fftAreaMax.x, m_fftAreaMax.y),
                                  text, style::lineWidth());
        // Y Axis
        m_window->DrawList->AddLine(ImVec2(m_fftAreaMin.x, m_fftAreaMin.y),
                                  ImVec2(m_fftAreaMin.x, m_fftAreaMax.y - 1),
                                  text, style::lineWidth());
    }

    void WaterFall::drawWaterfall() {
        if (m_waterfallUpdate) {
            m_waterfallUpdate = false;
            updateWaterfallTexture();
        }
        {
            std::lock_guard<std::mutex> lck(m_texMtx);
            m_window->DrawList->AddImage((void*)(intptr_t)m_textureId, m_wfMin, m_wfMax);
        }
        
        ImVec2 mPos = ImGui::GetMousePos();

        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) &&
            IS_IN_AREA(mPos, m_wfMin, m_wfMax) && !gui::mainWindow.lockWaterfallControls && !m_inputHandled) {
            for (auto const& [name, vfo] : m_map_vfos) {
                m_window->DrawList->AddRectFilled(vfo->m_wfRectMin, vfo->m_wfRectMax, vfo->m_color);
                if (!vfo->m_lineVisible) { continue; }
                m_window->DrawList->AddLine(vfo->m_wfLineMin, vfo->m_wfLineMax, (name == m_selectedVFO) ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 0, 255), style::lineWidth());
            }
        }
    }

    void WaterFall::drawVFOs() {
        for (auto const& [name, vfo] : m_map_vfos) {
            vfo->draw(m_window, name == m_selectedVFO);
        }
    }

    // Pan view / spectrum when dragging the frequency scale or when mouse wheeling or left / right keys
    // and the mouse hovers over the frequency scale.
    // viewDelta moves the view, not the trace: positive shifts the view up in frequency, which slides
    // the spectrum left on screen. Each caller negates as its input device requires, so that the
    // device convention ("dragging right pulls the trace right") stays visible at the call site.
    //FIXME respectCenterFrequencyLock parameter is questionable.
    // Maybe it shall be dropped and the lock shall be respected always?
    void WaterFall::panView(double viewDelta, bool respectCenterFrequencyLock) {
        // Drag handling calls this every frame while the button is held. Do not
        // report a center-frequency change until the pointer actually moves.
        if (viewDelta == 0.0) { return; }

        if (tuner::vfoLockedToCenter()) {
            // The VFO is synchronized with the IQ center frequency and the VFO is locked to the center of the view
            // or as close as possible to the center of the view considering the zoom factor.
            // See normalTuningLocked()
            // Tune the radio while maintaining the view offset and view bandwidth.
            //FIXME respectCenterFrequencyLock is ignored on purpose.
            m_centerFreq += viewDelta;
            m_centerFreqMoved = true;
        } else {
            m_viewOffset += viewDelta;
            if (m_viewOffset + (m_viewBandwidth / 2.0) > m_wholeBandwidth / 2.0) {
                double freqOffset = (m_viewOffset + (m_viewBandwidth / 2.0)) - (m_wholeBandwidth / 2.0);
                m_viewOffset = (m_wholeBandwidth / 2.0) - (m_viewBandwidth / 2.0);
                if (!respectCenterFrequencyLock || !m_centerFrequencyLocked) {
                    m_centerFreq += freqOffset;
                    m_centerFreqMoved = true;
                }
            }
            if (m_viewOffset - (m_viewBandwidth / 2.0) < -(m_wholeBandwidth / 2.0)) {
                double freqOffset = (m_viewOffset - (m_viewBandwidth / 2.0)) + (m_wholeBandwidth / 2.0);
                m_viewOffset = (m_viewBandwidth / 2.0) - (m_wholeBandwidth / 2.0);
                if (!respectCenterFrequencyLock || !m_centerFrequencyLocked) {
                    m_centerFreq += freqOffset;
                    m_centerFreqMoved = true;
                }
            }
        }

        m_lowerFreq = (m_centerFreq + m_viewOffset) - (m_viewBandwidth / 2.0);
        m_upperFreq = (m_centerFreq + m_viewOffset) + (m_viewBandwidth / 2.0);
        if(m_flagsFreqMoved == 1)
            m_flagsFreqMoved = 4;

        // updateAllVFOs() and updateWaterfallFb() do nothing with tuner::vfoLockedToCenter(), but that may change in the future.
        if (! tuner::vfoLockedToCenter() && m_viewBandwidth != m_wholeBandwidth) {
            updateAllVFOs();
            if (m_fullUpdate) { updateWaterfallFb(); };
        }
        if(m_centerFreqMoved && gui::mainWindow.getTuningMode() == tuner::TUNER_MODE_NORMAL)
        {
            for(auto const& [name, vfo] : m_map_vfos) {
                double newOffset = vfo->m_generalOffset - viewDelta;
                double testOffset = m_centerFreq + newOffset;
                if(testOffset < m_lowerFreq)
                    newOffset = - (m_viewBandwidth / 2.0);
                if(testOffset > m_upperFreq)
                    newOffset = m_viewBandwidth / 2.0;
                vfo->setOffset(newOffset);
            }
        }
    }

    void WaterFall::selectFirstVFO() {
        // Only signal a change when the selection actually moved: this is called
        // on every teardown and on startup with no VFOs yet, and a spurious
        // selectedVFOChanged makes listeners re-run their VFO switch handling.
        std::string first = m_map_vfos.empty() ? "" : m_map_vfos.begin()->first;
        if (first == m_selectedVFO) { return; }
        m_selectedVFO = first;
        m_selectedVFOChanged = true;
    }

    void WaterFall::processInputs(bool fftResizePillVisible, const ImVec2& fftResizePillCenter) {
        // Pre calculate useful values
        WaterfallVFO* selVfo = NULL;
        if (m_selectedVFO != "") {
            selVfo = m_map_vfos[m_selectedVFO];
        }
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        ImVec2 dragOrigin(mousePos.x - drag.x, mousePos.y - drag.y);

        bool mouseHovered, mouseHeld;
        bool mouseClicked = ImGui::ButtonBehavior(ImRect(m_fftAreaMin, m_wfMax), GetID("WaterfallID"), &mouseHovered, &mouseHeld,
                                                  ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_PressedOnClick);

        float splitterY = m_widgetPos.y + m_newFFTAreaHeight;
        bool mouseInFFTResizePill = false;
        m_mouseInFFTResize = false;
        if (m_waterfallVisible) {
            if (fftResizePillVisible) {
                // A 2 dp line cannot be hit reliably with a finger, so touch
                // style adds a visible pill that grabs immediately. This is a
                // visual/style choice independent of the platform-wide band
                // below: a touch-screen desktop needs the pill too.
                mouseInFFTResizePill = fabsf(dragOrigin.x - fftResizePillCenter.x) <= style::dp(24.0f) &&
                                       fabsf(dragOrigin.y - fftResizePillCenter.y) <= style::dp(18.0f);
            }

#ifdef __ANDROID__
            // Android always needs a finger-sized band, even if the optional
            // touch styling is disabled. Start it at the FFT area so it stays
            // clear of the menu splitter handle over the dB-scale strip.
            float separatorHitRadius = style::dp(12.0f);
            bool mouseInFFTResizeBand = dragOrigin.x > m_fftAreaMin.x && dragOrigin.x < m_fftAreaMax.x &&
                                        dragOrigin.y >= splitterY - separatorHitRadius && dragOrigin.y <= splitterY + separatorHitRadius;
#else
            // Desktop keeps a precise divider target. Touch style adds the pill
            // above instead of making this entire band steal nearby input.
            float separatorHitRadius = style::dp(2.0f);
            bool mouseInFFTResizeBand = dragOrigin.x > m_widgetPos.x && dragOrigin.x < m_widgetPos.x + m_widgetSize.x &&
                                        dragOrigin.y >= splitterY - separatorHitRadius && dragOrigin.y <= splitterY + separatorHitRadius;
#endif
            m_mouseInFFTResize = mouseInFFTResizePill || mouseInFFTResizeBand;
        }
        m_mouseInFreq = IS_IN_AREA(dragOrigin, m_freqAreaMin, m_freqAreaMax);
        m_mouseInFFT = IS_IN_AREA(dragOrigin, m_fftAreaMin, m_fftAreaMax);
        m_mouseInWaterfall = IS_IN_AREA(dragOrigin, m_wfMin, m_wfMax);

        float mouseWheel = ImGui::GetIO().MouseWheel;

        bool mouseMoved = false;
        if (mousePos.x != m_lastMousePos.x || mousePos.y != m_lastMousePos.y) { mouseMoved = true; }
        m_lastMousePos = mousePos;

        std::string hoveredVFOName;
        for (auto const& [name, _vfo] : m_map_vfos) {
            if (ImGui::IsMouseHoveringRect(_vfo->m_rectMin, _vfo->m_rectMax) || ImGui::IsMouseHoveringRect(_vfo->m_wfRectMin, _vfo->m_wfRectMax)) {
                hoveredVFOName = name;
                break;
            }
        }
        // An initial press on empty spectrum or on the already-selected VFO
        // may tune immediately. A different VFO is handled as selection below.
        bool allowVfoClickMove = hoveredVFOName.empty() || m_selectedVFO == hoveredVFOName;

        // Deselect everything if the mouse is released
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (m_fftResizeSelect) {
                m_FFTAreaHeight = m_newFFTAreaHeight;
                onResize();
#ifdef __ANDROID__
                backend::hapticTick();
#endif
            }
#ifdef __ANDROID__
            if (m_fftResizePending) {
                // The touch was held back waiting for its drag direction and
                // turned out to be a plain tap — give it its original meaning:
                // select the VFO under the finger, or tune to the tap.
                m_fftResizePending = false;
                if (hoveredVFOName != "") {
                    m_selectedVFO = hoveredVFOName;
                    m_selectedVFOChanged = true;
                }
                else if (selVfo != NULL && (m_mouseInFFT || m_mouseInWaterfall)) {
                    int refCenter = mousePos.x - m_fftAreaMin.x;
                    if (refCenter >= 0 && refCenter < m_dataWidth) {
                        double off = ((((double)refCenter / ((double)m_dataWidth / 2.0)) - 1.0) * (m_viewBandwidth / 2.0)) + m_viewOffset;
                        off += m_centerFreq;
                        off = (round(off / selVfo->m_snapInterval) * selVfo->m_snapInterval) - m_centerFreq;
                        selVfo->setOffset(off);
                    }
                }
            }
#endif

            m_fftResizeSelect = false;
            m_freqScaleSelect = false;
            m_vfoBorderSelect = false;
            m_lastDrag = 0;
        }

        bool targetFound = false;

        // If the mouse was clicked anywhere in the waterfall, check if the resize was clicked
        if (m_mouseInFFTResize) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
#ifdef __ANDROID__
                if (!mouseInFFTResizePill) {
                    // Not on the pill — hold the Android touch back until its
                    // drag direction is known (resolved below). This remains a
                    // platform behavior when touch styling, and therefore the
                    // optional immediate-grab pill, is disabled.
                    m_fftResizePending = true;
                    m_fftResizePendingPos = mousePos;
                }
                else {
                    m_fftResizeSelect = true;
                    m_fftResizeGrabOffset = splitterY - mousePos.y;
                    backend::hapticTick();
                }
#else
                m_fftResizeSelect = true;
                m_fftResizeGrabOffset = splitterY - mousePos.y;
#endif
                targetFound = true;
            }
        }

#ifdef __ANDROID__
        // A touch near the divider is on hold until its direction is known:
        // across the divider = resize, along it = the gesture the band would
        // otherwise steal (freq-scale pan or VFO tuning).
        if (m_fftResizePending) {
            float dx = mousePos.x - m_fftResizePendingPos.x;
            float dy = mousePos.y - m_fftResizePendingPos.y;
            if (std::max(fabsf(dx), fabsf(dy)) < style::dp(6.0f)) {
                return; // still undecided — swallow the touch for now
            }
            m_fftResizePending = false;
            if (fabsf(dy) > fabsf(dx)) {
                m_fftResizeSelect = true;
                m_fftResizeGrabOffset = splitterY - m_fftResizePendingPos.y;
                backend::hapticTick();
            }
            else if (m_mouseInFreq) {
                m_freqScaleSelect = true;
            }
            // else: the touch started in the waterfall part of the band; the
            // VFO-move branch below keys off the same drag origin and takes over.
        }
#endif

        // If mouse was clicked inside the central part, check what was clicked
        if (mouseClicked && !targetFound) {
            m_mouseDownPos = mousePos;

            // First, check if a VFO border was selected
            for (auto const& [name, _vfo] : m_map_vfos) {
                if (_vfo->m_bandwidthLocked) { continue; }
                if (_vfo->m_rectMax.x - _vfo->m_rectMin.x < 10) { continue; }
                bool resizing = false;
                if (_vfo->m_reference != REF_LOWER) {
                    if (IS_IN_AREA(mousePos, _vfo->m_lbwSelMin, _vfo->m_lbwSelMax)) { resizing = true; }
                    else if (IS_IN_AREA(mousePos, _vfo->m_wfLbwSelMin, _vfo->m_wfLbwSelMax)) {
                        resizing = true;
                    }
                }
                if (_vfo->m_reference != REF_UPPER) {
                    if (IS_IN_AREA(mousePos, _vfo->m_rbwSelMin, _vfo->m_rbwSelMax)) { resizing = true; }
                    else if (IS_IN_AREA(mousePos, _vfo->m_wfRbwSelMin, _vfo->m_wfRbwSelMax)) {
                        resizing = true;
                    }
                }
                if (!resizing) { continue; }
                m_relatedVfo = _vfo;
                m_vfoBorderSelect = true;
                targetFound = true;
                break;
            }

            // Next, check if a VFO was selected
            if (!targetFound && !hoveredVFOName.empty()) {
                if (m_selectedVFO != hoveredVFOName) {
                    m_selectedVFO = hoveredVFOName;
                    m_selectedVFOChanged = true;
                    // Selecting and moving are separate gestures. Latch this
                    // press until its release so motion from a finger (or a
                    // slightly moving mouse) cannot immediately drag the VFO.
                    m_vfoSelect = true;
                    // Clear hit state for the rest of this frame so the
                    // main-window wheel and arrow handlers stay inactive too.
                    m_mouseInFFTResize = false;
                    m_mouseInFreq = false;
                    m_mouseInFFT = false;
                    m_mouseInWaterfall = false;
                    targetFound = true;
                    return;
                }
            }

            // Now, check frequency scale
            if (!targetFound && m_mouseInFreq) {
                m_freqScaleSelect = true;
            }
        }

        // If the FFT resize bar was selected, resize FFT accordingly
        if (m_fftResizeSelect) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            m_newFFTAreaHeight = mousePos.y + m_fftResizeGrabOffset - m_widgetPos.y;
            m_newFFTAreaHeight = style::clampSplit(m_newFFTAreaHeight, m_widgetSize.y, 150.0f, 50.0f);
            ImGui::GetForegroundDrawList()->AddLine(ImVec2(m_widgetPos.x, m_newFFTAreaHeight + m_widgetPos.y), ImVec2(m_widgetEndPos.x, m_newFFTAreaHeight + m_widgetPos.y),
                                                    ImGui::GetColorU32(ImGuiCol_SeparatorActive), style::lineWidth());
            return;
        }

        // If a vfo border is selected, resize VFO accordingly
        if (m_vfoBorderSelect) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            double dist = (m_relatedVfo->m_reference == REF_CENTER) ? fabsf(mousePos.x - m_relatedVfo->m_lineMin.x) : (mousePos.x - m_relatedVfo->m_lineMin.x);
            if (m_relatedVfo->m_reference == REF_UPPER) { dist = -dist; }
            double hzDist = dist * (m_viewBandwidth / (double)m_dataWidth);
            if (m_relatedVfo->m_reference == REF_CENTER) {
                hzDist *= 2.0;
            }
            hzDist = std::clamp<double>(hzDist, m_relatedVfo->m_minBandwidth, m_relatedVfo->m_maxBandwidth);
            m_relatedVfo->setBandwidth(hzDist);
            m_relatedVfo->m_onUserChangedBandwidth.emit(hzDist);
            return;
        }

        // If the frequency scale is selected, move it
        if (m_freqScaleSelect) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            double deltax = drag.x - m_lastDrag;
            m_lastDrag = drag.x;
            // Negated: dragging right must pull the trace right with the pointer, i.e. move the view down.
            double viewDelta = -deltax * (m_viewBandwidth / (double)m_dataWidth);
            panView(viewDelta, true);
            return;
        }

        // Pinch-to-zoom from the Android gesture recognizer arrives on the
        // horizontal wheel axis (MouseWheelH) to avoid Ctrl key-state timing
        // issues.  Desktop Ctrl+scroll also zooms via the vertical axis.
        // Positive value = fingers apart / scroll up = zoom in (narrower view).
        float zoomWheel = 0.0f;
#ifdef __ANDROID__
        float mouseWheelH = ImGui::GetIO().MouseWheelH;
        if (mouseWheelH != 0.0f && (m_mouseInFFT || m_mouseInWaterfall || m_mouseInFreq))
            zoomWheel = mouseWheelH;
        else
#endif
        if (mouseWheel != 0.0f && ImGui::GetIO().KeyCtrl && (m_mouseInFFT || m_mouseInWaterfall || m_mouseInFreq))
            zoomWheel = mouseWheel;
        if (zoomWheel != 0.0f) {
            double newBw = m_viewBandwidth * std::pow(0.9, (double)zoomWheel);
            newBw = std::clamp(newBw, m_wholeBandwidth / 200.0, m_wholeBandwidth);
            // Anchor the zoom on the frequency under the cursor — the first
            // (tuning) finger when pinching on touch — so that frequency keeps
            // its screen position while the view stretches around it.
            double anchorX = (double)mousePos.x - m_fftAreaMin.x;
            if (newBw != m_viewBandwidth && anchorX >= 0.0 && anchorX < (double)m_dataWidth) {
                double norm = (anchorX / ((double)m_dataWidth / 2.0)) - 1.0;    // -1..1 across the view
                double anchorOff = m_viewOffset + (norm * m_viewBandwidth / 2.0); // Hz relative to center freq
                m_viewOffset = anchorOff - (norm * newBw / 2.0);                // setViewBandwidth clamps to band edges
            }
            setViewBandwidth(newBw);
            return;
        }

        // If the mouse wheel is moved on the frequency scale
        if (mouseWheel != 0.0f && m_mouseInFreq) {
            // Negated to match dragging: scrolling up slides the trace right, i.e. moves the view down.
            double viewDelta = -(double)mouseWheel * m_viewBandwidth / 20.0;
            panView(viewDelta);
            return;
        }

        // If the left and right keys are pressed while hovering the freq scale, move it too
        bool leftKeyPressed = ImGui::IsKeyPressed(ImGuiKey_LeftArrow);
        if ((leftKeyPressed || ImGui::IsKeyPressed(ImGuiKey_RightArrow)) && m_mouseInFreq) {
            double viewDelta = leftKeyPressed ? (m_viewBandwidth / 20.0) : (-m_viewBandwidth / 20.0);
            panView(viewDelta);
            return;
        }

        if(m_flagsFreqMoved && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            if(m_flagsFreqMoved < 4)
            {
                int refCenter = mousePos.x - m_fftAreaMin.x;
                if(refCenter >= 0 && refCenter < m_dataWidth) {
                    double off = ((((double)refCenter / ((double)m_dataWidth / 2.0)) - 1.0) * (m_viewBandwidth / 2.0)) + m_viewOffset;
                    off += m_centerFreq;
                    off = (round(off / selVfo->m_snapInterval) * selVfo->m_snapInterval) - m_centerFreq;
                    selVfo->setOffset(off);
                }
            }
            m_flagsFreqMoved = 0;
        }

        // Finally, if nothing else was selected, just move the VFO
        if ((m_VFOMoveSingleClick ? ImGui::IsMouseClicked(ImGuiMouseButton_Left) : ImGui::IsMouseDown(ImGuiMouseButton_Left)) && (m_mouseInFFT | m_mouseInWaterfall) && (mouseMoved || allowVfoClickMove)) {
            if (selVfo != NULL) {
                if(selVfo->m_rectMin.x  < mousePos.x && selVfo->m_rectMax.x  > mousePos.x)
                    m_flagsFreqMoved = 2;
                if(m_flagsFreqMoved == 2) {
                    int refCenter = mousePos.x - m_fftAreaMin.x;
                    if(refCenter >= 0 && refCenter < m_dataWidth) {
                        double off = ((((double)refCenter / ((double)m_dataWidth / 2.0)) - 1.0) * (m_viewBandwidth / 2.0)) + m_viewOffset;
                        off += m_centerFreq;
                        off = (round(off / selVfo->m_snapInterval) * selVfo->m_snapInterval) - m_centerFreq;
                        selVfo->setOffset(off);
                    }
                }else{
                    if(m_flagsFreqMoved == 0)
                        m_flagsFreqMoved = 1;
                    m_freqScaleSelect = true;
                }
            }
        }
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            m_flagsFreqMoved = 0;
            // Check if a VFO is hovered. If yes, show tooltip
            for (auto const& [name, _vfo] : m_map_vfos) {
                if (ImGui::IsMouseHoveringRect(_vfo->m_rectMin, _vfo->m_rectMax) || ImGui::IsMouseHoveringRect(_vfo->m_wfRectMin, _vfo->m_wfRectMax)) {
                    char buf[128];
                    ImGui::BeginTooltip();

                    ImGui::TextUnformatted(name.c_str());

                    if (ImGui::GetIO().KeyCtrl) {
                        ImGui::Separator();
                        printAndScale(_vfo->m_generalOffset + m_centerFreq, buf);
                        ImGui::Text("Frequency: %sHz", buf);
                        printAndScale(_vfo->m_bandwidth, buf);
                        ImGui::Text("Bandwidth: %sHz", buf);
                        ImGui::Text("Bandwidth Locked: %s", _vfo->m_bandwidthLocked ? "Yes" : "No");

                        // Hold buf_mtx while reading rawFFTs: this runs on the UI
                        // thread, and without the lock the FFT worker (getFFTBuffer/
                        // pushFFT) or onResize() can move currentFFTLine / reallocate
                        // rawFFTs underneath us and produce an out-of-bounds read.
                        float strength, snr;
                        bool infoValid;
                        {
                            std::lock_guard<std::recursive_mutex> lck(m_buf_mtx);
                            infoValid = calculateVFOSignalInfo(m_waterfallVisible ? &m_rawFFTs[m_currentFFTLine * m_rawFFTWidth] : m_rawFFTs, _vfo, strength, snr);
                        }
                        if (infoValid) {
                            ImGui::Text("Strength: %0.1fdBFS", strength);
                            ImGui::Text("SNR: %0.1fdB", snr);
                        }
                        else {
                            ImGui::TextUnformatted("Strength: ---.-dBFS");
                            ImGui::TextUnformatted("SNR: ---.-dB");
                        }
                    }

                    ImGui::EndTooltip();
                    break;
                }
            }
        }

        // Handle Page Up to cycle through VFOs
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp) && selVfo != NULL) {
            std::string next = (--m_map_vfos.end())->first;
            std::string lowest = "";
            double lowestOffset = INFINITY;
            double firstVfoOffset = selVfo->m_generalOffset;
            double smallestDistance = INFINITY;
            bool found = false;
            for (auto& [_name, _vfo] : m_map_vfos) {
                if (_vfo->m_generalOffset > firstVfoOffset && (_vfo->m_generalOffset - firstVfoOffset) < smallestDistance) {
                    next = _name;
                    smallestDistance = (_vfo->m_generalOffset - firstVfoOffset);
                    found = true;
                }
                if (_vfo->m_generalOffset < lowestOffset) {
                    lowestOffset = _vfo->m_generalOffset;
                    lowest = _name;
                }
            }
            m_selectedVFO = found ? next : lowest;
            m_selectedVFOChanged = true;
        }

        // Handle Page Down to cycle through VFOs
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown) && selVfo != NULL) {
            std::string next = (--m_map_vfos.end())->first;
            std::string highest = "";
            double highestOffset = -INFINITY;
            double firstVfoOffset = selVfo->m_generalOffset;
            double smallestDistance = INFINITY;
            bool found = false;
            for (auto& [_name, _vfo] : m_map_vfos) {
                if (_vfo->m_generalOffset < firstVfoOffset && (firstVfoOffset - _vfo->m_generalOffset) < smallestDistance) {
                    next = _name;
                    smallestDistance = (firstVfoOffset - _vfo->m_generalOffset);
                    found = true;
                }
                if (_vfo->m_generalOffset > highestOffset) {
                    highestOffset = _vfo->m_generalOffset;
                    highest = _name;
                }
            }
            m_selectedVFO = found ? next : highest;
            m_selectedVFOChanged = true;
        }
    }

    bool WaterFall::calculateVFOSignalInfo(float* fftLine, WaterfallVFO* _vfo, float& strength, float& snr) {
        if (fftLine == NULL || m_fftLines <= 0) { return false; }

        // Calculate FFT index data
        double vfoMinSizeFreq = _vfo->m_centerOffset - _vfo->m_bandwidth;
        double vfoMinFreq = _vfo->m_centerOffset - (_vfo->m_bandwidth / 2.0);
        double vfoMaxFreq = _vfo->m_centerOffset + (_vfo->m_bandwidth / 2.0);
        double vfoMaxSizeFreq = _vfo->m_centerOffset + _vfo->m_bandwidth;
        int vfoMinSideOffset = std::clamp<int>(((vfoMinSizeFreq / (m_wholeBandwidth / 2.0)) * (double)(m_rawFFTWidth / 2)) + (m_rawFFTWidth / 2), 0, m_rawFFTWidth);
        // Clamped to rawFFTWidth - 1: both are used as inclusive indices below,
        // unlike the side offsets which are only exclusive loop bounds.
        int vfoMinOffset = std::clamp<int>(((vfoMinFreq / (m_wholeBandwidth / 2.0)) * (double)(m_rawFFTWidth / 2)) + (m_rawFFTWidth / 2), 0, m_rawFFTWidth - 1);
        int vfoMaxOffset = std::clamp<int>(((vfoMaxFreq / (m_wholeBandwidth / 2.0)) * (double)(m_rawFFTWidth / 2)) + (m_rawFFTWidth / 2), 0, m_rawFFTWidth - 1);
        int vfoMaxSideOffset = std::clamp<int>(((vfoMaxSizeFreq / (m_wholeBandwidth / 2.0)) * (double)(m_rawFFTWidth / 2)) + (m_rawFFTWidth / 2), 0, m_rawFFTWidth);

        double avg = 0;
        float max = -INFINITY;
        int avgCount = 0;

        // Calculate Left average
        for (int i = vfoMinSideOffset; i < vfoMinOffset; i++) {
            avg += fftLine[i];
            avgCount++;
        }

        // Calculate Right average
        for (int i = vfoMaxOffset + 1; i < vfoMaxSideOffset; i++) {
            avg += fftLine[i];
            avgCount++;
        }

        // A VFO spanning the whole sampled band (or clamped against both FFT
        // edges) leaves no out-of-band bins, so there is no noise reference and
        // SNR is undefined. Report no info rather than dividing by zero: a NaN
        // here poisons the caller's rolling noise window for LEVEL_HOLD_FRAMES.
        if (avgCount == 0) { return false; }
        avg /= (double)(avgCount);

        // Calculate max
        for (int i = vfoMinOffset; i <= vfoMaxOffset; i++) {
            if (fftLine[i] > max) { max = fftLine[i]; }
        }

        strength = max;
        snr = max - avg;

        return true;
    }

    void WaterFall::updateWaterfallFb() {
        if (!m_waterfallVisible || m_rawFFTs == NULL || !m_saveHistory) {
            return;
        }
        double offsetRatio = m_viewOffset / (m_wholeBandwidth / 2.0);
        int drawDataSize;
        int drawDataStart;
        // TODO: Maybe put on the stack for faster alloc?
        float* tempData = new float[m_dataWidth];
        float pixel;
        float dataRange = m_waterfallMax - m_waterfallMin;
        int count = std::min<float>(m_rawFFTHeight, m_fftLines);
        if (m_rawFFTs != NULL && m_fftLines >= 0) {
            for (int i = 0; i < count; i++) {
                drawDataSize = (m_viewBandwidth / m_wholeBandwidth) * m_rawFFTWidth;
                drawDataStart = (((double)m_rawFFTWidth / 2.0) * (offsetRatio + 1)) - (drawDataSize / 2);
                doZoom(drawDataStart, drawDataSize, m_rawFFTWidth, m_dataWidth, &m_rawFFTs[((i + m_currentFFTLine) % m_rawFFTHeight) * m_rawFFTWidth], tempData);
                for (int j = 0; j < m_dataWidth; j++) {
                    pixel = (std::clamp<float>(tempData[j], m_waterfallMin, m_waterfallMax) - m_waterfallMin) / dataRange;
                    m_waterfallFb[(i * m_dataWidth) + j] = m_waterfallPallet[(int)(pixel * (WATERFALL_RESOLUTION - 1))];
                }
            }

            for (int i = count; i < m_waterfallHeight; i++) {
                for (int j = 0; j < m_dataWidth; j++) {
                    m_waterfallFb[(i * m_dataWidth) + j] = (uint32_t)255 << 24;
                }
            }
        }
        delete[] tempData;
        m_waterfallUpdate = true;
    }

    void WaterFall::drawBandPlan() {
        int count = m_bandplan->bands.size();
        double horizScale = (double)m_dataWidth / m_viewBandwidth;
        double start, end, center, aPos, bPos, cPos, width;
        ImVec2 txtSz;
        bool startVis, endVis;
        uint32_t color, colorTrans;

        float height = ImGui::CalcTextSize("0").y * 2.5f;
        float bpBottom;

        if (m_bandPlanPos == BANDPLAN_POS_BOTTOM) {
            bpBottom = m_fftAreaMax.y;
        }
        else {
            bpBottom = m_fftAreaMin.y + height + 1;
        }


        for (int i = 0; i < count; i++) {
            start = m_bandplan->bands[i].start;
            end = m_bandplan->bands[i].end;
            if (start < m_lowerFreq && end < m_lowerFreq) {
                continue;
            }
            if (start > m_upperFreq && end > m_upperFreq) {
                continue;
            }
            startVis = (start > m_lowerFreq);
            endVis = (end < m_upperFreq);
            start = std::clamp<double>(start, m_lowerFreq, m_upperFreq);
            end = std::clamp<double>(end, m_lowerFreq, m_upperFreq);
            center = (start + end) / 2.0;
            aPos = m_fftAreaMin.x + ((start - m_lowerFreq) * horizScale);
            bPos = m_fftAreaMin.x + ((end - m_lowerFreq) * horizScale);
            cPos = m_fftAreaMin.x + ((center - m_lowerFreq) * horizScale);
            width = bPos - aPos;
            txtSz = ImGui::CalcTextSize(m_bandplan->bands[i].name.c_str());
            if (bandplan::colorTable.find(m_bandplan->bands[i].type.c_str()) != bandplan::colorTable.end()) {
                color = bandplan::colorTable[m_bandplan->bands[i].type].colorValue;
                colorTrans = bandplan::colorTable[m_bandplan->bands[i].type].transColorValue;
            }
            else {
                color = IM_COL32(255, 255, 255, 255);
                colorTrans = IM_COL32(255, 255, 255, 100);
            }
            if (aPos <= m_fftAreaMin.x) {
                aPos = m_fftAreaMin.x + 1;
            }
            if (bPos <= m_fftAreaMin.x) {
                bPos = m_fftAreaMin.x + 1;
            }
            if (width >= 1.0) {
                m_window->DrawList->AddRectFilled(ImVec2(roundf(aPos), bpBottom - height),
                                                ImVec2(roundf(bPos), bpBottom), colorTrans);
                if (startVis) {
                    m_window->DrawList->AddLine(ImVec2(roundf(aPos), bpBottom - height - 1),
                                              ImVec2(roundf(aPos), bpBottom - 1), color, style::lineWidth());
                }
                if (endVis) {
                    m_window->DrawList->AddLine(ImVec2(roundf(bPos), bpBottom - height - 1),
                                              ImVec2(roundf(bPos), bpBottom - 1), color, style::lineWidth());
                }
            }
            if (txtSz.x <= width) {
                m_window->DrawList->AddText(ImVec2(cPos - (txtSz.x / 2.0), bpBottom - (height / 2.0f) - (txtSz.y / 2.0f)),
                                          IM_COL32(255, 255, 255, 255), m_bandplan->bands[i].name.c_str());
            }
        }
    }

    void WaterFall::updateWaterfallTexture() {
        std::lock_guard<std::mutex> lck(m_texMtx);
        glBindTexture(GL_TEXTURE_2D, m_textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_dataWidth, m_waterfallHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, (uint8_t*)m_waterfallFb);
    }

    void WaterFall::onPositionChange() {
        // Nothing to see here...
    }

    void WaterFall::onResize() {
        // buf_mtx must be held first: it guards rawFFTs, currentFFTLine and
        // waterfallHeight, which the FFT worker reads/writes in getFFTBuffer()/
        // pushFFT(). onResize() reallocates rawFFTs and rewrites those fields, so
        // without this lock the worker can index the old (smaller) allocation with
        // a currentFFTLine sized for the new waterfallHeight and read out of bounds
        // (e.g. on SpyServer connect, when FFT frames arrive mid-relayout). It is a
        // recursive_mutex and the other onResize() call sites already hold it, so
        // re-locking there is a no-op. Lock order: buf_mtx -> latestFFTMtx -> smoothingBufMtx.
        std::lock_guard<std::recursive_mutex> lck0(m_buf_mtx);
        std::lock_guard<std::recursive_mutex> lck(m_latestFFTMtx);
        std::lock_guard<std::mutex> lck2(m_smoothingBufMtx);
        // return if widget is too small
        if (m_widgetSize.x < style::dp(100.0f) || m_widgetSize.y < style::dp(100.0f)) {
            return;
        }

        int lastWaterfallHeight = m_waterfallHeight;
        int lastFftHeight = m_fftHeight;
        int lastRawFFTHeight = m_rawFFTHeight;
        int lastDataWidth = m_dataWidth;

        if (m_waterfallVisible) {
            int effectiveFFTAreaHeight = style::clampSplit(m_FFTAreaHeight, m_widgetSize.y, 150.0f, 50.0f);
            m_newFFTAreaHeight = effectiveFFTAreaHeight;
            m_fftHeight = effectiveFFTAreaHeight - style::scale(50.0f);
            m_waterfallHeight = m_widgetSize.y - m_fftHeight - style::scale(50.0f) - 2;
            m_rawFFTHeight = m_waterfallHeight;
            if(!m_saveHistory)
                m_rawFFTHeight = 2;
            m_currentFFTLine %= m_rawFFTHeight;
        }
        else {
            m_fftHeight = m_widgetSize.y - style::scale(50.0f);
        }
        m_dataWidth = m_widgetSize.x - style::scale(60.0f);
        bool updateWidth  = (lastDataWidth != m_dataWidth);
        bool updateHeight = lastWaterfallHeight != m_waterfallHeight || !m_waterfallVisible;
        

        if (m_waterfallVisible) {
            // Raw FFT resize
            m_fftLines = std::min<int>(m_fftLines, m_waterfallHeight) - 1;
            if (m_rawFFTs != NULL) {
                if (m_currentFFTLine != 0) {
                    float* tempWF = new float[m_currentFFTLine * m_rawFFTWidth];
                    unsigned int moveCount = unsigned(lastRawFFTHeight - m_currentFFTLine)%lastRawFFTHeight;
                    memcpy(tempWF, m_rawFFTs, m_currentFFTLine * m_rawFFTWidth * sizeof(float));
                    memmove(m_rawFFTs, &m_rawFFTs[m_currentFFTLine * m_rawFFTWidth], moveCount * m_rawFFTWidth * sizeof(float));
                    memcpy(&m_rawFFTs[moveCount * m_rawFFTWidth], tempWF, m_currentFFTLine * m_rawFFTWidth * sizeof(float));
                    delete[] tempWF;
                }
                m_currentFFTLine = 0;
                m_rawFFTs = (float*)realloc(m_rawFFTs, m_rawFFTHeight * m_rawFFTWidth * sizeof(float));
            }
            else {
                m_rawFFTs = (float*)malloc(m_rawFFTHeight * m_rawFFTWidth * sizeof(float));
            }
            // ==============
        }

        if(updateWidth)
        {
            // Reallocate display FFT
            if(m_latestFFT != NULL) {
                delete[] m_latestFFT;
            }
            m_latestFFT = new float[m_dataWidth];

            // Reallocate hold FFT
            if(m_latestFFTHold != NULL) {
                delete[] m_latestFFTHold;
            }
            m_latestFFTHold = new float[m_dataWidth];

            // Reallocate smoothing buffer
            if(m_fftSmoothing) {
                if(m_smoothingBuf) { delete[] m_smoothingBuf; }
                m_smoothingBuf = new float[m_dataWidth];
                for(int i = 0; i < m_dataWidth; i++) {
                    m_smoothingBuf[i] = -1000.0f;
                }
            }
        }
        if(updateWidth || updateHeight)
        {
            if(m_waterfallVisible) {
                delete[] m_waterfallFb;
                m_waterfallFb = new uint32_t[m_dataWidth * m_waterfallHeight];
                memset(m_waterfallFb, 0, m_dataWidth * m_waterfallHeight * sizeof(uint32_t));
            }
        }
        for(int i = 0; i < m_dataWidth; i++) {
            m_latestFFT[i] = -1000.0f; // Hide everything
            m_latestFFTHold[i] = -1000.0f;
        }

        // Whole-pixel area anchors: everything below (grid, trace, axes, the
        // waterfall texture) is placed relative to these.
        m_fftAreaMin = ImVec2(m_widgetPos.x + (float)style::scale(50.0f), m_widgetPos.y + (float)style::scale(9.0f));
        m_fftAreaMax = ImVec2(m_fftAreaMin.x + m_dataWidth, m_fftAreaMin.y + m_fftHeight + 1);

        m_freqAreaMin = ImVec2(m_fftAreaMin.x, m_fftAreaMax.y + 1);
        m_freqAreaMax = ImVec2(m_fftAreaMax.x, m_fftAreaMax.y + (float)style::scale(40.0f));

        m_wfMin = ImVec2(m_fftAreaMin.x, m_freqAreaMax.y + 1);
        m_wfMax = ImVec2(m_fftAreaMin.x + m_dataWidth, m_wfMin.y + m_waterfallHeight);

        // Use style::baseFont directly — GImGui->Font is stale when onResize() is called
        // from setFFTHeight() during a scale change (before NewFrame() restores it).
        {
            float tw = style::baseFont->CalcTextSizeA(style::baseFont->FontSize, FLT_MAX, 0.0f, "000.000").x;
            float th = style::baseFont->CalcTextSizeA(style::baseFont->FontSize, FLT_MAX, 0.0f, "000.000").y;
            m_maxHSteps = m_dataWidth / (tw + style::dp(10.0f));
            m_maxVSteps = m_fftHeight / th;
        }

        m_range = findBestRange(m_viewBandwidth, m_maxHSteps);
        m_vRange = findBestRange(m_fftMax - m_fftMin, m_maxVSteps);
        
        updateWaterfallFb();
        updateAllVFOs();

    }

    void WaterFall::draw() {
        m_buf_mtx.lock();
        m_window = GetCurrentWindow();

        m_widgetPos = ImGui::GetWindowContentRegionMin();
        m_widgetEndPos = ImGui::GetWindowContentRegionMax();
        m_widgetPos.x += m_window->Pos.x;
        m_widgetPos.y += m_window->Pos.y;
        m_widgetEndPos.x += m_window->Pos.x - 4; // Padding
        m_widgetEndPos.y += m_window->Pos.y;
        // Snap to whole pixels: a fractional origin would put every derived
        // line, the FFT trace and the waterfall texture on half-pixel
        // boundaries, blurring them at fractional UI scales.
        m_widgetPos = ImVec2(floorf(m_widgetPos.x), floorf(m_widgetPos.y));
        m_widgetEndPos = ImVec2(floorf(m_widgetEndPos.x), floorf(m_widgetEndPos.y));
        m_widgetSize = ImVec2(m_widgetEndPos.x - m_widgetPos.x, m_widgetEndPos.y - m_widgetPos.y);

        if (m_selectedVFO == "" && m_map_vfos.size() > 0) {
            selectFirstVFO();
        }

        if (m_widgetPos.x != m_lastWidgetPos.x || m_widgetPos.y != m_lastWidgetPos.y) {
            m_lastWidgetPos = m_widgetPos;
            onPositionChange();
        }
        // Force a layout recompute on scale changes: onContentScaleChanged()
        // triggers setFFTHeight() -> onResize() pre-frame with stale widgetPos,
        // and draw()'s size check can miss transitions where the widget pixel
        // size happens to stay the same across a scale change.
        uint64_t currentScaleEpoch = style::scaleEpoch();
        if (m_widgetSize.x != m_lastWidgetSize.x || m_widgetSize.y != m_lastWidgetSize.y || currentScaleEpoch != m_lastScaleEpoch) {
            m_lastWidgetSize = m_widgetSize;
            m_lastScaleEpoch = currentScaleEpoch;
            onResize();
        }

        // Compute the pill's visibility and position once so drawing and input
        // handling cannot drift apart.
        bool fftResizePillVisible = style::touchStyle && m_waterfallVisible;
        ImVec2 fftResizePillCenter;
        if (fftResizePillVisible) {
            fftResizePillCenter = ImVec2(m_fftAreaMin.x + (float)m_dataWidth * 0.75f,
                                        m_widgetPos.y + m_newFFTAreaHeight);
        }

        //window->DrawList->AddRectFilled(widgetPos, widgetEndPos, IM_COL32( 0, 0, 0, 255 ));
        ImU32 bg = ImGui::ColorConvertFloat4ToU32(gui::themeManager.waterfallBg);
        m_window->DrawList->AddRectFilled(m_widgetPos, m_widgetEndPos, bg);
        m_window->DrawList->AddRect(m_widgetPos, m_widgetEndPos, IM_COL32(50, 50, 50, 255), 0.0, 0, style::lineWidth());
        m_window->DrawList->AddLine(ImVec2(m_widgetPos.x, m_freqAreaMax.y), ImVec2(m_widgetPos.x + m_widgetSize.x, m_freqAreaMax.y), IM_COL32(50, 50, 50, 255), style::lineWidth());

        // A first press on an unselected VFO only changes the selection. Keep
        // the whole waterfall inert until that press is released, including
        // overlay input handlers and the wheel/key handling in main_window.
        // Do this outside the hover test so a release that happens after the
        // pointer leaves the waterfall still terminates the gesture.
        if (m_vfoSelect && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            m_vfoSelect = false;
        }

        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) &&
            !gui::mainWindow.lockWaterfallControls && !m_vfoSelect)
        {
            m_inputHandled = false;
            InputHandlerArgs args;
            args.fftRectMin = m_fftAreaMin;
            args.fftRectMax = m_fftAreaMax;
            args.freqScaleRectMin = m_freqAreaMin;
            args.freqScaleRectMax = m_freqAreaMax;
            args.waterfallRectMin = m_wfMin;
            args.waterfallRectMax = m_wfMax;
            args.lowFreq = m_lowerFreq;
            args.highFreq = m_upperFreq;
            args.freqToPixelRatio = (double)m_dataWidth / m_viewBandwidth;
            args.pixelToFreqRatio = m_viewBandwidth / (double)m_dataWidth;
            m_onInputProcess.emit(args);
            if (!m_inputHandled) { processInputs(fftResizePillVisible, fftResizePillCenter); }
        }
        else {
            // These are only recomputed inside processInputs(), which just got
            // skipped. Without this reset they keep whatever value the last
            // hovered frame left behind (a fast mouse exit can leave them
            // true), and main_window's wheel/arrow-key tuning keeps firing
            // while the cursor is over the menu or another window.
            m_mouseInFFTResize = false;
            m_mouseInFreq = false;
            m_mouseInFFT = false;
            m_mouseInWaterfall = false;
            m_fftResizePending = false;
        }

        updateAllVFOs(true);

        drawFFT();
        if (m_waterfallVisible) {
            drawWaterfall();
        }
        drawVFOs();
        if (m_bandplan != NULL && m_bandplanVisible) {
            drawBandPlan();
        }

        // FFT/waterfall divider drag handle: the fat touch target that grabs on
        // touch-down (see processInputs). Drawn last so it sits on top of the
        // freq scale and the waterfall; the dark backing keeps it readable.
        if (fftResizePillVisible) {
            float halfW = style::dp(16.0f) + (m_fftResizeSelect ? style::dp(4.0f) : 0.0f);
            float halfH = style::dp(m_fftResizeSelect ? 6.0f : 4.5f);
            float pad = style::dp(2.0f);
            m_window->DrawList->AddRectFilled(ImVec2(fftResizePillCenter.x - halfW - pad, fftResizePillCenter.y - halfH - pad),
                                            ImVec2(fftResizePillCenter.x + halfW + pad, fftResizePillCenter.y + halfH + pad),
                                            IM_COL32(0, 0, 0, 120), halfH + pad);
            m_window->DrawList->AddRectFilled(ImVec2(fftResizePillCenter.x - halfW, fftResizePillCenter.y - halfH),
                                            ImVec2(fftResizePillCenter.x + halfW, fftResizePillCenter.y + halfH),
                                            m_fftResizeSelect ? ImGui::GetColorU32(ImGuiCol_SeparatorActive) : IM_COL32(210, 210, 210, 200), halfH);
        }

        if (!m_waterfallVisible) {
            m_buf_mtx.unlock();
            return;
        }

        m_buf_mtx.unlock();
    }

    float* WaterFall::getFFTBuffer() {
        if (m_rawFFTs == NULL) { return NULL; }
        m_buf_mtx.lock();
        if (m_waterfallVisible) {
            m_fftLines++;
            if(!m_saveHistory)
            {
                m_currentFFTLine = 0;
                return m_rawFFTs;
            }
            m_currentFFTLine--;
            m_currentFFTLine = ((m_currentFFTLine + m_rawFFTHeight) % m_rawFFTHeight);
            m_fftLines = std::min<float>(m_fftLines, m_waterfallHeight);
            return &m_rawFFTs[m_currentFFTLine * m_rawFFTWidth];
        }
        return m_rawFFTs;
    }

    void WaterFall::pushFFT() {
        if (m_rawFFTs == NULL) { return; }
        std::lock_guard<std::recursive_mutex> lck(m_latestFFTMtx);
        double offsetRatio = m_viewOffset / (m_wholeBandwidth / 2.0);
        int drawDataSize = (m_viewBandwidth / m_wholeBandwidth) * m_rawFFTWidth;
        int drawDataStart = (((double)m_rawFFTWidth / 2.0) * (offsetRatio + 1)) - (drawDataSize / 2);

        if (m_waterfallVisible) {
            doZoom(drawDataStart, drawDataSize, m_rawFFTWidth, m_dataWidth, &m_rawFFTs[m_currentFFTLine * m_rawFFTWidth], m_latestFFT);
            memmove(&m_waterfallFb[m_dataWidth], m_waterfallFb, m_dataWidth * (m_waterfallHeight - 1) * sizeof(uint32_t));
            float pixel;
            float dataRange = m_waterfallMax - m_waterfallMin;
            for (int j = 0; j < m_dataWidth; j++) {
                pixel = (std::clamp<float>(m_latestFFT[j], m_waterfallMin, m_waterfallMax) - m_waterfallMin) / dataRange;
                int id = (int)(pixel * (WATERFALL_RESOLUTION - 1));
                m_waterfallFb[j] = m_waterfallPallet[id];
            }
            m_waterfallUpdate = true;
        }
        else {
            doZoom(drawDataStart, drawDataSize, m_rawFFTWidth, m_dataWidth, m_rawFFTs, m_latestFFT);
            m_fftLines = 1;
        }

        // Apply smoothing if enabled
        if (m_fftSmoothing && m_latestFFT != NULL && m_smoothingBuf != NULL && m_fftLines != 0) {
            std::lock_guard<std::mutex> lck2(m_smoothingBufMtx);
            volk_32f_s32f_multiply_32f(m_latestFFT, m_latestFFT, m_fftSmoothingAlpha, m_dataWidth);
            volk_32f_s32f_multiply_32f(m_smoothingBuf, m_smoothingBuf, m_fftSmoothingBeta, m_dataWidth);
            volk_32f_x2_add_32f(m_smoothingBuf, m_latestFFT, m_smoothingBuf, m_dataWidth);
            memcpy(m_latestFFT, m_smoothingBuf, m_dataWidth * sizeof(float));
        }

        if (m_selectedVFO != "" && m_map_vfos.size() > 0) {
            // Reset the peak hold when the VFO selection changes, so the new
            // VFO doesn't inherit the previous VFO's peak
            if (m_selectedVFO != m_levelHistoryVFO) {
                m_levelHistoryVFO = m_selectedVFO;
                m_levelHistoryPos = 0;
                m_levelHistoryCount = 0;
            }

            float newLevel = -INFINITY;
            float newSNR = NAN;
            bool infoValid = calculateVFOSignalInfo(m_waterfallVisible ? &m_rawFFTs[m_currentFFTLine * m_rawFFTWidth] : m_rawFFTs, m_map_vfos[m_selectedVFO], newLevel, newSNR);

            if (infoValid && std::isfinite(newLevel)) {
                m_selectedVFOLevel = newLevel;

                // Rolling window over the last LEVEL_HOLD_FRAMES FFT frames: hold
                // the peak in-band level, average the out-of-VFO-subband noise
                // floor over the same window, and report SNR as the held peak
                // over that averaged noise (dB difference = linear ratio).
                float newNoise = newLevel - newSNR; // out-of-band average (dB)
                m_levelHistory[m_levelHistoryPos] = newLevel;
                m_noiseHistory[m_levelHistoryPos] = newNoise;
                m_levelHistoryPos = (m_levelHistoryPos + 1) % LEVEL_HOLD_FRAMES;
                m_levelHistoryCount = std::min<int>(m_levelHistoryCount + 1, LEVEL_HOLD_FRAMES);
                float maxLevel = -INFINITY;
                float noiseSum = 0.0f;
                for (int i = 0; i < m_levelHistoryCount; i++) {
                    maxLevel = std::max<float>(maxLevel, m_levelHistory[i]);
                    noiseSum += m_noiseHistory[i];
                }
                m_selectedVFOLevelMax = maxLevel;
                m_selectedVFOSNR = maxLevel - (noiseSum / (float)m_levelHistoryCount);
            }
            else {
                // No usable FFT data: blank the meter instead of showing stale
                // values, and drop the history so values don't merge across a gap
                m_levelHistoryPos = 0;
                m_levelHistoryCount = 0;
                m_selectedVFOLevel = -INFINITY;
                m_selectedVFOLevelMax = -INFINITY;
                m_selectedVFOSNR = NAN;
            }
        }

        // If FFT hold is enabled, update it
        if (m_fftHold && m_latestFFT != NULL && m_latestFFTHold != NULL && m_fftLines != 0) {
            for (int i = 1; i < m_dataWidth; i++) {
                m_latestFFTHold[i] = std::max<float>(m_latestFFT[i], m_latestFFTHold[i] - m_fftHoldSpeed);
            }
        }

        m_buf_mtx.unlock();
    }

    void WaterFall::updatePallette(float colors[][3], int colorCount) {
        std::lock_guard<std::recursive_mutex> lck(m_buf_mtx);
        for (int i = 0; i < WATERFALL_RESOLUTION; i++) {
            int lowerId = floorf(((float)i / (float)WATERFALL_RESOLUTION) * colorCount);
            int upperId = ceilf(((float)i / (float)WATERFALL_RESOLUTION) * colorCount);
            lowerId = std::clamp<int>(lowerId, 0, colorCount - 1);
            upperId = std::clamp<int>(upperId, 0, colorCount - 1);
            float ratio = (((float)i / (float)WATERFALL_RESOLUTION) * colorCount) - lowerId;
            float r = (colors[lowerId][0] * (1.0 - ratio)) + (colors[upperId][0] * (ratio));
            float g = (colors[lowerId][1] * (1.0 - ratio)) + (colors[upperId][1] * (ratio));
            float b = (colors[lowerId][2] * (1.0 - ratio)) + (colors[upperId][2] * (ratio));
            m_waterfallPallet[i] = ((uint32_t)255 << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
        }
        updateWaterfallFb();
    }

    void WaterFall::updatePalletteFromArray(const float* colors, int colorCount) {
        // A color map that failed to load is empty; keep the current palette
        // instead of indexing a null array (and clamping to an empty range).
        if (!colors || colorCount <= 0) {
            flog::error("Ignoring an empty waterfall color map");
            return;
        }
        std::lock_guard<std::recursive_mutex> lck(m_buf_mtx);
        for (int i = 0; i < WATERFALL_RESOLUTION; i++) {
            int lowerId = floorf(((float)i / (float)WATERFALL_RESOLUTION) * colorCount);
            int upperId = ceilf(((float)i / (float)WATERFALL_RESOLUTION) * colorCount);
            lowerId = std::clamp<int>(lowerId, 0, colorCount - 1);
            upperId = std::clamp<int>(upperId, 0, colorCount - 1);
            float ratio = (((float)i / (float)WATERFALL_RESOLUTION) * colorCount) - lowerId;
            float r = (colors[(lowerId * 3) + 0] * (1.0 - ratio)) + (colors[(upperId * 3) + 0] * (ratio));
            float g = (colors[(lowerId * 3) + 1] * (1.0 - ratio)) + (colors[(upperId * 3) + 1] * (ratio));
            float b = (colors[(lowerId * 3) + 2] * (1.0 - ratio)) + (colors[(upperId * 3) + 2] * (ratio));
            m_waterfallPallet[i] = ((uint32_t)255 << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
        }
        updateWaterfallFb();
    }

    // Collect the raw FFT bins of the currently-visible span (current zoom/pan)
    // into `out`, excluding the device band edges and the DC/LO notch, so the
    // range estimators see only the meaningful noise+signal population.
    //
    // Estimating from the RAW line (not the max-downsampled display FFT) avoids
    // the upward bias that max-selection downsampling introduces when zoomed
    // out. Coordinates: rawFFTs is DC-centered (a (-1)^n factor is folded into
    // the FFT window, see iq_frontend.cpp), so DC is at N/2 and the +-Fs/2 band
    // edges are at 0 and N. Both exclusion zones are device-absolute and then
    // intersected with the visible window, so a zoom into a clean interior
    // slice keeps all of it, while a full-zoom-out drops the roll-off edges and
    // the center spike (matching the SDR Console / gqrx heuristics).
    //
    // Pooling several lines gives the one-shot a steadier estimate and rejects
    // a transient whole-band burst at the click instant: the burst's elevated
    // bins sort into the upper part of the pool, so a low quantile stays on the
    // good lines. Takes buf_mtx, which guards rawFFTs / currentFFTLine /
    // geometry (documented lock order buf_mtx -> latestFFTMtx, so this outermost
    // lock is safe and lets us read the newest line without racing the DSP
    // write). Returns false if too few bins survive.
    bool WaterFall::collectAutorangeBins(std::vector<float>& out, int lines) {
        // The band-edge and DC/LO exclusions are contiguous index ranges, not
        // per-bin predicates, so resolve them to at most two surviving segments.
        // All the boundary math is derived here, before taking buf_mtx: it
        // depends only on the view geometry (GUI-thread-owned, so stable in this
        // GUI-thread call) and rawFFTWidth. The lock then holds for essentially
        // just the memcpy.
        //
        // rawFFTWidth is the one caveat: the DSP thread (iq_frontend) reallocates
        // rawFFTs to a new rawFFTWidth under buf_mtx. We snapshot it as N here and
        // re-check it under the lock before copying -- if it changed, the offsets
        // no longer match the buffer, so we bail (the caller keeps the previous
        // range; the next tick reacquires). Every offset below is clamped into
        // [0, N], so a stale N can only cause a bail, never an out-of-bounds copy.
        int N = m_rawFFTWidth;
        if (N <= 0 || m_wholeBandwidth <= 0.0) { return false; }

        // Visible span in raw-bin coordinates (identical to pushFFT).
        double offsetRatio = m_viewOffset / (m_wholeBandwidth / 2.0);
        int drawDataSize = (int)((m_viewBandwidth / m_wholeBandwidth) * N);
        int drawDataStart = (int)(((double)N / 2.0) * (offsetRatio + 1)) - (drawDataSize / 2);
        int visStart = std::clamp<int>(drawDataStart, 0, N);
        int visEnd = std::clamp<int>(drawDataStart + drawDataSize, 0, N);
        if (visStart >= visEnd) { return false; }

        // Device band-edge zones (anti-alias roll-off): 15% per side. Keep
        // [edgeLo, edgeHi); intersect with the visible window to get [a, b).
        const double EDGE_FRACTION = 0.15;
        int edgeLo = (int)(EDGE_FRACTION * N);  // [0, edgeLo)  = -Fs/2 edge
        int edgeHi = N - edgeLo;                // [edgeHi, N)  = +Fs/2 edge
        int a = std::max(visStart, edgeLo);
        int b = std::min(visEnd, edgeHi);

        // DC/LO notch at center (N/2): +-250 Hz, capped at 5% of the visible
        // span so it can't dominate a narrowband source or an extreme zoom.
        const double CENTER_NOTCH_BANDWIDTH = 250.0; // Hz, half-width
        double hzPerBin = m_wholeBandwidth / (double)N;
        double notchHz = std::min(CENTER_NOTCH_BANDWIDTH, 0.05 * m_viewBandwidth);
        int notchHalf = (int)(notchHz / hzPerBin);
        int dcLo = (N / 2) - notchHalf;
        int dcHi = (N / 2) + notchHalf;

        // [dcLo, dcHi] inclusive is cut out of [a, b), leaving the bins below the
        // notch (segment 1) and above it (segment 2). The clamps collapse the
        // degenerate cases: a notch outside [a, b) leaves the whole interior in
        // one segment, a notch covering it leaves none. Lengths floored at 0 so
        // an empty segment contributes nothing; all offsets stay within [0, N].
        int seg1Lo = a;
        int len1 = std::max(0, std::min(b, dcLo) - a);
        int seg2Lo = std::max(a, dcHi + 1);
        int len2 = std::max(0, b - seg2Lo);
        int perLine = len1 + len2;
        if (perLine <= 0) { return false; }

        {
            std::lock_guard<std::recursive_mutex> lck(m_buf_mtx);
            // rawFFTWidth may have been reallocated under us (DSP thread); if so
            // the precomputed offsets no longer match rawFFTs -- bail rather than
            // index past it.
            if (m_rawFFTs == NULL || m_fftLines <= 0 || m_waterfallHeight <= 0 || m_rawFFTWidth != N) { return false; }
            int nLines = std::clamp<int>(lines, 1, m_fftLines);
            size_t total = (size_t)perLine * nLines;
            // resize (not reserve) so out.size() is exactly the copied count -- a
            // prior larger call must not leave a stale tail for the caller's
            // nth_element. Grows only past capacity, so the pooled buffer keeps
            // the steady-state path allocation-free.
            out.resize(total);
            int lines = std::min(nLines, m_rawFFTHeight) + 1;
            for(int l = 0; l < lines; l++) {
                // currentFFTLine is the newest; older lines follow at +1, +2...
                // When the waterfall is hidden the live line is rawFFTs[0], not
                // the frozen ring slot at currentFFTLine.
                const float* line = m_waterfallVisible
                    ? &m_rawFFTs[((m_currentFFTLine + l) % m_rawFFTHeight) * N]
                    : m_rawFFTs;
                size_t offs = (size_t)l * perLine;
                if(offs+1 >= out.size())
                    break;
                float* dst = out.data() + offs;
                if(len1 > 0) { memcpy(dst, &line[seg1Lo], len1 * sizeof(float)); dst += len1; }
                if(len2 > 0) { memcpy(dst, &line[seg2Lo], len2 * sizeof(float)); }
            }
        }

        // Reject the sentinel / non-finite values (nothing real sits below
        // -200 dBFS). Compact in place, unlocked; a no-op on a live band.
        out.erase(std::remove_if(out.begin(), out.end(),
                      [](float v) { return !(std::isfinite(v) && v > -200.0f); }),
                  out.end());
        // Too few bins to trust (e.g. zoomed onto pure roll-off): bail so the
        // caller keeps the previous range.
        return out.size() >= 64;
    }

    // Estimate just the noise floor (the bottom of the display window) from the
    // visible raw bins. The span/top is left to the user's Range. Reuses the
    // member output buffer (collectAutorangeBins resize()s it, keeping capacity)
    // so the periodic sticky path doesn't alloc/free a ~65k-float vector every
    // call.
    bool WaterFall::getAutorangeRef(float& refDb, int lines) {
        std::vector<float>& vals = m_autorangeVals;
        if (!collectAutorangeBins(vals, lines)) { return false; }

        // Robust low quantile instead of the raw minimum: raw min chases single
        // downward noise excursions. nth_element is O(n) vs O(n log n) for a
        // full sort, which matters on a ~65k-bin raw line.
        size_t k = (size_t)(0.30 * (vals.size() - 1));
        std::nth_element(vals.begin(), vals.begin() + k, vals.end());
        float floorDb = vals[k];
        // Place the floor 8 dB above the window bottom so noise sits just off
        // the darkest color instead of pure black.
        refDb = floorDb - 8.0f;
        return true;
    }

    // Release the pooled bin buffer. Call after a multi-line one-shot so the
    // single-line sticky path doesn't keep the 5x-larger allocation around.
    void WaterFall::freeAutorangeScratch() {
        std::lock_guard<std::recursive_mutex> lck(m_buf_mtx);
        std::vector<float>().swap(m_autorangeVals);
    }

    void WaterFall::setCenterFrequency(double freq) {
        m_centerFreq = freq;
        m_lowerFreq = (m_centerFreq + m_viewOffset) - (m_viewBandwidth / 2.0);
        m_upperFreq = (m_centerFreq + m_viewOffset) + (m_viewBandwidth / 2.0);
        updateAllVFOs();
    }

    double WaterFall::getCenterFrequency() {
        return m_centerFreq;
    }

    void WaterFall::setBandwidth(double bandWidth) {
        double currentRatio = m_viewBandwidth / m_wholeBandwidth;
        m_wholeBandwidth = bandWidth;
        setViewBandwidth(bandWidth * currentRatio);
        for (auto const& [name, vfo] : m_map_vfos) {
            if (vfo->m_lowerOffset < -(bandWidth / 2)) {
                vfo->setCenterOffset(-(bandWidth / 2));
            }
            if (vfo->m_upperOffset > (bandWidth / 2)) {
                vfo->setCenterOffset(bandWidth / 2);
            }
        }
        updateAllVFOs();
    }

    double WaterFall::getBandwidth() {
        return m_wholeBandwidth;
    }

    void WaterFall::setViewBandwidth(double bandWidth) {
        std::lock_guard<std::recursive_mutex> lck(m_buf_mtx);
        if (bandWidth == m_viewBandwidth) {
            return;
        }
        if (abs(m_viewOffset) + (bandWidth / 2.0) > m_wholeBandwidth / 2.0) {
            if (m_viewOffset < 0) {
                m_viewOffset = (bandWidth / 2.0) - (m_wholeBandwidth / 2.0);
            }
            else {
                m_viewOffset = (m_wholeBandwidth / 2.0) - (bandWidth / 2.0);
            }
        }
        m_viewBandwidth = bandWidth;
        m_lowerFreq = (m_centerFreq + m_viewOffset) - (m_viewBandwidth / 2.0);
        m_upperFreq = (m_centerFreq + m_viewOffset) + (m_viewBandwidth / 2.0);
        m_range = findBestRange(bandWidth, m_maxHSteps);
        if (m_fullUpdate) { updateWaterfallFb(); };
        updateAllVFOs();
    }

    double WaterFall::getViewBandwidth() {
        return m_viewBandwidth;
    }

    void WaterFall::setViewOffset(double offset) {
        std::lock_guard<std::recursive_mutex> lck(m_buf_mtx);
        if (offset == m_viewOffset) {
            return;
        }
        if (offset - (m_viewBandwidth / 2.0) < -(m_wholeBandwidth / 2.0)) {
            offset = (m_viewBandwidth / 2.0) - (m_wholeBandwidth / 2.0);
        }
        if (offset + (m_viewBandwidth / 2.0) > (m_wholeBandwidth / 2.0)) {
            offset = (m_wholeBandwidth / 2.0) - (m_viewBandwidth / 2.0);
        }
        m_viewOffset = offset;
        m_lowerFreq = (m_centerFreq + m_viewOffset) - (m_viewBandwidth / 2.0);
        m_upperFreq = (m_centerFreq + m_viewOffset) + (m_viewBandwidth / 2.0);
        if (m_fullUpdate) { updateWaterfallFb(); };
        updateAllVFOs();
    }

    double WaterFall::getViewOffset() {
        return m_viewOffset;
    }

    void WaterFall::setFFTMin(float min) {
        m_fftMin = min;
        m_vRange = findBestRange(m_fftMax - m_fftMin, m_maxVSteps);
    }

    float WaterFall::getFFTMin() {
        return m_fftMin;
    }

    void WaterFall::setFFTMax(float max) {
        m_fftMax = max;
        m_vRange = findBestRange(m_fftMax - m_fftMin, m_maxVSteps);
    }

    float WaterFall::getFFTMax() {
        return m_fftMax;
    }

    void WaterFall::setFullWaterfallUpdate(bool saveHistory, bool fullUpdate) {
        std::lock_guard<std::recursive_mutex> lck(m_buf_mtx);
        m_fullUpdate = fullUpdate;
        if(saveHistory != m_saveHistory)
        {
            m_saveHistory = saveHistory;
            onResize();
        }
    }

    void WaterFall::setWaterfallMinMax(float min, float max) {
        std::lock_guard<std::recursive_mutex> lck(m_buf_mtx);
        if (min == m_waterfallMin && max == m_waterfallMax) {
            return;
        }
        m_waterfallMin = min;
        m_waterfallMax = max;
        if (m_fullUpdate) { updateWaterfallFb(); };
    }

    float WaterFall::getWaterfallMin() {
        return m_waterfallMin;
    }

    float WaterFall::getWaterfallMax() {
        return m_waterfallMax;
    }

    void WaterFall::updateAllVFOs(bool checkRedrawRequired) {
        for (auto const& [name, vfo] : m_map_vfos) {
            if (checkRedrawRequired && !vfo->m_redrawRequired) { continue; }
            vfo->updateDrawingVars(m_viewBandwidth, m_dataWidth, m_viewOffset, m_widgetPos, m_fftHeight);
            vfo->m_wfRectMin = ImVec2(vfo->m_rectMin.x, m_wfMin.y);
            vfo->m_wfRectMax = ImVec2(vfo->m_rectMax.x, m_wfMax.y);
            vfo->m_wfLineMin = ImVec2(vfo->m_lineMin.x, m_wfMin.y - 1);
            vfo->m_wfLineMax = ImVec2(vfo->m_lineMax.x, m_wfMax.y - 1);
            float wfGrip = style::dp(2.0f);
            vfo->m_wfLbwSelMin = ImVec2(vfo->m_wfRectMin.x - wfGrip, vfo->m_wfRectMin.y);
            vfo->m_wfLbwSelMax = ImVec2(vfo->m_wfRectMin.x + wfGrip, vfo->m_wfRectMax.y);
            vfo->m_wfRbwSelMin = ImVec2(vfo->m_wfRectMax.x - wfGrip, vfo->m_wfRectMin.y);
            vfo->m_wfRbwSelMax = ImVec2(vfo->m_wfRectMax.x + wfGrip, vfo->m_wfRectMax.y);
            vfo->m_redrawRequired = false;
        }
    }

    void WaterFall::setRawFFTSize(int size) {
        std::lock_guard<std::recursive_mutex> lck(m_buf_mtx);
        m_rawFFTWidth = size;
        int wfSize = std::max<int>(1, m_rawFFTHeight);
        if (m_rawFFTs != NULL) {
            m_rawFFTs = (float*)realloc(m_rawFFTs, m_rawFFTWidth * wfSize * sizeof(float));
        }
        else {
            m_rawFFTs = (float*)malloc(m_rawFFTWidth * wfSize * sizeof(float));
        }
        m_fftLines = 0;
        memset(m_rawFFTs, 0, m_rawFFTWidth * m_rawFFTHeight * sizeof(float));
        updateWaterfallFb();
    }

    void WaterFall::setBandPlanPos(int pos) {
        m_bandPlanPos = pos;
    }

    void WaterFall::setFFTHold(bool hold) {
        m_fftHold = hold;
        if (m_fftHold && m_latestFFTHold) {
            for (int i = 0; i < m_dataWidth; i++) {
                m_latestFFTHold[i] = -1000.0;
            }
        }
    }

    void WaterFall::setFFTHoldSpeed(float speed) {
        m_fftHoldSpeed = speed;
    }

    void WaterFall::setFFTSmoothing(bool enabled) {
        std::lock_guard<std::mutex> lck(m_smoothingBufMtx);
        m_fftSmoothing = enabled;

        // Free buffer if not null
        if (m_smoothingBuf) {delete[] m_smoothingBuf; }

        // If disabled, stop here
        if (!enabled) {
            m_smoothingBuf = NULL;
            return;
        }

        // Allocate and copy existing FFT into it
        m_smoothingBuf = new float[m_dataWidth];
        if (m_latestFFT) {
            std::lock_guard<std::recursive_mutex> lck2(m_latestFFTMtx);
            memcpy(m_smoothingBuf, m_latestFFT, m_dataWidth * sizeof(float));
        }
        else {
            memset(m_smoothingBuf, 0, m_dataWidth * sizeof(float));
        }
    }

    void WaterFall::setFFTSmoothingSpeed(float speed) {
        std::lock_guard<std::mutex> lck(m_smoothingBufMtx);
        m_fftSmoothingAlpha = speed;
        m_fftSmoothingBeta = 1.0f - speed;
    }

    float* WaterFall::acquireLatestFFT(int& width) {
        m_latestFFTMtx.lock();
        if (!m_latestFFT) {
            m_latestFFTMtx.unlock();
            return NULL;
        }
        width = m_dataWidth;
        return m_latestFFT;
    }

    void WaterFall::releaseLatestFFT() {
        m_latestFFTMtx.unlock();
    }

    void WaterfallVFO::setOffset(double offset) {
        m_generalOffset = offset;
        if (m_reference == REF_CENTER) {
            m_centerOffset = offset;
            m_lowerOffset = offset - (m_bandwidth / 2.0);
            m_upperOffset = offset + (m_bandwidth / 2.0);
        }
        else if (m_reference == REF_LOWER) {
            m_lowerOffset = offset;
            m_centerOffset = offset + (m_bandwidth / 2.0);
            m_upperOffset = offset + m_bandwidth;
        }
        else if (m_reference == REF_UPPER) {
            m_upperOffset = offset;
            m_centerOffset = offset - (m_bandwidth / 2.0);
            m_lowerOffset = offset - m_bandwidth;
        }
        m_centerOffsetChanged = true;
        m_upperOffsetChanged = true;
        m_lowerOffsetChanged = true;
        m_redrawRequired = true;
    }

    void WaterfallVFO::setCenterOffset(double offset) {
        if (m_reference == REF_CENTER) {
            m_generalOffset = offset;
        }
        else if (m_reference == REF_LOWER) {
            m_generalOffset = offset - (m_bandwidth / 2.0);
        }
        else if (m_reference == REF_UPPER) {
            m_generalOffset = offset + (m_bandwidth / 2.0);
        }
        m_centerOffset = offset;
        m_lowerOffset = offset - (m_bandwidth / 2.0);
        m_upperOffset = offset + (m_bandwidth / 2.0);
        m_centerOffsetChanged = true;
        m_upperOffsetChanged = true;
        m_lowerOffsetChanged = true;
        m_redrawRequired = true;
    }

    void WaterfallVFO::setBandwidth(double bw) {
        if (m_bandwidth == bw || bw < 0) {
            return;
        }
        m_bandwidth = bw;
        if (m_reference == REF_CENTER) {
            m_lowerOffset = m_centerOffset - (m_bandwidth / 2.0);
            m_upperOffset = m_centerOffset + (m_bandwidth / 2.0);
        }
        else if (m_reference == REF_LOWER) {
            m_centerOffset = m_lowerOffset + (m_bandwidth / 2.0);
            m_upperOffset = m_lowerOffset + m_bandwidth;
            m_centerOffsetChanged = true;
        }
        else if (m_reference == REF_UPPER) {
            m_centerOffset = m_upperOffset - (m_bandwidth / 2.0);
            m_lowerOffset = m_upperOffset - m_bandwidth;
            m_centerOffsetChanged = true;
        }
        m_bandwidthChanged = true;
        m_redrawRequired = true;
    }

    void WaterfallVFO::setReference(int ref) {
        if (m_reference == ref || ref < 0 || ref >= _REF_COUNT) {
            return;
        }
        m_reference = ref;
        setOffset(m_generalOffset);
    }

    void WaterfallVFO::setNotchOffset(double offset) {
        m_notchOffset = offset;
        m_redrawRequired = true;
    }

    void WaterfallVFO::setNotchVisible(bool visible) {
        m_notchVisible = visible;
        m_redrawRequired = true;
    }

    void WaterfallVFO::updateDrawingVars(double viewBandwidth, float dataWidth, double viewOffset, ImVec2 widgetPos, int fftHeight) {
        int center = roundf((((m_centerOffset - viewOffset) / (viewBandwidth / 2.0)) + 1.0) * ((double)dataWidth / 2.0));
        int left = roundf((((m_lowerOffset - viewOffset) / (viewBandwidth / 2.0)) + 1.0) * ((double)dataWidth / 2.0));
        int right = roundf((((m_upperOffset - viewOffset) / (viewBandwidth / 2.0)) + 1.0) * ((double)dataWidth / 2.0));
        int notch = roundf((((m_notchOffset + m_centerOffset - viewOffset) / (viewBandwidth / 2.0)) + 1.0) * ((double)dataWidth / 2.0));

        // Check weather the line is visible
        if (left >= 0 && left < dataWidth && m_reference == REF_LOWER) {
            m_lineVisible = true;
        }
        else if (center >= 0 && center < dataWidth && m_reference == REF_CENTER) {
            m_lineVisible = true;
        }
        else if (right >= 0 && right < dataWidth && m_reference == REF_UPPER) {
            m_lineVisible = true;
        }
        else {
            m_lineVisible = false;
        }

        // Calculate the position of the line
        if (m_reference == REF_LOWER) {
            m_lineMin = ImVec2(gui::waterfall.m_fftAreaMin.x + left, gui::waterfall.m_fftAreaMin.y);
            m_lineMax = ImVec2(gui::waterfall.m_fftAreaMin.x + left, gui::waterfall.m_fftAreaMax.y - 1);
        }
        else if (m_reference == REF_CENTER) {
            m_lineMin = ImVec2(gui::waterfall.m_fftAreaMin.x + center, gui::waterfall.m_fftAreaMin.y);
            m_lineMax = ImVec2(gui::waterfall.m_fftAreaMin.x + center, gui::waterfall.m_fftAreaMax.y - 1);
        }
        else if (m_reference == REF_UPPER) {
            m_lineMin = ImVec2(gui::waterfall.m_fftAreaMin.x + right, gui::waterfall.m_fftAreaMin.y);
            m_lineMax = ImVec2(gui::waterfall.m_fftAreaMin.x + right, gui::waterfall.m_fftAreaMax.y - 1);
        }

        int _left = left;
        int _right = right;
        left = std::clamp<int>(left, 0, dataWidth - 1);
        right = std::clamp<int>(right, 0, dataWidth - 1);
        m_leftClamped = (left != _left);
        m_rightClamped = (right != _right);

        m_rectMin = ImVec2(gui::waterfall.m_fftAreaMin.x + left, gui::waterfall.m_fftAreaMin.y + 1);
        m_rectMax = ImVec2(gui::waterfall.m_fftAreaMin.x + right + 1, gui::waterfall.m_fftAreaMax.y);

        float gripSize = style::dp(2.0f);
        m_lbwSelMin = ImVec2(m_rectMin.x - gripSize, m_rectMin.y);
        m_lbwSelMax = ImVec2(m_rectMin.x + gripSize, m_rectMax.y);
        m_rbwSelMin = ImVec2(m_rectMax.x - gripSize, m_rectMin.y);
        m_rbwSelMax = ImVec2(m_rectMax.x + gripSize, m_rectMax.y);

        m_notchMin = ImVec2(gui::waterfall.m_fftAreaMin.x + notch - gripSize, gui::waterfall.m_fftAreaMin.y);
        m_notchMax = ImVec2(gui::waterfall.m_fftAreaMin.x + notch + gripSize, gui::waterfall.m_fftAreaMax.y - 1);
    }

    void WaterfallVFO::draw(ImGuiWindow* window, bool selected) {
        window->DrawList->AddRectFilled(m_rectMin, m_rectMax, m_color);
        if (m_lineVisible) {
            window->DrawList->AddLine(m_lineMin, m_lineMax, selected ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 0, 255), style::lineWidth());
        }

        if (m_notchVisible) {
            window->DrawList->AddRectFilled(m_notchMin, m_notchMax, IM_COL32(255, 0, 0, 127));
        }

        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_None) &&
            !gui::mainWindow.lockWaterfallControls && !gui::waterfall.m_inputHandled)
        {
            ImVec2 mousePos = ImGui::GetMousePos();
            if (m_rectMax.x - m_rectMin.x < 10) { return; }
            if (m_reference != REF_LOWER && !m_bandwidthLocked && !m_leftClamped) {
                if (IS_IN_AREA(mousePos, m_lbwSelMin, m_lbwSelMax)) { ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW); }
                else if (IS_IN_AREA(mousePos, m_wfLbwSelMin, m_wfLbwSelMax)) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                }
            }
            if (m_reference != REF_UPPER && !m_bandwidthLocked && !m_rightClamped) {
                if (IS_IN_AREA(mousePos, m_rbwSelMin, m_rbwSelMax)) { ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW); }
                else if (IS_IN_AREA(mousePos, m_wfRbwSelMin, m_wfRbwSelMax)) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                }
            }
        }
    };

    void WaterFall::showWaterfall() {
        m_buf_mtx.lock();
        if (m_rawFFTs == NULL) {
            flog::error("Null rawFFT");
        }
        m_waterfallVisible = true;
        onResize();
        memset(m_rawFFTs, 0, m_rawFFTHeight * m_rawFFTWidth * sizeof(float));
        updateWaterfallFb();
        m_buf_mtx.unlock();
    }

    void WaterFall::hideWaterfall() {
        m_buf_mtx.lock();
        m_waterfallVisible = false;
        onResize();
        m_buf_mtx.unlock();
    }

    void WaterFall::setFFTHeight(int height) {
        m_FFTAreaHeight = height;
        m_newFFTAreaHeight = height;
        m_buf_mtx.lock();
        onResize();
        m_buf_mtx.unlock();
    }

    int WaterFall::getFFTHeight() {
        return m_FFTAreaHeight;
    }

    void WaterFall::showBandplan() {
        m_bandplanVisible = true;
    }

    void WaterFall::hideBandplan() {
        m_bandplanVisible = false;
    }

    void WaterfallVFO::setSnapInterval(double interval) {
        m_snapInterval = interval;
    }
};
