#pragma once

#include "ReverbEngine.h"
#include "ir/IrAnalysis.h"
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

    /** Requests a new IR file; safe to call from any non-audio thread. */
    void requestFile (const juce::File& file);

    /** Forces a rebuild, e.g. after the host sample rate changes. */
    void invalidate();

    /** Untouched mid-band T60 of the loaded IR in seconds, or 0 before anything has loaded. */
    float getOriginalT60() const noexcept { return originalT60.load(); }

private:
    void run() override;
    void loadFile (const juce::File& file);
    void install (float ratio);

    ReverbEngine& engine;
    std::atomic<float>& decayPercent;
    juce::CriticalSection requestLock;
    juce::File pendingFile;
    bool fileDirty = false;
    std::atomic<bool> forceRebuild { false };
    std::atomic<float> originalT60 { 0.0f };

    juce::AudioBuffer<float> original;
    double originalRate = 0.0;
    DecayProfile profile;
    float installedRatio = -1.0f;
};

} // namespace lucky
