// TWIN-MU — analog character stages. Both run at the oversampled rate.
#pragma once

#include "Filters.h"

namespace twinmu::dsp
{
/** Asymmetric tube-style stage.
    A biased tanh transfer curve produces mainly even (2nd) harmonics; the static offset is
    subtracted and a DC blocker removes the dynamic DC that asymmetric clipping creates.
    Small-signal gain is normalised to 1, so the control changes colour, not level.
    'extraDrive' lets the variable-mu model push the tube harder when it compresses more. */
class TubeStage
{
public:
    void prepare (double sampleRate) noexcept
    {
        dc.setup (sampleRate, 5.0);
        reset();
    }
    void reset() noexcept { dc.reset(); }

    float process (float x, float amount, float extraDrive, bool eco) noexcept
    {
        if (amount <= 0.0f)
        {
            dc.track (x); // exact bypass at 0 %
            return x;
        }

        const float drive = 1.0f + 3.0f * amount + extraDrive;
        const float bias = 0.15f * amount;
        const float u = x * kHeadroom;
        const float t0 = eco ? fastTanh (drive * bias) : std::tanh (drive * bias);
        const float t1 = eco ? fastTanh (drive * (u + bias)) : std::tanh (drive * (u + bias));
        const float norm = 1.0f / (drive * (1.0f - t0 * t0));
        const float wet = (t1 - t0) * norm / kHeadroom;
        return dc.process (x + amount * (wet - x));
    }

private:
    static constexpr float kHeadroom = 0.5f; // the stage starts clipping hard around +6 dBFS
    DcBlocker dc;
};

/** Transformer-style stage: core flux is proportional to the integral of the voltage,
    so low frequencies saturate first. The low band (leaky integrator ~ 1-pole LP) is
    soft-saturated and recombined; a gentle HF loss models winding capacitance. */
class TransformerStage
{
public:
    void prepare (double sampleRate) noexcept
    {
        flux.setup (sampleRate, 90.0);
        hf.setup (sampleRate, std::min (24000.0, 0.45 * sampleRate));
        reset();
    }
    void reset() noexcept
    {
        flux.reset();
        hf.reset();
    }

    float process (float x, float amount, bool eco) noexcept
    {
        const float low = flux.process (x);
        const float smoothed = hf.process (x);
        if (amount <= 0.0f)
            return x;

        const float fluxGain = 1.0f + 6.0f * amount;
        const float driven = low * fluxGain;
        const float sat = (eco ? fastTanh (driven) : std::tanh (driven)) / fluxGain;
        float y = x + (sat - low);                   // replace the LF band by its saturated version
        y += 0.5f * amount * (smoothed - x);         // up to ~ -1.5 dB at 20 kHz (base 44.1/48 kHz)
        return y;
    }

private:
    OnePoleLowPass flux, hf;
};
} // namespace twinmu::dsp
