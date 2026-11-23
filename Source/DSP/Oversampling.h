// TWIN-MU — linear-phase half-band FIR oversampler, 1x / 2x / 4x / 8x.
// Each 2x stage uses a 65-tap Kaiser-windowed half-band filter evaluated in polyphase form.
// Latencies are whole base-rate samples, so the dry path can be aligned exactly:
//   up: 16 / 24 / 28 samples, up+down: 32 / 48 / 56 samples (2x / 4x / 8x).
#pragma once

#include <array>
#include <vector>

namespace twinmu::dsp
{
class Oversampler
{
public:
    static constexpr int kMaxStages = 3;  // 8x
    static constexpr int kMaxChannels = 2;
    static constexpr int kTaps = 65;
    static constexpr int kCentre = 32;
    static constexpr int kOddTaps = 32;   // taps at offsets +-1, +-3 ... +-31

    Oversampler();

    /** Allocates everything for the worst case. Not real-time safe. */
    void prepare (int maxBlockSize);
    void reset() noexcept;

    /** 1, 2, 4 or 8. Allocation-free, but resets the filter state (call with audio suspended). */
    void setFactor (int newFactor) noexcept;
    int getFactor() const noexcept { return 1 << numStages; }

    int getUpLatency() const noexcept;    // base-rate samples
    int getTotalLatency() const noexcept; // up + down, base-rate samples

    /** Returns a buffer of numSamples * factor samples (valid until the next call for that channel). */
    float* upsample (int channel, const float* input, int numSamples) noexcept;

    /** Decimates the buffer returned by upsample() (processed in place) into output. */
    void downsample (int channel, float* output, int numSamples) noexcept;

private:
    struct StageState
    {
        std::array<float, 2 * kOddTaps> upHist {};   // doubled ring: contiguous reads
        int upPos = 0;
        std::array<float, 2 * kTaps> downHist {};
        int downPos = 0;
    };

    void upStage (StageState& s, const float* in, float* out, int n) noexcept;
    void downStage (StageState& s, const float* in, float* out, int n) noexcept;

    std::array<float, kOddTaps> oddCoeffs {};       // h[centre - (2m+1)] == h[centre + (2m+1)]
    std::array<float, kTaps> taps {};
    int numStages = 0;
    int maxBlock = 0;
    std::array<std::array<StageState, kMaxStages>, kMaxChannels> state {};
    std::array<std::array<std::vector<float>, 2>, kMaxChannels> work; // ping-pong buffers
    std::array<int, kMaxChannels> finalBuffer {};
};
} // namespace twinmu::dsp
