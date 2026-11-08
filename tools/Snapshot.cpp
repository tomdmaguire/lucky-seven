#include "PluginProcessor.h"
#include <CoreFoundation/CoreFoundation.h>
#include <iostream>

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
/** Lets queued JUCE messages run (catalogue updates, async repaints) for the given time. */
void pumpMessages (double seconds)
{
    CFRunLoopRunInMode (kCFRunLoopDefaultMode, seconds, false);
}

juce::Component* findById (juce::Component& parent, const juce::String& id)
{
    for (auto* child : parent.getChildren())
    {
        if (child->getComponentID() == id)
            return child;
        if (auto* found = findById (*child, id))
            return found;
    }
    return nullptr;
}

juce::RangedAudioParameter& parameter (LuckySevenProcessor& processor, const juce::String& id)
{
    return *processor.getParameters().getParameter (id);
}

juce::String describe (LuckySevenProcessor& processor)
{
    const auto& state = processor.getParameters().state;
    return "ir=" + processor.getSelectedIrId() + "  decay=" + parameter (processor, LuckySevenProcessor::decayId).getCurrentValueAsText()
         + "  mix=" + parameter (processor, LuckySevenProcessor::mixId).getCurrentValueAsText()
         + "  slot=" + juce::String (processor.getActiveSlot())
         + "  filter=" + state.getProperty (LuckySevenProcessor::mapFilterProperty).toString();
}

void setValue (LuckySevenProcessor& processor, const juce::String& id, float value)
{
    auto& target = parameter (processor, id);
    target.setValueNotifyingHost (target.convertTo0to1 (value));
}

/** Exercises A/B switching and a full save/restore into a fresh instance, printing each step. */
int checkState()
{
    std::unique_ptr<juce::AudioProcessor> owner (createPluginFilter());
    auto& processor = dynamic_cast<LuckySevenProcessor&> (*owner);

    setValue (processor, LuckySevenProcessor::decayId, 50.0f);
    processor.getParameters().state.setProperty (LuckySevenProcessor::mapFilterProperty, "1,3", nullptr);
    std::cout << "A edited:       " << describe (processor) << "\n";

    processor.switchToSlot (1);
    std::cout << "B (copy of A):  " << describe (processor) << "\n";

    setValue (processor, LuckySevenProcessor::mixId, 80.0f);
    processor.selectIr ("Plates/06 Vocal Plate");
    std::cout << "B edited:       " << describe (processor) << "\n";

    processor.switchToSlot (0);
    std::cout << "back to A:      " << describe (processor) << "\n";

    juce::MemoryBlock blob;
    processor.getStateInformation (blob);
    std::unique_ptr<juce::AudioProcessor> restoredOwner (createPluginFilter());
    auto& restored = dynamic_cast<LuckySevenProcessor&> (*restoredOwner);
    restored.setStateInformation (blob.getData(), static_cast<int> (blob.getSize()));
    std::cout << "restored copy:  " << describe (restored) << "\n";

    restored.switchToSlot (1);
    std::cout << "restored, B:    " << describe (restored) << "\n";
    return 0;
}

/** Presses Shift+Right twice and Shift+Left once on the map, printing the program after each step. */
int checkKeys()
{
    std::unique_ptr<juce::AudioProcessor> owner (createPluginFilter());
    auto& processor = dynamic_cast<LuckySevenProcessor&> (*owner);
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
    pumpMessages (1.5);

    auto* map = findById (*editor, "soundMap");
    std::cout << "start:        " << processor.getSelectedIrId() << "\n";
    for (const auto& [name, key] : { std::pair { "shift+right", juce::KeyPress::rightKey }, std::pair { "shift+right", juce::KeyPress::rightKey },
                                     std::pair { "shift+left", juce::KeyPress::leftKey } })
    {
        map->keyPressed (juce::KeyPress (key, juce::ModifierKeys::shiftModifier, 0));
        pumpMessages (0.1);
        std::cout << juce::String (name).paddedRight (' ', 14) << processor.getSelectedIrId() << "\n";
    }

    owner->editorBeingDeleted (editor.get());
    return 0;
}

/** Renders the editor to a PNG. Modes: "list", "active" (plays test hits first), "timing", or a scale such as "150%". */
int snapshot (const juce::String& path, const juce::String& mode)
{
    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());
    processor->prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditorAndMakeActive());
    pumpMessages (1.5);

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

    if (mode.endsWithChar ('%'))
        editor->setSize (juce::roundToInt (900 * mode.getFloatValue() / 100.0f), juce::roundToInt (600 * mode.getFloatValue() / 100.0f));

    if (mode == "list")
        if (auto* listTab = dynamic_cast<juce::Button*> (findById (*editor, "listTab")))
            listTab->triggerClick();

    pumpMessages (0.3);
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    const auto output = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    output.deleteFile();
    juce::FileOutputStream stream (output);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    std::cout << "wrote " << output.getFullPathName() << "\n";

    processor->editorBeingDeleted (editor.get());
    editor.reset();
    processor->releaseResources();
    return 0;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: LuckySevenSnapshot <out.png> [list|active|timing|150%]\n"
                     "       LuckySevenSnapshot --check-state\n";
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI juce;
    const juce::String first (argv[1]);
    if (first == "--check-state")
        return checkState();
    if (first == "--check-keys")
        return checkKeys();
    return snapshot (first, argc > 2 ? argv[2] : "");
}
