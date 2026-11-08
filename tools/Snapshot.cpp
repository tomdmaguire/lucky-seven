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

    if (argc > 2 && juce::String (argv[2]) == "list")
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
