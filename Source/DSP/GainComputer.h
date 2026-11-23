// TWIN-MU — static gain computers. All functions return gain reduction in dB (>= 0).
#pragma once

#include "DspMath.h"

namespace twinmu::dsp
{
/** Classic compressor curve with a quadratic soft knee (Giannoulis / Massberg / Reiss). */
inline float cleanGainReduction (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept
{
    const float slope = 1.0f - 1.0f / std::max (ratio, 1.0f);
    const float over = levelDb - thresholdDb;

    if (kneeDb > 0.0f && 2.0f * over > -kneeDb && 2.0f * over < kneeDb)
    {
        const float t = over + 0.5f * kneeDb;
        return slope * t * t / (2.0f * kneeDb);
    }
    return over > 0.0f ? slope * over : 0.0f;
}

/** Variable-mu curve inspired by remote-cutoff tube compressors:
    the incremental ratio grows with level (gentle near threshold, limiting on loud peaks).
    'ratio' is the incremental ratio reached 10 dB above threshold.
    The knee is always soft (min 6 dB), smoothed with a softplus. */
inline float variableMuGainReduction (float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept
{
    const float k = (std::sqrt (std::max (ratio, 1.0f)) - 1.0f) * 0.1f;
    const float width = std::max (kneeDb, 6.0f);
    const float diff = levelDb - thresholdDb;
    const float z = 4.0f * diff / width;
    const float over = z > 20.0f ? diff : (z < -20.0f ? 0.0f : 0.25f * width * std::log1p (std::exp (z)));
    // output = threshold + over / (1 + k*over)  ->  GR = over - over / (1 + k*over)
    return k * over * over / (1.0f + k * over);
}
} // namespace twinmu::dsp
