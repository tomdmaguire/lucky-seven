#pragma once

#include "ListBrowserView.h"
#include "SoundMapView.h"

namespace lucky
{

/** Hosts the sound map and the list browser behind a Map | List toggle. */
class BrowserPanel final : public juce::Component
{
public:
    enum class View { map, list };

    BrowserPanel (SelectionCallback onSelect, std::function<void (View)> onViewChanged);

    void setItems (const std::vector<BrowserItem>& items);
    void setSelectedId (const juce::String& id);
    void setDecayRatio (float ratio);
    void setView (View view);

    void paint (juce::Graphics&) override;
    void resized() override;

    static juce::String viewToString (View view) { return view == View::list ? "list" : "map"; }
    static View viewFromString (const juce::String& text) { return text == "list" ? View::list : View::map; }

private:
    std::function<void (View)> onViewChanged;
    SoundMapView map;
    ListBrowserView list;
    juce::TextButton mapButton { "Map" }, listButton { "List" };
    View currentView = View::map;
};

} // namespace lucky
