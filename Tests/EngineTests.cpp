// Integration tests of the complete engine.
#include <doctest.h>

#include "DSP/GainComputer.h"
#include "TestHelpers.h"

#include <chrono>
#include <memory>

using namespace twinmu;
using namespace twinmu::dsp;
using namespace twinmu::test;

namespace
{
std::unique_ptr<Engine> makeEngine (double sr, int maxBlock, const EngineParams& p, int os = 1)
{
    auto e = std::make_unique<Engine>();
    e->setParameters (p);
    e->prepare (sr, maxBlock, 2);
    e->setOversamplingFactor (os);
    return e;
}
} // namespace

TEST_CASE ("engine: reported latency matches the measured impulse delay")
{
    for (int os : { 1, 2, 4, 8 })
    {
        const auto p = transparentParams();
        auto e = makeEngine (48000.0, 256, p, os);
        Buffer l (8000, 0.0f), r (8000, 0.0f);
        l[1000] = r[1000] = 0.5f;
        run (*e, l, r, 256, p);
        int argmax = 0;
        for (int i = 0; i < (int) l.size(); ++i)
            if (std::abs (l[(size_t) i]) > std::abs (l[(size_t) argmax]))
                argmax = i;
        CAPTURE (os);
        CHECK (argmax - 1000 == e->getLatencySamples());
        CHECK (l[(size_t) argmax] == doctest::Approx (0.5f).epsilon (0.02));
    }
}

TEST_CASE ("engine: transparent settings reproduce the input (1x)")
{
    const auto p = transparentParams();
    auto e = makeEngine (48000.0, 512, p);
    const Buffer inL = whiteNoise (24000, 0.5f, 3), inR = whiteNoise (24000, 0.5f, 4);
    Buffer l = inL, r = inR;
    run (*e, l, r, 512, p);
    const int lat = e->getLatencySamples();
    float err = 0.0f;
    for (int i = lat; i < (int) l.size(); ++i)
        err = std::max ({ err, std::abs (l[(size_t) i] - inL[(size_t) (i - lat)]), std::abs (r[(size_t) i] - inR[(size_t) (i - lat)]) });
    CHECK (err < 1.0e-6f);
}

TEST_CASE ("engine: static ratio and threshold are accurate (compressor mode)")
{
    struct Case { float levelDb, thrDb, ratio; };
    for (const auto c : { Case { -6.0f, -20.0f, 4.0f }, Case { -10.0f, -30.0f, 2.0f }, Case { -3.0f, -24.0f, 10.0f } })
    {
        auto p = transparentParams();
        p.a.thresholdDb = c.thrDb;
        p.a.ratio = c.ratio;
        p.a.attackMs = 0.1f;
        p.a.releaseMs = 2000.0f;
        auto e = makeEngine (48000.0, 512, p);
        Buffer l = sine (1000.0, 48000.0, 96000, dbToGain (c.levelDb)), r = l;
        run (*e, l, r, 512, p);
        const float expected = c.thrDb + (c.levelDb - c.thrDb) / c.ratio;
        const float measured = gainToDb (peak (l, 72000, 96000));
        CAPTURE (c.ratio);
        CAPTURE (measured);
        CAPTURE (expected);
        CHECK (std::abs (measured - expected) < 0.4f);
    }
}

TEST_CASE ("engine: attack makes gain reduction build up over time")
{
    auto p = transparentParams();
    p.a.thresholdDb = -30.0f;
    p.a.ratio = 10.0f;
    p.a.attackMs = 20.0f;
    p.a.releaseMs = 500.0f;
    auto e = makeEngine (48000.0, 64, p);
    Buffer l = sine (1000.0, 48000.0, 48000, 0.5f), r = l;
    run (*e, l, r, 64, p);
    const int lat = e->getLatencySamples();
    const float early = peak (l, lat, lat + 96);               // first 2 ms
    const float late = peak (l, lat + 24000, lat + 24096);     // after 500 ms
    CHECK (gainToDb (early) - gainToDb (late) > 10.0f);
}

TEST_CASE ("engine: mix 0 % returns the dry signal exactly, whatever the settings")
{
    EngineParams p; // vintage, heavy colour
    p.tubePercent = 100.0f;
    p.a.thresholdDb = -40.0f;
    p.mixPercent = 0.0f;
    auto e = makeEngine (48000.0, 256, p, 4);
    const Buffer in = sine (220.0, 48000.0, 20000, 0.8f);
    Buffer l = in, r = in;
    run (*e, l, r, 256, p);
    const int lat = e->getLatencySamples();
    float err = 0.0f;
    for (int i = lat; i < (int) l.size(); ++i)
        err = std::max (err, std::abs (l[(size_t) i] - in[(size_t) (i - lat)]));
    CHECK (err < 1.0e-7f);
}

TEST_CASE ("engine: bypass returns the latency-compensated input")
{
    EngineParams p;
    p.bypass = true;
    p.limiterOn = true;
    auto e = makeEngine (48000.0, 256, p, 2);
    const Buffer in = whiteNoise (20000, 0.9f, 7);
    Buffer l = in, r = in;
    run (*e, l, r, 256, p);
    const int lat = e->getLatencySamples();
    float err = 0.0f;
    for (int i = 4000; i < (int) l.size(); ++i)
        err = std::max (err, std::abs (l[(size_t) i] - in[(size_t) (i - lat)]));
    CHECK (err < 1.0e-6f);
}

TEST_CASE ("engine: mid/side matrix reconstructs perfectly, width 0 is mono")
{
    auto p = transparentParams();
    p.stereoMode = StereoMode::MidSide;
    auto e = makeEngine (48000.0, 512, p);
    const Buffer inL = whiteNoise (10000, 0.5f, 11), inR = whiteNoise (10000, 0.5f, 12);
    Buffer l = inL, r = inR;
    run (*e, l, r, 512, p);
    const int lat = e->getLatencySamples();
    float err = 0.0f;
    for (int i = lat; i < (int) l.size(); ++i)
        err = std::max ({ err, std::abs (l[(size_t) i] - inL[(size_t) (i - lat)]), std::abs (r[(size_t) i] - inR[(size_t) (i - lat)]) });
    CHECK (gainToDb (err) < -120.0f);

    p.widthPercent = 0.0f;
    auto e2 = makeEngine (48000.0, 512, p);
    l = inL;
    r = inR;
    run (*e2, l, r, 512, p);
    float diff = 0.0f;
    for (int i = 0; i < (int) l.size(); ++i)
        diff = std::max (diff, std::abs (l[(size_t) i] - r[(size_t) i]));
    CHECK (diff == 0.0f);
}

TEST_CASE ("engine: mid/side compresses mid and side independently")
{
    auto p = transparentParams();
    p.stereoMode = StereoMode::MidSide;
    p.a.thresholdDb = -40.0f; // mid: heavy
    p.a.ratio = 10.0f;
    p.b.thresholdDb = 0.0f;   // side: untouched
    p.b.ratio = 1.0f;
    p.a.attackMs = p.b.attackMs = 1.0f;
    auto e = makeEngine (48000.0, 512, p);
    // pure side signal (L = -R) must pass untouched, pure mid must be compressed
    Buffer sl = sine (500.0, 48000.0, 24000, 0.5f), sr = sl;
    for (auto& s : sr) s = -s;
    run (*e, sl, sr, 512, p);
    CHECK (gainToDb (peak (sl, 12000, 24000)) == doctest::Approx (gainToDb (0.5f)).epsilon (0.01));

    auto e2 = makeEngine (48000.0, 512, p);
    Buffer ml = sine (500.0, 48000.0, 24000, 0.5f), mr = ml;
    run (*e2, ml, mr, 512, p);
    CHECK (gainToDb (peak (ml, 12000, 24000)) < gainToDb (0.5f) - 20.0f);
}

TEST_CASE ("engine: stereo link vs dual mono")
{
    auto measure = [] (StereoMode mode, float& grL, float& grR)
    {
        auto p = transparentParams();
        p.stereoMode = mode;
        p.a.thresholdDb = p.b.thresholdDb = -30.0f;
        p.a.ratio = p.b.ratio = 4.0f;
        p.a.attackMs = p.b.attackMs = 1.0f;
        p.a.releaseMs = p.b.releaseMs = 300.0f;
        auto e = makeEngine (48000.0, 512, p);
        Buffer l = sine (1000.0, 48000.0, 48000, 0.5f), r = sine (1000.0, 48000.0, 48000, 0.05f);
        run (*e, l, r, 512, p);
        grL = gainToDb (0.5f) - gainToDb (peak (l, 36000, 48000));
        grR = gainToDb (0.05f) - gainToDb (peak (r, 36000, 48000));
    };
    float gl = 0, gr = 0;
    measure (StereoMode::Linked, gl, gr);
    CHECK (std::abs (gl - gr) < 0.3f); // image preserved
    measure (StereoMode::DualMono, gl, gr);
    CHECK (gl - gr > 10.0f);           // independent
}

TEST_CASE ("engine: anti-phase stereo stays anti-phase when linked")
{
    auto p = transparentParams();
    p.a.thresholdDb = -30.0f;
    p.a.ratio = 6.0f;
    auto e = makeEngine (48000.0, 128, p, 2);
    Buffer l = whiteNoise (20000, 0.7f, 21), r = l;
    for (auto& s : r) s = -s;
    run (*e, l, r, 128, p);
    float sum = 0.0f;
    for (size_t i = 0; i < l.size(); ++i)
        sum = std::max (sum, std::abs (l[i] + r[i]));
    CHECK (sum < 1.0e-6f);
}

TEST_CASE ("engine: external sidechain drives the compression")
{
    auto p = transparentParams();
    p.a.thresholdDb = -20.0f;
    p.a.ratio = 10.0f;
    p.a.attackMs = 1.0f;
    const Buffer key = sine (100.0, 48000.0, 24000, 0.9f);

    auto e = makeEngine (48000.0, 256, p);
    Buffer l = sine (1000.0, 48000.0, 24000, 0.05f), r = l;
    run (*e, l, r, 256, p, &key, &key); // source Internal: key is ignored
    CHECK (gainToDb (peak (l, 12000, 24000)) == doctest::Approx (gainToDb (0.05f)).epsilon (0.01));

    p.scSource = SidechainSource::External;
    auto e2 = makeEngine (48000.0, 256, p);
    l = sine (1000.0, 48000.0, 24000, 0.05f);
    r = l;
    run (*e2, l, r, 256, p, &key, &key);
    CHECK (gainToDb (peak (l, 12000, 24000)) < gainToDb (0.05f) - 10.0f);
}

TEST_CASE ("engine: sidechain high-pass stops low frequencies from triggering compression")
{
    auto grFor = [] (SidechainFilter filter)
    {
        auto p = transparentParams();
        p.a.thresholdDb = -20.0f;
        p.a.ratio = 4.0f;
        p.scFilter = filter;
        p.scFreqHz = 1000.0f;
        auto e = makeEngine (48000.0, 512, p);
        Buffer l = sine (50.0, 48000.0, 48000, 0.5f), r = l;
        run (*e, l, r, 512, p);
        return gainToDb (0.5f) - gainToDb (peak (l, 24000, 48000));
    };
    const float unfiltered = grFor (SidechainFilter::Off);
    const float filtered = grFor (SidechainFilter::HighPass);
    MESSAGE ("50 Hz tone GR: no filter " << unfiltered << " dB, SC HP 1 kHz " << filtered << " dB");
    CHECK (unfiltered > 8.0f);
    CHECK (filtered < 0.5f);
}

TEST_CASE ("engine: output limiter holds the ceiling (sample peak)")
{
    EngineParams p;
    p.limiterOn = true;
    p.limCeilingDb = -1.0f;
    p.inputGainDb = 18.0f;
    auto e = makeEngine (48000.0, 512, p, 2);
    Buffer l = whiteNoise (48000, 0.9f, 5), r = whiteNoise (48000, 0.9f, 6);
    run (*e, l, r, 512, p);
    CHECK (peak (l) <= dbToGain (-1.0f) * 1.00001f);
    CHECK (peak (r) <= dbToGain (-1.0f) * 1.00001f);
}

TEST_CASE ("engine: vintage mode is stable with extreme levels and transients")
{
    EngineParams p;
    p.a.releaseMode = ReleaseMode::TC6;
    p.tubePercent = 100.0f;
    p.transformerPercent = 100.0f;
    auto e = makeEngine (96000.0, 64, p, 4);
    Buffer l (96000, 0.0f), r (96000, 0.0f);
    for (int i = 0; i < 96000; i += 4800) // +24 dBFS clicks and bursts
    {
        l[(size_t) i] = 15.8f;
        r[(size_t) i] = -15.8f;
    }
    const Buffer loud = sine (60.0, 96000.0, 48000, 15.8f);
    std::copy (loud.begin(), loud.end(), l.begin() + 48000);
    run (*e, l, r, 64, p);
    for (size_t i = 0; i < l.size(); ++i)
    {
        REQUIRE (std::isfinite (l[i]));
        REQUIRE (std::isfinite (r[i]));
    }
    CHECK (peak (l) < 20.0f);
}

TEST_CASE ("engine: NaN and Inf in the input never reach the output")
{
    EngineParams p;
    auto e = makeEngine (48000.0, 128, p, 2);
    Buffer l = whiteNoise (4800, 0.5f, 8), r = l;
    l[100] = std::nanf ("");
    r[200] = INFINITY;
    l[3000] = -INFINITY;
    run (*e, l, r, 128, p);
    for (size_t i = 0; i < l.size(); ++i)
    {
        REQUIRE (std::isfinite (l[i]));
        REQUIRE (std::isfinite (r[i]));
    }
}

TEST_CASE ("engine: silence in, silence out (noise generator off)")
{
    EngineParams p;
    p.tubePercent = 100.0f;
    auto e = makeEngine (44100.0, 256, p, 8);
    Buffer l (20000, 0.0f), r (20000, 0.0f);
    run (*e, l, r, 256, p);
    CHECK (peak (l) == 0.0f);
}

TEST_CASE ("engine: identical output for any block size")
{
    EngineParams p;
    p.limiterOn = true;
    p.a.releaseMode = ReleaseMode::TC5;
    const Buffer inL = whiteNoise (30000, 0.8f, 31), inR = whiteNoise (30000, 0.8f, 32);

    Buffer refL = inL, refR = inR;
    auto ref = makeEngine (48000.0, 512, p, 2);
    run (*ref, refL, refR, 512, p);

    for (int block : { 1, 32, 37, 64, 128, 256, 4096 }) // 4096 > max block: split internally
    {
        Buffer l = inL, r = inR;
        auto e = makeEngine (48000.0, 512, p, 2);
        run (*e, l, r, block, p);
        float err = 0.0f;
        for (size_t i = 0; i < l.size(); ++i)
            err = std::max (err, std::abs (l[i] - refL[i]));
        CAPTURE (block);
        CHECK (err < 1.0e-6f);
    }
}

TEST_CASE ("engine: all sample rates and buffer sizes stay bounded and finite")
{
    EngineParams p;
    p.limiterOn = true;
    p.limCeilingDb = -0.3f;
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        for (int block : { 32, 64, 128, 256, 512 })
        {
            auto e = makeEngine (sr, 512, p, sr > 100000.0 ? 2 : 4);
            Buffer l = whiteNoise ((int) (sr * 0.1), 2.0f, 9), r = whiteNoise ((int) (sr * 0.1), 2.0f, 10);
            run (*e, l, r, block, p);
            bool ok = true;
            for (size_t i = 0; i < l.size(); ++i)
                ok = ok && std::isfinite (l[i]) && std::isfinite (r[i]);
            CAPTURE (sr);
            CAPTURE (block);
            CHECK (ok);
            CHECK (peak (l) <= dbToGain (-0.3f) * 1.00001f);
        }
}

TEST_CASE ("engine: oversampling reduces aliasing of the tube stage")
{
    auto aliasDb = [] (int os)
    {
        auto p = transparentParams();
        p.tubePercent = 100.0f;
        auto e = makeEngine (48000.0, 512, p, os);
        Buffer l = sine (15000.0, 48000.0, 48000, 0.9f), r = l;
        run (*e, l, r, 512, p);
        // 2nd harmonic (30 kHz) folds back to 18 kHz at 48 kHz
        return toneLevelDb (l, 18000.0, 48000.0, 16384, 16384) - toneLevelDb (l, 15000.0, 48000.0, 16384, 16384);
    };
    const float a1 = aliasDb (1), a4 = aliasDb (4);
    MESSAGE ("alias product at 18 kHz: 1x " << a1 << " dB, 4x " << a4 << " dB");
    CHECK (a4 < a1 - 20.0f);
}

TEST_CASE ("engine: CPU load (informative)")
{
    EngineParams p;
    p.limiterOn = true;
    for (int os : { 1, 2, 4 })
    {
        auto e = makeEngine (48000.0, 64, p, os);
        Buffer l = whiteNoise (480000, 0.5f, 1), r = whiteNoise (480000, 0.5f, 2); // 10 s
        const auto t0 = std::chrono::steady_clock::now();
        run (*e, l, r, 64, p);
        const double seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        MESSAGE ("vintage + limiter, OS " << os << "x, 48 kHz / 64 samples: " << 10.0 / seconds << "x real time");
        CHECK (seconds < 10.0);
    }
}
