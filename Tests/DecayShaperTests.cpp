#include "dsp/DecayShaper.h"
#include <juce_core/juce_core.h>

namespace
{
constexpr double sampleRate = 48000.0;

juce::AudioBuffer<float> makeExponentialNoise (float t60Seconds, float lengthSeconds, int channels)
{
    juce::Random random (42);
    const auto numSamples = static_cast<int> (lengthSeconds * sampleRate);
    const auto onset = static_cast<int> (0.01 * sampleRate);
    juce::AudioBuffer<float> buffer (channels, numSamples);
    buffer.clear();

    for (int ch = 0; ch < channels; ++ch)
        for (int i = onset; i < numSamples; ++i)
        {
            const auto t = static_cast<float> (i - onset) / static_cast<float> (sampleRate);
            buffer.setSample (ch, i, (random.nextFloat() * 2.0f - 1.0f) * std::exp (-6.907755f * t / t60Seconds));
        }

    return buffer;
}
} // namespace

class DecayShaperTests final : public juce::UnitTest
{
public:
    DecayShaperTests() : juce::UnitTest ("DecayShaper", "Lucky Seven") {}

    void runTest() override
    {
        const auto ir = makeExponentialNoise (2.0f, 4.0f, 2);
        const auto profile = lucky::analyse (ir, sampleRate);

        beginTest ("analysis measures the synthetic T60");
        expectWithinAbsoluteError (profile.midT60, 2.0f, 0.2f);
        for (size_t band = 1; band < lucky::analysisBands.size(); ++band)
            expectWithinAbsoluteError (profile.bandT60[band], 2.0f, 0.2f, "band " + juce::String (lucky::analysisBands[band]));

        beginTest ("ratio 1 leaves the IR untouched");
        const auto untouched = lucky::shortenDecay (ir, sampleRate, profile, 1.0f);
        expectEquals (untouched.getNumSamples(), ir.getNumSamples());
        auto identical = true;
        for (int ch = 0; ch < ir.getNumChannels(); ++ch)
            for (int i = 0; i < ir.getNumSamples(); ++i)
                identical = identical && juce::exactlyEqual (untouched.getSample (ch, i), ir.getSample (ch, i));
        expect (identical);

        beginTest ("ratio 0.5 halves the decay in every band");
        const auto shortened = lucky::shortenDecay (ir, sampleRate, profile, 0.5f);
        const auto shortenedProfile = lucky::analyse (shortened, sampleRate);
        for (size_t band = 1; band < lucky::analysisBands.size(); ++band)
            expectWithinAbsoluteError (shortenedProfile.bandT60[band], 1.0f, 0.1f, "band " + juce::String (lucky::analysisBands[band]));

        beginTest ("shortened IR is trimmed");
        expectLessThan (shortened.getNumSamples(), ir.getNumSamples());

        beginTest ("near-unity ratio reconstructs the early response");
        const auto nearUnity = lucky::shortenDecay (ir, sampleRate, profile, 0.99f);
        const auto window = static_cast<int> (0.1 * sampleRate);
        double errorEnergy = 0.0, signalEnergy = 0.0;
        for (int i = 0; i < window; ++i)
        {
            const auto difference = nearUnity.getSample (0, i) - ir.getSample (0, i);
            errorEnergy += difference * difference;
            signalEnergy += ir.getSample (0, i) * ir.getSample (0, i);
        }
        expectLessThan (errorEnergy / signalEnergy, 1.0e-3);
    }
};

static DecayShaperTests decayShaperTests;

class NoiseFloorTests final : public juce::UnitTest
{
public:
    NoiseFloorTests() : juce::UnitTest ("NoiseFloor", "Lucky Seven") {}

    void runTest() override
    {
        beginTest ("decay is measured above a hiss floor instead of being stretched by it");
        auto noisy = makeExponentialNoise (1.0f, 5.0f, 2);
        juce::Random random (9);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < noisy.getNumSamples(); ++i)
                noisy.setSample (ch, i, noisy.getSample (ch, i) + (random.nextFloat() - 0.5f) * 0.002f);

        const auto profile = lucky::analyse (noisy, sampleRate);
        expectWithinAbsoluteError (profile.midT60, 1.0f, 0.15f);

        beginTest ("the hiss tail is trimmed off");
        auto trimmed = noisy;
        lucky::trimNoiseFloor (trimmed, sampleRate);
        expectLessThan (trimmed.getNumSamples(), static_cast<int> (2.5 * sampleRate));

        beginTest ("a long tail that is still decaying when the file ends is left alone");
        auto truncated = makeExponentialNoise (6.0f, 3.0f, 2);
        const auto before = truncated.getNumSamples();
        lucky::trimNoiseFloor (truncated, sampleRate);
        expectEquals (truncated.getNumSamples(), before);

        beginTest ("a clean IR is left alone");
        auto clean = makeExponentialNoise (1.0f, 4.0f, 2);
        const auto cleanLength = clean.getNumSamples();
        lucky::trimNoiseFloor (clean, sampleRate);
        expectEquals (clean.getNumSamples(), cleanLength);
    }
};

static NoiseFloorTests noiseFloorTests;
