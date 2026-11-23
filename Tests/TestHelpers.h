// Signal generators and measurements shared by the tests.
#pragma once

#include "DSP/Engine.h"

#include <cmath>
#include <complex>
#include <vector>

namespace twinmu::test
{
using Buffer = std::vector<float>;

inline Buffer sine (double freq, double sampleRate, int length, float amplitude, double phase = 0.0)
{
    Buffer b ((size_t) length);
    for (int i = 0; i < length; ++i)
        b[(size_t) i] = amplitude * (float) std::sin (2.0 * dsp::kPi * freq * i / sampleRate + phase);
    return b;
}

inline Buffer whiteNoise (int length, float amplitude, uint32_t seed = 42)
{
    dsp::Xorshift32 rng (seed);
    Buffer b ((size_t) length);
    for (auto& s : b)
        s = amplitude * rng.nextBipolar();
    return b;
}

inline float peak (const Buffer& b, int from = 0, int to = -1)
{
    if (to < 0) to = (int) b.size();
    float p = 0.0f;
    for (int i = from; i < to; ++i)
        p = std::max (p, std::abs (b[(size_t) i]));
    return p;
}

inline float rmsDb (const Buffer& b, int from, int to)
{
    double sum = 0.0;
    for (int i = from; i < to; ++i)
        sum += (double) b[(size_t) i] * b[(size_t) i];
    return dsp::gainToDb ((float) std::sqrt (sum / std::max (1, to - from)));
}

/** Amplitude of one frequency component (single-bin DFT, Hann window), in dB re full scale sine. */
inline float toneLevelDb (const Buffer& b, double freq, double sampleRate, int from, int length)
{
    std::complex<double> acc { 0.0, 0.0 };
    double wsum = 0.0;
    for (int i = 0; i < length; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * dsp::kPi * i / (length - 1));
        const double ph = -2.0 * dsp::kPi * freq * i / sampleRate;
        acc += w * (double) b[(size_t) (from + i)] * std::complex<double> (std::cos (ph), std::sin (ph));
        wsum += w;
    }
    return dsp::gainToDb ((float) (2.0 * std::abs (acc) / wsum), -200.0f);
}

/** Neutral settings: no compression, no colour, no limiter. Useful as a test baseline. */
inline dsp::EngineParams transparentParams()
{
    dsp::EngineParams p;
    p.mode = dsp::Mode::Compressor;
    p.a.thresholdDb = p.b.thresholdDb = 0.0f;
    p.a.ratio = p.b.ratio = 1.0f;
    p.kneeDb = 0.0f;
    p.tubePercent = 0.0f;
    p.transformerPercent = 0.0f;
    p.scFilter = dsp::SidechainFilter::Off;
    p.limiterOn = false;
    return p;
}

/** Runs a stereo signal through the engine in blocks of blockSize. */
inline void run (dsp::Engine& engine, Buffer& left, Buffer& right, int blockSize,
                 const dsp::EngineParams& params, const Buffer* scL = nullptr, const Buffer* scR = nullptr)
{
    const int total = (int) left.size();
    for (int pos = 0; pos < total; pos += blockSize)
    {
        const int n = std::min (blockSize, total - pos);
        engine.setParameters (params);
        float* io[2] = { left.data() + pos, right.data() + pos };
        if (scL != nullptr)
        {
            const float* sc[2] = { scL->data() + pos, (scR ? scR : scL)->data() + pos };
            engine.process (io, io, n, sc, 2);
        }
        else
            engine.process (io, io, n);
    }
}
} // namespace twinmu::test
