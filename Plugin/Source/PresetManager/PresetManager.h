// TWIN-MU — factory and user presets. Message thread only (does file I/O).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>

namespace twinmu
{
struct FactoryPreset
{
    juce::String name, category;
    std::map<juce::String, double> values; // parameter id -> real (denormalised) value; others = default
};

const std::vector<FactoryPreset>& factoryPresets();

class PresetManager
{
public:
    struct Entry
    {
        juce::String name, category;
        bool factory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    static constexpr const char* fileExtension = ".u067preset";
    static juce::StringArray categories();

    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    juce::File getUserDirectory() const;
    void refresh();
    const juce::Array<Entry>& getEntries() const noexcept { return entries; }

    bool load (int index);
    void next();
    void previous();
    int getCurrentIndex() const noexcept { return currentIndex; }
    juce::String getCurrentName() const;
    void setCurrentName (const juce::String& name) { currentName = name; currentIndex = -1; }

    bool saveUser (const juce::String& name, const juce::String& category);
    bool deleteUser (int index);
    bool renameUser (int index, const juce::String& newName);
    bool importFile (const juce::File& file);
    bool exportCurrent (const juce::File& file) const;

    /** Current settings as XML text (copy/paste, A/B). */
    juce::String toXmlString() const;
    bool fromXmlString (const juce::String& text);

    /** Moves every unlockable continuous parameter by up to +-amount (normalised). */
    void randomize (float amount);

private:
    std::unique_ptr<juce::XmlElement> createPresetXml (const juce::String& name, const juce::String& category) const;
    bool applyPresetXml (const juce::XmlElement& xml);
    void setParameter (juce::RangedAudioParameter& p, float realValue);

    juce::AudioProcessorValueTreeState& apvts;
    juce::Array<Entry> entries;
    int currentIndex = -1;
    juce::String currentName { "Default" };
};
} // namespace twinmu
