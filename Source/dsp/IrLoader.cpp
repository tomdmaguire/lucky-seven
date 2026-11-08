#include "IrLoader.h"
#include "DecayShaper.h"

namespace lucky
{

namespace
{
constexpr int pollIntervalMs = 20;
constexpr float ratioTolerance = 0.001f;
constexpr double maxIrSeconds = 30.0;
} // namespace

IrLoader::IrLoader (ReverbEngine& engineToFeed, std::atomic<float>& decayPercentSource)
    : juce::Thread ("Lucky Seven IR loader"), engine (engineToFeed), decayPercent (decayPercentSource)
{
    startThread();
}

IrLoader::~IrLoader()
{
    stopThread (5000);
}

void IrLoader::requestEntry (const IrEntry& entry, double hostSampleRate)
{
    const juce::ScopedLock scope (requestLock);
    pendingEntry = entry;
    pendingRate = hostSampleRate;
    notify();
}

void IrLoader::invalidate()
{
    forceRebuild = true;
    notify();
}

void IrLoader::run()
{
    auto lastSeenRatio = -1.0f;
    auto stableSinceMs = juce::Time::getMillisecondCounter();

    while (! threadShouldExit())
    {
        wait (pollIntervalMs);

        std::optional<IrEntry> entryToLoad;
        double rateToLoad = 48000.0;
        {
            const juce::ScopedLock scope (requestLock);
            entryToLoad = std::exchange (pendingEntry, std::nullopt);
            rateToLoad = pendingRate;
        }

        const auto ratio = juce::jlimit (0.1f, 1.0f, decayPercent.load() / 100.0f);
        const auto now = juce::Time::getMillisecondCounter();
        if (std::abs (ratio - lastSeenRatio) > ratioTolerance)
        {
            lastSeenRatio = ratio;
            stableSinceMs = now;
        }

        if (entryToLoad)
        {
            loadEntry (*entryToLoad, rateToLoad);
            install (ratio);
            continue;
        }

        const auto settled = now - stableSinceMs >= static_cast<juce::uint32> (debounceMs);
        const auto ratioChanged = std::abs (ratio - installedRatio) > ratioTolerance;

        if (original.getNumSamples() > 0 && (forceRebuild.exchange (false) || (settled && ratioChanged)))
            install (ratio);
    }
}

void IrLoader::loadEntry (const IrEntry& entry, double hostSampleRate)
{
    double rate = 0.0;
    auto buffer = readIrEntry (entry, hostSampleRate, rate);
    if (buffer.getNumSamples() == 0)
        return;

    const auto maxSamples = static_cast<int> (maxIrSeconds * rate);
    if (buffer.getNumSamples() > maxSamples)
    {
        const auto fade = static_cast<int> (0.05 * rate);
        buffer.setSize (buffer.getNumChannels(), maxSamples, true, true, true);
        buffer.applyGainRamp (maxSamples - fade, fade, 1.0f, 0.0f);
    }

    trimNoiseFloor (buffer, rate);
    normaliseEnergy (buffer);
    trimTail (buffer, rate);
    profile = analyse (buffer, rate);
    original = std::move (buffer);
    originalRate = rate;
    originalT60 = profile.midT60;
}

void IrLoader::install (float ratio)
{
    if (original.getNumSamples() == 0)
        return;

    const auto shaped = shortenDecay (original, originalRate, profile, ratio);
    engine.loadIr (splitTrueStereo (shaped, originalRate));
    installedRatio = ratio;
}

} // namespace lucky
