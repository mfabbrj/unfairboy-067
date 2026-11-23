// TWIN-MU — optional analog noise floor: pink hiss + mains hum with harmonics.
#pragma once

#include "DspMath.h"

namespace twinmu::dsp
{
class AnalogNoise
{
public:
    explicit AnalogNoise (uint32_t seed = 0x1234567u) noexcept : rng (seed) {}

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }
    void reset() noexcept
    {
        b0 = b1 = b2 = 0.0f;
        phase = 0.0;
    }

    /** levelDb <= -120 disables the generator. humHz: 0, 50 or 60. */
    float next (float levelDb, int humHz) noexcept
    {
        if (levelDb <= -120.0f)
            return 0.0f;

        // Paul Kellet's economy pink filter
        const float white = rng.nextBipolar();
        b0 = 0.99765f * b0 + white * 0.0990460f;
        b1 = 0.96300f * b1 + white * 0.2965164f;
        b2 = 0.57000f * b2 + white * 1.0526913f;
        const float pink = (b0 + b1 + b2 + white * 0.1848f) * 0.11f; // ~unity RMS -> scaled below

        float hum = 0.0f;
        if (humHz > 0)
        {
            phase += 2.0 * kPi * humHz / sampleRate;
            if (phase > 2.0 * kPi)
                phase -= 2.0 * kPi;
            hum = (float) (0.5 * std::sin (phase) + 0.25 * std::sin (2.0 * phase) + 0.12 * std::sin (3.0 * phase));
        }
        return dbToGain (levelDb) * (pink + 0.5f * hum);
    }

private:
    Xorshift32 rng;
    double sampleRate = 48000.0, phase = 0.0;
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
};
} // namespace twinmu::dsp
