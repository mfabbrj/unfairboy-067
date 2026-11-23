#include "Visualizers.h"

namespace twinmu::gui
{
namespace
{
juce::Font mono (float size) { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, juce::Font::plain)); }
} // namespace

void CurveView::update (std::function<float (float)> grFunction, float thresholdDb, float inputPeakDb, float grDb,
                        const juce::String& modeName)
{
    for (int i = 0; i < (int) curve.size(); ++i)
    {
        const float in = -60.0f + (float) i * 0.5f;
        curve[(size_t) i] = in - grFunction (in);
    }
    threshold = thresholdDb;
    workIn = inputPeakDb;
    workOut = inputPeakDb - grDb;
    history[(size_t) historyPos] = grDb;
    historyPos = (historyPos + 1) % (int) history.size();
    mode = modeName;
    repaint();
}

void CurveView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (fixed::screen);
    g.fillRoundedRectangle (b, 5.0f);

    const float pad = 16.0f;
    const auto plot = juce::Rectangle<float> (pad, pad, b.getWidth() - 2.0f * pad, b.getHeight() - 2.0f * pad - 22.0f);
    auto X = [&] (float db) { return plot.getX() + (db + 60.0f) / 60.0f * plot.getWidth(); };
    auto Y = [&] (float db) { return plot.getBottom() - (juce::jlimit (-60.0f, 0.0f, db) + 60.0f) / 60.0f * plot.getHeight(); };

    g.setColour (fixed::screenGrid);
    for (float d = -60.0f; d <= 0.0f; d += 12.0f)
    {
        g.drawVerticalLine (juce::roundToInt (X (d)), plot.getY(), plot.getBottom());
        g.drawHorizontalLine (juce::roundToInt (Y (d)), plot.getX(), plot.getRight());
    }
    g.setColour (juce::Colour (0xff2f4a3c));
    g.drawLine (X (-60.0f), Y (-60.0f), X (0.0f), Y (0.0f), 1.0f);
    g.setColour (fixed::amber.withAlpha (0.45f));
    g.drawVerticalLine (juce::roundToInt (X (threshold)), plot.getY(), plot.getBottom());

    juce::Path path;
    for (int i = 0; i < (int) curve.size(); ++i)
    {
        const float x = X (-60.0f + (float) i * 0.5f), y = Y (curve[(size_t) i]);
        if (i == 0) path.startNewSubPath (x, y);
        else path.lineTo (x, y);
    }
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2ed3c0), plot.getX(), 0.0f, juce::Colour (0xffff6b7a), plot.getRight(), 0.0f, false));
    g.strokePath (path, juce::PathStrokeType (2.5f));

    if (workIn > -60.0f)
    {
        const juce::Point<float> p (X (std::min (workIn, 0.0f)), Y (workOut));
        g.setColour (fixed::amber.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (p));
        g.setColour (fixed::amber);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (p));
    }

    // GR history strip (0 .. 20 dB), newest on the right
    const auto strip = juce::Rectangle<float> (pad, b.getHeight() - 20.0f, b.getWidth() - 2.0f * pad, 14.0f);
    const int n = (int) history.size();
    const float step = strip.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const float gr = history[(size_t) ((historyPos + i) % n)];
        const float hgt = juce::jlimit (0.0f, 1.0f, gr / 20.0f) * strip.getHeight();
        g.setColour (gr > 10.0f ? juce::Colour (0xffff6b7a) : juce::Colour (0xffa98bff));
        g.fillRect (strip.getX() + (float) i * step, strip.getY(), std::max (1.0f, step - 0.5f), hgt);
    }

    g.setColour (fixed::screenText);
    g.setFont (mono (9.0f));
    g.drawText ("TRANSFER - " + mode.toUpperCase(), juce::Rectangle<float> (pad + 2.0f, 4.0f, 200.0f, 12.0f), juce::Justification::left);
    g.drawText ("GR HISTORY", juce::Rectangle<float> (b.getWidth() - 90.0f, 4.0f, 74.0f, 12.0f), juce::Justification::right);
}

void FilterView::setFilter (int type, float freqHz, float q, bool active)
{
    if (type == filterType && freqHz == frequency && q == quality && active == isActive)
        return;
    filterType = type;
    frequency = freqHz;
    quality = q;
    isActive = active;
    repaint();
}

void FilterView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (fixed::screen);
    g.fillRoundedRectangle (b, 5.0f);

    juce::Path path;
    const int points = 120;
    for (int i = 0; i <= points; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, (double) i / points);
        const double w = f / frequency;
        const double den = std::sqrt (std::pow (1.0 - w * w, 2.0) + std::pow (w / quality, 2.0));
        double mag = 1.0;
        if (filterType == 1) mag = w * w / den;
        else if (filterType == 2) mag = 1.0 / den;
        else if (filterType == 3) mag = (w / quality) / den;
        const float db = (float) juce::jlimit (-36.0, 6.0, 20.0 * std::log10 (std::max (mag, 1.0e-6)));
        const float x = b.getWidth() * (float) i / (float) points;
        const float y = 10.0f + (-db + 6.0f) / 42.0f * (b.getHeight() - 22.0f);
        if (i == 0) path.startNewSubPath (x, y);
        else path.lineTo (x, y);
    }
    g.setColour (juce::Colour (0xffb6ea5a).withAlpha (isActive ? 1.0f : 0.3f));
    g.strokePath (path, juce::PathStrokeType (1.6f));

    static const char* names[] = { "OFF", "HP", "LP", "BP" };
    g.setColour (fixed::screenText);
    g.setFont (mono (9.0f));
    const auto freqText = frequency >= 1000.0f ? juce::String (frequency / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (frequency)) + " Hz";
    g.drawText (juce::String ("SC ") + names[juce::jlimit (0, 3, filterType)] + " " + freqText,
                juce::Rectangle<float> (6.0f, b.getHeight() - 13.0f, b.getWidth() - 12.0f, 11.0f), juce::Justification::left);
}
} // namespace twinmu::gui
