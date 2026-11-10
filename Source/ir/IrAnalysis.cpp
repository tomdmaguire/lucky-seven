#include "IrAnalysis.h"

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <future>
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
constexpr double noiseWindowSeconds = 0.01;
constexpr double directSoundSkipSeconds = 0.01;
constexpr double cleanFloor = 1.0e-8;
constexpr double flatSlopeDbPerSecond = 1.5;
constexpr double floorMargin = 10.0;
constexpr double noiseFadeSeconds = 0.05;

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

size_t noiseFloorStart (const std::vector<double>& energy, double sampleRate)
{
    const auto window = std::max<size_t> (1, static_cast<size_t> (noiseWindowSeconds * sampleRate));
    const auto windows = energy.size() / window;
    if (windows < 40)
        return energy.size();

    std::vector<double> level (windows, 0.0);
    for (size_t w = 0; w < windows; ++w)
        level[w] = std::accumulate (energy.begin() + static_cast<std::ptrdiff_t> (w * window),
                                    energy.begin() + static_cast<std::ptrdiff_t> ((w + 1) * window), 0.0) / static_cast<double> (window);

    const auto peakIndex = static_cast<size_t> (std::distance (level.begin(), std::max_element (level.begin(), level.end())));
    const auto segment = std::max<size_t> (3, windows / 20);
    if (peakIndex + 3 * segment >= windows)
        return energy.size();

    const auto mean = [&] (size_t from, size_t count)
    {
        return std::accumulate (level.begin() + static_cast<std::ptrdiff_t> (from),
                                level.begin() + static_cast<std::ptrdiff_t> (from + count), 0.0) / static_cast<double> (count);
    };

    const auto floor = mean (windows - segment, segment);

    const auto fitStart = std::max (peakIndex + 1, windows - windows / 4);
    double sumX = 0.0, sumY = 0.0, sumXX = 0.0, sumXY = 0.0;
    for (size_t w = fitStart; w < windows; ++w)
    {
        const auto x = static_cast<double> (w) * noiseWindowSeconds;
        const auto y = 10.0 * std::log10 (std::max (level[w], 1.0e-30));
        sumX += x;
        sumY += y;
        sumXX += x * x;
        sumXY += x * y;
    }
    const auto n = static_cast<double> (windows - fitStart);
    const auto slopeDbPerSecond = (n * sumXY - sumX * sumY) / std::max (n * sumXX - sumX * sumX, 1.0e-12);
    const auto isFlat = std::abs (slopeDbPerSecond) < flatSlopeDbPerSecond;
    if (floor <= level[peakIndex] * cleanFloor || ! isFlat)
        return energy.size();

    for (auto w = windows; w-- > peakIndex;)
        if (level[w] >= floor * floorMargin)
            return std::min (energy.size(), (w + 1) * window);

    return energy.size();
}

void trimNoiseFloor (juce::AudioBuffer<float>& ir, double sampleRate)
{
    std::vector<double> energy (static_cast<size_t> (ir.getNumSamples()), 0.0);
    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
    {
        const auto* data = ir.getReadPointer (ch);
        for (size_t i = 0; i < energy.size(); ++i)
            energy[i] += static_cast<double> (data[i]) * data[i];
    }

    const auto cut = static_cast<int> (noiseFloorStart (energy, sampleRate));
    if (cut >= ir.getNumSamples())
        return;

    const auto fade = std::min (cut, static_cast<int> (noiseFadeSeconds * sampleRate));
    ir.setSize (ir.getNumChannels(), cut, true, true, true);
    ir.applyGainRamp (cut - fade, fade, 1.0f, 0.0f);
}

float estimateT60 (const std::vector<double>& fullEnergy, double sampleRate)
{
    const auto cut = noiseFloorStart (fullEnergy, sampleRate);
    const std::vector<double> energy (fullEnergy.begin(), fullEnergy.begin() + static_cast<std::ptrdiff_t> (cut));

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
    const auto decayStart = static_cast<std::ptrdiff_t> (profile.onsetSample + static_cast<int> (directSoundSkipSeconds * sampleRate));
    const auto afterDirectSound = [decayStart] (const std::vector<double>& energy)
    {
        return decayStart < static_cast<std::ptrdiff_t> (energy.size()) ? std::vector<double> (energy.begin() + decayStart, energy.end()) : energy;
    };

    struct BandResult
    {
        float t60;
        double tailEnergy;
    };

    std::array<std::future<BandResult>, analysisBands.size()> bands;
    for (size_t band = 0; band < bands.size(); ++band)
        bands[band] = std::async (std::launch::async, [&, band]
        {
            const auto energy = bandEnergy (ir, sampleRate, analysisBands[band]);
            const auto tailEnergy = skip < energy.size() ? std::accumulate (energy.begin() + static_cast<std::ptrdiff_t> (skip), energy.end(), 0.0) : 0.0;
            return BandResult { estimateT60 (afterDirectSound (energy), sampleRate), tailEnergy };
        });

    const auto broadbandT60 = estimateT60 (afterDirectSound (broadbandEnergy (ir)), sampleRate);

    double weightedLogFrequency = 0.0, totalTailEnergy = 0.0;

    for (size_t band = 0; band < bands.size(); ++band)
    {
        const auto result = bands[band].get();
        profile.bandT60[band] = result.t60 > 0.0f ? result.t60 : broadbandT60;
        weightedLogFrequency += result.tailEnergy * std::log2 (static_cast<double> (analysisBands[band]));
        totalTailEnergy += result.tailEnergy;
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
