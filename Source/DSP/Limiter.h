// TWIN-MU — look-ahead brick-wall limiter with optional true-peak detection.
//
// Gain path (per sample, stereo-linked to preserve the image):
//   required gain  g = min(1, ceiling / peak)          peak = sample or 4x-interpolated true peak
//   hold           minimum of g over the last L+1 samples (monotonic deque, O(1))
//   release        upward one-pole smoothing (downward moves are immediate)
//   attack         moving average over L+1 samples
// The audio is delayed by L (+ 4 samples for the true-peak window), so the averaged gain is
// guaranteed to be at or below the required gain when each sample is output: sample peaks
// never exceed the ceiling. Latency is constant whether the limiter is on or off.
#pragma once

#include "DspMath.h"

#include <array>
#include <vector>

namespace twinmu::dsp
{
class Limiter
{
public:
    static constexpr int kTruePeakDelay = 4;

    /** Allocates for the maximum look-ahead. Not real-time safe. */
    void prepare (double sampleRate, float maxLookaheadMs = 5.0f);
    void reset() noexcept;

    /** Allocation-free; resets the limiter (changes latency). */
    void setLookaheadMs (float ms) noexcept;
    int getLatency() const noexcept { return lookahead + kTruePeakDelay; }

    void setParameters (float ceilingDb, float releaseMs, float amount01, bool truePeak, bool enabled) noexcept;

    /** right may be nullptr (mono). Processes in place. */
    void process (float* left, float* right, int numSamples) noexcept;

    /** Largest gain reduction of the last processed block, in dB (>= 0). */
    float getLastGainReductionDb() const noexcept { return lastGrDb; }

private:
    float detectPeak (int channel) const noexcept;

    double sampleRate = 48000.0;
    int maxLookahead = 0, lookahead = 0, window = 1, mask = 0;
    float ceiling = 0.891f, amount = 1.0f;
    double releaseCoeff = 0.0;
    bool useTruePeak = true, active = false;

    std::array<std::vector<float>, 2> delay;
    int delayPos = 0;
    std::array<std::array<float, 8>, 2> recent {}; // last 8 input samples, oldest first
    std::array<std::array<float, 8>, 3> tpCoeffs {}; // fractions 0.25, 0.5, 0.75

    std::vector<float> dequeValue;
    std::vector<int64_t> dequeTime;
    int dequeHead = 0, dequeSize = 0;
    std::vector<float> boxRing;
    double boxSum = 0.0;
    double envelope = 1.0;
    int64_t time = 0;
    float lastGrDb = 0.0f;
};
} // namespace twinmu::dsp
