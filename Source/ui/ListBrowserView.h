#pragma once

#include "BrowserItem.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** Two-column browser: categories on the left, the selected category's programs on the right, both alphabetical.
    A search (set by BrowserPanel) filters both columns and adds an "All results" entry spanning every category. */
class ListBrowserView final : public juce::Component,
                              private juce::KeyListener
{
public:
    ListBrowserView (SelectionCallback onSelect, SelectionCallback onToggleFavourite);
    ~ListBrowserView() override;

    /** Replaces the programs shown, keeping the current selection where possible. */
    void setItems (std::vector<BrowserItem> newItems);

    /** Filters both columns to programs matching every search word; adds an "All results" category while searching. */
    void setSearch (const juce::StringArray& terms);

    /** Moves keyboard focus to the first result and loads it. */
    void focusFirstResult();

    /** Highlights the given program and its category without firing the selection callback. */
    void setSelectedId (const juce::String& id);

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    class CategoryModel;
    class ProgramModel;
    class ProgramRow;

    using juce::Component::keyPressed;
    bool keyPressed (const juce::KeyPress& key, juce::Component* originator) override;

    void showCategory (int categoryRow);
    void selectCategoryRow (int categoryRow);
    void rebuildCategories();
    bool showsMixedCategories() const;
    std::vector<const BrowserItem*> programsInCategory (int categoryRow) const;

    SelectionCallback onSelect, onToggleFavourite;
    std::vector<BrowserItem> items;
    std::vector<std::pair<int, juce::String>> categories;
    std::unique_ptr<CategoryModel> categoryModel;
    std::unique_ptr<ProgramModel> programModel;
    juce::ListBox categoryList, programList;
    juce::StringArray searchTerms;
    juce::String selectedId;
    bool updating = false;
};

} // namespace lucky
