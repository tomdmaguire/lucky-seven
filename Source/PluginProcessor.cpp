#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::String defaultIrId { "Halls/01 Large Hall" };
} // namespace

LuckySevenProcessor::LuckySevenProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "LuckySeven", createLayout())
{
    predelayMs = parameters.getRawParameterValue (predelayId);
    mixPercent = parameters.getRawParameterValue (mixId);
    lowCutHz = parameters.getRawParameterValue (lowCutId);
    highCutHz = parameters.getRawParameterValue (highCutId);
    loader = std::make_unique<lucky::IrLoader> (engine, *parameters.getRawParameterValue (decayId));
    selectIr (defaultIrId);
}

LuckySevenProcessor::~LuckySevenProcessor()
{
    loader.reset();
}

juce::AudioProcessorValueTreeState::ParameterLayout LuckySevenProcessor::createLayout()
{
    using Range = juce::NormalisableRange<float>;
    const auto msLabel = juce::AudioParameterFloatAttributes().withLabel ("ms");
    const auto percentLabel = juce::AudioParameterFloatAttributes().withLabel ("%");
    const auto hertzLabel = juce::AudioParameterFloatAttributes().withLabel ("Hz");
    const auto logRange = [] (float start, float end)
    {
        return Range (start, end,
            [] (float from, float to, float proportion) { return from * std::pow (to / from, proportion); },
            [] (float from, float to, float value) { return std::log (value / from) / std::log (to / from); },
            [] (float, float, float value) { return value; });
    };

    return {
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { predelayId, 1 }, "Pre-delay",
                                                     Range (0.0f, lucky::ReverbEngine::maxPredelayMs, 0.1f, 0.5f), 0.0f, msLabel),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { decayId, 1 }, "Decay",
                                                     Range (10.0f, 100.0f, 0.1f), 100.0f, percentLabel),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { mixId, 1 }, "Mix",
                                                     Range (0.0f, 100.0f, 0.1f), 30.0f, percentLabel),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { lowCutId, 2 }, "Low Cut",
                                                     logRange (lucky::ReverbEngine::minCutHz, lowCutMaxHz),
                                                     lucky::ReverbEngine::minCutHz, hertzLabel),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { highCutId, 2 }, "High Cut",
                                                     logRange (highCutMinHz, lucky::ReverbEngine::maxCutHz),
                                                     lucky::ReverbEngine::maxCutHz, hertzLabel),
    };
}

void LuckySevenProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto rateChanged = ! juce::approximatelyEqual (sampleRate, hostSampleRate);
    hostSampleRate = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);

    if (rateChanged)
        requestSelectedFile();
}

void LuckySevenProcessor::releaseResources() {}

bool LuckySevenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    const auto supported = out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
    return supported && (in == out || in == juce::AudioChannelSet::mono());
}

void LuckySevenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (getTotalNumInputChannels() == 1 && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, buffer.getNumSamples());

    engine.setPredelayMs (predelayMs->load());
    engine.setMix (mixPercent->load() / 100.0f);
    engine.setWetFilter (lowCutHz->load(), highCutHz->load());
    engine.process (buffer);
}

double LuckySevenProcessor::getTailLengthSeconds() const
{
    const auto decay = parameters.getRawParameterValue (decayId)->load() / 100.0f;
    const auto seconds = getOriginalT60() * decay * 1.5f + predelayMs->load() / 1000.0f;
    return std::max (1.0, static_cast<double> (seconds));
}

juce::AudioProcessorEditor* LuckySevenProcessor::createEditor()
{
    return new LuckySevenEditor (*this);
}

void LuckySevenProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (const auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LuckySevenProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    parameters.replaceState (juce::ValueTree::fromXml (*xml));
    requestSelectedFile();
}

void LuckySevenProcessor::selectIr (const juce::String& id)
{
    parameters.state.setProperty (irIdProperty, id, nullptr);
    requestSelectedFile();
}

juce::String LuckySevenProcessor::getSelectedIrId() const
{
    return parameters.state.getProperty (irIdProperty, defaultIrId).toString();
}

void LuckySevenProcessor::requestSelectedFile()
{
    if (const auto entry = catalogue->findEntry (getSelectedIrId()))
        loader->requestFile (entry->fileForRate (hostSampleRate));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LuckySevenProcessor();
}
