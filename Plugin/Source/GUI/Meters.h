// Unfairboy 067 —meters. Values come from the engine's atomics; ballistics run on the GUI timer.
#pragma once

#include "TwinMuLookAndFeel.h"

namespace twinmu::gui
{
/** Back-lit analogue gain-reduction meter. */
class VuMeter : public juce::Component
{
public:
    explicit VuMeter (juce::String channelName);
    void setAccent (juce::Colour c) { accent = c; repaint(); }
    void setCaption (const juce::String& text) { caption = text; repaint(); }

    /** newGrDb: largest gain reduction since the previous frame (>= 0). dt in seconds. */
    void update (float newGrDb, float dt);
    void paint (juce::Graphics&) override;

private:
    juce::String channel, caption { "GAIN REDUCTION" };
    juce::Colour accent { fixed::amber };
    float needle = 0.0f, readout = 0.0f;
};

/** Two vertical peak bars (L/R) with peak hold and clip LED. */
class BarMeter : public juce::Component
{
public:
    void update (float peakL, float peakR, float dt);
    void paint (juce::Graphics&) override;

private:
    float level[2] {}, hold[2] {}, holdTime[2] {};
    float clipTime = 0.0f;
};
} // namespace twinmu::gui
