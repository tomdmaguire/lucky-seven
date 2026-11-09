#pragma once

#include "TailConvolver.h"
#include "ir/IrLibrary.h"
#include <juce_dsp/juce_dsp.h>

namespace lucky
{

/** True-stereo convolution reverb: predelay -> (L-in and R-in convolutions summed) -> low/high cut -> dry and wet gains.
    Each convolution is split into a zero-latency head on the audio thread and a tail computed on a background thread,
    so no single audio block carries the long-tail work. */
class ReverbEngine
{
public:
    static constexpr float maxPredelayMs = 250.0f;
    static constexpr float minCutHz = 20.0f;
    static constexpr float maxCutHz = 20000.0f;

    static constexpr int defaultHeadSize = 1024;

    /** headSize is the first-stage partition size of the head convolution. */
    explicit ReverbEngine (int headSize = defaultHeadSize);
    ~ReverbEngine();

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

    /** Length in samples of the IR in use on both stereo sides (head plus tail), or 0 or 1 before one has loaded. */
    int getCurrentIrSize() const
    {
        return std::min (fromLeft.getCurrentIRSize() + tails[0].getTailLength(), fromRight.getCurrentIRSize() + tails[1].getTailLength());
    }

    /** Tells the engine whether the host is rendering offline, where the tail is computed inline for exact results. */
    void setNonRealtime (bool isNonRealtime) noexcept { nonRealtime = isNonRealtime; }

    /** Tail samples that had to be silenced because the background thread fell behind (0 in normal operation). */
    juce::int64 getTailUnderflowCount() const noexcept { return tails[0].getUnderflowCount() + tails[1].getUnderflowCount(); }

    /** Processes a mono or stereo buffer in place; skips all work once the input has been silent for longer than the tail. */
    void process (juce::AudioBuffer<float>& buffer);

private:
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int length);
    void filterWet (juce::dsp::AudioBlock<float> wet);
    bool isIdle (const juce::AudioBuffer<float>& buffer);
    void applyOutputGains (juce::AudioBuffer<float>& buffer, int start, int length, int numChannels);

    using Filter = juce::dsp::StateVariableTPTFilter<float>;

    /** Background thread that keeps both tail convolvers ahead of the audio thread. */
    struct TailWorker final : juce::Thread
    {
        explicit TailWorker (std::array<TailConvolver, 2>& tailsToServe);
        void run() override;
        std::array<TailConvolver, 2>& tails;
    };

    juce::dsp::ConvolutionMessageQueue queue;
    juce::dsp::Convolution fromLeft, fromRight;
    std::array<TailConvolver, 2> tails;
    TailWorker worker { tails };
    std::atomic<bool> nonRealtime { false };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> predelay;
    juce::SmoothedValue<float> dryGain { 1.0f }, wetGain { 0.0f };
    juce::SmoothedValue<float> predelaySamples;
    Filter lowCut, highCut;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> lowCutHz { minCutHz }, highCutHz { maxCutHz };
    juce::AudioBuffer<float> leftBus, rightBus, dryBus, tailInput;
    double currentSampleRate = 48000.0;
    int capacity = 0;
    juce::int64 silentSamples = 0;
    std::atomic<float> inputPeak { 0.0f }, wetPeak { 0.0f };
};

} // namespace lucky
