#include <juce_audio_processors/juce_audio_processors.h>
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>

namespace
{
/** Lets queued JUCE messages run (catalogue updates, async repaints) for the given time. */
void pumpMessages (double seconds)
{
    CFRunLoopRunInMode (kCFRunLoopDefaultMode, seconds, false);
}
} // namespace

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: LuckySevenSnapshot <out.png> [list]\n";
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI juce;
    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());
    processor->prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());
    pumpMessages (1.5);

    const juce::String mode = argc > 2 ? argv[2] : "";

    if (mode == "active")
    {
        juce::AudioBuffer<float> block (2, 512);
        juce::MidiBuffer midi;
        juce::Random random (5);
        for (int step = 0; step < 50; ++step)
        {
            block.clear();
            if (step % 12 == 0 || step == 47)
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 512; ++i)
                        block.setSample (ch, i, (random.nextFloat() - 0.5f) * std::exp (-static_cast<float> (i) / 120.0f));
            processor->processBlock (block, midi);
            pumpMessages (0.033);
        }
    }

    if (mode == "timing")
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();
        constexpr int frames = 100;
        for (int frame = 0; frame < frames; ++frame)
            editor->createComponentSnapshot (editor->getLocalBounds().withTrimmedTop (48).withTrimmedBottom (156), true, 2.0f);
        std::cout << "map frame at 2x: " << (juce::Time::getMillisecondCounterHiRes() - start) / frames << " ms\n";
    }

    if (mode == "list")
        if (auto* listTab = dynamic_cast<juce::Button*> (editor->findChildWithID ("listTab")))
            listTab->triggerClick();

    pumpMessages (0.3);
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    const auto output = juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    output.deleteFile();
    juce::FileOutputStream stream (output);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    std::cout << "wrote " << output.getFullPathName() << "\n";

    processor->editorBeingDeleted (editor.get());
    editor.reset();
    processor->releaseResources();
    return 0;
}
