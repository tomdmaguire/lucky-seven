#pragma once

#include "ir/IrLibrary.h"
#include <juce_dsp/juce_dsp.h>

namespace lucky
{

/** True-stereo convolution reverb: predelay -> (L-in and R-in convolutions summed) -> low/high cut -> dry and wet gains. */
class ReverbEngine
{
public:
    static constexpr float maxPredelayMs = 250.0f;
    static constexpr float minCutHz = 20.0f;
    static constexpr float maxCutHz = 20000.0f;

    static constexpr int defaultHeadSize = 4096;

    /** headSize is the first-stage partition size of the non-uniform convolution. */
    explicit ReverbEngine (int headSize = defaultHeadSize);

    /** Allocates buffers for the given sample rate and maximum block size. */
    void prepare (double sampleRate, int maximumBlockSize);

    /** Clears delay lines and convolution state. */
    void reset();

    /** Sets the target predelay; changes are smoothed. */
    void setPredelayMs (float milliseconds);

    /** Sets the linear gains applied to the dry and reverb signals; changes are smoothed. */
    void setOutputGains (float dryGain, float wetGain);

    /** Sets the 12 dB/oct low and high cut applied to the reverb signal only. */
    void setWetFilter (float lowCutHz, float highCutHz);

    /** Hands a new impulse response to the convolvers; call from a single non-audio thread. */
    void loadIr (TrueStereoIr&& ir);

    /** Highest input and reverb peaks since the last call, then resets them; safe to call from the UI thread. */
    std::pair<float, float> takePeaks() noexcept { return { inputPeak.exchange (0.0f), wetPeak.exchange (0.0f) }; }

    /** Length in samples of the IR currently in use, or 0 before one has loaded. */
    int getCurrentIrSize() const { return fromLeft.getCurrentIRSize(); }

    /** Processes a mono or stereo buffer in place; skips all work once the input has been silent for longer than the tail. */
    void process (juce::AudioBuffer<float>& buffer);

private:
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int length);
    void filterWet (juce::dsp::AudioBlock<float> wet);
    bool isIdle (const juce::AudioBuffer<float>& buffer);
    void applyOutputGains (juce::AudioBuffer<float>& buffer, int start, int length, int numChannels);

    using Filter = juce::dsp::StateVariableTPTFilter<float>;

    juce::dsp::ConvolutionMessageQueue queue;
    juce::dsp::Convolution fromLeft, fromRight;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> predelay;
    juce::SmoothedValue<float> dryGain { 1.0f }, wetGain { 0.0f };
    juce::SmoothedValue<float> predelaySamples;
    Filter lowCut, highCut;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> lowCutHz { minCutHz }, highCutHz { maxCutHz };
    juce::AudioBuffer<float> leftBus, rightBus, dryBus;
    double currentSampleRate = 48000.0;
    int capacity = 0;
    juce::int64 silentSamples = 0;
    std::atomic<float> inputPeak { 0.0f }, wetPeak { 0.0f };
};

} // namespace lucky
