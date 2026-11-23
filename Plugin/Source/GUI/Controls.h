// Unfairboy 067 —interactive controls bound to host parameters.
#pragma once

#include "TwinMuLookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace twinmu::gui
{
/** Rotary slider: vertical drag (Shift = fine), wheel, double-click = default,
    right-click = type a value. */
class ValueSlider : public juce::Slider
{
public:
    ValueSlider();
    void mouseDown (const juce::MouseEvent&) override;

private:
    void showValueEntry();
};

/** Knob with caption and live value read-out, attached to one parameter. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& caption,
          juce::Colour accent, int knobSize, const juce::String& tooltip = {});

    void setAccent (juce::Colour c);
    void paint (juce::Graphics&) override;
    void resized() override;

    /** Places the knob like the mock-up: centre x, top y. */
    void place (int centreX, int top);

private:
    ValueSlider slider;
    juce::String caption;
    int size;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

/** Row of segmented buttons for a choice parameter. */
class ChoiceSegment : public juce::Component
{
public:
    ChoiceSegment (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
                   const juce::StringArray& labels, juce::Colour accent, juce::UndoManager* undo);
    void setAccent (juce::Colour c);
    void resized() override;
    int getSelectedIndex() const noexcept { return selected; }

private:
    void update (float value);

    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment attachment;
    int selected = 0;
};

/** Toggle button attached to a bool parameter. */
class ToggleButton : public juce::TextButton
{
public:
    ToggleButton (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text,
                  juce::Colour accent, const juce::String& tooltip = {});

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
} // namespace twinmu::gui
