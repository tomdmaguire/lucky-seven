#include "BrowserPanel.h"

namespace lucky
{

BrowserPanel::BrowserPanel (SelectionCallback onSelect, SelectionCallback onToggleFavourite, EmptyStateView::Actions emptyActions)
    : map (onSelect), list (onSelect, std::move (onToggleFavourite)), emptyState (std::move (emptyActions))
{
    addChildComponent (map);
    addChildComponent (list);
    addChildComponent (emptyState);
    setView (View::map);
}

void BrowserPanel::setItems (const std::vector<BrowserItem>& items)
{
    map.setItems (items);
    list.setItems (items);
    emptyState.setVisible (items.empty());
}

void BrowserPanel::setFolder (const juce::File& folder)
{
    emptyState.setFolder (folder);
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
    emptyState.setBounds (getLocalBounds());
}

} // namespace lucky
