#include "IrAnalysis.h"

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <numeric>

namespace lucky
{

namespace
{
constexpr float octaveQ = 1.41421356f;
constexpr double fitUpperDb = -5.0;
constexpr double fitLowerDbT30 = -35.0;
constexpr double fitLowerDbT20 = -25.0;
constexpr double brightnessSkipSeconds = 0.05;

std::vector<double> bandEnergy (const juce::AudioBuffer<float>& ir, double sampleRate, float centreHz)
{
    const auto numSamples = ir.getNumSamples();
    std::vector<double> energy (static_cast<size_t> (numSamples), 0.0);

    if (centreHz >= sampleRate * 0.45)
        return energy;

    const auto coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, centreHz, octaveQ);

    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
    {
        juce::dsp::IIR::Filter<float> first (coefficients), second (coefficients);
        const auto* input = ir.getReadPointer (ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const auto filtered = second.processSample (first.processSample (input[i]));
            energy[static_cast<size_t> (i)] += static_cast<double> (filtered) * filtered;
        }
    }

    return energy;
}

std::vector<double> broadbandEnergy (const juce::AudioBuffer<float>& ir)
{
    std::vector<double> energy (static_cast<size_t> (ir.getNumSamples()), 0.0);

    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
    {
        const auto* input = ir.getReadPointer (ch);
        for (size_t i = 0; i < energy.size(); ++i)
            energy[i] += static_cast<double> (input[i]) * input[i];
    }

    return energy;
}

float linearFitT60 (const std::vector<double>& edcDb, double sampleRate, double lowerDb)
{
    double sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumXY = 0.0;
    size_t count = 0;

    for (size_t i = 0; i < edcDb.size(); ++i)
    {
        const auto db = edcDb[i];
        if (db > fitUpperDb)
            continue;
        if (db < lowerDb)
            break;

        const auto t = static_cast<double> (i) / sampleRate;
        sumX += t;
        sumY += db;
        sumXX += t * t;
        sumXY += t * db;
        ++count;
    }

    if (count < 16)
        return 0.0f;

    const auto n = static_cast<double> (count);
    const auto denominator = n * sumXX - sumX * sumX;
    if (denominator <= 0.0)
        return 0.0f;

    const auto slopeDbPerSecond = (n * sumXY - sumX * sumY) / denominator;
    return slopeDbPerSecond < 0.0 ? static_cast<float> (-60.0 / slopeDbPerSecond) : 0.0f;
}
} // namespace

int findOnset (const juce::AudioBuffer<float>& ir)
{
    float peak = 0.0f;
    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
        peak = std::max (peak, ir.getMagnitude (ch, 0, ir.getNumSamples()));

    if (peak <= 0.0f)
        return 0;

    const auto threshold = peak * 0.1f;

    for (int i = 0; i < ir.getNumSamples(); ++i)
        for (int ch = 0; ch < ir.getNumChannels(); ++ch)
            if (std::abs (ir.getSample (ch, i)) >= threshold)
                return i;

    return 0;
}

float estimateT60 (const std::vector<double>& energy, double sampleRate)
{
    std::vector<double> edc (energy.size());
    std::partial_sum (energy.rbegin(), energy.rend(), edc.rbegin());

    if (edc.empty() || edc.front() <= 0.0)
        return 0.0f;

    const auto total = edc.front();
    for (auto& value : edc)
        value = 10.0 * std::log10 (std::max (value / total, 1.0e-30));

    const auto t30 = linearFitT60 (edc, sampleRate, fitLowerDbT30);
    return t30 > 0.0f ? t30 : linearFitT60 (edc, sampleRate, fitLowerDbT20);
}

DecayProfile analyse (const juce::AudioBuffer<float>& ir, double sampleRate)
{
    DecayProfile profile;
    profile.onsetSample = findOnset (ir);

    const auto skip = static_cast<size_t> (profile.onsetSample + static_cast<int> (brightnessSkipSeconds * sampleRate));
    const auto broadbandT60 = estimateT60 (broadbandEnergy (ir), sampleRate);

    double weightedLogFrequency = 0.0, totalTailEnergy = 0.0;

    for (size_t band = 0; band < analysisBands.size(); ++band)
    {
        const auto energy = bandEnergy (ir, sampleRate, analysisBands[band]);
        const auto t60 = estimateT60 (energy, sampleRate);
        profile.bandT60[band] = t60 > 0.0f ? t60 : broadbandT60;

        const auto tailEnergy = skip < energy.size() ? std::accumulate (energy.begin() + static_cast<std::ptrdiff_t> (skip), energy.end(), 0.0) : 0.0;
        weightedLogFrequency += tailEnergy * std::log2 (static_cast<double> (analysisBands[band]));
        totalTailEnergy += tailEnergy;
    }

    profile.midT60 = 0.5f * (profile.bandT60[2] + profile.bandT60[3]);
    profile.brightnessHz = totalTailEnergy > 0.0 ? static_cast<float> (std::exp2 (weightedLogFrequency / totalTailEnergy)) : 0.0f;
    return profile;
}

float t60AtFrequency (const DecayProfile& profile, float frequencyHz)
{
    const auto& bands = analysisBands;
    const auto& t60 = profile.bandT60;

    if (frequencyHz <= bands.front())
        return t60.front();
    if (frequencyHz >= bands.back())
        return t60.back();

    for (size_t i = 1; i < bands.size(); ++i)
    {
        if (frequencyHz <= bands[i])
        {
            const auto position = std::log2 (frequencyHz / bands[i - 1]) / std::log2 (bands[i] / bands[i - 1]);
            return t60[i - 1] + position * (t60[i] - t60[i - 1]);
        }
    }

    return t60.back();
}

void normaliseEnergy (juce::AudioBuffer<float>& ir)
{
    double energy = 0.0;
    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
    {
        const auto* data = ir.getReadPointer (ch);
        for (int i = 0; i < ir.getNumSamples(); ++i)
            energy += static_cast<double> (data[i]) * data[i];
    }

    if (energy <= 0.0)
        return;

    const auto targetEnergy = static_cast<double> (ir.getNumChannels()) * 0.5;
    ir.applyGain (static_cast<float> (std::sqrt (targetEnergy / energy)));
}

} // namespace lucky
