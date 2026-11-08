#include "dsp/ReverbEngine.h"

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;
constexpr int irLength = 24000;

lucky::TrueStereoIr makeDecayingIr()
{
    juce::AudioBuffer<float> quad (4, irLength);
    juce::Random random (7);
    for (int ch = 0; ch < 4; ++ch)
        for (int i = 0; i < irLength; ++i)
            quad.setSample (ch, i, (random.nextFloat() - 0.5f) * std::exp (-6.9f * static_cast<float> (i) / irLength));
    return lucky::splitTrueStereo (quad, sampleRate);
}

float blockPeak (const juce::AudioBuffer<float>& buffer)
{
    return std::max (buffer.getMagnitude (0, 0, buffer.getNumSamples()), buffer.getMagnitude (1, 0, buffer.getNumSamples()));
}
} // namespace

class ReverbEngineTests final : public juce::UnitTest
{
public:
    ReverbEngineTests() : juce::UnitTest ("ReverbEngine", "Lucky Seven") {}

    void runTest() override
    {
        lucky::ReverbEngine engine;
        engine.prepare (sampleRate, blockSize);
        engine.setMix (1.0f);
        engine.loadIr (makeDecayingIr());

        juce::AudioBuffer<float> block (2, blockSize);
        for (int attempt = 0; attempt < 2000 && engine.getCurrentIrSize() < irLength; ++attempt)
        {
            for (int ch = 0; ch < 2; ++ch)
                block.setSample (ch, 0, 1.0e-3f);
            engine.process (block);
            block.clear();
            juce::Thread::sleep (1);
        }

        beginTest ("IR loads");
        expectEquals (engine.getCurrentIrSize(), irLength);

        for (int i = 0; i < 400; ++i)
        {
            block.clear();
            engine.process (block);
        }

        beginTest ("the tail rings out after the input stops");
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

        beginTest ("goes idle and outputs silence after the tail");
        block.clear();
        engine.process (block);
        expectEquals (blockPeak (block), 0.0f);
    }
};

static ReverbEngineTests reverbEngineTests;
