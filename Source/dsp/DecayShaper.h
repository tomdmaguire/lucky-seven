#pragma once

#include "ir/IrAnalysis.h"

namespace lucky
{

/** Damping frequency at or above which damping is off. */
inline constexpr float dampingOffHz = 19999.0f;

/** Returns a copy of the impulse response with every frequency's decay time scaled by ratio (0 < ratio <= 1),
    and above dampingHz further scaled by dampingHz / frequency, so each octave above it halves the decay time. */
juce::AudioBuffer<float> shortenDecay (const juce::AudioBuffer<float>& ir, double sampleRate,
                                       const DecayProfile& profile, float ratio, float dampingHz = dampingOffHz);

/** Removes trailing samples below -96 dB of the peak and applies a short fade-out to the new end. */
void trimTail (juce::AudioBuffer<float>& ir, double sampleRate);

} // namespace lucky
