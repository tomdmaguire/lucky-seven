#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace lucky
{

/** Octave band centre frequencies used for decay analysis and shaping. */
inline constexpr std::array<float, 7> analysisBands { 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f };

/** Measured decay characteristics of an impulse response. */
struct DecayProfile
{
    std::array<float, analysisBands.size()> bandT60 {};
    float midT60 = 0.0f;
    float brightnessHz = 0.0f;
    int onsetSample = 0;
};

/** Index of the first sample whose magnitude reaches -20 dB relative to the peak across all channels. */
int findOnset (const juce::AudioBuffer<float>& ir);

/** Estimates T60 in seconds from a squared-signal energy envelope using Schroeder backward integration. */
float estimateT60 (const std::vector<double>& energy, double sampleRate);

/** Measures per-octave T60, mid-band T60 and spectral brightness of an impulse response. */
DecayProfile analyse (const juce::AudioBuffer<float>& ir, double sampleRate);

/** Returns the T60 at an arbitrary frequency by log-frequency interpolation between octave bands. */
float t60AtFrequency (const DecayProfile& profile, float frequencyHz);

/** Scales the buffer in place so its total energy across channels equals that of a unit impulse per channel pair. */
void normaliseEnergy (juce::AudioBuffer<float>& ir);

} // namespace lucky
