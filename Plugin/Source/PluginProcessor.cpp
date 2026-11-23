#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace twinmu
{
TwinMuProcessor::TwinMuProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undoManager, "TwinMuState", createParameterLayout()),
      presets (apvts)
{
    cache.attach (apvts);
    startTimerHz (10); // applies latency-changing settings outside the audio thread
}

TwinMuProcessor::~TwinMuProcessor() { stopTimer(); }

bool TwinMuProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != mainOut)
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void TwinMuProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParameters (cache.read());
    engine.prepare (sampleRate, std::max (32, samplesPerBlock), getMainBusNumOutputChannels());
    appliedOversampling = cache.oversamplingFactor();
    appliedLookahead = cache.lookaheadMs();
    engine.setOversamplingFactor (appliedOversampling);
    engine.setLookaheadMs (appliedLookahead);
    setLatencySamples (engine.getLatencySamples());
    prepared = true;
}

void TwinMuProcessor::applyStructuralChanges()
{
    const int os = cache.oversamplingFactor();
    const float look = cache.lookaheadMs();
    if (os == appliedOversampling && std::abs (look - appliedLookahead) < 0.01f)
        return;

    // Hosts call processBlock under the callback lock, so after this no block is running.
    suspendProcessing (true);
    if (os != appliedOversampling)
        engine.setOversamplingFactor (os);
    if (std::abs (look - appliedLookahead) >= 0.01f)
        engine.setLookaheadMs (look);
    appliedOversampling = os;
    appliedLookahead = look;
    setLatencySamples (engine.getLatencySamples());
    suspendProcessing (false);
}

void TwinMuProcessor::timerCallback()
{
    if (prepared)
        applyStructuralChanges();
}

void TwinMuProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    auto main = getBusBuffer (buffer, false, 0);
    const int mainChannels = main.getNumChannels();

    // clear any output channel beyond the main bus
    for (int ch = mainChannels; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
    if (mainChannels == 0 || numSamples == 0)
        return;

    engine.setParameters (cache.read());

    const float* const* scPtrs = nullptr;
    int scChannels = 0;
    if (getBusCount (true) > 1)
    {
        if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled())
        {
            auto sc = getBusBuffer (buffer, true, 1);
            if (sc.getNumChannels() > 0)
            {
                scPtrs = sc.getArrayOfReadPointers();
                scChannels = sc.getNumChannels();
            }
        }
    }
    sidechainActive.store (scChannels > 0, std::memory_order_relaxed);

    // main input and output share the same channels (in-place processing is supported)
    engine.process (main.getArrayOfReadPointers(), main.getArrayOfWritePointers(), numSamples, scPtrs, scChannels);
}

juce::AudioProcessorParameter* TwinMuProcessor::getBypassParameter() const
{
    return apvts.getParameter (ids::bypass);
}

void TwinMuProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TwinMuProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes); xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        presets.setCurrentName (tree.getProperty ("presetName", "Default").toString());
        apvts.replaceState (tree);
    }
}

void TwinMuProcessor::switchToSlot (int slot)
{
    if (slot == activeSlot || ! juce::isPositiveAndBelow (slot, 2))
        return;
    slots[(size_t) activeSlot] = apvts.copyState();
    if (slots[(size_t) slot].isValid())
        apvts.replaceState (slots[(size_t) slot].createCopy());
    activeSlot = slot;
}

juce::AudioProcessorEditor* TwinMuProcessor::createEditor() { return new TwinMuEditor (*this); }
} // namespace twinmu

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new twinmu::TwinMuProcessor(); }
