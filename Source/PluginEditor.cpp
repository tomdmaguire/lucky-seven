#include "PluginEditor.h"

LuckySevenEditor::LuckySevenEditor (LuckySevenProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (720, 480);
}

void LuckySevenEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15161a));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (28.0f));
    g.drawFittedText ("Lucky Seven", getLocalBounds(), juce::Justification::centred, 1);
}

void LuckySevenEditor::resized() {}
