#include "PluginProcessor.h"
#include "PluginEditor.h"

LuckySevenProcessor::LuckySevenProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void LuckySevenProcessor::prepareToPlay (double, int) {}

void LuckySevenProcessor::releaseResources() {}

bool LuckySevenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    const auto supported = out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
    return supported && (in == out || in == juce::AudioChannelSet::mono());
}

void LuckySevenProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
}

juce::AudioProcessorEditor* LuckySevenProcessor::createEditor()
{
    return new LuckySevenEditor (*this);
}

void LuckySevenProcessor::getStateInformation (juce::MemoryBlock&) {}

void LuckySevenProcessor::setStateInformation (const void*, int) {}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LuckySevenProcessor();
}
