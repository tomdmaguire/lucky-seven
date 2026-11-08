#pragma once

#include "IrAnalysis.h"
#include "IrLibrary.h"
#include <juce_data_structures/juce_data_structures.h>
#include <set>

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

    /** Re-reads the IR folder to pick up added, removed or renamed files, then re-analyses anything new. */
    void rescan();

    /** Snapshot of all programs in browse order. */
    std::vector<IrEntry> getEntries() const;

    /** Analysis for the program with the given id, if it has finished. */
    std::optional<DecayProfile> getProfile (const juce::String& id) const;

    /** Program with the given id, if present. */
    std::optional<IrEntry> findEntry (const juce::String& id) const;

    /** True if the program is in the user's favourites, shared by every instance. */
    bool isFavourite (const juce::String& id) const;

    /** Adds or removes a program from favourites and notifies listeners. */
    void toggleFavourite (const juce::String& id);

    /** Window scale shared by every instance (1.0 = 900 x 600). */
    float getUiScale() const;

    /** Remembers the window scale; written to disk shortly after the last change. */
    void setUiScale (float scale);

    /** Default folder used before the user picks one. */
    static juce::File defaultRoot();

private:
    void run() override;
    void loadCache();
    void saveCache() const;
    static juce::String cacheKey (const juce::File& file);

    mutable juce::ApplicationProperties settings;
    mutable juce::CriticalSection lock;
    std::vector<IrEntry> entries;
    std::map<juce::String, DecayProfile> profiles;
    std::map<juce::String, DecayProfile> cache;
    std::set<juce::String> favourites;
};

} // namespace lucky
