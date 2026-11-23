// TWIN-MU — audio processor: host bridge around the framework-independent DSP engine.
#pragma once

#include "DSP/Engine.h"
#include "Parameters/Parameters.h"
#include "PresetManager/PresetManager.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace twinmu
{
class TwinMuProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    TwinMuProcessor();
    ~TwinMuProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    // ---- used by the editor (message thread)
    dsp::Meters& getMeters() noexcept { return engine.getMeters(); }
    bool isExternalSidechainActive() const noexcept { return sidechainActive.load (std::memory_order_relaxed); }
    void switchToSlot (int slot);
    int getActiveSlot() const noexcept { return activeSlot; }

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets;

private:
    void timerCallback() override;
    void applyStructuralChanges();

    dsp::Engine engine;
    ParameterCache cache;
    std::atomic<bool> sidechainActive { false };
    bool prepared = false;
    int appliedOversampling = -1;
    float appliedLookahead = -1.0f;
    std::array<juce::ValueTree, 2> slots;
    int activeSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwinMuProcessor)
};
} // namespace twinmu
