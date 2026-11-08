#include "ReverbEngine.h"

namespace lucky
{

namespace
{
constexpr int convolutionHeadSize = 1024;
constexpr double predelaySmoothingSeconds = 0.05;
} // namespace

ReverbEngine::ReverbEngine()
    : fromLeft (juce::dsp::Convolution::NonUniform { convolutionHeadSize }, queue),
      fromRight (juce::dsp::Convolution::NonUniform { convolutionHeadSize }, queue)
{
    mixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
}

void ReverbEngine::prepare (double sampleRate, int maximumBlockSize)
{
    currentSampleRate = sampleRate;
    capacity = std::max (maximumBlockSize, 1);

    const juce::dsp::ProcessSpec stereoSpec { sampleRate, static_cast<juce::uint32> (capacity), 2 };
    fromLeft.prepare (stereoSpec);
    fromRight.prepare (stereoSpec);
    mixer.prepare (stereoSpec);

    predelay.setMaximumDelayInSamples (static_cast<int> (std::ceil (maxPredelayMs * 0.001 * sampleRate)) + 1);
    predelay.prepare (stereoSpec);

    predelaySamples.reset (sampleRate, predelaySmoothingSeconds);
    leftBus.setSize (2, capacity);
    rightBus.setSize (2, capacity);
    reset();
}

void ReverbEngine::reset()
{
    fromLeft.reset();
    fromRight.reset();
    predelay.reset();
    mixer.reset();
    predelaySamples.setCurrentAndTargetValue (predelaySamples.getTargetValue());
}

void ReverbEngine::setPredelayMs (float milliseconds)
{
    const auto clamped = juce::jlimit (0.0f, maxPredelayMs, milliseconds);
    predelaySamples.setTargetValue (static_cast<float> (clamped * 0.001 * currentSampleRate));
}

void ReverbEngine::setMix (float wetProportion)
{
    mixer.setWetMixProportion (juce::jlimit (0.0f, 1.0f, wetProportion));
}

void ReverbEngine::loadIr (TrueStereoIr&& ir)
{
    const auto rate = ir.sampleRate;
    fromLeft.loadImpulseResponse (std::move (ir.fromLeft), rate, juce::dsp::Convolution::Stereo::yes,
                                  juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    fromRight.loadImpulseResponse (std::move (ir.fromRight), rate, juce::dsp::Convolution::Stereo::yes,
                                   juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
}

void ReverbEngine::process (juce::AudioBuffer<float>& buffer)
{
    for (int start = 0; start < buffer.getNumSamples(); start += capacity)
        processChunk (buffer, start, std::min (capacity, buffer.getNumSamples() - start));
}

void ReverbEngine::processChunk (juce::AudioBuffer<float>& buffer, int start, int length)
{
    const auto numChannels = std::min (buffer.getNumChannels(), 2);
    if (numChannels == 0)
        return;

    auto ioBlock = juce::dsp::AudioBlock<float> (buffer).getSubBlock (static_cast<size_t> (start), static_cast<size_t> (length))
                                                         .getSubsetChannelBlock (0, static_cast<size_t> (numChannels));
    mixer.pushDrySamples (ioBlock);

    const auto* inLeft = buffer.getReadPointer (0, start);
    const auto* inRight = buffer.getReadPointer (numChannels - 1, start);
    auto* leftL = leftBus.getWritePointer (0);
    auto* leftR = leftBus.getWritePointer (1);
    auto* rightL = rightBus.getWritePointer (0);
    auto* rightR = rightBus.getWritePointer (1);

    for (int i = 0; i < length; ++i)
    {
        predelay.setDelay (predelaySamples.getNextValue());
        predelay.pushSample (0, inLeft[i]);
        predelay.pushSample (1, inRight[i]);
        leftL[i] = leftR[i] = predelay.popSample (0);
        rightL[i] = rightR[i] = predelay.popSample (1);
    }

    auto leftBlock = juce::dsp::AudioBlock<float> (leftBus).getSubBlock (0, static_cast<size_t> (length));
    auto rightBlock = juce::dsp::AudioBlock<float> (rightBus).getSubBlock (0, static_cast<size_t> (length));
    fromLeft.process (juce::dsp::ProcessContextReplacing<float> (leftBlock));
    fromRight.process (juce::dsp::ProcessContextReplacing<float> (rightBlock));

    if (numChannels == 2)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            buffer.copyFrom (ch, start, leftBus, ch, 0, length);
            buffer.addFrom (ch, start, rightBus, ch, 0, length);
        }
    }
    else
    {
        buffer.clear (0, start, length);
        buffer.addFrom (0, start, leftBus, 0, 0, length, 0.5f);
        buffer.addFrom (0, start, leftBus, 1, 0, length, 0.5f);
        buffer.addFrom (0, start, rightBus, 0, 0, length, 0.5f);
        buffer.addFrom (0, start, rightBus, 1, 0, length, 0.5f);
    }

    mixer.mixWetSamples (ioBlock);
}

} // namespace lucky
