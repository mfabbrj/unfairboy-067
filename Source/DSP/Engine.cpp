#include "Engine.h"
#include "GainComputer.h"

namespace twinmu::dsp
{
namespace
{
constexpr float kDbToNeper = 0.11512925464970229f; // ln(10) / 20
constexpr float kMaxLookaheadMs = 5.0f;
constexpr float kFreqDepCornerHz = 250.0f;
} // namespace

Engine::Engine()
    : noise { AnalogNoise (0x1234567u), AnalogNoise (0x7654321u) }
{
}

void Engine::prepare (double newSampleRate, int maxBlockSize, int newNumChannels)
{
    sampleRate = newSampleRate;
    maxBlock = std::max (1, maxBlockSize);
    numChannels = std::clamp (newNumChannels, 1, 2);

    oversampler.prepare (maxBlock);
    limiter.prepare (sampleRate, kMaxLookaheadMs);

    for (int ch = 0; ch < 2; ++ch)
    {
        signal[(size_t) ch].assign ((size_t) maxBlock, 0.0f);
        gain[(size_t) ch].assign ((size_t) maxBlock, 1.0f);
        grDb[(size_t) ch].assign ((size_t) maxBlock, 0.0f);
        listen[(size_t) ch].assign ((size_t) maxBlock, 0.0f);
    }
    tubeValues.assign ((size_t) maxBlock, 0.0f);
    transformerValues.assign ((size_t) maxBlock, 0.0f);

    const int maxLatency = Oversampler::kCentre * 2 + (int) std::ceil (kMaxLookaheadMs * 0.001 * sampleRate)
                         + Limiter::kTruePeakDelay;
    const int drySize = nextPowerOfTwo (maxBlock + maxLatency + 2);
    dryMask = drySize - 1;
    for (int ch = 0; ch < 2; ++ch)
    {
        dryRaw[(size_t) ch].assign ((size_t) drySize, 0.0f);
        dryPost[(size_t) ch].assign ((size_t) drySize, 0.0f);
        freqDepFilters[(size_t) ch].setup (sampleRate, kFreqDepCornerHz);
        noise[(size_t) ch].prepare (sampleRate);
        envelopes[(size_t) ch].prepare (sampleRate);
    }

    inputGain.prepare (sampleRate, 20.0, dbToGain (params.inputGainDb));
    outputGain.prepare (sampleRate, 20.0, dbToGain (params.outputGainDb));
    mix.prepare (sampleRate, 20.0, params.mixPercent * 0.01f);
    width.prepare (sampleRate, 20.0, params.widthPercent * 0.01f);
    tubeAmount.prepare (sampleRate, 30.0, params.tubePercent * 0.01f);
    transformerAmount.prepare (sampleRate, 30.0, params.transformerPercent * 0.01f);
    bypassMix.prepare (sampleRate, 20.0, params.bypass ? 1.0f : 0.0f);
    for (auto& m : makeup)
        m.prepare (sampleRate, 20.0, 1.0f);

    envelopeKeys = {};
    prepareOversampledStages();
    setParameters (params);
    for (auto* s : { &inputGain, &outputGain, &mix, &width, &tubeAmount, &transformerAmount, &bypassMix, &makeup[0], &makeup[1] })
        s->snap (s->getTarget()); // start at the current values, no ramp after prepare
    reset();
}

void Engine::prepareOversampledStages() noexcept
{
    const double highRate = sampleRate * oversampler.getFactor();
    for (int ch = 0; ch < 2; ++ch)
    {
        tubes[(size_t) ch].prepare (highRate);
        transformers[(size_t) ch].prepare (highRate);
    }
}

void Engine::reset() noexcept
{
    oversampler.reset();
    limiter.reset();
    for (int ch = 0; ch < 2; ++ch)
    {
        envelopes[(size_t) ch].reset();
        tubes[(size_t) ch].reset();
        transformers[(size_t) ch].reset();
        scFilters[(size_t) ch].reset();
        freqDepFilters[(size_t) ch].reset();
        noise[(size_t) ch].reset();
        std::fill (dryRaw[(size_t) ch].begin(), dryRaw[(size_t) ch].end(), 0.0f);
        std::fill (dryPost[(size_t) ch].begin(), dryPost[(size_t) ch].end(), 0.0f);
        gainRing[(size_t) ch].fill (1.0f);
        grRing[(size_t) ch].fill (0.0f);
        previousGain[(size_t) ch] = 1.0f;
        feedbackSignal[(size_t) ch] = 0.0f;
    }
    gainRingPos = 0;
    dryPos = 0;
}

void Engine::setOversamplingFactor (int factor) noexcept
{
    oversampler.setFactor (factor);
    prepareOversampledStages();
    reset();
}

void Engine::setLookaheadMs (float ms) noexcept
{
    limiter.setLookaheadMs (std::clamp (ms, 0.0f, kMaxLookaheadMs));
    reset();
}

int Engine::getLatencySamples() const noexcept
{
    return oversampler.getTotalLatency() + limiter.getLatency();
}

const ChannelParams& Engine::channelParams (int channel) const noexcept
{
    const bool linked = numChannels > 1 && params.stereoMode == StereoMode::Linked;
    return (channel == 0 || linked) ? params.a : params.b;
}

float Engine::staticGainReductionDb (float levelDb, int channel) const noexcept
{
    const auto& cp = channelParams (channel);
    if (! cp.enabled)
        return 0.0f;
    switch (params.mode)
    {
        case Mode::Vintage:    return variableMuGainReduction (levelDb, cp.thresholdDb, cp.ratio, params.kneeDb);
        case Mode::Limiter:    return cleanGainReduction (levelDb, cp.thresholdDb, std::max (cp.ratio, 20.0f), params.kneeDb);
        case Mode::Compressor: break;
    }
    return cleanGainReduction (levelDb, cp.thresholdDb, cp.ratio, params.kneeDb);
}

void Engine::configureEnvelopes() noexcept
{
    const bool limiterMode = params.mode == Mode::Limiter;
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto& cp = channelParams (ch);
        const EnvelopeKey key { cp.releaseMode, cp.attackMs, cp.releaseMs, limiterMode };
        if (key == envelopeKeys[(size_t) ch])
            continue;
        envelopeKeys[(size_t) ch] = key;

        const double attack = limiterMode ? std::min (cp.attackMs, 1.0f) : cp.attackMs;
        GainReductionEnvelope::Stage stages[GainReductionEnvelope::kMaxStages];
        const int count = makeEnvelopeStages (cp.releaseMode, attack, cp.releaseMs, stages);
        envelopes[(size_t) ch].configure (stages, count);
    }
}

void Engine::setParameters (const EngineParams& p) noexcept
{
    params = p;

    inputGain.setTarget (dbToGain (p.inputGainDb));
    outputGain.setTarget (dbToGain (p.outputGainDb));
    mix.setTarget (std::clamp (p.mixPercent, 0.0f, 100.0f) * 0.01f);
    width.setTarget (std::clamp (p.widthPercent, 0.0f, 200.0f) * 0.01f);
    tubeAmount.setTarget (std::clamp (p.tubePercent, 0.0f, 100.0f) * 0.01f);
    transformerAmount.setTarget (std::clamp (p.transformerPercent, 0.0f, 100.0f) * 0.01f);
    bypassMix.setTarget (p.bypass ? 1.0f : 0.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        const auto& cp = channelParams (ch);
        float makeupDb = cp.makeupDb;
        if (p.autoMakeup)
            makeupDb += 0.5f * staticGainReductionDb (0.0f, ch);
        makeup[(size_t) ch].setTarget (dbToGain (makeupDb));

        SvfTpt::Type type = SvfTpt::Type::HighPass;
        if (p.scFilter == SidechainFilter::LowPass) type = SvfTpt::Type::LowPass;
        if (p.scFilter == SidechainFilter::BandPass) type = SvfTpt::Type::BandPass;
        scFilters[(size_t) ch].setup (sampleRate, p.scFreqHz, p.scQ, type);
    }

    configureEnvelopes();
    limiter.setParameters (p.limCeilingDb, p.limReleaseMs, p.limAmountPercent * 0.01f, p.truePeak, p.limiterOn);
}

void Engine::process (const float* const* input, float* const* output, int numSamples,
                      const float* const* sidechain, int sidechainChannels) noexcept
{
    for (int offset = 0; offset < numSamples; offset += maxBlock)
        processChunk (input, output, offset, std::min (maxBlock, numSamples - offset), sidechain, sidechainChannels);
}

void Engine::processChunk (const float* const* in, float* const* out, int offset, int n,
                           const float* const* sc, int scChannels) noexcept
{
    const bool stereo = numChannels > 1;
    const bool midSide = stereo && params.stereoMode == StereoMode::MidSide;
    const bool linked = stereo && params.stereoMode == StereoMode::Linked;
    const bool vintage = params.mode == Mode::Vintage;
    const bool useExternal = params.scSource == SidechainSource::External && sc != nullptr && scChannels > 0;
    const float externalMix = useExternal ? std::clamp (params.scMixPercent * 0.01f, 0.0f, 1.0f) : 0.0f;
    const float link = linked ? std::clamp (params.linkPercent * 0.01f, 0.0f, 1.0f) : 0.0f;
    const bool filterOn = params.scFilter != SidechainFilter::Off;
    const bool freqDepOn = std::abs (params.freqDepDb) > 0.01f;
    const float freqDepGain = dbToGain (params.freqDepDb) - 1.0f;
    const float sensitivity = dbToGain (params.scSensitivityDb);
    const int chunkStart = dryPos;
    const int activeCh = stereo ? 2 : 1;

    // ---------------------------------------------------------------- pass 1: detection (base rate)
    float inPeak[2] = { 0.0f, 0.0f };
    float maxGr[2] = { 0.0f, 0.0f };
    for (int i = 0; i < n; ++i)
    {
        float xl = sanitize (in[0][offset + i]);
        float xr = stereo ? sanitize (in[1][offset + i]) : 0.0f;
        const int pos = (chunkStart + i) & dryMask;
        dryRaw[0][(size_t) pos] = xl;
        dryRaw[1][(size_t) pos] = xr;

        const float ig = inputGain.next();
        xl *= ig;
        xr *= ig;
        dryPost[0][(size_t) pos] = xl;
        dryPost[1][(size_t) pos] = xr;
        inPeak[0] = std::max (inPeak[0], std::abs (xl));
        inPeak[1] = std::max (inPeak[1], std::abs (xr));

        const float a = midSide ? 0.5f * (xl + xr) : xl;
        const float b = midSide ? 0.5f * (xl - xr) : xr;
        signal[0][(size_t) i] = a;
        signal[1][(size_t) i] = b;

        // Vintage = feedback topology: the detector listens to the output of the gain stage.
        float det[2] = { vintage ? feedbackSignal[0] : a, vintage ? feedbackSignal[1] : b };
        if (useExternal)
        {
            const float el = sanitize (sc[0][offset + i]);
            const float er = scChannels > 1 ? sanitize (sc[1][offset + i]) : el;
            const float ea = midSide ? 0.5f * (el + er) : el;
            const float eb = midSide ? 0.5f * (el - er) : er;
            det[0] += externalMix * (ea - det[0]);
            det[1] += externalMix * (eb - det[1]);
        }

        float level[2];
        for (int ch = 0; ch < activeCh; ++ch)
        {
            float d = det[ch];
            if (filterOn)
                d = scFilters[(size_t) ch].process (d);
            if (freqDepOn)
                d += freqDepGain * freqDepFilters[(size_t) ch].process (d);
            d *= sensitivity;
            listen[(size_t) ch][(size_t) i] = d;
            level[ch] = gainToDb (std::abs (d));
        }
        if (link > 0.0f)
        {
            const float m = std::max (level[0], level[1]);
            level[0] += link * (m - level[0]);
            level[1] += link * (m - level[1]);
        }

        const float sig[2] = { a, b };
        for (int ch = 0; ch < activeCh; ++ch)
        {
            const float target = staticGainReductionDb (level[ch], ch);
            const float gr = (float) envelopes[(size_t) ch].process (target);
            const float g = std::exp (-gr * kDbToNeper);
            gain[(size_t) ch][(size_t) i] = g;
            grDb[(size_t) ch][(size_t) i] = gr;
            feedbackSignal[(size_t) ch] = sig[ch] * g;
            maxGr[ch] = std::max (maxGr[ch], gr);
        }
    }

    // ---------------------------------------------------------------- pass 2: gain + colour (oversampled)
    for (int i = 0; i < n; ++i)
    {
        tubeValues[(size_t) i] = tubeAmount.next();
        transformerValues[(size_t) i] = transformerAmount.next();
    }

    const int factor = oversampler.getFactor();
    const int upLatency = oversampler.getUpLatency();
    const float invFactor = 1.0f / (float) factor;
    const int ringStart = gainRingPos;
    for (int ch = 0; ch < activeCh; ++ch)
    {
        auto& ring = gainRing[(size_t) ch];
        auto& grR = grRing[(size_t) ch];
        float* hi = oversampler.upsample (ch, signal[(size_t) ch].data(), n);
        float g0 = previousGain[(size_t) ch];

        for (int i = 0; i < n; ++i)
        {
            const int w = (ringStart + i) & (kGainRing - 1);
            ring[(size_t) w] = gain[(size_t) ch][(size_t) i];
            grR[(size_t) w] = grDb[(size_t) ch][(size_t) i];
            const int r = (ringStart + i - upLatency) & (kGainRing - 1);
            const float g1 = ring[(size_t) r];
            const float tube = tubeValues[(size_t) i];
            const float xf = transformerValues[(size_t) i];
            const float extraDrive = vintage ? tube * grR[(size_t) r] * 0.04f : 0.0f;

            for (int j = 0; j < factor; ++j)
            {
                const float g = g0 + (g1 - g0) * (float) (j + 1) * invFactor;
                float s = hi[i * factor + j] * g;
                s = tubes[(size_t) ch].process (s, tube, extraDrive, params.eco);
                s = transformers[(size_t) ch].process (s, xf, params.eco);
                hi[i * factor + j] = s;
            }
            g0 = g1;
        }
        previousGain[(size_t) ch] = g0;
        oversampler.downsample (ch, signal[(size_t) ch].data(), n);
    }
    gainRingPos = (ringStart + n) & (kGainRing - 1);

    // ---------------------------------------------------------------- pass 3: matrix, mix, output (base rate)
    const int osLatency = oversampler.getTotalLatency();
    float* outL = out[0] + offset;
    float* outR = stereo ? out[1] + offset : nullptr;
    for (int i = 0; i < n; ++i)
    {
        const float a = signal[0][(size_t) i] * makeup[0].next();
        const float b = signal[1][(size_t) i] * makeup[1].next();
        float l = midSide ? a + b : a;
        float r = midSide ? a - b : b;

        const float w = width.next();
        if (stereo)
        {
            const float m = 0.5f * (l + r);
            const float s = 0.5f * (l - r) * w;
            l = m + s;
            r = m - s;
        }

        const int dp = (chunkStart + i - osLatency) & dryMask;
        const float mx = mix.next();
        // weighted sum: exactly wet at 100 %, exactly dry at 0 %
        l = dryPost[0][(size_t) dp] * (1.0f - mx) + l * mx;
        r = dryPost[1][(size_t) dp] * (1.0f - mx) + r * mx;

        if (params.scListen)
        {
            const float la = listen[0][(size_t) i];
            const float lb = stereo ? listen[1][(size_t) i] : la;
            l = midSide ? la + lb : la;
            r = midSide ? la - lb : lb;
        }

        const float og = outputGain.next();
        l = l * og + noise[0].next (params.noiseDb, params.humHz);
        r = r * og + noise[1].next (params.noiseDb, params.humHz);

        outL[i] = l;
        if (outR != nullptr)
            outR[i] = r;
    }

    // ---------------------------------------------------------------- pass 4: limiter, soft clip, bypass
    limiter.process (outL, outR, n);

    const int totalLatency = osLatency + limiter.getLatency();
    const float clipCeiling = params.limiterOn ? dbToGain (params.limCeilingDb) : 1.0f;
    float outPeak[2] = { 0.0f, 0.0f };
    bool finite = true;
    for (int i = 0; i < n; ++i)
    {
        const int dp = (chunkStart + i - totalLatency) & dryMask;
        const float bp = bypassMix.next();
        for (int ch = 0; ch < activeCh; ++ch)
        {
            float* o = ch == 0 ? outL : outR;
            float y = o[i];
            if (params.softClip)
            {
                // cubic soft clip, unity slope at 0, output reaches the ceiling at 1.5 x ceiling
                const float u = std::clamp (y / (1.5f * clipCeiling), -1.0f, 1.0f);
                y = clipCeiling * (1.5f * u - 0.5f * u * u * u);
            }
            y += bp * (dryRaw[(size_t) ch][(size_t) dp] - y);
            finite = finite && std::isfinite (y);
            o[i] = y;
            outPeak[ch] = std::max (outPeak[ch], std::abs (y));
        }
    }

    if (! finite)
    {
        // Never pass NaN/Inf to the host: silence this chunk and restart from a clean state.
        for (int ch = 0; ch < activeCh; ++ch)
            std::fill (ch == 0 ? outL : outR, (ch == 0 ? outL : outR) + n, 0.0f);
        reset();
        return;
    }

    dryPos = (chunkStart + n) & dryMask;

    for (int ch = 0; ch < activeCh; ++ch)
    {
        Meters::raise (meters.inputPeak[(size_t) ch], inPeak[ch]);
        Meters::raise (meters.outputPeak[(size_t) ch], outPeak[ch]);
        Meters::raise (meters.gainReductionDb[(size_t) ch], maxGr[ch]);
        if (outPeak[ch] >= 1.0f)
            meters.clip.store (true, std::memory_order_relaxed);
    }
    Meters::raise (meters.limiterGainReductionDb, limiter.getLastGainReductionDb());
}
} // namespace twinmu::dsp
