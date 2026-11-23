// TWIN-MU — filters used by the detector and the analog stages.
#pragma once

#include "DspMath.h"

namespace twinmu::dsp
{
/** Topology-preserving-transform state-variable filter (Zavalishin / Cytomic).
    Stable under fast modulation, so the sidechain frequency can be automated freely. */
class SvfTpt
{
public:
    enum class Type { LowPass, HighPass, BandPass };

    void setup (double sampleRate, double frequency, double q, Type newType) noexcept
    {
        frequency = std::clamp (frequency, 5.0, 0.49 * sampleRate);
        const double g = std::tan (kPi * frequency / sampleRate);
        k = 1.0 / std::max (q, 0.05);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        type = newType;
    }

    void reset() noexcept { ic1 = ic2 = 0.0; }

    float process (float input) noexcept
    {
        const double v0 = input;
        const double v3 = v0 - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;

        switch (type)
        {
            case Type::LowPass:  return (float) v2;
            case Type::BandPass: return (float) (k * v1); // unity gain at the centre frequency
            case Type::HighPass: return (float) (v0 - k * v1 - v2);
        }
        return input;
    }

private:
    double k = 1.4142, a1 = 0.0, a2 = 0.0, a3 = 0.0, ic1 = 0.0, ic2 = 0.0;
    Type type = Type::HighPass;
};

/** First-order low-pass (matched pole). */
class OnePoleLowPass
{
public:
    void setup (double sampleRate, double frequency) noexcept
    {
        frequency = std::clamp (frequency, 1.0, 0.49 * sampleRate);
        a = std::exp (-2.0 * kPi * frequency / sampleRate);
    }
    void reset() noexcept { z = 0.0; }
    float process (float x) noexcept
    {
        z = x + a * (z - x);
        return (float) z;
    }

private:
    double a = 0.0, z = 0.0;
};

/** DC blocker: y[n] = x[n] - x[n-1] + R * y[n-1]. */
class DcBlocker
{
public:
    void setup (double sampleRate, double cornerHz = 5.0) noexcept
    {
        r = std::exp (-2.0 * kPi * cornerHz / sampleRate);
    }
    void reset() noexcept { x1 = y1 = 0.0; }

    /** Keeps the state consistent while the stage is bypassed, so re-enabling it is click-free. */
    void track (float x) noexcept { x1 = y1 = x; }

    float process (float x) noexcept
    {
        const double y = x - x1 + r * y1;
        x1 = x;
        y1 = y;
        return (float) y;
    }

private:
    double r = 0.999, x1 = 0.0, y1 = 0.0;
};
} // namespace twinmu::dsp
