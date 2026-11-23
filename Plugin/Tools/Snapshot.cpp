// Renders the real plugin editor to PNG files (one per theme) without a host.
// Usage: unfairboy_snapshot <output folder>
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    twinmu::TwinMuProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    // feed some audio so meters and curve show a working point
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < 200; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (ch, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * (float) (block * 512 + i) / 48000.0f));
        processor.processBlock (buffer, midi);
    }

    const char* names[] = { "ardesia", "crema", "menta" };
    for (int theme = 0; theme < 3; ++theme)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
        auto* e = dynamic_cast<twinmu::TwinMuEditor*> (editor.get());
        e->setTheme (theme);
        editor->setSize (1100, 680);
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        const auto file = outDir.getChildFile (juce::String ("unfairboy_") + names[theme] + ".png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        png.writeImageToStream (image, stream);
        std::cout << file.getFullPathName() << std::endl;
        if (theme == 2)
            e->setTheme (0); // leave the user's preference on the default theme
    }
    return 0;
}
