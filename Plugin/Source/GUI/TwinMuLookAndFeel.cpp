#include "TwinMuLookAndFeel.h"

namespace twinmu::gui
{
TwinMuLookAndFeel::TwinMuLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1d1f21));
    setColour (juce::PopupMenu::textColourId, fixed::ink);
    setColour (juce::PopupMenu::headerTextColourId, fixed::amber);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a3d41));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xfff4efe0));
    setColour (juce::TooltipWindow::textColourId, juce::Colour (0xff1a1a1a));
    setColour (juce::TooltipWindow::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::AlertWindow::backgroundColourId, juce::Colour (0xff232528));
    setColour (juce::AlertWindow::textColourId, fixed::ink);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0e0f10));
    setColour (juce::TextEditor::textColourId, fixed::ink);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff0e0f10));
    setColour (juce::ComboBox::textColourId, fixed::ink);
    setColour (juce::TextButton::textColourOffId, fixed::ink);
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff10151a));
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xfff4efe0));
    setColour (juce::BubbleComponent::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xff1a1a1a));
}

void TwinMuLookAndFeel::setAccent (juce::Component& c, juce::Colour colour)
{
    c.getProperties().set ("accent", (juce::int64) colour.getARGB());
}

juce::Colour TwinMuLookAndFeel::getAccent (const juce::Component& c)
{
    const auto v = c.getProperties()["accent"];
    return v.isVoid() ? fixed::amber : juce::Colour ((juce::uint32) (juce::int64) v);
}

juce::Font TwinMuLookAndFeel::font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

void TwinMuLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float size = std::min (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float r = size * 0.5f;
    const auto accent = getAccent (slider).withMultipliedAlpha (slider.isEnabled() ? 1.0f : 0.4f);
    const float angle = startAngle + pos * (endAngle - startAngle);

    // scale ticks
    g.setColour (theme->tick);
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const float inner = r - (i % 5 == 0 ? 7.0f : 4.0f);
        g.drawLine (juce::Line<float> (centre.getPointOnCircumference (r - 1.0f, a), centre.getPointOnCircumference (inner, a)), 1.0f);
    }

    // value track
    const float arcR = r - 10.0f;
    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    const juce::PathStrokeType stroke (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.strokePath (track, stroke);
    g.setColour (accent.withAlpha (0.35f));
    g.strokePath (value, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (value, stroke);

    // knob body
    const float bodyR = r - 14.0f;
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR + 2.0f, bodyR * 2.0f, bodyR * 2.0f);
    juce::ColourGradient body (juce::Colour (0xff6b6e72), centre.x - bodyR * 0.3f, centre.y - bodyR * 0.45f,
                               juce::Colour (0xff111214), centre.x + bodyR * 0.6f, centre.y + bodyR, true);
    body.addColour (0.55, juce::Colour (0xff2a2c2f));
    g.setGradientFill (body);
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (accent.withAlpha (0.6f));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.5f);

    // chrome cap
    const float capR = bodyR * 0.55f;
    juce::ColourGradient cap (juce::Colour (0xfff2f2f0), centre.x - capR * 0.3f, centre.y - capR * 0.4f,
                              juce::Colour (0xff4d4f52), centre.x + capR * 0.6f, centre.y + capR, true);
    cap.addColour (0.6, juce::Colour (0xff9c9ea0));
    g.setGradientFill (cap);
    g.fillEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);

    // pointer
    g.setColour (accent);
    g.drawLine (juce::Line<float> (centre.getPointOnCircumference (r * 0.2f, angle), centre.getPointOnCircumference (r * 0.52f, angle)), 2.6f);
}

void TwinMuLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                              bool isMouseOver, bool isDown)
{
    const auto b = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();
    const auto accent = getAccent (button);

    if (on)
    {
        g.setColour (accent.withAlpha (0.35f));
        g.fillRoundedRectangle (b.expanded (1.5f), 5.0f);
        g.setGradientFill (juce::ColourGradient (accent.brighter (0.25f), b.getX(), b.getY(), accent.darker (0.15f), b.getX(), b.getBottom(), false));
    }
    else
    {
        const auto top = juce::Colour (0xff3a3d41).brighter (isMouseOver ? 0.15f : 0.0f);
        g.setGradientFill (juce::ColourGradient (top, b.getX(), b.getY(), juce::Colour (0xff2b2d30), b.getX(), b.getBottom(), false));
    }
    if (isDown)
        g.setOpacity (0.85f);
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
    if (! on)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine ((int) b.getY() + 1, b.getX() + 3.0f, b.getRight() - 3.0f);
    }
}

void TwinMuLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const bool on = button.getToggleState();
    const auto accent = getAccent (button);
    // dark text on bright accents, white text on dark accents
    const auto onText = accent.getPerceivedBrightness() > 0.55f ? juce::Colour (0xff10151a) : juce::Colours::white;
    g.setColour ((on ? onText : fixed::ink).withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.4f));
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

juce::Font TwinMuLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (std::min (11.0f, (float) buttonHeight * 0.46f), true).withExtraKerningFactor (0.08f);
}

void TwinMuLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&)
{
    const auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (juce::Colour (0xff0e0f10));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) width - 12.0f, ay = (float) height * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (fixed::ink2);
    g.fillPath (arrow);
}

juce::Font TwinMuLookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (11.0f, true); }
juce::Font TwinMuLookAndFeel::getPopupMenuFont() { return font (14.0f); }

juce::Label* TwinMuLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (font (11.0f, true));
    return label;
}
} // namespace twinmu::gui
