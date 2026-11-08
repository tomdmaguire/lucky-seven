#pragma once

#include "ListBrowserView.h"
#include "SoundMapView.h"

namespace lucky
{

/** Hosts the sound map and the list browser, showing one at a time. */
class BrowserPanel final : public juce::Component
{
public:
    enum class View { map, list };

    explicit BrowserPanel (SelectionCallback onSelect);

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
};

} // namespace lucky
