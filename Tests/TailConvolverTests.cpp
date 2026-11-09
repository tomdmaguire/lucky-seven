#include "dsp/TailConvolver.h"

namespace
{
constexpr int inputLength = 30000;
constexpr int tailLength = 9000;

/** Reference: full convolution with an IR whose first headLength samples are silent and whose rest is the tail. */
std::array<std::vector<float>, 2> reference (const std::vector<float>& input, const juce::AudioBuffer<float>& tail)
{
    std::array<std::vector<float>, 2> out { std::vector<float> (input.size(), 0.0f), std::vector<float> (input.size(), 0.0f) };
    for (int ch = 0; ch < 2; ++ch)
        for (size_t n = 0; n < input.size(); ++n)
        {
            double sum = 0.0;
            for (int k = 0; k < tail.getNumSamples(); ++k)
            {
                const auto delay = static_cast<long> (k + lucky::TailConvolver::headLength);
                if (static_cast<long> (n) - delay < 0)
                    break;
                sum += static_cast<double> (tail.getSample (ch, k)) * input[n - static_cast<size_t> (delay)];
            }
            out[static_cast<size_t> (ch)][n] = static_cast<float> (sum);
        }
    return out;
}

struct Worker final : juce::Thread
{
    explicit Worker (lucky::TailConvolver& t) : juce::Thread ("tail test worker"), tail (t) {}
    void run() override
    {
        while (! threadShouldExit())
        {
            tail.processPending();
            wait (1);
        }
    }
    lucky::TailConvolver& tail;
};
} // namespace

class TailConvolverTests final : public juce::UnitTest
{
public:
    TailConvolverTests() : juce::UnitTest ("TailConvolver", "Lucky Seven") {}

    void runTest() override
    {
        juce::Random random (21);
        std::vector<float> input (inputLength);
        for (auto& sample : input)
            sample = random.nextFloat() - 0.5f;

        juce::AudioBuffer<float> tail (2, tailLength);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < tailLength; ++i)
                tail.setSample (ch, i, (random.nextFloat() - 0.5f) * std::exp (-3.0f * static_cast<float> (i) / tailLength) * 0.1f);

        const auto expected = reference (input, tail);

        for (const auto blockSize : { 64, 100 })
        {
            beginTest ("matches direct convolution with a background worker, block " + juce::String (blockSize));
            expectLessThan (render (input, tail, blockSize, true), 1.0e-4f);
            expectLessThan (lastError (expected), 1.0e-4f);
        }

        beginTest ("matches direct convolution when rendering offline");
        render (input, tail, 512, false);
        expectLessThan (lastError (expected), 1.0e-4f);

        beginTest ("a stalled worker yields silence and counts the gap without blocking");
        lucky::TailConvolver stalled;
        stalled.prepare (256);
        stalled.setTail (tail);
        std::vector<float> left (256), right (256);
        for (int b = 0; b < 200; ++b)
        {
            std::fill (left.begin(), left.end(), 0.0f);
            std::fill (right.begin(), right.end(), 0.0f);
            stalled.process (input.data(), left.data(), right.data(), 256, true);
        }
        expectGreaterThan (stalled.getUnderflowCount(), static_cast<juce::int64> (0));
    }

private:
    /** Renders the input in blocks and keeps the result in output; returns 0 (the error is checked separately). */
    float render (const std::vector<float>& input, const juce::AudioBuffer<float>& tail, int blockSize, bool realtime)
    {
        lucky::TailConvolver convolver;
        convolver.prepare (blockSize);
        convolver.setTail (tail);

        Worker worker (convolver);
        if (realtime)
            worker.startThread();

        for (auto& channel : output)
            channel.assign (input.size(), 0.0f);

        for (size_t start = 0; start < input.size(); start += static_cast<size_t> (blockSize))
        {
            const auto count = static_cast<int> (std::min<size_t> (static_cast<size_t> (blockSize), input.size() - start));
            convolver.process (input.data() + start, output[0].data() + start, output[1].data() + start, count, realtime);
            if (realtime)
                juce::Thread::sleep (1);
        }

        worker.stopThread (2000);
        expectEquals (convolver.getUnderflowCount(), static_cast<juce::int64> (0));
        return 0.0f;
    }

    float lastError (const std::array<std::vector<float>, 2>& expected) const
    {
        auto worst = 0.0f;
        for (size_t ch = 0; ch < 2; ++ch)
            for (size_t i = 0; i < output[ch].size(); ++i)
                worst = std::max (worst, std::abs (output[ch][i] - expected[ch][i]));
        return worst;
    }

    std::array<std::vector<float>, 2> output;
};

static TailConvolverTests tailConvolverTests;
