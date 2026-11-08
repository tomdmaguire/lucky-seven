#include "BrowserPanel.h"

namespace lucky
{

BrowserPanel::BrowserPanel (SelectionCallback onSelect, SelectionCallback onToggleFavourite)
    : map (onSelect), list (onSelect, std::move (onToggleFavourite))
{
    addChildComponent (map);
    addChildComponent (list);
    setView (View::map);
}

void BrowserPanel::setItems (const std::vector<BrowserItem>& items)
{
    map.setItems (items);
    list.setItems (items);
}

void BrowserPanel::setSelectedId (const juce::String& id)
{
    map.setSelectedId (id);
    list.setSelectedId (id);
}

void BrowserPanel::setDecayRatio (float ratio)
{
    map.setDecayRatio (ratio);
}

void BrowserPanel::setView (View view)
{
    map.setVisible (view == View::map);
    list.setVisible (view == View::list);
}

void BrowserPanel::resized()
{
    map.setBounds (getLocalBounds());
    list.setBounds (getLocalBounds());
}

} // namespace lucky
