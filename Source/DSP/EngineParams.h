// TWIN-MU — plain parameter structure consumed by the DSP engine.
// The plugin layer copies its (atomic) host parameters into this struct at the start of every block.
#pragma once

namespace twinmu::dsp
{
enum class Mode { Compressor, Limiter, Vintage };
enum class StereoMode { Linked, DualMono, MidSide };
enum class ReleaseMode { Fixed, Auto, TC1, TC2, TC3, TC4, TC5, TC6 };
enum class SidechainSource { Internal, External };
enum class SidechainFilter { Off, HighPass, LowPass, BandPass };

/** Per-channel dynamics settings. Channel A = Left/Mid (or both channels when linked),
    channel B = Right/Side. */
struct ChannelParams
{
    float thresholdDb = -18.0f;  // -60 .. 0
    float ratio = 4.0f;          // 1 .. 1000 (1000 = infinity)
    float attackMs = 10.0f;      // 0.05 .. 200
    float releaseMs = 250.0f;    // 5 .. 5000
    ReleaseMode releaseMode = ReleaseMode::Fixed;
    float makeupDb = 0.0f;       // 0 .. +24
    bool enabled = true;
};

struct EngineParams
{
    Mode mode = Mode::Vintage;
    StereoMode stereoMode = StereoMode::Linked;
    float linkPercent = 100.0f;  // only used in Linked mode

    float inputGainDb = 0.0f;
    float kneeDb = 6.0f;         // 0 = hard knee
    ChannelParams a, b;
    bool autoMakeup = false;

    // Analog character
    float tubePercent = 35.0f;
    float transformerPercent = 30.0f;
    float noiseDb = -200.0f;     // <= -120 means off
    int humHz = 0;               // 0, 50 or 60
    float freqDepDb = 0.0f;      // -6 .. +6, low-shelf emphasis on the detector only

    // Sidechain
    SidechainSource scSource = SidechainSource::Internal;
    float scMixPercent = 100.0f; // External only: 0 = internal, 100 = external
    SidechainFilter scFilter = SidechainFilter::HighPass;
    float scFreqHz = 80.0f;
    float scQ = 0.707f;
    float scSensitivityDb = 0.0f;
    bool scListen = false;

    // Output limiter
    bool limiterOn = false;
    float limCeilingDb = -1.0f;
    float limReleaseMs = 50.0f;
    float limAmountPercent = 100.0f;
    bool truePeak = true;
    bool softClip = false;

    float widthPercent = 100.0f;  // 0 .. 200
    float mixPercent = 100.0f;
    float outputGainDb = 0.0f;
    bool bypass = false;
    bool eco = false;
};
} // namespace twinmu::dsp
