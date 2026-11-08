#include "ReverbEngine.h"

namespace lucky
{

namespace
{
constexpr double predelaySmoothingSeconds = 0.05;
constexpr double filterSmoothingSeconds = 0.03;
constexpr double gainSmoothingSeconds = 0.03;
constexpr int filterUpdateInterval = 16;
constexpr float butterworthQ = 0.70710678f;
constexpr float silenceThreshold = 1.0e-6f;

void holdPeak (std::atomic<float>& peak, float value) noexcept
{
    if (value > peak.load (std::memory_order_relaxed))
        peak.store (value, std::memory_order_relaxed);
}
constexpr float openLowCutHz = ReverbEngine::minCutHz + 0.5f;
constexpr float openHighCutHz = ReverbEngine::maxCutHz - 1.0f;
} // namespace

ReverbEngine::ReverbEngine (int headSize)
    : fromLeft (juce::dsp::Convolution::NonUniform { headSize }, queue),
      fromRight (juce::dsp::Convolution::NonUniform { headSize }, queue)
{
}

void ReverbEngine::prepare (double sampleRate, int maximumBlockSize)
{
    currentSampleRate = sampleRate;
    capacity = std::max (maximumBlockSize, 1);

    const juce::dsp::ProcessSpec stereoSpec { sampleRate, static_cast<juce::uint32> (capacity), 2 };
    fromLeft.prepare (stereoSpec);
    fromRight.prepare (stereoSpec);

    predelay.setMaximumDelayInSamples (static_cast<int> (std::ceil (maxPredelayMs * 0.001 * sampleRate)) + 1);
    predelay.prepare (stereoSpec);

    predelaySamples.reset (sampleRate, predelaySmoothingSeconds);
    dryGain.reset (sampleRate, gainSmoothingSeconds);
    wetGain.reset (sampleRate, gainSmoothingSeconds);
    lowCutHz.reset (sampleRate, filterSmoothingSeconds);
    highCutHz.reset (sampleRate, filterSmoothingSeconds);

    lowCut.setType (Filter::Type::highpass);
    highCut.setType (Filter::Type::lowpass);
    for (auto* filter : { &lowCut, &highCut })
    {
        filter->setResonance (butterworthQ);
        filter->prepare (stereoSpec);
    }

    leftBus.setSize (2, capacity);
    rightBus.setSize (2, capacity);
    dryBus.setSize (2, capacity);
    reset();
}

void ReverbEngine::reset()
{
    fromLeft.reset();
    fromRight.reset();
    predelay.reset();
    dryGain.setCurrentAndTargetValue (dryGain.getTargetValue());
    wetGain.setCurrentAndTargetValue (wetGain.getTargetValue());
    predelaySamples.setCurrentAndTargetValue (predelaySamples.getTargetValue());
    lowCutHz.setCurrentAndTargetValue (lowCutHz.getTargetValue());
    highCutHz.setCurrentAndTargetValue (highCutHz.getTargetValue());

    lowCut.reset();
    highCut.reset();

    silentSamples = 0;
}

void ReverbEngine::setPredelayMs (float milliseconds)
{
    const auto clamped = juce::jlimit (0.0f, maxPredelayMs, milliseconds);
    predelaySamples.setTargetValue (static_cast<float> (clamped * 0.001 * currentSampleRate));
}

void ReverbEngine::setOutputGains (float dry, float wet)
{
    dryGain.setTargetValue (std::max (dry, 0.0f));
    wetGain.setTargetValue (std::max (wet, 0.0f));
}

void ReverbEngine::setWetFilter (float lowCutFrequency, float highCutFrequency)
{
    const auto nyquistLimit = static_cast<float> (currentSampleRate * 0.45);
    lowCutHz.setTargetValue (juce::jlimit (minCutHz, maxCutHz, lowCutFrequency));
    highCutHz.setTargetValue (juce::jlimit (minCutHz, std::min (maxCutHz, nyquistLimit), highCutFrequency));
}

void ReverbEngine::filterWet (juce::dsp::AudioBlock<float> wet)
{
    const auto length = static_cast<int> (wet.getNumSamples());
    const auto fullyOpen = ! lowCutHz.isSmoothing() && ! highCutHz.isSmoothing()
                        && lowCutHz.getTargetValue() <= openLowCutHz
                        && highCutHz.getTargetValue() >= std::min (openHighCutHz, static_cast<float> (currentSampleRate * 0.45) - 1.0f);
    if (fullyOpen)
        return;

    for (int start = 0; start < length; start += filterUpdateInterval)
    {
        const auto count = std::min (filterUpdateInterval, length - start);
        const auto low = lowCutHz.skip (count);
        const auto high = highCutHz.skip (count);

        lowCut.setCutoffFrequency (low);
        highCut.setCutoffFrequency (high);

        auto section = wet.getSubBlock (static_cast<size_t> (start), static_cast<size_t> (count));
        juce::dsp::ProcessContextReplacing<float> context (section);
        lowCut.process (context);
        highCut.process (context);
    }
}

void ReverbEngine::loadIr (TrueStereoIr&& ir)
{
    const auto rate = ir.sampleRate;
    fromLeft.loadImpulseResponse (std::move (ir.fromLeft), rate, juce::dsp::Convolution::Stereo::yes,
                                  juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    fromRight.loadImpulseResponse (std::move (ir.fromRight), rate, juce::dsp::Convolution::Stereo::yes,
                                   juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
}

bool ReverbEngine::isIdle (const juce::AudioBuffer<float>& buffer)
{
    auto peak = 0.0f;
    for (int ch = 0; ch < std::min (buffer.getNumChannels(), 2); ++ch)
        peak = std::max (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));

    if (peak >= silenceThreshold)
    {
        silentSamples = 0;
        return false;
    }

    silentSamples += buffer.getNumSamples();
    const auto tailSamples = static_cast<juce::int64> (getCurrentIrSize())
                           + static_cast<juce::int64> (maxPredelayMs * 0.001 * currentSampleRate) + capacity;
    return silentSamples > tailSamples && ! predelaySamples.isSmoothing();
}

void ReverbEngine::process (juce::AudioBuffer<float>& buffer)
{
    if (isIdle (buffer))
        return;

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
    auto blockInputPeak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        dryBus.copyFrom (ch, 0, buffer, ch, start, length);
        blockInputPeak = std::max (blockInputPeak, buffer.getMagnitude (ch, start, length));
    }
    holdPeak (inputPeak, blockInputPeak);

    const auto* inLeft = buffer.getReadPointer (0, start);
    const auto* inRight = buffer.getReadPointer (numChannels - 1, start);
    auto* leftL = leftBus.getWritePointer (0);
    auto* leftR = leftBus.getWritePointer (1);
    auto* rightL = rightBus.getWritePointer (0);
    auto* rightR = rightBus.getWritePointer (1);

    const auto smoothing = predelaySamples.isSmoothing();
    if (! smoothing)
        predelay.setDelay (predelaySamples.getTargetValue());

    for (int i = 0; i < length; ++i)
    {
        if (smoothing)
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

    filterWet (ioBlock);

    auto blockWetPeak = 0.0f;
    for (int ch = 0; ch < numChannels; ++ch)
        blockWetPeak = std::max (blockWetPeak, buffer.getMagnitude (ch, start, length));
    holdPeak (wetPeak, blockWetPeak * wetGain.getTargetValue());

    applyOutputGains (buffer, start, length, numChannels);
}

void ReverbEngine::applyOutputGains (juce::AudioBuffer<float>& buffer, int start, int length, int numChannels)
{
    if (! dryGain.isSmoothing() && ! wetGain.isSmoothing())
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.applyGain (ch, start, length, wetGain.getTargetValue());
            buffer.addFrom (ch, start, dryBus, ch, 0, length, dryGain.getTargetValue());
        }
        return;
    }

    std::array<float*, 2> out { buffer.getWritePointer (0, start), buffer.getWritePointer (numChannels - 1, start) };
    std::array<const float*, 2> dry { dryBus.getReadPointer (0), dryBus.getReadPointer (numChannels - 1) };

    for (int i = 0; i < length; ++i)
    {
        const auto dryNow = dryGain.getNextValue();
        const auto wetNow = wetGain.getNextValue();
        for (size_t ch = 0; ch < static_cast<size_t> (numChannels); ++ch)
            out[ch][i] = out[ch][i] * wetNow + dry[ch][i] * dryNow;
    }
}

} // namespace lucky
