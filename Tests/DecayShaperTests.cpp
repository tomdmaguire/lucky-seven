#include "dsp/DecayShaper.h"
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

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

        beginTest ("damping off leaves the IR untouched");
        const auto undamped = lucky::shortenDecay (ir, sampleRate, profile, 1.0f, lucky::dampingOffHz);
        auto unchanged = undamped.getNumSamples() == ir.getNumSamples();
        for (int ch = 0; unchanged && ch < ir.getNumChannels(); ++ch)
            for (int i = 0; i < ir.getNumSamples(); ++i)
                unchanged = unchanged && juce::exactlyEqual (undamped.getSample (ch, i), ir.getSample (ch, i));
        expect (unchanged);

        beginTest ("damping at 2 kHz keeps the low bands and halves the decay per octave above");
        expectBandT60s (lucky::analyse (lucky::shortenDecay (ir, sampleRate, profile, 1.0f, 2000.0f), sampleRate), 2.0f);

        beginTest ("damping combines with decay");
        expectBandT60s (lucky::analyse (lucky::shortenDecay (ir, sampleRate, profile, 0.5f, 2000.0f), sampleRate), 1.0f);

        beginTest ("damping darkens the early response above its frequency only");
        const auto darkened = lucky::shortenDecay (ir, sampleRate, profile, 1.0f, 1000.0f);
        const auto earlyDb = [] (const juce::AudioBuffer<float>& buffer, juce::dsp::IIR::Coefficients<float>::Ptr coefficients)
        {
            juce::dsp::IIR::Filter<float> first (coefficients), second (coefficients);
            double energy = 0.0;
            for (int i = 0; i < static_cast<int> (0.03 * sampleRate); ++i)
            {
                const auto filtered = second.processSample (first.processSample (buffer.getSample (0, i)));
                energy += filtered * filtered;
            }
            return 10.0 * std::log10 (energy);
        };
        const auto highs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 6000.0f);
        const auto lows = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 500.0f);
        expectLessThan (earlyDb (darkened, highs), earlyDb (ir, highs) - 6.0);
        expectWithinAbsoluteError (earlyDb (darkened, lows), earlyDb (ir, lows), 0.5);
    }

private:
    /** Expects bands up to 1 kHz at lowT60 and, for damped bands, a T60 between the damped values at the band's centre and lower edge,
        since an octave band's measured decay is dominated by its slowest-decaying frequencies (with some headroom for slower
        frequencies leaking in through the analysis filter's skirt). */
    void expectBandT60s (const lucky::DecayProfile& damped, float lowT60)
    {
        constexpr float dampingHz = 2000.0f;

        for (size_t band = 1; band < lucky::analysisBands.size(); ++band)
        {
            const auto frequency = lucky::analysisBands[band];
            const auto name = "band " + juce::String (frequency);

            if (frequency < dampingHz)
            {
                expectWithinAbsoluteError (damped.bandT60[band], lowT60, lowT60 * 0.1f, name);
            }
            else if (frequency > dampingHz)
            {
                expectGreaterThan (damped.bandT60[band], 0.9f * lowT60 * dampingHz / frequency, name);
                expectLessThan (damped.bandT60[band], 1.25f * lowT60 * dampingHz / (frequency / juce::MathConstants<float>::sqrt2), name);
            }
        }
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
