#include "BrowserPanel.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr int searchHeight = 30;
constexpr int searchMargin = 10;
} // namespace

BrowserPanel::BrowserPanel (SelectionCallback onSelect, SelectionCallback onToggleFavourite, EmptyStateView::Actions emptyActions)
    : map (onSelect), list (onSelect, std::move (onToggleFavourite)), emptyState (std::move (emptyActions))
{
    addChildComponent (map);
    addChildComponent (list);

    searchBox.setComponentID ("search");
    searchBox.setTextToShowWhenEmpty ("Search", theme::textDim);
    searchBox.setFont (juce::FontOptions (14.0f));
    searchBox.setIndents (30, 7);
    searchBox.setColour (juce::TextEditor::backgroundColourId, theme::topBar);
    searchBox.setColour (juce::TextEditor::textColourId, theme::text);
    searchBox.setColour (juce::TextEditor::outlineColourId, theme::outline);
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, theme::textDim);
    searchBox.setColour (juce::TextEditor::highlightColourId, theme::text.withAlpha (0.2f));
    searchBox.setColour (juce::CaretComponent::caretColourId, theme::text);
    searchBox.onTextChange = [this] { applySearch(); };
    searchBox.onEscapeKey = [this] { searchBox.clear(); applySearch(); };
    searchBox.onReturnKey = [this] { jumpToFirstResult(); };
    searchBox.addKeyListener (this);
    addAndMakeVisible (searchBox);

    clearSearchButton.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
    clearSearchButton.getProperties().set ("round", true);
    clearSearchButton.setTooltip ("Clear search");
    clearSearchButton.onClick = [this] { searchBox.clear(); applySearch(); searchBox.grabKeyboardFocus(); };
    addChildComponent (clearSearchButton);

    addChildComponent (emptyState);
    setView (View::map);
}

BrowserPanel::~BrowserPanel()
{
    searchBox.removeKeyListener (this);
}

bool BrowserPanel::keyPressed (const juce::KeyPress& key, juce::Component* originator)
{
    if (originator == &searchBox && key.getKeyCode() == juce::KeyPress::downKey)
    {
        jumpToFirstResult();
        return true;
    }
    return false;
}

void BrowserPanel::applySearch()
{
    const auto terms = parseSearch (searchBox.getText());
    clearSearchButton.setVisible (searchBox.getText().isNotEmpty());
    list.setSearch (terms);
    map.setSearch (terms);
}

void BrowserPanel::jumpToFirstResult()
{
    if (currentView == View::list)
        list.focusFirstResult();
    else
        map.selectFirstVisible();
}

void BrowserPanel::setItems (const std::vector<BrowserItem>& items)
{
    map.setItems (items);
    list.setItems (items);
    emptyState.setVisible (items.empty());
    searchBox.setVisible (! items.empty());
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
    currentView = view;
    map.setVisible (view == View::map);
    list.setVisible (view == View::list);
}

void BrowserPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
}

void BrowserPanel::paintOverChildren (juce::Graphics& g)
{
    if (! searchBox.isVisible())
        return;

    const auto icon = searchBox.getBounds().toFloat().removeFromLeft (30.0f).withSizeKeepingCentre (11.0f, 11.0f);
    g.setColour (theme::textDim);
    g.drawEllipse (icon.withTrimmedRight (3.0f).withTrimmedBottom (3.0f), 1.3f);
    g.drawLine (icon.getRight() - 4.0f, icon.getBottom() - 4.0f, icon.getRight(), icon.getBottom(), 1.3f);
}

void BrowserPanel::resized()
{
    auto bounds = getLocalBounds();
    emptyState.setBounds (bounds);

    auto searchRow = bounds.removeFromTop (searchHeight + 2 * searchMargin).reduced (searchMargin + 2, searchMargin);
    searchBox.setBounds (searchRow);
    clearSearchButton.setBounds (searchRow.removeFromRight (searchHeight).reduced (3));

    map.setBounds (bounds);
    list.setBounds (bounds);
}

} // namespace lucky
