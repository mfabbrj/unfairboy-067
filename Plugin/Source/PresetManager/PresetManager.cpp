#include "PresetManager.h"
#include "Parameters/ParameterIDs.h"

namespace twinmu
{
namespace
{
constexpr int kSchemaVersion = 1;
const juce::Identifier presetTag ("TwinMuPreset");

bool isRandomizable (const juce::String& id)
{
    // never randomise levels that could be dangerous, or structural/utility switches
    static const juce::StringArray excluded { ids::inGain, ids::outGain, ids::mix, ids::bypass, ids::os, ids::eco,
                                              ids::limLook, ids::limCeil, ids::scListen, ids::noise, ids::hum,
                                              ids::scSource, ids::stereoMode, ids::mode };
    return ! excluded.contains (id);
}
} // namespace

juce::StringArray PresetManager::categories()
{
    return { "Vocal", "Drum Bus", "Kick", "Snare", "Bass", "Master Bus", "Mix Bus", "Piano", "Strings",
             "Guitar", "Synth", "Live Sound", "Limiting", "Vintage Compression", "User" };
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state) { refresh(); }

juce::File PresetManager::getUserDirectory() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Unfairboy 067").getChildFile ("Presets");
}

void PresetManager::refresh()
{
    const juce::String previousName = getCurrentName();
    entries.clear();
    const auto& factory = factoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
        entries.add ({ factory[(size_t) i].name, factory[(size_t) i].category, true, i, {} });

    const auto dir = getUserDirectory();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles (juce::File::findFiles, true, juce::String ("*") + fileExtension);
        files.sort();
        for (const auto& f : files)
        {
            juce::String category = f.getParentDirectory() == dir ? juce::String ("User") : f.getParentDirectory().getFileName();
            entries.add ({ f.getFileNameWithoutExtension(), category, false, -1, f });
        }
    }

    currentIndex = -1;
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].name == previousName)
            currentIndex = i;
}

juce::String PresetManager::getCurrentName() const
{
    return juce::isPositiveAndBelow (currentIndex, entries.size()) ? entries[currentIndex].name : currentName;
}

void PresetManager::setParameter (juce::RangedAudioParameter& p, float realValue)
{
    p.beginChangeGesture();
    p.setValueNotifyingHost (p.convertTo0to1 (realValue));
    p.endChangeGesture();
}

bool PresetManager::load (int index)
{
    if (! juce::isPositiveAndBelow (index, entries.size()))
        return false;

    const auto& e = entries.getReference (index);
    if (e.factory)
    {
        const auto& preset = factoryPresets()[(size_t) e.factoryIndex];
        for (auto* param : apvts.processor.getParameters())
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                if (p->getParameterID() == ids::bypass)
                    continue;
                const auto it = preset.values.find (p->getParameterID());
                const float value = it != preset.values.end() ? (float) it->second : p->convertFrom0to1 (p->getDefaultValue());
                setParameter (*p, value);
            }
    }
    else
    {
        const auto xml = juce::XmlDocument::parse (e.file);
        if (xml == nullptr || ! applyPresetXml (*xml))
            return false;
    }
    currentIndex = index;
    currentName = e.name;
    return true;
}

void PresetManager::next()
{
    if (! entries.isEmpty())
        load ((currentIndex + 1) % entries.size());
}

void PresetManager::previous()
{
    if (! entries.isEmpty())
        load ((currentIndex - 1 + entries.size()) % entries.size());
}

std::unique_ptr<juce::XmlElement> PresetManager::createPresetXml (const juce::String& name, const juce::String& category) const
{
    auto xml = std::make_unique<juce::XmlElement> (presetTag);
    xml->setAttribute ("name", name);
    xml->setAttribute ("category", category);
    xml->setAttribute ("version", kSchemaVersion);
    for (auto* param : apvts.processor.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            if (p->getParameterID() == ids::bypass)
                continue;
            auto* child = xml->createNewChildElement ("PARAM");
            child->setAttribute ("id", p->getParameterID());
            child->setAttribute ("value", (double) p->convertFrom0to1 (p->getValue()));
        }
    return xml;
}

bool PresetManager::applyPresetXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (presetTag))
        return false;
    // Parameters missing from older presets fall back to their defaults (schema migration).
    for (auto* param : apvts.processor.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            if (p->getParameterID() == ids::bypass)
                continue;
            float value = p->convertFrom0to1 (p->getDefaultValue());
            for (auto* child : xml.getChildWithTagNameIterator ("PARAM"))
                if (child->getStringAttribute ("id") == p->getParameterID())
                    value = (float) child->getDoubleAttribute ("value", value);
            setParameter (*p, value);
        }
    currentName = xml.getStringAttribute ("name", "Untitled");
    return true;
}

bool PresetManager::saveUser (const juce::String& rawName, const juce::String& category)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;
    auto dir = getUserDirectory().getChildFile (juce::File::createLegalFileName (category.isEmpty() ? "User" : category));
    if (! dir.createDirectory())
        return false;
    const auto file = dir.getChildFile (name + fileExtension);
    if (! createPresetXml (name, category)->writeTo (file))
        return false;
    currentName = name;
    refresh();
    for (int i = 0; i < entries.size(); ++i)
        if (! entries[i].factory && entries[i].file == file)
            currentIndex = i;
    return true;
}

bool PresetManager::deleteUser (int index)
{
    if (! juce::isPositiveAndBelow (index, entries.size()) || entries[index].factory)
        return false;
    const bool ok = entries[index].file.moveToTrash();
    refresh();
    return ok;
}

bool PresetManager::renameUser (int index, const juce::String& newName)
{
    if (! juce::isPositiveAndBelow (index, entries.size()) || entries[index].factory)
        return false;
    const auto& e = entries.getReference (index);
    const auto target = e.file.getSiblingFile (juce::File::createLegalFileName (newName.trim()) + fileExtension);
    if (target.exists())
        return false;
    auto xml = juce::XmlDocument::parse (e.file);
    if (xml == nullptr)
        return false;
    xml->setAttribute ("name", newName.trim());
    if (! xml->writeTo (target) || ! e.file.deleteFile())
        return false;
    currentName = newName.trim();
    refresh();
    return true;
}

bool PresetManager::importFile (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (presetTag))
        return false;
    const auto category = xml->getStringAttribute ("category", "User");
    auto dir = getUserDirectory().getChildFile (juce::File::createLegalFileName (category));
    if (! dir.createDirectory())
        return false;
    if (! file.copyFileTo (dir.getNonexistentChildFile (file.getFileNameWithoutExtension(), fileExtension, false)))
        return false;
    refresh();
    return applyPresetXml (*xml);
}

bool PresetManager::exportCurrent (const juce::File& file) const
{
    const auto category = juce::isPositiveAndBelow (currentIndex, entries.size()) ? entries[currentIndex].category : juce::String ("User");
    return createPresetXml (getCurrentName(), category)->writeTo (file.withFileExtension (fileExtension));
}

juce::String PresetManager::toXmlString() const
{
    return createPresetXml (getCurrentName(), "User")->toString();
}

bool PresetManager::fromXmlString (const juce::String& text)
{
    const auto xml = juce::XmlDocument::parse (text);
    if (xml == nullptr || ! applyPresetXml (*xml))
        return false;
    currentIndex = -1;
    return true;
}

void PresetManager::randomize (float amount)
{
    juce::Random rng;
    for (auto* param : apvts.processor.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            if (! isRandomizable (p->getParameterID()) || dynamic_cast<juce::AudioParameterBool*> (p) != nullptr
                || dynamic_cast<juce::AudioParameterChoice*> (p) != nullptr)
                continue;
            const float v = juce::jlimit (0.0f, 1.0f, p->getValue() + amount * (2.0f * rng.nextFloat() - 1.0f));
            setParameter (*p, p->convertFrom0to1 (v));
        }
    currentIndex = -1;
    currentName = getCurrentName() + " *";
}
} // namespace twinmu
