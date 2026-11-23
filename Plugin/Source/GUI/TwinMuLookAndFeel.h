// TWIN-MU — vector look and feel: rotary knobs, hardware-style buttons, menus.
#pragma once

#include "Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace twinmu::gui
{
class TwinMuLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TwinMuLookAndFeel();

    void setTheme (const Theme& t) noexcept { theme = &t; }
    const Theme& getTheme() const noexcept { return *theme; }

    /** Accent colour stored on a component (falls back to amber). */
    static void setAccent (juce::Component& c, juce::Colour colour);
    static juce::Colour getAccent (const juce::Component& c);

    static juce::Font font (float height, bool bold = false);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOverButton, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

private:
    const Theme* theme = &themes()[0];
};
} // namespace twinmu::gui
