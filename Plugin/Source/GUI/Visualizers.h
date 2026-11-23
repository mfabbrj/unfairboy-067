// Unfairboy 067 —transfer curve, gain-reduction history and sidechain filter displays.
#pragma once

#include "TwinMuLookAndFeel.h"

#include <array>
#include <functional>

namespace twinmu::gui
{
/** Static transfer curve with the live working point and a scrolling GR history strip
    (transients show up as spikes). */
class CurveView : public juce::Component
{
public:
    /** grFunction: detector level (dB) -> gain reduction (dB) for the current settings. */
    void update (std::function<float (float)> grFunction, float thresholdDb, float inputPeakDb, float grDb,
                 const juce::String& modeName);
    void paint (juce::Graphics&) override;

private:
    std::array<float, 121> curve {};
    std::array<float, 140> history {};
    int historyPos = 0;
    float threshold = -18.0f, workIn = -90.0f, workOut = -90.0f;
    juce::String mode;
};

class FilterView : public juce::Component
{
public:
    void setFilter (int type, float freqHz, float q, bool active);
    void paint (juce::Graphics&) override;

private:
    int filterType = 1;
    float frequency = 80.0f, quality = 0.707f;
    bool isActive = true;
};
} // namespace twinmu::gui
