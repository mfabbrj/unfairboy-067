// TWIN-MU — gain-reduction envelope with up to three parallel "capacitors".
// A single stage gives classic attack/release. Several stages model program-dependent
// release: short peaks only charge the fast stage (quick recovery), sustained compression
// also charges the slow stages (long, pump-free recovery). Output = max of all stages.
#pragma once

#include "DspMath.h"
#include "EngineParams.h"

namespace twinmu::dsp
{
class GainReductionEnvelope
{
public:
    struct Stage
    {
        double chargeMs = 1.0;
        double releaseMs = 100.0;
    };
    static constexpr int kMaxStages = 3;

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : states)
            s = 0.0;
    }

    void configure (const Stage* stages, int count) noexcept
    {
        numStages = std::clamp (count, 1, kMaxStages);
        for (int i = 0; i < numStages; ++i)
        {
            attackCoeff[i] = onePoleCoeff (stages[i].chargeMs, sampleRate);
            releaseCoeff[i] = onePoleCoeff (stages[i].releaseMs, sampleRate);
        }
        for (int i = numStages; i < kMaxStages; ++i)
            states[i] = 0.0;
    }

    /** targetDb: instantaneous gain reduction from the gain computer (>= 0 dB). */
    double process (double targetDb) noexcept
    {
        double out = 0.0;
        for (int i = 0; i < numStages; ++i)
        {
            double& s = states[i];
            const double c = targetDb > s ? attackCoeff[i] : releaseCoeff[i];
            s = targetDb + c * (s - targetDb);
            if (s < 1.0e-12)
                s = 0.0; // denormal guard
            out = std::max (out, s);
        }
        return out;
    }

    double getState (int stage) const noexcept { return states[stage]; }

private:
    double sampleRate = 48000.0;
    int numStages = 1;
    double attackCoeff[kMaxStages] {};
    double releaseCoeff[kMaxStages] {};
    double states[kMaxStages] {};
};

/** Maps the release mode to envelope stages. TC values are inspired by the published
    time constants of the classic six-position variable-mu limiter. Returns the stage count. */
inline int makeEnvelopeStages (ReleaseMode mode, double attackMs, double releaseMs,
                               GainReductionEnvelope::Stage out[GainReductionEnvelope::kMaxStages]) noexcept
{
    using M = ReleaseMode;
    switch (mode)
    {
        case M::Fixed: out[0] = { attackMs, releaseMs }; return 1;
        case M::Auto:
            out[0] = { attackMs, releaseMs };
            out[1] = { 250.0, releaseMs * 4.0 };
            return 2;
        case M::TC1: out[0] = { 0.2, 300.0 }; return 1;
        case M::TC2: out[0] = { 0.2, 800.0 }; return 1;
        case M::TC3: out[0] = { 0.4, 2000.0 }; return 1;
        case M::TC4: out[0] = { 0.8, 5000.0 }; return 1;
        case M::TC5:
            out[0] = { 0.2, 2000.0 };
            out[1] = { 800.0, 10000.0 };
            return 2;
        case M::TC6:
            out[0] = { 0.4, 300.0 };
            out[1] = { 300.0, 10000.0 };
            out[2] = { 3000.0, 25000.0 };
            return 3;
    }
    out[0] = { attackMs, releaseMs };
    return 1;
}
} // namespace twinmu::dsp
