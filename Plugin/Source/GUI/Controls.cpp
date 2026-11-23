#include "Controls.h"

namespace twinmu::gui
{
// ------------------------------------------------------------------ ValueSlider
ValueSlider::ValueSlider() : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox)
{
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setVelocityBasedMode (false);
    setMouseDragSensitivity (220);
    setPopupDisplayEnabled (true, false, nullptr);
    setScrollWheelEnabled (true);
}

void ValueSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showValueEntry();
        return;
    }
    juce::Slider::mouseDown (e);
}

void ValueSlider::showValueEntry()
{
    auto* window = new juce::AlertWindow ("Enter value", getName(), juce::MessageBoxIconType::NoIcon, this);
    window->addTextEditor ("value", getTextFromValue (getValue()));
    window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<ValueSlider> safeThis (this);
    window->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, window] (int result)
    {
        if (result == 1 && safeThis != nullptr)
            safeThis->setValue (safeThis->getValueFromText (window->getTextEditorContents ("value")), juce::sendNotificationSync);
    }), true);
}

// ------------------------------------------------------------------ Knob
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& captionText,
            juce::Colour accent, int knobSize, const juce::String& tooltip)
    : caption (captionText), size (knobSize)
{
    auto* param = state.getParameter (paramId);
    jassert (param != nullptr);
    slider.setName (param->getName (64));
    slider.setTooltip (tooltip.isNotEmpty() ? tooltip : param->getName (64) + "  (double-click: default, right-click: type value)");
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
    setAccent (accent);
    setSize (size + 20, size + 28);
}

void Knob::setAccent (juce::Colour c)
{
    TwinMuLookAndFeel::setAccent (slider, c);
    TwinMuLookAndFeel::setAccent (*this, c);
    repaint();
}

void Knob::place (int centreX, int top) { setTopLeftPosition (centreX - getWidth() / 2, top); }

void Knob::resized() { slider.setBounds (10, 0, size, size); }

void Knob::paint (juce::Graphics& g)
{
    const auto* lnf = dynamic_cast<TwinMuLookAndFeel*> (&getLookAndFeel());
    const auto labelColour = lnf != nullptr ? lnf->getTheme().label : fixed::ink;
    g.setColour (labelColour);
    g.setFont (TwinMuLookAndFeel::font (9.5f, true).withExtraKerningFactor (0.16f));
    g.drawText (caption, 0, size, getWidth(), 13, juce::Justification::centred);
    g.setColour (TwinMuLookAndFeel::getAccent (*this));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 10.5f, juce::Font::bold)));
    g.drawText (slider.getTextFromValue (slider.getValue()), 0, size + 13, getWidth(), 13, juce::Justification::centred);
}

// ------------------------------------------------------------------ ChoiceSegment
ChoiceSegment::ChoiceSegment (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
                              const juce::StringArray& labels, juce::Colour accent, juce::UndoManager* undo)
    : attachment (*state.getParameter (paramId), [this] (float v) { update (v); }, undo)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setTooltip (state.getParameter (paramId)->getName (64) + ": " + labels[i]);
        b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    setAccent (accent);
    attachment.sendInitialUpdate();
}

void ChoiceSegment::setAccent (juce::Colour c)
{
    for (auto* b : buttons)
        TwinMuLookAndFeel::setAccent (*b, c);
    repaint();
}

void ChoiceSegment::update (float value)
{
    selected = juce::roundToInt (value);
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == selected, juce::dontSendNotification);
}

void ChoiceSegment::resized()
{
    // widths proportional to the label length, 4 px gaps
    float total = 0.0f;
    for (auto* b : buttons)
        total += (float) b->getButtonText().length() + 4.0f;
    const float available = (float) getWidth() - 4.0f * (float) (buttons.size() - 1);
    float x = 0.0f;
    for (auto* b : buttons)
    {
        const float w = available * ((float) b->getButtonText().length() + 4.0f) / total;
        b->setBounds (juce::roundToInt (x), 0, juce::roundToInt (w), getHeight());
        x += w + 4.0f;
    }
}

// ------------------------------------------------------------------ ToggleButton
ToggleButton::ToggleButton (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& text,
                            juce::Colour accent, const juce::String& tooltip)
    : juce::TextButton (text)
{
    setClickingTogglesState (true);
    TwinMuLookAndFeel::setAccent (*this, accent);
    setTooltip (tooltip.isNotEmpty() ? tooltip : state.getParameter (paramId)->getName (64));
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramId, *this);
}
} // namespace twinmu::gui
