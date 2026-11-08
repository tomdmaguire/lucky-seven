#include "BrowserPanel.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr int toggleHeight = 26;
constexpr int toggleWidth = 64;
constexpr int radioGroup = 7001;
} // namespace

BrowserPanel::BrowserPanel (SelectionCallback onSelect, std::function<void (View)> viewCallback)
    : onViewChanged (std::move (viewCallback)), map (onSelect), list (onSelect)
{
    for (auto* button : { &mapButton, &listButton })
    {
        button->setClickingTogglesState (true);
        button->setRadioGroupId (radioGroup);
        addAndMakeVisible (*button);
    }

    mapButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    listButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    mapButton.onClick = [this] { if (mapButton.getToggleState()) { setView (View::map); onViewChanged (View::map); } };
    listButton.onClick = [this] { if (listButton.getToggleState()) { setView (View::list); onViewChanged (View::list); } };

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
    currentView = view;
    mapButton.setToggleState (view == View::map, juce::dontSendNotification);
    listButton.setToggleState (view == View::list, juce::dontSendNotification);
    map.setVisible (view == View::map);
    list.setVisible (view == View::list);
}

void BrowserPanel::paint (juce::Graphics& g)
{
    g.setColour (theme::outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().withTrimmedTop (toggleHeight + 8.0f).reduced (0.5f), 6.0f, 1.0f);
}

void BrowserPanel::resized()
{
    auto bounds = getLocalBounds();
    auto toggles = bounds.removeFromTop (toggleHeight).removeFromRight (toggleWidth * 2);
    mapButton.setBounds (toggles.removeFromLeft (toggleWidth));
    listButton.setBounds (toggles);
    bounds.removeFromTop (8);

    const auto content = bounds.reduced (1);
    map.setBounds (content);
    list.setBounds (content);
}

} // namespace lucky
