// Unfairboy 067 — editor. The whole panel is laid out at 1100 x 680 and scaled (100-200 %)
// with an affine transform, so every element is vector-drawn and sharp on HiDPI screens.
#pragma once

#include "GUI/Controls.h"
#include "GUI/Meters.h"
#include "GUI/Visualizers.h"
#include "PluginProcessor.h"

namespace twinmu
{
class MainPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int designWidth = 1100;
    static constexpr int designHeight = 680;

    MainPanel (TwinMuProcessor& processor, gui::TwinMuLookAndFeel& lookAndFeel, std::function<void (int)> onThemeChanged);
    ~MainPanel() override;

    void paint (juce::Graphics&) override;
    void applyTheme();

private:
    void timerCallback() override;
    void layout();
    void showPresetMenu();
    void showSaveDialog();
    void showRenameDialog();
    void confirmDelete();
    void importPreset();
    void exportPreset();
    void selectTab (int tab);
    void paintSection (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title, juce::Colour accent, bool flatTopLeft = false);

    TwinMuProcessor& proc;
    juce::AudioProcessorValueTreeState& state;
    gui::TwinMuLookAndFeel& lnf;
    std::function<void (int)> themeCallback;

    // top bar
    juce::TextButton prevButton { "<" }, presetButton, nextButton { ">" }, saveButton { "SAVE" };
    juce::TextButton slotA { "A" }, slotB { "B" }, undoButton { "UNDO" }, redoButton { "REDO" }, themeButton { "THEME" };
    juce::ComboBox oversampling;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAttachment;
    gui::ToggleButton ecoButton, bypassButton;

    // meters
    gui::BarMeter inputMeter, outputMeter;
    gui::VuMeter vuA { "A" }, vuB { "B" };
    gui::CurveView curve;

    // modes
    gui::ChoiceSegment modeSelector, stereoSelector;

    // channel strips
    gui::Knob thrA, ratioA, tcA, atkA, relA, mkA, knee;
    gui::Knob thrB, ratioB, tcB, atkB, relB, mkB, link;
    gui::ToggleButton enA, enB;

    // character
    gui::Knob tube, xfmr, mix, outGain;

    // tabs
    juce::TextButton tabButtons[3];
    int currentTab = 0;
    // sidechain tab
    gui::Knob scFreq, scQ, scSens, scMix, fdc;
    gui::ChoiceSegment scSource, scFilter;
    gui::ToggleButton scListen;
    gui::FilterView filterView;
    // limiter tab
    gui::ToggleButton limOn, truePeak, softClip;
    gui::Knob limCeil, limRel, limLook, limAmt;
    // input / stereo tab
    gui::Knob inGain, width, noise;
    gui::ToggleButton autoMakeup;
    gui::ChoiceSegment hum;

    juce::Array<juce::Component*> tabContent[3];
    juce::String titleA, titleB;
    int lastStereoMode = -1;
    float limiterGr = 0.0f;
    std::unique_ptr<juce::FileChooser> fileChooser;
    double lastTick = 0.0;
};

class TwinMuEditor : public juce::AudioProcessorEditor
{
public:
    explicit TwinMuEditor (TwinMuProcessor&);
    ~TwinMuEditor() override;
    void resized() override;

    /** -1 cycles to the next theme. */
    void setTheme (int index);

private:

    gui::TwinMuLookAndFeel lookAndFeel;
    juce::ApplicationProperties settings;
    MainPanel panel;
    juce::TooltipWindow tooltips { this, 700 };
};
} // namespace twinmu
