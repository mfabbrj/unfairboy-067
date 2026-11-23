// TWIN-MU — host parameter layout and the lock-free bridge to the DSP engine.
#pragma once

#include "DSP/EngineParams.h"
#include "ParameterIDs.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace twinmu
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Caches the APVTS atomics once, then builds an EngineParams per block without locks. */
class ParameterCache
{
public:
    void attach (juce::AudioProcessorValueTreeState& state);

    dsp::EngineParams read() const noexcept;

    /** Structural values (they change the latency, applied on the message thread). */
    int oversamplingFactor() const noexcept;
    float lookaheadMs() const noexcept;

private:
    struct Channel
    {
        std::atomic<float>* thr = nullptr;
        std::atomic<float>* ratio = nullptr;
        std::atomic<float>* atk = nullptr;
        std::atomic<float>* rel = nullptr;
        std::atomic<float>* relMode = nullptr;
        std::atomic<float>* mk = nullptr;
        std::atomic<float>* en = nullptr;
    };

    std::atomic<float>* mode = nullptr;
    std::atomic<float>* stereoMode = nullptr;
    std::atomic<float>* link = nullptr;
    std::atomic<float>* inGain = nullptr;
    std::atomic<float>* knee = nullptr;
    std::atomic<float>* autoMakeup = nullptr;
    Channel a, b;
    std::atomic<float>* tube = nullptr;
    std::atomic<float>* xfmr = nullptr;
    std::atomic<float>* noise = nullptr;
    std::atomic<float>* hum = nullptr;
    std::atomic<float>* fdc = nullptr;
    std::atomic<float>* scSource = nullptr;
    std::atomic<float>* scMix = nullptr;
    std::atomic<float>* scFilter = nullptr;
    std::atomic<float>* scFreq = nullptr;
    std::atomic<float>* scQ = nullptr;
    std::atomic<float>* scSens = nullptr;
    std::atomic<float>* scListen = nullptr;
    std::atomic<float>* limOn = nullptr;
    std::atomic<float>* limCeil = nullptr;
    std::atomic<float>* limRel = nullptr;
    std::atomic<float>* limLook = nullptr;
    std::atomic<float>* limAmt = nullptr;
    std::atomic<float>* truePeak = nullptr;
    std::atomic<float>* softClip = nullptr;
    std::atomic<float>* width = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* outGain = nullptr;
    std::atomic<float>* os = nullptr;
    std::atomic<float>* eco = nullptr;
    std::atomic<float>* bypass = nullptr;
};
} // namespace twinmu
