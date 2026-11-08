#pragma once

#include "PluginProcessor.h"

class LuckySevenEditor final : public juce::AudioProcessorEditor
{
public:
    explicit LuckySevenEditor (LuckySevenProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    LuckySevenProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckySevenEditor)
};
