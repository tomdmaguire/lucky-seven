#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>

namespace lucky
{

/** Convolves a mono input with the late part of a stereo impulse response (everything from headLength samples on).
    The heavy work runs on a background thread through processPending(); the audio thread only moves samples through
    lock-free FIFOs. The output is delayed by exactly headLength samples, matching where the tail sits in the IR, so a
    separate zero-latency head convolution plus this tail reproduces the full convolution sample for sample. */
class TailConvolver
{
public:
    static constexpr int headLength = 16384;
    static constexpr int partitionSize = 4096;

    TailConvolver();
    ~TailConvolver();

    /** Allocates the FIFOs; call before processing, with no worker running. */
    void prepare (int maximumBlockSize);

    /** Clears all history and re-primes the output delay; call with no worker running or from the worker's owner. */
    void reset();

    /** Hands over a new stereo tail (the IR from headLength onwards); call from any single non-audio thread. */
    void setTail (const juce::AudioBuffer<float>& stereoTail);

    /** Audio thread: feeds n input samples and adds n tail output samples into outL/outR. When realtime is false
        (offline rendering) any pending blocks are computed inline so the result never depends on thread timing. */
    void process (const float* input, float* outL, float* outR, int n, bool realtime);

    /** Worker thread: computes every complete input block waiting in the FIFO. Safe to call from one thread at a time. */
    void processPending();

    /** Number of output samples the audio thread had to fill with silence because the worker fell behind. */
    juce::int64 getUnderflowCount() const noexcept { return underflowSamples.load(); }

    /** Length in samples of the tail currently in use. */
    int getTailLength() const noexcept { return activeTailLength.load(); }

private:
    struct Spectra;

    void computeBlock();
    void ensureHistory (size_t partitions);
    static std::unique_ptr<Spectra> makeSpectra (const juce::AudioBuffer<float>& tail);

    juce::dsp::FFT fft;
    juce::SpinLock processLock;

    juce::AbstractFifo inputFifo { 1 }, outputFifo { 1 };
    std::vector<float> inputRing;
    std::array<std::vector<float>, 2> outputRing;

    std::vector<float> frame, previousBlock, inputBlock;
    std::vector<std::vector<float>> history;
    size_t historyFront = 0;
    std::array<std::vector<float>, 2> accumulator, previousAccumulator;

    std::unique_ptr<Spectra> active, fading;
    std::atomic<Spectra*> pending { nullptr };
    std::atomic<int> activeTailLength { 0 };
    std::atomic<juce::int64> underflowSamples { 0 };
    juce::int64 skipDebt = 0;
};

} // namespace lucky
