#include "IrLoader.h"
#include "DecayShaper.h"

namespace lucky
{

namespace
{
constexpr int pollIntervalMs = 10;
constexpr float ratioTolerance = 0.001f;
constexpr float dampingToleranceHz = 1.0f;
constexpr double maxIrSeconds = 30.0;
} // namespace

bool IrLoader::Shape::differsFrom (const Shape& other) const noexcept
{
    return std::abs (ratio - other.ratio) > ratioTolerance || std::abs (dampingHz - other.dampingHz) > dampingToleranceHz;
}

IrLoader::IrLoader (ReverbEngine& engineToFeed, std::atomic<float>& decayPercentSource, std::atomic<float>& dampingHzSource)
    : juce::Thread ("Lucky Seven IR loader"), engine (engineToFeed), decayPercent (decayPercentSource), dampingHz (dampingHzSource)
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

IrLoader::Shape IrLoader::readShape() const noexcept
{
    return { juce::jlimit (0.1f, 1.0f, decayPercent.load() / 100.0f), dampingHz.load() };
}

void IrLoader::run()
{
    auto lastInstallMs = juce::Time::getMillisecondCounter();

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

        const auto shape = readShape();
        const auto now = juce::Time::getMillisecondCounter();

        if (entryToLoad)
        {
            loadEntry (*entryToLoad, rateToLoad);
            install (shape);
            lastInstallMs = now;
            continue;
        }

        const auto intervalElapsed = now - lastInstallMs >= static_cast<juce::uint32> (minRebuildIntervalMs);
        if (original.getNumSamples() > 0 && (forceRebuild.exchange (false) || (intervalElapsed && shape.differsFrom (installed))))
        {
            install (shape);
            lastInstallMs = now;
        }
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

void IrLoader::install (Shape shape)
{
    if (original.getNumSamples() == 0)
        return;

    const auto shaped = shortenDecay (original, originalRate, profile, shape.ratio, shape.dampingHz);
    engine.loadIr (splitTrueStereo (shaped, originalRate));
    installed = shape;
    installedDecayRatio = shape.ratio;
    installedDampingHz = shape.dampingHz;
}

} // namespace lucky
