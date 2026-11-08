#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 512;
constexpr int renderSamples = 96000;
constexpr int tailStart = 2400;

juce::AudioBuffer<float> renderImpulse (juce::AudioPluginInstance& plugin)
{
    plugin.reset();
    juce::AudioBuffer<float> output (2, renderSamples);
    juce::AudioBuffer<float> block (std::max (2, plugin.getTotalNumInputChannels()), blockSize);
    juce::MidiBuffer midi;

    for (int i = 0; i < 8; ++i)
    {
        block.clear();
        plugin.processBlock (block, midi);
    }

    for (int start = 0; start < renderSamples; start += blockSize)
    {
        block.clear();
        if (start == 0)
            for (int ch = 0; ch < 2; ++ch)
                block.setSample (ch, 0, 1.0f);

        plugin.processBlock (block, midi);
        for (int ch = 0; ch < 2; ++ch)
            output.copyFrom (ch, start, block, ch, 0, std::min (blockSize, renderSamples - start));
    }

    return output;
}

double tailEnergy (const juce::AudioBuffer<float>& buffer)
{
    double energy = 0.0;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = tailStart; i < buffer.getNumSamples(); ++i)
            energy += static_cast<double> (buffer.getSample (ch, i)) * buffer.getSample (ch, i);
    return energy;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: LuckySevenProbe <plugin.vst3> <mix parameter name>\n";
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI juce;
    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);

    juce::OwnedArray<juce::PluginDescription> descriptions;
    for (auto* format : formats.getFormats())
        format->findAllTypesForFile (descriptions, argv[1]);

    if (descriptions.isEmpty())
    {
        std::cerr << "no plugin found\n";
        return 1;
    }

    juce::String error;
    auto plugin = formats.createPluginInstance (*descriptions[0], sampleRate, blockSize, error);
    if (plugin == nullptr)
    {
        std::cerr << "load failed: " << error << "\n";
        return 1;
    }

    plugin->setPlayConfigDetails (2, 2, sampleRate, blockSize);
    plugin->prepareToPlay (sampleRate, blockSize);

    {
        juce::AudioBuffer<float> probe (2, blockSize);
        juce::MidiBuffer midi;
        for (int attempt = 0; attempt < 6; ++attempt)
        {
            probe.clear();
            probe.setSample (0, 0, 1.0f);
            plugin->processBlock (probe, midi);
            std::cout << "warm-up block " << attempt << ": first sample " << probe.getSample (0, 0)
                      << ", nan " << (std::isnan (probe.getSample (0, 0)) ? "yes" : "no") << "\n";
            juce::Thread::sleep (attempt == 0 ? 0 : 400);
        }
    }

    juce::AudioProcessorParameter* mix = nullptr;
    for (auto* parameter : plugin->getParameters())
    {
        std::cout << "param: " << parameter->getName (64) << " = " << parameter->getCurrentValueAsText() << "\n";
        if (parameter->getName (64).equalsIgnoreCase (argv[2]))
            mix = parameter;
    }

    if (mix == nullptr)
    {
        std::cerr << "no parameter named " << argv[2] << "\n";
        return 1;
    }

    mix->setValueNotifyingHost (0.0f);
    const auto dryOnly = renderImpulse (*plugin);
    auto drySample = 0;
    for (int i = 1; i < 4800; ++i)
        if (std::abs (dryOnly.getSample (0, i)) > std::abs (dryOnly.getSample (0, drySample)))
            drySample = i;
    const auto dryReference = dryOnly.getSample (0, drySample);

    mix->setValueNotifyingHost (1.0f);
    const auto wetOnly = renderImpulse (*plugin);
    const auto wetReference = tailEnergy (wetOnly);

    std::cout << "latency " << plugin->getLatencySamples() << ", dry impulse at sample " << drySample << "\n";
    std::cout << "knob   text        dry gain   wet gain   dry dB   wet dB\n";

    for (int step = 0; step <= 10; ++step)
    {
        const auto value = static_cast<float> (step) / 10.0f;
        mix->setValueNotifyingHost (value);
        const auto rendered = renderImpulse (*plugin);

        const auto wetGain = std::sqrt (tailEnergy (rendered) / wetReference);
        const auto dryGain = (rendered.getSample (0, drySample) - wetGain * wetOnly.getSample (0, drySample)) / dryReference;

        std::cout << juce::String (value, 1).paddedRight (' ', 7) << mix->getCurrentValueAsText().paddedRight (' ', 12)
                  << juce::String (dryGain, 3).paddedRight (' ', 11) << juce::String (wetGain, 3).paddedRight (' ', 11)
                  << juce::String (juce::Decibels::gainToDecibels (dryGain), 1).paddedRight (' ', 9)
                  << juce::String (juce::Decibels::gainToDecibels (wetGain), 1) << "\n";
    }

    plugin->releaseResources();
    return 0;
}
