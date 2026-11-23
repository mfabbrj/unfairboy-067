#include "Parameters.h"

namespace twinmu
{
namespace
{
using juce::String;

String formatDb (float v, int) { return (v > 0.0f ? "+" : "") + String (v, 1) + " dB"; }
String formatPercent (float v, int) { return String (juce::roundToInt (v)) + " %"; }
String formatMs (float v, int)
{
    if (v >= 1000.0f) return String (v / 1000.0f, 2) + " s";
    if (v < 1.0f) return String (v, 2) + " ms";
    if (v < 10.0f) return String (v, 1) + " ms";
    return String (juce::roundToInt (v)) + " ms";
}
String formatHz (float v, int)
{
    return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : String (juce::roundToInt (v)) + " Hz";
}
String formatRatio (float v, int)
{
    if (v >= 999.0f) return "INF : 1";
    return String (v, v < 10.0f ? 1 : 0) + " : 1";
}
float parseNumber (const String& text) { return text.retainCharacters ("+-.0123456789").getFloatValue(); }
float parseMs (const String& text)
{
    const float v = parseNumber (text);
    return text.containsIgnoreCase ("ms") || ! text.containsIgnoreCase ("s") ? v : v * 1000.0f;
}
float parseHz (const String& text) { return parseNumber (text) * (text.containsIgnoreCase ("k") ? 1000.0f : 1.0f); }
float parseRatio (const String& text)
{
    if (text.containsIgnoreCase ("inf")) return 1000.0f;
    return parseNumber (text.upToFirstOccurrenceOf (":", false, false));
}

using Layout = juce::AudioProcessorValueTreeState::ParameterLayout;

void addFloat (Layout& layout, const char* id, const String& name, float min, float max, float def, float skewCentre,
               String (*format) (float, int), float (*parse) (const String&) = nullptr)
{
    juce::NormalisableRange<float> range (min, max);
    if (skewCentre > 0.0f)
        range.setSkewForCentre (skewCentre);
    auto attributes = juce::AudioParameterFloatAttributes().withStringFromValueFunction (format);
    attributes = attributes.withValueFromStringFunction (parse != nullptr ? parse : &parseNumber);
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def, attributes));
}

void addChoice (Layout& layout, const char* id, const String& name, const juce::StringArray& choices, int def)
{
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, def));
}

void addBool (Layout& layout, const char* id, const String& name, bool def)
{
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
}

void addChannel (Layout& layout, const String& suffix, const char* thr, const char* ratio, const char* atk,
                 const char* rel, const char* relMode, const char* mk, const char* en)
{
    addFloat (layout, thr, "Threshold " + suffix, -60.0f, 0.0f, -18.0f, 0.0f, formatDb);
    addFloat (layout, ratio, "Ratio " + suffix, 1.0f, 1000.0f, 4.0f, 6.0f, formatRatio, parseRatio);
    addFloat (layout, atk, "Attack " + suffix, 0.05f, 200.0f, 10.0f, 5.0f, formatMs, parseMs);
    addFloat (layout, rel, "Release " + suffix, 5.0f, 5000.0f, 250.0f, 250.0f, formatMs, parseMs);
    addChoice (layout, relMode, "Time Constant " + suffix, { "VAR", "AUTO", "TC1", "TC2", "TC3", "TC4", "TC5", "TC6" }, 3);
    addFloat (layout, mk, "Makeup " + suffix, 0.0f, 24.0f, 0.0f, 0.0f, formatDb);
    addBool (layout, en, "Channel " + suffix + " On", true);
}

float load (const std::atomic<float>* p) noexcept { return p->load (std::memory_order_relaxed); }
bool loadBool (const std::atomic<float>* p) noexcept { return load (p) > 0.5f; }
int loadInt (const std::atomic<float>* p) noexcept { return juce::roundToInt (load (p)); }
} // namespace

Layout createParameterLayout()
{
    Layout layout;
    addChoice (layout, ids::mode, "Mode", { "Compressor", "Limiter", "Vintage" }, 2);
    addChoice (layout, ids::stereoMode, "Stereo Mode", { "Linked", "Dual Mono", "Mid/Side" }, 0);
    addFloat (layout, ids::link, "Stereo Link", 0.0f, 100.0f, 100.0f, 0.0f, formatPercent);
    addFloat (layout, ids::inGain, "Input Gain", -24.0f, 24.0f, 0.0f, 0.0f, formatDb);
    addFloat (layout, ids::knee, "Knee", 0.0f, 24.0f, 6.0f, 0.0f, formatDb);
    addBool (layout, ids::autoMakeup, "Auto Makeup", false);

    addChannel (layout, "A", ids::thrA, ids::ratioA, ids::atkA, ids::relA, ids::relModeA, ids::mkA, ids::enA);
    addChannel (layout, "B", ids::thrB, ids::ratioB, ids::atkB, ids::relB, ids::relModeB, ids::mkB, ids::enB);

    addFloat (layout, ids::tube, "Tube Character", 0.0f, 100.0f, 35.0f, 0.0f, formatPercent);
    addFloat (layout, ids::xfmr, "Transformer", 0.0f, 100.0f, 30.0f, 0.0f, formatPercent);
    addFloat (layout, ids::noise, "Analog Noise", -120.0f, -60.0f, -120.0f, 0.0f,
              [] (float v, int) { return v <= -119.5f ? String ("OFF") : String (juce::roundToInt (v)) + " dB"; });
    addChoice (layout, ids::hum, "Hum", { "Off", "50 Hz", "60 Hz" }, 0);
    addFloat (layout, ids::fdc, "Freq-Dependent Comp", -6.0f, 6.0f, 0.0f, 0.0f, formatDb);

    addChoice (layout, ids::scSource, "Sidechain Source", { "Internal", "External" }, 0);
    addFloat (layout, ids::scMix, "Sidechain Mix", 0.0f, 100.0f, 100.0f, 0.0f, formatPercent);
    addChoice (layout, ids::scFilter, "Sidechain Filter", { "Off", "HP", "LP", "BP" }, 1);
    addFloat (layout, ids::scFreq, "Sidechain Freq", 20.0f, 20000.0f, 80.0f, 1000.0f, formatHz, parseHz);
    addFloat (layout, ids::scQ, "Sidechain Q", 0.3f, 6.0f, 0.707f, 1.0f, [] (float v, int) { return String (v, 2); });
    addFloat (layout, ids::scSens, "Sidechain Sensitivity", -24.0f, 24.0f, 0.0f, 0.0f, formatDb);
    addBool (layout, ids::scListen, "Sidechain Listen", false);

    addBool (layout, ids::limOn, "Limiter", false);
    addFloat (layout, ids::limCeil, "Limiter Ceiling", -12.0f, 0.0f, -1.0f, 0.0f, formatDb);
    addFloat (layout, ids::limRel, "Limiter Release", 1.0f, 1000.0f, 50.0f, 60.0f, formatMs, parseMs);
    addFloat (layout, ids::limLook, "Limiter Lookahead", 0.0f, 5.0f, 1.5f, 0.0f, formatMs, parseMs);
    addFloat (layout, ids::limAmt, "Limit Amount", 0.0f, 100.0f, 100.0f, 0.0f, formatPercent);
    addBool (layout, ids::truePeak, "True Peak", true);
    addBool (layout, ids::softClip, "Soft Clip", false);

    addFloat (layout, ids::width, "Stereo Width", 0.0f, 200.0f, 100.0f, 0.0f, formatPercent);
    addFloat (layout, ids::mix, "Mix", 0.0f, 100.0f, 100.0f, 0.0f, formatPercent);
    addFloat (layout, ids::outGain, "Output Gain", -24.0f, 24.0f, 0.0f, 0.0f, formatDb);
    addChoice (layout, ids::os, "Oversampling", { "1x", "2x", "4x", "8x" }, 1);
    addBool (layout, ids::eco, "ECO", false);
    addBool (layout, ids::bypass, "Bypass", false);
    return layout;
}

void ParameterCache::attach (juce::AudioProcessorValueTreeState& s)
{
    auto get = [&s] (const char* id)
    {
        auto* p = s.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };
    mode = get (ids::mode);
    stereoMode = get (ids::stereoMode);
    link = get (ids::link);
    inGain = get (ids::inGain);
    knee = get (ids::knee);
    autoMakeup = get (ids::autoMakeup);
    a = { get (ids::thrA), get (ids::ratioA), get (ids::atkA), get (ids::relA), get (ids::relModeA), get (ids::mkA), get (ids::enA) };
    b = { get (ids::thrB), get (ids::ratioB), get (ids::atkB), get (ids::relB), get (ids::relModeB), get (ids::mkB), get (ids::enB) };
    tube = get (ids::tube);
    xfmr = get (ids::xfmr);
    noise = get (ids::noise);
    hum = get (ids::hum);
    fdc = get (ids::fdc);
    scSource = get (ids::scSource);
    scMix = get (ids::scMix);
    scFilter = get (ids::scFilter);
    scFreq = get (ids::scFreq);
    scQ = get (ids::scQ);
    scSens = get (ids::scSens);
    scListen = get (ids::scListen);
    limOn = get (ids::limOn);
    limCeil = get (ids::limCeil);
    limRel = get (ids::limRel);
    limLook = get (ids::limLook);
    limAmt = get (ids::limAmt);
    truePeak = get (ids::truePeak);
    softClip = get (ids::softClip);
    width = get (ids::width);
    mix = get (ids::mix);
    outGain = get (ids::outGain);
    os = get (ids::os);
    eco = get (ids::eco);
    bypass = get (ids::bypass);
}

dsp::EngineParams ParameterCache::read() const noexcept
{
    auto channel = [] (const Channel& c)
    {
        dsp::ChannelParams cp;
        cp.thresholdDb = load (c.thr);
        cp.ratio = load (c.ratio);
        cp.attackMs = load (c.atk);
        cp.releaseMs = load (c.rel);
        cp.releaseMode = (dsp::ReleaseMode) juce::jlimit (0, 7, loadInt (c.relMode));
        cp.makeupDb = load (c.mk);
        cp.enabled = loadBool (c.en);
        return cp;
    };

    dsp::EngineParams p;
    p.mode = (dsp::Mode) juce::jlimit (0, 2, loadInt (mode));
    p.stereoMode = (dsp::StereoMode) juce::jlimit (0, 2, loadInt (stereoMode));
    p.linkPercent = load (link);
    p.inputGainDb = load (inGain);
    p.kneeDb = load (knee);
    p.autoMakeup = loadBool (autoMakeup);
    p.a = channel (a);
    p.b = channel (b);
    p.tubePercent = load (tube);
    p.transformerPercent = load (xfmr);
    p.noiseDb = load (noise) <= -119.5f ? -200.0f : load (noise);
    p.humHz = loadInt (hum) == 1 ? 50 : (loadInt (hum) == 2 ? 60 : 0);
    p.freqDepDb = load (fdc);
    p.scSource = (dsp::SidechainSource) juce::jlimit (0, 1, loadInt (scSource));
    p.scMixPercent = load (scMix);
    p.scFilter = (dsp::SidechainFilter) juce::jlimit (0, 3, loadInt (scFilter));
    p.scFreqHz = load (scFreq);
    p.scQ = load (scQ);
    p.scSensitivityDb = load (scSens);
    p.scListen = loadBool (scListen);
    p.limiterOn = loadBool (limOn);
    p.limCeilingDb = load (limCeil);
    p.limReleaseMs = load (limRel);
    p.limAmountPercent = load (limAmt);
    p.truePeak = loadBool (truePeak);
    p.softClip = loadBool (softClip);
    p.widthPercent = load (width);
    p.mixPercent = load (mix);
    p.outputGainDb = load (outGain);
    p.bypass = loadBool (bypass);
    p.eco = loadBool (eco);
    return p;
}

int ParameterCache::oversamplingFactor() const noexcept
{
    return loadBool (eco) ? 1 : 1 << juce::jlimit (0, 3, loadInt (os));
}

float ParameterCache::lookaheadMs() const noexcept { return load (limLook); }
} // namespace twinmu
