#pragma once

#include "ir/IrLibrary.h"
#include <juce_dsp/juce_dsp.h>

namespace lucky
{

/** True-stereo convolution reverb: predelay -> (L-in and R-in convolutions summed) -> dry/wet mix. */
class ReverbEngine
{
public:
    static constexpr float maxPredelayMs = 250.0f;

    ReverbEngine();

    /** Allocates buffers for the given sample rate and maximum block size. */
    void prepare (double sampleRate, int maximumBlockSize);

    /** Clears delay lines and convolution state. */
    void reset();

    /** Sets the target predelay; changes are smoothed. */
    void setPredelayMs (float milliseconds);

    /** Sets the wet proportion between 0 and 1. */
    void setMix (float wetProportion);

    /** Hands a new impulse response to the convolvers; call from a single non-audio thread. */
    void loadIr (TrueStereoIr&& ir);

    /** Processes a mono or stereo buffer in place. */
    void process (juce::AudioBuffer<float>& buffer);

private:
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int length);

    juce::dsp::ConvolutionMessageQueue queue;
    juce::dsp::Convolution fromLeft, fromRight;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> predelay;
    juce::dsp::DryWetMixer<float> mixer;
    juce::SmoothedValue<float> predelaySamples;
    juce::AudioBuffer<float> leftBus, rightBus;
    double currentSampleRate = 48000.0;
    int capacity = 0;
};

} // namespace lucky
