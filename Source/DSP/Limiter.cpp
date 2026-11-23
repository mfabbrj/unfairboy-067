#include "Limiter.h"

namespace twinmu::dsp
{
void Limiter::prepare (double newSampleRate, float maxLookaheadMs)
{
    sampleRate = newSampleRate;
    maxLookahead = (int) std::ceil (maxLookaheadMs * 0.001 * sampleRate);
    const int size = nextPowerOfTwo (maxLookahead + kTruePeakDelay + 2);
    mask = size - 1;
    for (auto& d : delay)
        d.assign ((size_t) size, 0.0f);
    dequeValue.assign ((size_t) size, 1.0f);
    dequeTime.assign ((size_t) size, 0);
    boxRing.assign ((size_t) size, 1.0f);

    // Windowed-sinc interpolators for the true-peak estimate: taps at offsets -3 .. +4.
    const float fractions[3] = { 0.25f, 0.5f, 0.75f };
    for (int f = 0; f < 3; ++f)
    {
        double sum = 0.0;
        for (int k = 0; k < 8; ++k)
        {
            const double x = (double) (k - 3) - fractions[f];
            const double sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (kPi * x) / (kPi * x);
            const double w = 0.5 * (1.0 + std::cos (kPi * x / 4.5));
            tpCoeffs[(size_t) f][(size_t) k] = (float) (sinc * w);
            sum += sinc * w;
        }
        for (auto& c : tpCoeffs[(size_t) f])
            c = (float) (c / sum);
    }

    setLookaheadMs (1.5f);
}

void Limiter::reset() noexcept
{
    for (auto& d : delay)
        std::fill (d.begin(), d.end(), 0.0f);
    for (auto& r : recent)
        r.fill (0.0f);
    std::fill (boxRing.begin(), boxRing.end(), 1.0f);
    boxSum = (double) window;
    dequeHead = dequeSize = 0;
    envelope = 1.0;
    time = 0;
    delayPos = 0;
    lastGrDb = 0.0f;
}

void Limiter::setLookaheadMs (float ms) noexcept
{
    lookahead = std::clamp ((int) std::lround (ms * 0.001 * sampleRate), 0, maxLookahead);
    window = lookahead + 1;
    reset();
}

void Limiter::setParameters (float ceilingDb, float releaseMs, float amount01, bool truePeak, bool enabled) noexcept
{
    ceiling = dbToGain (ceilingDb);
    releaseCoeff = onePoleCoeff (std::max (1.0f, releaseMs), sampleRate);
    amount = std::clamp (amount01, 0.0f, 1.0f);
    useTruePeak = truePeak;
    active = enabled;
}

float Limiter::detectPeak (int channel) const noexcept
{
    const auto& x = recent[(size_t) channel]; // x[3] is the sample being evaluated
    float peak = std::abs (x[3]);
    if (useTruePeak)
    {
        for (const auto& c : tpCoeffs)
        {
            float v = 0.0f;
            for (int k = 0; k < 8; ++k)
                v += c[(size_t) k] * x[(size_t) k];
            peak = std::max (peak, std::abs (v));
        }
    }
    return peak;
}

void Limiter::process (float* left, float* right, int numSamples) noexcept
{
    const int numCh = right != nullptr ? 2 : 1;
    float* io[2] = { left, right };
    const int totalDelay = lookahead + kTruePeakDelay;
    float minGain = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& r = recent[(size_t) ch];
            for (int k = 0; k < 7; ++k)
                r[(size_t) k] = r[(size_t) k + 1];
            r[7] = io[ch][i];
            delay[(size_t) ch][(size_t) delayPos] = io[ch][i];
            peak = std::max (peak, detectPeak (ch));
        }

        float gain = 1.0f;
        if (active)
        {
            const float required = peak > ceiling ? ceiling / peak : 1.0f;

            // sliding minimum over the last 'window' samples
            const int cap = mask + 1;
            while (dequeSize > 0 && dequeValue[(size_t) ((dequeHead + dequeSize - 1) & mask)] >= required)
                --dequeSize;
            const int tail = (dequeHead + dequeSize) & mask;
            dequeValue[(size_t) tail] = required;
            dequeTime[(size_t) tail] = time;
            ++dequeSize;
            while (dequeTime[(size_t) dequeHead] <= time - window)
            {
                dequeHead = (dequeHead + 1) & mask;
                --dequeSize;
            }
            (void) cap;
            const double held = dequeValue[(size_t) dequeHead];

            envelope = held < envelope ? held : held + releaseCoeff * (envelope - held);

            const int slot = (int) (time % window);
            boxSum += envelope - boxRing[(size_t) slot];
            boxRing[(size_t) slot] = (float) envelope;
            const double avg = std::min (1.0, boxSum / window);
            gain = (float) (amount >= 1.0f ? avg : std::pow (avg, (double) amount));
        }
        minGain = std::min (minGain, gain);

        const int readPos = (delayPos - totalDelay) & mask;
        for (int ch = 0; ch < numCh; ++ch)
            io[ch][i] = delay[(size_t) ch][(size_t) readPos] * gain;

        delayPos = (delayPos + 1) & mask;
        ++time;
    }
    lastGrDb = -gainToDb (minGain);
}
} // namespace twinmu::dsp
