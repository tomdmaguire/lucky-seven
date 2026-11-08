#pragma once

#include "BrowserItem.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** Two-column browser: categories on the left, the selected category's programs on the right. */
class ListBrowserView final : public juce::Component,
                              private juce::KeyListener
{
public:
    explicit ListBrowserView (SelectionCallback onSelect);
    ~ListBrowserView() override;

    /** Replaces the programs shown, keeping the current selection where possible. */
    void setItems (std::vector<BrowserItem> newItems);

    /** Highlights the given program and its category without firing the selection callback. */
    void setSelectedId (const juce::String& id);

    void resized() override;

private:
    class CategoryModel;
    class ProgramModel;

    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress& key, juce::Component* originator) override;

    void showCategory (int categoryRow);
    std::vector<const BrowserItem*> programsInCategory (int categoryRow) const;

    SelectionCallback onSelect;
    std::vector<BrowserItem> items;
    std::vector<std::pair<int, juce::String>> categories;
    std::unique_ptr<CategoryModel> categoryModel;
    std::unique_ptr<ProgramModel> programModel;
    juce::ListBox categoryList, programList;
    juce::String selectedId;
    bool updating = false;
};

} // namespace lucky
