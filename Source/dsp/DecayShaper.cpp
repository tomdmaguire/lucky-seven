#include "DecayShaper.h"

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace lucky
{

namespace
{
constexpr int fftOrder = 11;
constexpr int frameSize = 1 << fftOrder;
constexpr int hopSize = frameSize / 4;
constexpr float bypassRatio = 0.999f;
constexpr float ln1000 = 6.907755f;
constexpr float trimFloor = 1.5849e-5f;
constexpr double fadeOutSeconds = 0.01;
constexpr float firstPassSeconds = 0.025f;

float dampingFactor (float frequency, float dampingHz)
{
    return dampingHz < dampingOffHz && frequency > dampingHz ? dampingHz / frequency : 1.0f;
}

/** Per-bin envelope: the gain at time t after the onset is exp (-rate * t) * firstPassGain. */
struct Envelope
{
    std::vector<float> rate, firstPassGain;
};

/** Damping also darkens the start of the response by the amount it would over firstPassSeconds,
    like a damping filter inside an algorithmic reverb's loop that even the first reflections pass through once. */
Envelope makeEnvelope (const DecayProfile& profile, double sampleRate, float ratio, float dampingHz)
{
    const auto bins = static_cast<size_t> (frameSize / 2 + 1);
    Envelope envelope { std::vector<float> (bins), std::vector<float> (bins) };

    for (size_t bin = 0; bin < bins; ++bin)
    {
        const auto frequency = static_cast<float> (static_cast<double> (bin) * sampleRate / frameSize);
        const auto t60 = std::max (t60AtFrequency (profile, frequency), 0.01f);
        const auto decayRate = ln1000 * (1.0f / (ratio * t60) - 1.0f / t60);
        const auto binRatio = std::max (ratio * dampingFactor (frequency, dampingHz), 0.01f);
        envelope.rate[bin] = ln1000 * (1.0f / (binRatio * t60) - 1.0f / t60);
        envelope.firstPassGain[bin] = std::exp (-(envelope.rate[bin] - decayRate) * firstPassSeconds);
    }

    return envelope;
}
} // namespace

juce::AudioBuffer<float> shortenDecay (const juce::AudioBuffer<float>& ir, double sampleRate,
                                       const DecayProfile& profile, float ratio, float dampingHz)
{
    juce::AudioBuffer<float> result (ir);

    if ((ratio >= bypassRatio && dampingHz >= dampingOffHz) || ir.getNumSamples() == 0)
        return result;

    const auto numSamples = ir.getNumSamples();
    const auto envelope = makeEnvelope (profile, sampleRate, std::max (std::min (ratio, 1.0f), 0.01f), dampingHz);

    juce::dsp::FFT fft (fftOrder);
    juce::dsp::WindowingFunction<float>::WindowingMethod method = juce::dsp::WindowingFunction<float>::hann;
    std::vector<float> window (frameSize);
    juce::dsp::WindowingFunction<float>::fillWindowingTables (window.data(), frameSize, method, false);

    const auto paddedLength = numSamples + 2 * frameSize;
    std::vector<float> windowSum (static_cast<size_t> (paddedLength), 0.0f);
    std::vector<float> output (static_cast<size_t> (paddedLength));
    std::vector<float> frame (2 * frameSize);

    for (int start = -frameSize; start < numSamples; start += hopSize)
        for (int i = 0; i < frameSize; ++i)
            windowSum[static_cast<size_t> (start + frameSize + i)] += window[static_cast<size_t> (i)] * window[static_cast<size_t> (i)];

    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
    {
        const auto* input = ir.getReadPointer (ch);
        std::fill (output.begin(), output.end(), 0.0f);

        for (int start = -frameSize; start < numSamples; start += hopSize)
        {
            std::fill (frame.begin(), frame.end(), 0.0f);
            for (int i = 0; i < frameSize; ++i)
            {
                const auto index = start + i;
                if (index >= 0 && index < numSamples)
                    frame[static_cast<size_t> (i)] = input[index] * window[static_cast<size_t> (i)];
            }

            fft.performRealOnlyForwardTransform (frame.data(), true);

            const auto centre = start + frameSize / 2 - profile.onsetSample;
            const auto t = static_cast<float> (std::max (centre, 0)) / static_cast<float> (sampleRate);

            for (size_t bin = 0; bin < envelope.rate.size(); ++bin)
            {
                const auto gain = std::exp (-envelope.rate[bin] * t) * envelope.firstPassGain[bin];
                frame[2 * bin] *= gain;
                frame[2 * bin + 1] *= gain;
            }

            fft.performRealOnlyInverseTransform (frame.data());

            for (int i = 0; i < frameSize; ++i)
                output[static_cast<size_t> (start + frameSize + i)] += frame[static_cast<size_t> (i)] * window[static_cast<size_t> (i)];
        }

        auto* destination = result.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto index = static_cast<size_t> (i + frameSize);
            destination[i] = windowSum[index] > 1.0e-6f ? output[index] / windowSum[index] : 0.0f;
        }
    }

    trimTail (result, sampleRate);
    return result;
}

void trimTail (juce::AudioBuffer<float>& ir, double sampleRate)
{
    float peak = 0.0f;
    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
        peak = std::max (peak, ir.getMagnitude (ch, 0, ir.getNumSamples()));

    if (peak <= 0.0f)
        return;

    const auto threshold = peak * trimFloor;
    auto lastAudible = 0;

    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
    {
        const auto* data = ir.getReadPointer (ch);
        for (int i = ir.getNumSamples() - 1; i > lastAudible; --i)
        {
            if (std::abs (data[i]) >= threshold)
            {
                lastAudible = i;
                break;
            }
        }
    }

    const auto newLength = std::min (ir.getNumSamples(), lastAudible + 1);
    const auto fadeLength = std::min (newLength, static_cast<int> (fadeOutSeconds * sampleRate));

    ir.setSize (ir.getNumChannels(), newLength, true, true, true);
    ir.applyGainRamp (newLength - fadeLength, fadeLength, 1.0f, 0.0f);
}

} // namespace lucky
