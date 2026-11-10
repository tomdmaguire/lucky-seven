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
} // namespace

ReverbEngine::TailWorker::TailWorker (std::array<TailConvolver, 2>& tailsToServe)
    : juce::Thread ("Lucky Seven reverb tail"), tails (tailsToServe)
{
}

void ReverbEngine::TailWorker::run()
{
    while (! threadShouldExit())
    {
        for (auto& tail : tails)
            tail.processPending();
        wait (2);
    }
}

ReverbEngine::ReverbEngine (int headSize)
    : fromLeft (juce::dsp::Convolution::NonUniform { headSize }, queue),
      fromRight (juce::dsp::Convolution::NonUniform { headSize }, queue)
{
}

ReverbEngine::~ReverbEngine()
{
    worker.stopThread (2000);
}

void ReverbEngine::prepare (double sampleRate, int maximumBlockSize)
{
    worker.stopThread (2000);
    currentSampleRate = sampleRate;
    capacity = std::max (maximumBlockSize, 1);

    for (auto& tail : tails)
        tail.prepare (capacity);

    const juce::dsp::ProcessSpec stereoSpec { sampleRate, static_cast<juce::uint32> (capacity), 2 };
    fromLeft.prepare (stereoSpec);
    fromRight.prepare (stereoSpec);

    predelay.setMaximumDelayInSamples (static_cast<int> (std::ceil (maxPredelayMs * 0.001 * sampleRate)) + 1);
    predelay.prepare (stereoSpec);

    predelaySamples.reset (sampleRate, predelaySmoothingSeconds);
    dryGain.reset (sampleRate, gainSmoothingSeconds);
    wetGain.reset (sampleRate, gainSmoothingSeconds);
    width.reset (sampleRate, gainSmoothingSeconds);
    lowCutHz.reset (sampleRate, filterSmoothingSeconds);

    lowCut.setType (Filter::Type::highpass);
    lowCut.setResonance (butterworthQ);
    lowCut.prepare (stereoSpec);

    leftBus.setSize (2, capacity);
    rightBus.setSize (2, capacity);
    dryBus.setSize (2, capacity);
    tailInput.setSize (2, capacity);
    reset();
    worker.startThread (juce::Thread::Priority::high);
}

void ReverbEngine::reset()
{
    fromLeft.reset();
    fromRight.reset();
    predelay.reset();
    dryGain.setCurrentAndTargetValue (dryGain.getTargetValue());
    wetGain.setCurrentAndTargetValue (wetGain.getTargetValue());
    predelaySamples.setCurrentAndTargetValue (predelaySamples.getTargetValue());
    width.setCurrentAndTargetValue (width.getTargetValue());
    lowCutHz.setCurrentAndTargetValue (lowCutHz.getTargetValue());
    lowCut.reset();

    for (auto& tail : tails)
        tail.reset();

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

void ReverbEngine::setLowCut (float frequency)
{
    lowCutHz.setTargetValue (juce::jlimit (minCutHz, maxCutHz, frequency));
}

void ReverbEngine::setWidth (float newWidth)
{
    width.setTargetValue (juce::jlimit (0.0f, 1.0f, newWidth));
}

void ReverbEngine::narrowWet (juce::AudioBuffer<float>& buffer, int start, int length)
{
    if (! width.isSmoothing() && width.getTargetValue() >= 1.0f)
        return;

    auto* left = buffer.getWritePointer (0, start);
    auto* right = buffer.getWritePointer (1, start);
    for (int i = 0; i < length; ++i)
    {
        const auto mid = 0.5f * (left[i] + right[i]);
        const auto side = 0.5f * (left[i] - right[i]) * width.getNextValue();
        left[i] = mid + side;
        right[i] = mid - side;
    }
}

void ReverbEngine::filterWet (juce::dsp::AudioBlock<float> wet)
{
    const auto length = static_cast<int> (wet.getNumSamples());
    if (! lowCutHz.isSmoothing() && lowCutHz.getTargetValue() <= openLowCutHz)
        return;

    for (int start = 0; start < length; start += filterUpdateInterval)
    {
        const auto count = std::min (filterUpdateInterval, length - start);
        lowCut.setCutoffFrequency (lowCutHz.skip (count));

        auto section = wet.getSubBlock (static_cast<size_t> (start), static_cast<size_t> (count));
        juce::dsp::ProcessContextReplacing<float> context (section);
        lowCut.process (context);
    }
}

void ReverbEngine::loadIr (TrueStereoIr&& ir)
{
    const auto hostRate = currentSampleRate;
    const auto resample = [&] (const juce::AudioBuffer<float>& source)
    {
        if (juce::approximatelyEqual (ir.sampleRate, hostRate) || source.getNumSamples() == 0)
            return source;

        const auto ratio = ir.sampleRate / hostRate;
        const auto length = static_cast<int> (std::ceil (source.getNumSamples() / ratio));
        juce::AudioBuffer<float> resampled (source.getNumChannels(), length);
        for (int ch = 0; ch < source.getNumChannels(); ++ch)
        {
            juce::WindowedSincInterpolator interpolator;
            interpolator.process (ratio, source.getReadPointer (ch), resampled.getWritePointer (ch), length,
                                  source.getNumSamples(), 0);
        }
        resampled.applyGain (static_cast<float> (ratio));
        return resampled;
    };

    const auto install = [&] (juce::dsp::Convolution& head, TailConvolver& tail, const juce::AudioBuffer<float>& source)
    {
        const auto full = resample (source);
        const auto headLength = std::min (full.getNumSamples(), TailConvolver::headLength);
        juce::AudioBuffer<float> headPart (2, std::max (headLength, 1));
        headPart.clear();
        for (int ch = 0; ch < std::min (2, full.getNumChannels()); ++ch)
            headPart.copyFrom (ch, 0, full, ch, 0, headLength);

        const auto tailLength = std::max (0, full.getNumSamples() - TailConvolver::headLength);
        juce::AudioBuffer<float> tailPart (2, tailLength);
        for (int ch = 0; ch < std::min (2, full.getNumChannels()); ++ch)
            if (tailLength > 0)
                tailPart.copyFrom (ch, 0, full, ch, TailConvolver::headLength, tailLength);

        tail.setTail (tailPart);
        head.loadImpulseResponse (std::move (headPart), hostRate, juce::dsp::Convolution::Stereo::yes,
                                  juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    };

    install (fromLeft, tails[0], ir.fromLeft);
    install (fromRight, tails[1], ir.fromRight);
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

    tailInput.copyFrom (0, 0, leftBus, 0, 0, length);
    tailInput.copyFrom (1, 0, rightBus, 0, 0, length);

    auto leftBlock = juce::dsp::AudioBlock<float> (leftBus).getSubBlock (0, static_cast<size_t> (length));
    auto rightBlock = juce::dsp::AudioBlock<float> (rightBus).getSubBlock (0, static_cast<size_t> (length));
    fromLeft.process (juce::dsp::ProcessContextReplacing<float> (leftBlock));
    fromRight.process (juce::dsp::ProcessContextReplacing<float> (rightBlock));

    const auto realtime = ! nonRealtime.load();
    tails[0].process (tailInput.getReadPointer (0), leftBus.getWritePointer (0), leftBus.getWritePointer (1), length, realtime);
    tails[1].process (tailInput.getReadPointer (1), rightBus.getWritePointer (0), rightBus.getWritePointer (1), length, realtime);

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
    if (numChannels == 2)
        narrowWet (buffer, start, length);

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
