#pragma once

#include "ListBrowserView.h"
#include "SoundMapView.h"
#include "EmptyStateView.h"

namespace lucky
{

/** Hosts the sound map and the list browser, showing one at a time. */
class BrowserPanel final : public juce::Component
{
public:
    enum class View { map, list };

    BrowserPanel (SelectionCallback onSelect, SelectionCallback onToggleFavourite, EmptyStateView::Actions emptyActions);

    /** Folder shown in the empty-state explanation. */
    void setFolder (const juce::File& folder);

    void setItems (const std::vector<BrowserItem>& items);
    void setSelectedId (const juce::String& id);
    void setDecayRatio (float ratio);
    void setView (View view);

    void resized() override;

    static juce::String viewToString (View view) { return view == View::list ? "list" : "map"; }
    static View viewFromString (const juce::String& text) { return text == "list" ? View::list : View::map; }

private:
    SoundMapView map;
    ListBrowserView list;
    EmptyStateView emptyState;
};

} // namespace lucky
