#pragma once

#include "ir/IrAnalysis.h"

namespace lucky
{

/** Returns a copy of the impulse response with every frequency's decay time scaled by ratio (0 < ratio <= 1). */
juce::AudioBuffer<float> shortenDecay (const juce::AudioBuffer<float>& ir, double sampleRate,
                                       const DecayProfile& profile, float ratio);

/** Removes trailing samples below -96 dB of the peak and applies a short fade-out to the new end. */
void trimTail (juce::AudioBuffer<float>& ir, double sampleRate);

} // namespace lucky
