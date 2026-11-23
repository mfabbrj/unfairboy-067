// Unit tests of the individual DSP building blocks.
#include <doctest.h>

#include "DSP/GainComputer.h"
#include "TestHelpers.h"

using namespace twinmu;
using namespace twinmu::dsp;
using namespace twinmu::test;

TEST_CASE ("dB conversions round-trip")
{
    for (float db : { -100.0f, -20.0f, -6.0f, 0.0f, 12.0f })
        CHECK (gainToDb (dbToGain (db)) == doctest::Approx (db).epsilon (1e-4));
    CHECK (gainToDb (0.0f) == -150.0f);
    CHECK (sanitize (std::nanf ("")) == 0.0f);
    CHECK (sanitize (INFINITY) == 0.0f);
}

TEST_CASE ("clean gain computer: hard knee ratio and threshold")
{
    // 10 dB over threshold at 4:1 -> output 2.5 dB over -> GR 7.5 dB
    CHECK (cleanGainReduction (-10.0f, -20.0f, 4.0f, 0.0f) == doctest::Approx (7.5f));
    CHECK (cleanGainReduction (-25.0f, -20.0f, 4.0f, 0.0f) == 0.0f);
    // infinity:1 -> output pinned at threshold
    CHECK (cleanGainReduction (-5.0f, -20.0f, 1000.0f, 0.0f) == doctest::Approx (15.0f).epsilon (0.01));
    CHECK (cleanGainReduction (0.0f, -20.0f, 1.0f, 0.0f) == 0.0f);
}

TEST_CASE ("clean gain computer: soft knee is continuous and monotonic")
{
    float previous = 0.0f;
    for (float level = -40.0f; level <= 0.0f; level += 0.01f)
    {
        const float gr = cleanGainReduction (level, -20.0f, 4.0f, 12.0f);
        CHECK (gr >= previous - 1e-5f);
        CHECK (gr - previous < 0.01f); // no jumps
        previous = gr;
    }
    // outside the knee it matches the hard-knee curve
    CHECK (cleanGainReduction (-5.0f, -20.0f, 4.0f, 12.0f) == doctest::Approx (11.25f));
}

TEST_CASE ("variable-mu curve: ratio grows with level")
{
    auto outLevel = [] (float in) { return in - variableMuGainReduction (in, -20.0f, 8.0f, 6.0f); };
    auto slope = [&] (float in) { return (outLevel (in + 0.5f) - outLevel (in - 0.5f)) / 1.0f; };

    CHECK (variableMuGainReduction (-40.0f, -20.0f, 8.0f, 6.0f) < 0.01f);
    const float s1 = slope (-18.0f), s2 = slope (-10.0f), s3 = slope (0.0f);
    CHECK (s1 > s2);
    CHECK (s2 > s3);                         // incremental ratio increases
    CHECK (1.0f / s2 == doctest::Approx (8.0f).epsilon (0.15)); // ~8:1 at 10 dB over
    CHECK (variableMuGainReduction (-10.0f, -20.0f, 1.0f, 6.0f) == 0.0f);
}

TEST_CASE ("envelope: attack and release time constants")
{
    GainReductionEnvelope env;
    env.prepare (48000.0);
    const GainReductionEnvelope::Stage stage { 10.0, 100.0 };
    env.configure (&stage, 1);

    double v = 0.0;
    for (int i = 0; i < 480; ++i) // 10 ms
        v = env.process (10.0);
    CHECK (v == doctest::Approx (6.321).epsilon (0.01));

    for (int i = 0; i < 48000; ++i)
        v = env.process (10.0);
    for (int i = 0; i < 4800; ++i) // 100 ms release
        v = env.process (0.0);
    CHECK (v == doctest::Approx (3.679).epsilon (0.01));
}

TEST_CASE ("envelope: TC5 release is program dependent")
{
    auto timeToRecover = [] (double burstMs)
    {
        GainReductionEnvelope env;
        env.prepare (1000.0); // 1 sample = 1 ms keeps the test fast
        GainReductionEnvelope::Stage stages[3];
        const int n = makeEnvelopeStages (ReleaseMode::TC5, 0.0, 0.0, stages);
        env.configure (stages, n);
        for (int i = 0; i < (int) burstMs; ++i)
            env.process (10.0);
        for (int i = 0; i < 60000; ++i)
            if (env.process (0.0) < 3.0)
                return i;
        return 60000;
    };
    const int shortPeak = timeToRecover (5.0);
    const int sustained = timeToRecover (3000.0);
    MESSAGE ("TC5 recovery to 3 dB: isolated peak " << shortPeak << " ms, sustained " << sustained << " ms");
    CHECK (shortPeak == doctest::Approx (2400).epsilon (0.1));
    CHECK (sustained > 3 * shortPeak);
}

TEST_CASE ("SVF sidechain filters: -3 dB at cut-off, unity band-pass peak")
{
    const double sr = 48000.0;
    auto response = [&] (SvfTpt::Type type, double freq)
    {
        SvfTpt f;
        f.setup (sr, 1000.0, 0.7071, type);
        Buffer x = sine (freq, sr, 24000, 1.0f);
        for (auto& s : x)
            s = f.process (s);
        return toneLevelDb (x, freq, sr, 12000, 12000);
    };
    CHECK (response (SvfTpt::Type::HighPass, 1000.0) == doctest::Approx (-3.01).epsilon (0.03));
    CHECK (response (SvfTpt::Type::LowPass, 1000.0) == doctest::Approx (-3.01).epsilon (0.03));
    CHECK (response (SvfTpt::Type::BandPass, 1000.0) == doctest::Approx (0.0).scale (1.0).epsilon (0.05));
    CHECK (response (SvfTpt::Type::HighPass, 50.0) < -50.0f);
}

TEST_CASE ("oversampler: transparent pass-band and exact integer latency")
{
    const double sr = 48000.0;
    for (int factor : { 1, 2, 4, 8 })
    {
        Oversampler os;
        os.prepare (256);
        os.setFactor (factor);
        const int latency = os.getTotalLatency();
        CHECK (latency == (factor == 1 ? 0 : factor == 2 ? 32 : factor == 4 ? 48 : 56));

        for (double freq : { 1000.0, 15000.0 })
        {
            const Buffer x = sine (freq, sr, 38 * 256, 0.5f); // whole number of blocks
            Buffer y (x.size());
            for (int pos = 0; pos < (int) x.size(); pos += 256)
            {
                os.upsample (0, x.data() + pos, 256);
                os.downsample (0, y.data() + pos, 256);
            }
            float err = 0.0f;
            for (int i = 1000; i < (int) x.size(); ++i)
                err = std::max (err, std::abs (y[(size_t) i] - x[(size_t) (i - latency)]));
            CAPTURE (factor);
            CAPTURE (freq);
            CHECK (err < (freq < 10000.0 ? 1.0e-3f : 1.0e-2f));
        }
    }
}

TEST_CASE ("oversampler: images are rejected")
{
    Oversampler os;
    os.prepare (4800);
    os.setFactor (2);
    const Buffer x = sine (15000.0, 48000.0, 4800, 0.5f);
    const float* hi = os.upsample (0, x.data(), 4800);
    const Buffer up (hi, hi + 9600);
    const float wanted = toneLevelDb (up, 15000.0, 96000.0, 1000, 8192);
    const float image = toneLevelDb (up, 33000.0, 96000.0, 1000, 8192);
    MESSAGE ("2x image rejection: " << wanted - image << " dB");
    CHECK (wanted - image > 70.0f);
}

TEST_CASE ("limiter: sample peaks never exceed the ceiling")
{
    Limiter lim;
    lim.prepare (48000.0);
    lim.setLookaheadMs (1.5f);
    lim.setParameters (-1.0f, 50.0f, 1.0f, true, true);
    Buffer l = whiteNoise (48000, 4.0f, 1), r = whiteNoise (48000, 4.0f, 2); // ~ +12 dBFS peaks
    for (int pos = 0; pos < 48000; pos += 128)
        lim.process (l.data() + pos, r.data() + pos, 128);
    const float ceiling = dbToGain (-1.0f);
    CHECK (peak (l) <= ceiling * 1.00001f);
    CHECK (peak (r) <= ceiling * 1.00001f);
    CHECK (lim.getLastGainReductionDb() > 6.0f);
}

TEST_CASE ("limiter: latency is look-ahead + 4 and low signals pass untouched")
{
    Limiter lim;
    lim.prepare (48000.0);
    lim.setLookaheadMs (1.5f);
    lim.setParameters (-1.0f, 50.0f, 1.0f, true, true);
    CHECK (lim.getLatency() == 72 + 4);
    Buffer x (1000, 0.0f);
    x[100] = 0.1f;
    lim.process (x.data(), nullptr, 1000);
    CHECK (x[(size_t) (100 + lim.getLatency())] == doctest::Approx (0.1f));
    CHECK (peak (x, 0, 100 + lim.getLatency()) == 0.0f);
}

TEST_CASE ("tube stage: transparent at 0 %, mostly 2nd harmonic when driven")
{
    const double sr = 48000.0;
    auto harmonics = [&] (float amount, float& h2, float& h3)
    {
        TubeStage t;
        t.prepare (sr);
        Buffer x = sine (1000.0, sr, 48000, 0.5f);
        for (auto& s : x)
            s = t.process (s, amount, 0.0f, false);
        const float f = toneLevelDb (x, 1000.0, sr, 24000, 24000);
        h2 = toneLevelDb (x, 2000.0, sr, 24000, 24000) - f;
        h3 = toneLevelDb (x, 3000.0, sr, 24000, 24000) - f;
    };
    float h2 = 0, h3 = 0;
    harmonics (0.0f, h2, h3);
    CHECK (h2 < -120.0f);
    harmonics (1.0f, h2, h3);
    MESSAGE ("tube 100 % @ -6 dBFS: H2 " << h2 << " dB, H3 " << h3 << " dB");
    CHECK (h2 > -50.0f);
    CHECK (h2 > h3);
}

TEST_CASE ("transformer stage: low frequencies distort more than high frequencies")
{
    const double sr = 48000.0;
    auto thirdHarmonic = [&] (double freq)
    {
        TransformerStage t;
        t.prepare (sr);
        Buffer x = sine (freq, sr, 96000, 0.7f);
        for (auto& s : x)
            s = t.process (s, 1.0f, false);
        return toneLevelDb (x, 3.0 * freq, sr, 48000, 48000) - toneLevelDb (x, freq, sr, 48000, 48000);
    };
    const float low = thirdHarmonic (40.0), high = thirdHarmonic (4000.0);
    MESSAGE ("transformer H3: 40 Hz " << low << " dB, 4 kHz " << high << " dB");
    CHECK (low > high + 20.0f);
}
