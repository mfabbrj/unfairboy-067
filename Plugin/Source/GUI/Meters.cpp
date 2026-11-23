#include "Meters.h"
#include "DSP/DspMath.h"

namespace twinmu::gui
{
namespace
{
constexpr float kVuMaxDb = 20.0f;
constexpr float kVuHalfAngle = 0.733f; // 42 degrees

float needleAngle (float grDb)
{
    const float t = juce::jlimit (0.0f, 1.0f, grDb / kVuMaxDb);
    return kVuHalfAngle - 2.0f * kVuHalfAngle * std::sqrt (t); // expanded scale near 0 dB
}
} // namespace

VuMeter::VuMeter (juce::String channelName) : channel (std::move (channelName)) {}

void VuMeter::update (float newGrDb, float dt)
{
    // ~10 ms rise / ~300 ms fall: fast enough to show transients, smooth like a VU needle
    const float coeff = newGrDb > needle ? 1.0f - std::exp (-dt / 0.010f) : 1.0f - std::exp (-dt / 0.300f);
    const float previous = needle;
    needle += (newGrDb - needle) * coeff;
    readout = needle;
    if (std::abs (previous - needle) > 0.01f)
        repaint();
}

void VuMeter::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float w = b.getWidth(), h = b.getHeight();
    const juce::Point<float> pivot (w * 0.5f, h + 52.0f);
    const float radius = 170.0f;

    juce::ColourGradient face (juce::Colour (0xfffde6b0), pivot.x, h * 0.85f, juce::Colour (0xffb9873a), 0.0f, 0.0f, true);
    face.addColour (0.6, juce::Colour (0xfff1c779));
    g.setGradientFill (face);
    g.fillRoundedRectangle (b, 6.0f);

    g.setColour (fixed::vuInk);
    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, radius, radius, 0.0f, -kVuHalfAngle, kVuHalfAngle, true);
    g.strokePath (arc, juce::PathStrokeType (1.2f));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain)));
    for (int mark : { 0, 1, 2, 3, 5, 7, 10, 15, 20 })
    {
        const float a = needleAngle ((float) mark);
        const bool major = mark % 5 == 0;
        g.drawLine (juce::Line<float> (pivot.getPointOnCircumference (radius, a),
                                       pivot.getPointOnCircumference (radius - (major ? 13.0f : 8.0f), a)), major ? 1.6f : 1.0f);
        const auto p = pivot.getPointOnCircumference (radius - 24.0f, a);
        g.drawText (juce::String (mark), juce::Rectangle<float> (24.0f, 12.0f).withCentre (p), juce::Justification::centred);
    }

    g.setFont (TwinMuLookAndFeel::font (11.0f, true).withExtraKerningFactor (0.25f));
    g.drawText (caption, juce::Rectangle<float> (0.0f, h - 58.0f, w, 14.0f), juce::Justification::centred);
    g.setColour (juce::Colour (0xff5a4426));
    g.setFont (TwinMuLookAndFeel::font (9.0f).withExtraKerningFactor (0.2f));
    g.drawText ("CHANNEL " + channel + "  dB", juce::Rectangle<float> (0.0f, h - 43.0f, w, 12.0f), juce::Justification::centred);
    g.setColour (fixed::vuInk);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 15.0f, juce::Font::bold)));
    g.drawText (juce::String (-readout, 1), juce::Rectangle<float> (w - 80.0f, h - 30.0f, 66.0f, 18.0f), juce::Justification::centredRight);

    // needle
    g.setColour (juce::Colour (0xff1a1410));
    g.drawLine (juce::Line<float> (pivot, pivot.getPointOnCircumference (radius + 4.0f, needleAngle (needle))), 1.6f);

    // channel colour bands, LED, glass reflection
    g.setColour (accent);
    g.fillRect (0.0f, 0.0f, w, 5.0f);
    g.fillRect (0.0f, h - 7.0f, w, 7.0f);
    g.setColour (accent.withAlpha (0.4f));
    g.fillEllipse (9.0f, 13.0f, 14.0f, 14.0f);
    g.setColour (accent);
    g.fillEllipse (11.0f, 15.0f, 10.0f, 10.0f);
    juce::Path glass;
    glass.startNewSubPath (0.0f, 0.0f);
    glass.lineTo (w, 0.0f);
    glass.lineTo (w, 40.0f);
    glass.quadraticTo (w * 0.5f, 70.0f, 0.0f, 40.0f);
    glass.closeSubPath();
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillPath (glass);

    g.setColour (juce::Colour (0xff0b0b0b));
    g.drawRoundedRectangle (b.reduced (1.0f), 6.0f, 2.0f);
}

void BarMeter::update (float peakL, float peakR, float dt)
{
    const float in[2] = { peakL, peakR };
    for (int ch = 0; ch < 2; ++ch)
    {
        const float db = dsp::gainToDb (in[ch], -90.0f);
        level[ch] = db > level[ch] ? db : level[ch] - 24.0f * dt; // 24 dB/s fall-back
        if (db >= hold[ch]) { hold[ch] = db; holdTime[ch] = 1.5f; }
        else if ((holdTime[ch] -= dt) <= 0.0f) hold[ch] -= 12.0f * dt;
        if (in[ch] >= 1.0f) clipTime = 2.0f;
    }
    clipTime = std::max (0.0f, clipTime - dt);
    repaint();
}

void BarMeter::paint (juce::Graphics& g)
{
    const float barW = 10.0f, gap = 3.0f;
    const float h = (float) getHeight() - 12.0f;
    auto toY = [h] (float db) { return 12.0f + h * (1.0f - juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f)); };

    g.setColour (clipTime > 0.0f ? juce::Colour (0xffe0533d) : juce::Colour (0xff3a1a14));
    g.fillEllipse (barW - 3.0f, 0.0f, 7.0f, 7.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        const float x = (float) ch * (barW + gap);
        const auto track = juce::Rectangle<float> (x, 12.0f, barW, h);
        g.setColour (juce::Colour (0xff0b0c0d));
        g.fillRoundedRectangle (track, 2.0f);
        const float top = toY (level[ch]);
        juce::ColourGradient grad (juce::Colour (0xff5fae55), x, 12.0f + h, juce::Colour (0xffe0533d), x, 12.0f, false);
        grad.addColour (0.6, juce::Colour (0xff9bd16a));
        grad.addColour (0.85, juce::Colour (0xfff3c13c));
        g.setGradientFill (grad);
        g.fillRect (juce::Rectangle<float> (x + 1.0f, top, barW - 2.0f, 12.0f + h - top));
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.fillRect (x + 1.0f, toY (hold[ch]), barW - 2.0f, 1.5f);
    }
}
} // namespace twinmu::gui
