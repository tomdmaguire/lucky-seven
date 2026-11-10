#pragma once

#include "ListBrowserView.h"
#include "SoundMapView.h"
#include "EmptyStateView.h"

namespace lucky
{

/** Hosts the sound map and the list browser (one at a time) under a shared search box that filters both. */
class BrowserPanel final : public juce::Component,
                           private juce::KeyListener
{
public:
    enum class View { map, list };

    BrowserPanel (SelectionCallback onSelect, SelectionCallback onToggleFavourite, EmptyStateView::Actions emptyActions);
    ~BrowserPanel() override;

    /** Folder shown in the empty-state explanation. */
    void setFolder (const juce::File& folder);

    /** Progress of a Bricasti download, shown on the first-run panel. */
    void setDownloadStatus (const IrDownloader::Status& status) { emptyState.setDownloadStatus (status); }

    void setItems (const std::vector<BrowserItem>& items);
    void setSelectedId (const juce::String& id);
    void setDecayRatio (float ratio);
    void setMapFilter (std::set<int> categories) { map.setFilter (std::move (categories)); }
    void onMapFilterChanged (std::function<void (const std::set<int>&)> callback) { map.onFilterChanged = std::move (callback); }
    void setLevelSource (std::function<std::pair<float, float>()> source) { map.setLevelSource (std::move (source)); }
    void setView (View view);

    /** Gives keyboard focus to the visible map or list, so the arrow keys browse it. */
    void focusCurrentView();

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    static juce::String viewToString (View view) { return view == View::list ? "list" : "map"; }
    static View viewFromString (const juce::String& text) { return text == "list" ? View::list : View::map; }

private:
    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress& key, juce::Component* originator) override;
    void applySearch();
    void jumpToFirstResult();

    SoundMapView map;
    ListBrowserView list;
    EmptyStateView emptyState;
    juce::TextEditor searchBox;
    juce::TextButton clearSearchButton;
    View currentView = View::map;
};

} // namespace lucky
