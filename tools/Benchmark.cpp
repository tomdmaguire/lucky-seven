#include "dsp/DecayShaper.h"
#include "dsp/ReverbEngine.h"
#include "ir/IrLibrary.h"
#include <iostream>

namespace
{
constexpr double secondsToProcess = 20.0;
double sampleRate = 48000.0;
double irSampleRate = 48000.0;

struct Result { double averagePercent, worstBlockPercent, p999Percent; };

Result measure (const juce::AudioBuffer<float>& quad, int headSize, int blockSize)
{
    lucky::ReverbEngine engine (headSize);
    engine.prepare (sampleRate, blockSize);
    engine.setOutputGains (0.0f, 1.0f);
    engine.setWetFilter (80.0f, 12000.0f);

    juce::AudioBuffer<float> copy (quad);
    engine.loadIr (lucky::splitTrueStereo (copy, irSampleRate));
    const auto expectedSize = static_cast<int> (quad.getNumSamples() * sampleRate / irSampleRate * 0.9);

    juce::AudioBuffer<float> block (2, blockSize);
    juce::Random random (1);
    const auto fillNoise = [&]
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                block.setSample (ch, i, random.nextFloat() * 0.2f - 0.1f);
    };

    while (engine.getCurrentIrSize() < expectedSize)
    {
        fillNoise();
        engine.process (block);
        juce::Thread::sleep (1);
    }

    const auto numBlocks = static_cast<int> (secondsToProcess * sampleRate / blockSize);
    double busySeconds = 0.0, worstBlock = 0.0;
    std::vector<double> times;
    times.reserve (static_cast<size_t> (numBlocks));
    for (int b = 0; b < numBlocks; ++b)
    {
        fillNoise();
        const auto start = juce::Time::getHighResolutionTicks();
        engine.process (block);
        const auto elapsed = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
        busySeconds += elapsed;
        worstBlock = std::max (worstBlock, elapsed);
        times.push_back (elapsed);
    }

    std::sort (times.begin(), times.end());
    const auto p999 = times[static_cast<size_t> (static_cast<double> (times.size()) * 0.999)];

    return { 100.0 * busySeconds / secondsToProcess, 100.0 * worstBlock / (blockSize / sampleRate), 100.0 * p999 / (blockSize / sampleRate) };
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: LuckySevenBench <ir.wav> [decay ratio] [block size] [host sample rate]\n";
        return 1;
    }

    double rate = 0.0;
    auto ir = lucky::readIrFile (juce::File (argv[1]), rate);
    irSampleRate = rate;
    sampleRate = argc > 4 ? std::atof (argv[4]) : rate;
    std::cout << "host rate " << sampleRate << " Hz, IR rate " << rate << " Hz\n";
    lucky::normaliseEnergy (ir);

    if (argc > 2)
    {
        const auto ratio = static_cast<float> (std::atof (argv[2]));
        const auto profile = lucky::analyse (ir, rate);
        const auto start = juce::Time::getMillisecondCounterHiRes();
        ir = lucky::shortenDecay (ir, rate, profile, ratio);
        std::cout << "shortenDecay(" << ratio << ") took " << juce::Time::getMillisecondCounterHiRes() - start << " ms\n";
    }

    std::cout << juce::File (argv[1]).getFileNameWithoutExtension() << ": " << ir.getNumSamples() / rate << " s, "
              << ir.getNumChannels() << " ch\n";

    const auto blockSizes = argc > 3 ? std::vector<int> { std::atoi (argv[3]) } : std::vector<int> { 64, 128, 512 };
    for (const auto blockSize : blockSizes)
        for (const auto headSize : { 1024, 2048, 4096 })
        {
            const auto result = measure (ir, headSize, blockSize);
            std::cout << "block " << blockSize << "  head " << headSize << "  avg " << juce::String (result.averagePercent, 2)
                      << " %  p99.9 block " << juce::String (result.p999Percent, 1) << " %  worst block " << juce::String (result.worstBlockPercent, 1) << " % of budget\n";
        }

    return 0;
}
