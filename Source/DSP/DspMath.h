// TWIN-MU — basic DSP utilities shared by every module.
// Pure C++20, no framework dependencies, real-time safe (no allocation, no locks).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace twinmu::dsp
{
inline constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) noexcept { return std::exp (db * 0.11512925464970229f); } // ln(10)/20
inline float gainToDb (float gain, float floorDb = -150.0f) noexcept
{
    return gain > 1.0e-30f ? std::max (floorDb, 20.0f * std::log10 (gain)) : floorDb;
}

/** Replaces NaN / Inf by silence so that one bad sample can never poison filter states. */
inline float sanitize (float x) noexcept { return std::isfinite (x) ? x : 0.0f; }

/** One-pole smoothing coefficient: a step reaches 63.2 % after timeMs.
    Returned in double precision: release times of 25 s at 8x oversampling need it. */
inline double onePoleCoeff (double timeMs, double sampleRate) noexcept
{
    if (timeMs <= 0.0 || sampleRate <= 0.0)
        return 0.0;
    return std::exp (-1.0 / (timeMs * 0.001 * sampleRate));
}

/** Rational tanh approximation (max error ~2 %), used in ECO mode. */
inline float fastTanh (float x) noexcept
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

/** Linear parameter ramp, advanced once per sample. */
class LinearSmoother
{
public:
    void prepare (double sampleRate, double rampMs, float initialValue) noexcept
    {
        steps = std::max (1, (int) std::lround (sampleRate * rampMs * 0.001));
        snap (initialValue);
    }

    void setTarget (float newTarget) noexcept
    {
        if (newTarget == target)
            return;
        target = newTarget;
        remaining = steps;
        increment = (target - current) / (float) steps;
    }

    void snap (float value) noexcept
    {
        current = target = value;
        remaining = 0;
        increment = 0.0f;
    }

    float next() noexcept
    {
        if (remaining > 0)
        {
            current += increment;
            if (--remaining == 0)
                current = target;
        }
        return current;
    }

    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept { return target; }
    bool isSmoothing() const noexcept { return remaining > 0; }

private:
    float current = 0.0f, target = 0.0f, increment = 0.0f;
    int steps = 1, remaining = 0;
};

/** Deterministic white-noise generator (no system calls on the audio thread). */
class Xorshift32
{
public:
    explicit Xorshift32 (uint32_t seed = 0x9E3779B9u) noexcept : state (seed ? seed : 1u) {}
    float nextBipolar() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }

private:
    uint32_t state;
};

inline int nextPowerOfTwo (int v) noexcept
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}
} // namespace twinmu::dsp
