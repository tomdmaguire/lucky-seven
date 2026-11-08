#include "dsp/ReverbEngine.h"

namespace
{
constexpr int blockSize = 256;
constexpr std::array<double, 4> sampleRates { 44100.0, 48000.0, 88200.0, 96000.0 };

lucky::TrueStereoIr makeDecayingIr (double sampleRate, int length)
{
    juce::AudioBuffer<float> quad (4, length);
    juce::Random random (7);
    for (int ch = 0; ch < 4; ++ch)
        for (int i = 0; i < length; ++i)
            quad.setSample (ch, i, (random.nextFloat() - 0.5f) * std::exp (-6.9f * static_cast<float> (i) / static_cast<float> (length)));
    return lucky::splitTrueStereo (quad, sampleRate);
}

lucky::TrueStereoIr makeUnitImpulse (double sampleRate)
{
    juce::AudioBuffer<float> quad (4, 64);
    quad.clear();
    quad.setSample (0, 0, 1.0f);
    quad.setSample (3, 0, 1.0f);
    return lucky::splitTrueStereo (quad, sampleRate);
}

float blockPeak (const juce::AudioBuffer<float>& buffer)
{
    return std::max (buffer.getMagnitude (0, 0, buffer.getNumSamples()), buffer.getMagnitude (1, 0, buffer.getNumSamples()));
}

/** Processes near-silent blocks until the engine reports an IR of the expected length, or gives up. */
void waitForIr (lucky::ReverbEngine& engine, int expectedLength)
{
    juce::AudioBuffer<float> block (2, blockSize);
    for (int attempt = 0; attempt < 2000 && engine.getCurrentIrSize() != expectedLength; ++attempt)
    {
        block.clear();
        for (int ch = 0; ch < 2; ++ch)
            block.setSample (ch, 0, 1.0e-3f);
        engine.process (block);
        juce::Thread::sleep (1);
    }
}

void processSilence (lucky::ReverbEngine& engine, int blocks)
{
    juce::AudioBuffer<float> block (2, blockSize);
    for (int i = 0; i < blocks; ++i)
    {
        block.clear();
        engine.process (block);
    }
}
} // namespace

class ReverbEngineTests final : public juce::UnitTest
{
public:
    ReverbEngineTests() : juce::UnitTest ("ReverbEngine", "Lucky Seven") {}

    void runTest() override
    {
        for (const auto rate : sampleRates)
        {
            const auto label = " at " + juce::String (rate / 1000.0, 1) + " kHz";
            testTailAndIdle (rate, label);
            testPredelay (rate, label);

            beginTest ("dry passes at exactly unity with the reverb at -inf" + label);
            expectLessThan (dryError (rate, 1.0f, 0.0f, 1.0f), 1.0e-6f);

            beginTest ("send mode removes the dry signal" + label);
            expectLessThan (dryError (rate, 0.0f, 0.0f, 0.0f), 1.0e-6f);
        }
    }

private:
    void testTailAndIdle (double rate, const juce::String& label)
    {
        const auto irLength = static_cast<int> (rate / 2.0);
        lucky::ReverbEngine engine;
        engine.prepare (rate, blockSize);
        engine.setOutputGains (0.0f, 1.0f);
        engine.loadIr (makeDecayingIr (rate, irLength));
        waitForIr (engine, irLength);

        beginTest ("IR loads" + label);
        expectEquals (engine.getCurrentIrSize(), irLength);

        processSilence (engine, 4 * irLength / blockSize);

        beginTest ("the tail rings out after the input stops" + label);
        juce::AudioBuffer<float> block (2, blockSize);
        block.clear();
        block.setSample (0, 0, 1.0f);
        block.setSample (1, 0, 1.0f);
        engine.process (block);

        auto lastAudibleBlock = 0;
        for (int b = 1; b < 3 * irLength / blockSize; ++b)
        {
            block.clear();
            engine.process (block);
            if (blockPeak (block) > 1.0e-5f)
                lastAudibleBlock = b;
        }
        expectGreaterThan (lastAudibleBlock * blockSize, irLength * 3 / 4);

        beginTest ("goes idle and outputs silence after the tail" + label);
        block.clear();
        engine.process (block);
        expectEquals (blockPeak (block), 0.0f);
    }

    void testPredelay (double rate, const juce::String& label)
    {
        beginTest ("100 ms pre-delay lands on the right sample" + label);
        lucky::ReverbEngine engine;
        engine.prepare (rate, blockSize);
        engine.setOutputGains (0.0f, 1.0f);
        engine.setPredelayMs (100.0f);
        engine.reset();
        engine.loadIr (makeUnitImpulse (rate));
        waitForIr (engine, 64);
        processSilence (engine, static_cast<int> (rate / blockSize));

        const auto expectedDelay = juce::roundToInt (0.1 * rate);
        const auto blocks = expectedDelay / blockSize + 4;
        juce::AudioBuffer<float> block (2, blockSize);
        auto peakIndex = -1;
        auto peakValue = 0.0f;

        for (int b = 0; b < blocks; ++b)
        {
            block.clear();
            if (b == 0)
            {
                block.setSample (0, 0, 1.0f);
                block.setSample (1, 0, 1.0f);
            }
            engine.process (block);

            for (int i = 0; i < blockSize; ++i)
                if (std::abs (block.getSample (0, i)) > peakValue)
                {
                    peakValue = std::abs (block.getSample (0, i));
                    peakIndex = b * blockSize + i;
                }
        }

        expectWithinAbsoluteError (peakIndex, expectedDelay, 1);
    }

    /** Largest difference between output and input * expectedDryGain with the given output gains and no IR loaded. */
    static float dryError (double rate, float dry, float wet, float expectedDryGain)
    {
        lucky::ReverbEngine engine;
        engine.prepare (rate, blockSize);
        engine.setOutputGains (dry, wet);
        engine.reset();

        juce::AudioBuffer<float> input (2, blockSize);
        juce::Random random (3);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                input.setSample (ch, i, random.nextFloat() - 0.5f);

        auto output = input;
        engine.process (output);

        auto worst = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                worst = std::max (worst, std::abs (output.getSample (ch, i) - input.getSample (ch, i) * expectedDryGain));
        return worst;
    }
};

static ReverbEngineTests reverbEngineTests;
