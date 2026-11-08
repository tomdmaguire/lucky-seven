#pragma once

#include "IrAnalysis.h"
#include "IrLibrary.h"
#include <juce_data_structures/juce_data_structures.h>

namespace lucky
{

/** IR library shared by all plugin instances: scans the IR folder and analyses every program in the background. */
class IrCatalogue final : public juce::ChangeBroadcaster,
                          private juce::Thread
{
public:
    /** Sample rate whose files are analysed for the browser. */
    static constexpr double analysisRate = 48000.0;

    IrCatalogue();
    ~IrCatalogue() override;

    /** The folder containing the IRs, persisted across sessions. */
    juce::File getRoot() const;

    /** Changes the IR folder, rescans and restarts analysis. */
    void setRoot (const juce::File& newRoot);

    /** Snapshot of all programs in browse order. */
    std::vector<IrEntry> getEntries() const;

    /** Analysis for the program with the given id, if it has finished. */
    std::optional<DecayProfile> getProfile (const juce::String& id) const;

    /** Program with the given id, if present. */
    std::optional<IrEntry> findEntry (const juce::String& id) const;

    /** Default folder used before the user picks one. */
    static juce::File defaultRoot();

private:
    void run() override;
    void rescan();
    void loadCache();
    void saveCache() const;
    static juce::String cacheKey (const juce::File& file);

    mutable juce::ApplicationProperties settings;
    mutable juce::CriticalSection lock;
    std::vector<IrEntry> entries;
    std::map<juce::String, DecayProfile> profiles;
    std::map<juce::String, DecayProfile> cache;
};

} // namespace lucky
