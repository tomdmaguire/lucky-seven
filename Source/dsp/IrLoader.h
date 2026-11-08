#pragma once

#include "ReverbEngine.h"
#include "ir/IrAnalysis.h"
#include "ir/IrLibrary.h"
#include <juce_core/juce_core.h>

namespace lucky
{

/** Background worker that reads, normalises, shortens and installs IRs whenever the program or decay changes. */
class IrLoader final : private juce::Thread
{
public:
    /** Rebuilds once the decay control has been still for this long. */
    static constexpr int debounceMs = 80;

    IrLoader (ReverbEngine& engineToFeed, std::atomic<float>& decayPercentSource);
    ~IrLoader() override;

    /** Requests a program at the file closest to the host rate; safe to call from any non-audio thread. */
    void requestEntry (const IrEntry& entry, double hostSampleRate);

    /** Forces a rebuild, e.g. after the host sample rate changes. */
    void invalidate();

    /** Untouched mid-band T60 of the loaded IR in seconds, or 0 before anything has loaded. */
    float getOriginalT60() const noexcept { return originalT60.load(); }

private:
    void run() override;
    void loadEntry (const IrEntry& entry, double hostSampleRate);
    void install (float ratio);

    ReverbEngine& engine;
    std::atomic<float>& decayPercent;
    juce::CriticalSection requestLock;
    std::optional<IrEntry> pendingEntry;
    double pendingRate = 48000.0;
    std::atomic<bool> forceRebuild { false };
    std::atomic<float> originalT60 { 0.0f };

    juce::AudioBuffer<float> original;
    double originalRate = 0.0;
    DecayProfile profile;
    float installedRatio = -1.0f;
};

} // namespace lucky
