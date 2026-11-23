// TWIN-MU — complete processing chain (framework independent).
//
//  in -> input gain -> [L/R or M/S matrix] -> detector (sidechain, filters, link) -> gain computer
//     -> envelope -> [oversampled: gain stage -> tube -> transformer] -> makeup -> [inverse matrix]
//     -> width -> dry/wet mix (dry latency-compensated) -> output gain -> noise -> limiter
//     -> soft clip -> bypass crossfade -> out
//
// Real-time contract: prepare() allocates; process() never allocates, locks or does I/O.
// setOversamplingFactor() / setLookaheadMs() change the latency and must be called while
// audio processing is suspended (the plugin layer handles this on the message thread).
#pragma once

#include "AnalogNoise.h"
#include "EngineParams.h"
#include "Envelope.h"
#include "Filters.h"
#include "Limiter.h"
#include "Oversampling.h"
#include "Saturation.h"

#include <array>
#include <atomic>
#include <vector>

namespace twinmu::dsp
{
/** Values published by the audio thread for the GUI. The GUI reads them with exchange(0)
    so that peaks between two repaints are never lost. */
struct Meters
{
    std::array<std::atomic<float>, 2> inputPeak {};
    std::array<std::atomic<float>, 2> outputPeak {};
    std::array<std::atomic<float>, 2> gainReductionDb {};   // channel A / B, real applied GR
    std::atomic<float> limiterGainReductionDb { 0.0f };
    std::atomic<bool> clip { false };

    static void raise (std::atomic<float>& target, float value) noexcept
    {
        if (value > target.load (std::memory_order_relaxed))
            target.store (value, std::memory_order_relaxed);
    }
};

class Engine
{
public:
    Engine();

    /** numChannels: 1 or 2. Not real-time safe. */
    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;

    /** Call from the audio thread at the start of each block. */
    void setParameters (const EngineParams& newParams) noexcept;

    /** 1, 2, 4, 8. Changes latency: call with processing suspended. */
    void setOversamplingFactor (int factor) noexcept;
    int getOversamplingFactor() const noexcept { return oversampler.getFactor(); }

    /** 0 .. 5 ms. Changes latency: call with processing suspended. */
    void setLookaheadMs (float ms) noexcept;

    int getLatencySamples() const noexcept;

    /** input/output may alias. sidechain may be nullptr; sidechainChannels 1 or 2.
        Blocks larger than maxBlockSize are split internally. */
    void process (const float* const* input, float* const* output, int numSamples,
                  const float* const* sidechain = nullptr, int sidechainChannels = 0) noexcept;

    Meters& getMeters() noexcept { return meters; }
    const EngineParams& getParameters() const noexcept { return params; }

    /** Static gain reduction of the current curve for a given detector level (for GUI curve and auto makeup). */
    float staticGainReductionDb (float levelDb, int channel) const noexcept;

private:
    void processChunk (const float* const* in, float* const* out, int offset, int n,
                       const float* const* sc, int scChannels) noexcept;
    void configureEnvelopes() noexcept;
    void prepareOversampledStages() noexcept;
    const ChannelParams& channelParams (int channel) const noexcept;

    EngineParams params;
    double sampleRate = 48000.0;
    int maxBlock = 512;
    int numChannels = 2;

    Oversampler oversampler;
    Limiter limiter;
    std::array<GainReductionEnvelope, 2> envelopes;
    std::array<TubeStage, 2> tubes;
    std::array<TransformerStage, 2> transformers;
    std::array<SvfTpt, 2> scFilters;
    std::array<OnePoleLowPass, 2> freqDepFilters;
    std::array<AnalogNoise, 2> noise;

    LinearSmoother inputGain, outputGain, mix, width, tubeAmount, transformerAmount, bypassMix;
    std::array<LinearSmoother, 2> makeup;

    // per-chunk work buffers (allocated in prepare)
    std::array<std::vector<float>, 2> signal, gain, grDb, listen;
    std::vector<float> tubeValues, transformerValues;

    // gain delay (aligns the base-rate gain with the up-sampled audio)
    static constexpr int kGainRing = 64;
    std::array<std::array<float, kGainRing>, 2> gainRing {}, grRing {};
    int gainRingPos = 0;
    std::array<float, 2> previousGain { 1.0f, 1.0f };

    // dry delay lines: raw (for bypass) and post input gain (for dry/wet)
    std::array<std::vector<float>, 2> dryRaw, dryPost;
    int dryMask = 0, dryPos = 0;

    std::array<float, 2> feedbackSignal { 0.0f, 0.0f };

    struct EnvelopeKey
    {
        ReleaseMode mode = ReleaseMode::Fixed;
        float attack = -1.0f, release = -1.0f;
        bool limiterMode = false;
        bool operator== (const EnvelopeKey&) const = default;
    };
    std::array<EnvelopeKey, 2> envelopeKeys {};

    Meters meters;
};
} // namespace twinmu::dsp
