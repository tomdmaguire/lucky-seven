#pragma once

#include "ReverbEngine.h"
#include "ir/IrAnalysis.h"
#include "ir/IrLibrary.h"
#include <juce_core/juce_core.h>

namespace lucky
{

/** Background worker that reads, normalises, shortens and installs IRs whenever the program, decay or damping changes. */
class IrLoader final : private juce::Thread
{
public:
    /** Minimum time between rebuilds while decay or damping moves; matches the head convolver's 50 ms crossfade,
        so each swap finishes fading before the next one starts. */
    static constexpr int minRebuildIntervalMs = 50;

    IrLoader (ReverbEngine& engineToFeed, std::atomic<float>& decayPercentSource, std::atomic<float>& dampingHzSource);
    ~IrLoader() override;

    /** Requests a program at the file closest to the host rate; safe to call from any non-audio thread. */
    void requestEntry (const IrEntry& entry, double hostSampleRate);

    /** Forces a rebuild, e.g. after the host sample rate changes. */
    void invalidate();

    /** Decay ratio of the IR currently installed in the engine, or -1 before anything has loaded. */
    float getInstalledDecayRatio() const noexcept { return installedDecayRatio.load(); }

    /** Damping frequency of the IR currently installed in the engine, or -1 before anything has loaded. */
    float getInstalledDampingHz() const noexcept { return installedDampingHz.load(); }

    /** Untouched mid-band T60 of the loaded IR in seconds, or 0 before anything has loaded. */
    float getOriginalT60() const noexcept { return originalT60.load(); }

private:
    /** How the loaded IR is reshaped: decay ratio and damping frequency. */
    struct Shape
    {
        float ratio = -1.0f;
        float dampingHz = -1.0f;

        bool differsFrom (const Shape& other) const noexcept;
    };

    Shape readShape() const noexcept;
    void run() override;
    void loadEntry (const IrEntry& entry, double hostSampleRate);
    void install (Shape shape);

    ReverbEngine& engine;
    std::atomic<float>& decayPercent;
    std::atomic<float>& dampingHz;
    juce::CriticalSection requestLock;
    std::optional<IrEntry> pendingEntry;
    double pendingRate = 48000.0;
    std::atomic<bool> forceRebuild { false };
    std::atomic<float> originalT60 { 0.0f };
    std::atomic<float> installedDecayRatio { -1.0f }, installedDampingHz { -1.0f };

    juce::AudioBuffer<float> original;
    double originalRate = 0.0;
    DecayProfile profile;
    Shape installed;
};

} // namespace lucky
