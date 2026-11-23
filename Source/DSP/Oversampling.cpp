#include "Oversampling.h"
#include "DspMath.h"

#include <cstring>

namespace twinmu::dsp
{
namespace
{
double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    const double q = x * x * 0.25;
    for (int k = 1; k < 50; ++k)
    {
        term *= q / ((double) k * (double) k);
        sum += term;
        if (term < 1.0e-12 * sum)
            break;
    }
    return sum;
}
} // namespace

Oversampler::Oversampler()
{
    // Kaiser-windowed half-band low-pass, cut-off at a quarter of the high sample rate.
    constexpr double beta = 8.0; // ~80 dB stop-band
    const double denom = besselI0 (beta);
    double oddSum = 0.0;
    for (int j = 0; j < kTaps; ++j)
    {
        const int offset = j - kCentre;
        double h = 0.0;
        if (offset == 0)
            h = 0.5;
        else if (offset % 2 != 0)
        {
            const double x = 0.5 * offset;
            const double r = (double) offset / (double) kCentre;
            const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / denom;
            h = 0.5 * std::sin (kPi * x) / (kPi * x) * w;
            oddSum += h;
        }
        taps[(size_t) j] = (float) h;
    }
    // Exact unity DC gain: odd taps must sum to 0.5.
    for (int j = 0; j < kTaps; ++j)
        if ((j - kCentre) % 2 != 0)
            taps[(size_t) j] = (float) (taps[(size_t) j] * 0.5 / oddSum);

    for (int m = 0; m < kOddTaps; ++m)
        oddCoeffs[(size_t) m] = taps[(size_t) (2 * m + 1)];
}

void Oversampler::prepare (int maxBlockSize)
{
    maxBlock = std::max (1, maxBlockSize);
    for (auto& ch : work)
        for (auto& b : ch)
            b.assign ((size_t) maxBlock << kMaxStages, 0.0f);
    reset();
}

void Oversampler::reset() noexcept
{
    for (auto& ch : state)
        for (auto& s : ch)
        {
            s.upHist.fill (0.0f);
            s.downHist.fill (0.0f);
            s.upPos = s.downPos = 0;
        }
}

void Oversampler::setFactor (int newFactor) noexcept
{
    int stages = 0;
    while ((1 << stages) < newFactor && stages < kMaxStages)
        ++stages;
    numStages = stages;
    reset();
}

int Oversampler::getUpLatency() const noexcept
{
    int latency = 0;
    for (int s = 0; s < numStages; ++s)
        latency += kCentre >> (s + 1); // 16, 8, 4
    return latency;
}

int Oversampler::getTotalLatency() const noexcept { return 2 * getUpLatency(); }

void Oversampler::upStage (StageState& s, const float* in, float* out, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        s.upHist[(size_t) s.upPos] = in[i];
        s.upHist[(size_t) (s.upPos + kOddTaps)] = in[i];
        s.upPos = (s.upPos + 1) % kOddTaps;

        // window [upPos, upPos + 31] holds oldest .. newest; x[n-m] = hist[upPos + 31 - m]
        const float* h = s.upHist.data() + s.upPos;
        float odd = 0.0f;
        for (int m = 0; m < kOddTaps; ++m)
            odd += oddCoeffs[(size_t) m] * h[kOddTaps - 1 - m];

        out[2 * i] = h[kOddTaps - 1 - kCentre / 2];  // x[n-16]  (2 * h[centre] == 1)
        out[2 * i + 1] = 2.0f * odd;
    }
}

void Oversampler::downStage (StageState& s, const float* in, float* out, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        s.downHist[(size_t) s.downPos] = in[2 * i];
        s.downHist[(size_t) (s.downPos + kTaps)] = in[2 * i];
        s.downPos = (s.downPos + 1) % kTaps;

        // window [downPos, downPos + 64]: v[2i - j] = hist[downPos + 64 - j]
        const float* h = s.downHist.data() + s.downPos;
        float acc = 0.5f * h[kTaps - 1 - kCentre];
        for (int m = 0; m < kOddTaps; ++m)
            acc += oddCoeffs[(size_t) m] * h[kTaps - 2 - 2 * m];
        out[i] = acc;

        s.downHist[(size_t) s.downPos] = in[2 * i + 1];
        s.downHist[(size_t) (s.downPos + kTaps)] = in[2 * i + 1];
        s.downPos = (s.downPos + 1) % kTaps;
    }
}

float* Oversampler::upsample (int channel, const float* input, int numSamples) noexcept
{
    auto& buffers = work[(size_t) channel];
    if (numStages == 0)
    {
        std::memcpy (buffers[0].data(), input, sizeof (float) * (size_t) numSamples);
        finalBuffer[(size_t) channel] = 0;
        return buffers[0].data();
    }

    const float* src = input;
    int idx = 0;
    for (int s = 0; s < numStages; ++s)
    {
        idx = s % 2;
        upStage (state[(size_t) channel][(size_t) s], src, buffers[(size_t) idx].data(), numSamples << s);
        src = buffers[(size_t) idx].data();
    }
    finalBuffer[(size_t) channel] = idx;
    return buffers[(size_t) idx].data();
}

void Oversampler::downsample (int channel, float* output, int numSamples) noexcept
{
    auto& buffers = work[(size_t) channel];
    int idx = finalBuffer[(size_t) channel];
    if (numStages == 0)
    {
        std::memcpy (output, buffers[(size_t) idx].data(), sizeof (float) * (size_t) numSamples);
        return;
    }

    for (int s = numStages - 1; s >= 0; --s)
    {
        const float* src = buffers[(size_t) idx].data();
        float* dst = (s == 0) ? output : buffers[(size_t) (1 - idx)].data();
        downStage (state[(size_t) channel][(size_t) s], src, dst, numSamples << s);
        idx = 1 - idx;
    }
}
} // namespace twinmu::dsp
