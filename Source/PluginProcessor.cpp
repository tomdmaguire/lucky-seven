#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::String defaultIrId { "Chambers/17 Sunset Chamber" };

/** Re-expresses damping in A/B slots saved before its range was extended down from 1 kHz, keeping the same frequency. */
void migrateSlotDamping (juce::ValueTree& state)
{
    const juce::Identifier damping (LuckySevenProcessor::dampingId);
    const auto maxHz = lucky::ReverbEngine::maxCutHz;

    for (auto slot : state)
        if (slot.hasType (LuckySevenProcessor::slotType) && slot.hasProperty (damping))
        {
            const auto proportion = static_cast<float> (slot.getProperty (damping));
            const auto hertz = LuckySevenProcessor::legacyDampingMinHz * std::pow (maxHz / LuckySevenProcessor::legacyDampingMinHz, proportion);
            slot.setProperty (damping, std::log (hertz / LuckySevenProcessor::dampingMinHz) / std::log (maxHz / LuckySevenProcessor::dampingMinHz), nullptr);
        }
}
} // namespace

LuckySevenProcessor::LuckySevenProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "LuckySeven", createLayout())
{
    predelayMs = parameters.getRawParameterValue (predelayId);
    mixPercent = parameters.getRawParameterValue (mixId);
    blendDb = parameters.getRawParameterValue (blendId);
    outputMode = parameters.getRawParameterValue (outputModeId);
    lowCutHz = parameters.getRawParameterValue (lowCutId);
    widthPercent = parameters.getRawParameterValue (widthId);
    parameters.state.setProperty (stateVersionProperty, stateVersion, nullptr);
    loader = std::make_unique<lucky::IrLoader> (engine, *parameters.getRawParameterValue (decayId),
                                                *parameters.getRawParameterValue (dampingId));
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
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { blendId, 1 }, "Blend",
                                                     Range (blendSilentDb, 6.0f, 0.1f, 2.5f), -15.0f,
                                                     juce::AudioParameterFloatAttributes().withLabel ("dB")),
        std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { outputModeId, 1 }, "Output Mode", outputModes, 0),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { lowCutId, 2 }, "Low Cut",
                                                     logRange (lucky::ReverbEngine::minCutHz, lowCutMaxHz),
                                                     lucky::ReverbEngine::minCutHz, hertzLabel),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { dampingId, 2 }, "Damping",
                                                     logRange (dampingMinHz, lucky::ReverbEngine::maxCutHz),
                                                     lucky::ReverbEngine::maxCutHz, hertzLabel),
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { widthId, 3 }, "Width",
                                                     Range (0.0f, 100.0f, 0.1f), 100.0f, percentLabel),
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

    engine.setNonRealtime (isNonRealtime());
    engine.setPredelayMs (predelayMs->load());
    const auto mode = juce::roundToInt (outputMode->load());
    if (mode == 1)
    {
        const auto db = blendDb->load();
        engine.setOutputGains (1.0f, db <= blendSilentDb ? 0.0f : juce::Decibels::decibelsToGain (db));
    }
    else if (mode == 2)
    {
        engine.setOutputGains (0.0f, 1.0f);
    }
    else
    {
        const auto wet = mixPercent->load() / 100.0f;
        engine.setOutputGains (std::sqrt (std::max (0.0f, 1.0f - wet * wet)), wet);
    }
    engine.setLowCut (lowCutHz->load());
    engine.setWidth (widthPercent->load() / 100.0f);
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

    auto state = juce::ValueTree::fromXml (*xml);
    if (static_cast<int> (state.getProperty (stateVersionProperty, 1)) < 2)
        migrateSlotDamping (state);
    state.setProperty (stateVersionProperty, stateVersion, nullptr);

    parameters.replaceState (state);
    requestSelectedFile();
}

void LuckySevenProcessor::selectIr (const juce::String& id)
{
    parameters.state.setProperty (irIdProperty, id, nullptr);
    requestSelectedFile();
}

int LuckySevenProcessor::getActiveSlot() const
{
    return static_cast<int> (parameters.state.getProperty (activeSlotProperty, 0));
}

juce::ValueTree LuckySevenProcessor::findSlot (int slot) const
{
    return parameters.state.getChildWithProperty (slotIndexProperty, slot);
}

juce::ValueTree LuckySevenProcessor::captureSlot (int slot) const
{
    juce::ValueTree snapshot (slotType);
    snapshot.setProperty (slotIndexProperty, slot, nullptr);
    snapshot.setProperty (irIdProperty, getSelectedIrId(), nullptr);

    for (auto* parameter : AudioProcessor::getParameters())
        if (const auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            snapshot.setProperty (juce::Identifier (withId->paramID), parameter->getValue(), nullptr);

    return snapshot;
}

void LuckySevenProcessor::applySlot (const juce::ValueTree& snapshot)
{
    for (auto* parameter : AudioProcessor::getParameters())
        if (const auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            if (snapshot.hasProperty (juce::Identifier (withId->paramID)))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (static_cast<float> (snapshot.getProperty (juce::Identifier (withId->paramID))));
                parameter->endChangeGesture();
            }

    if (snapshot.hasProperty (irIdProperty))
        selectIr (snapshot.getProperty (irIdProperty).toString());
}

void LuckySevenProcessor::switchToSlot (int slot)
{
    const auto active = getActiveSlot();
    if (slot == active)
        return;

    const auto replaceSlot = [this] (const juce::ValueTree& snapshot)
    {
        const auto index = static_cast<int> (snapshot.getProperty (slotIndexProperty));
        if (auto existing = findSlot (index); existing.isValid())
            parameters.state.removeChild (existing, nullptr);
        parameters.state.appendChild (snapshot, nullptr);
    };

    const auto target = findSlot (slot).createCopy();
    replaceSlot (captureSlot (active));

    if (target.isValid())
        applySlot (target);
    else
        replaceSlot (captureSlot (slot));

    parameters.state.setProperty (activeSlotProperty, slot, nullptr);
}

juce::String LuckySevenProcessor::getSelectedIrId() const
{
    return parameters.state.getProperty (irIdProperty, defaultIrId).toString();
}

void LuckySevenProcessor::requestSelectedFile()
{
    if (const auto entry = catalogue->findEntry (getSelectedIrId()))
        loader->requestEntry (*entry, hostSampleRate);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LuckySevenProcessor();
}
