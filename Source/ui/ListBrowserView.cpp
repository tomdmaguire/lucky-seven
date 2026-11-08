#include "ListBrowserView.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr int rowHeight = 28;
constexpr int categoryColumnWidth = 190;
constexpr int heartColumnWidth = 36;
constexpr int searchHeight = 30;
constexpr int allResultsCategory = -1;
constexpr int categoryTagWidth = 110;

void paintRowBackground (juce::Graphics& g, int width, int height, bool selected, juce::Colour colour)
{
    if (selected)
    {
        g.setColour (colour.withAlpha (0.12f));
        g.fillRect (0, 0, width, height);
        g.setColour (colour);
        g.fillRect (0, 0, 2, height);
    }
}
} // namespace

class ListBrowserView::CategoryModel final : public juce::ListBoxModel
{
public:
    explicit CategoryModel (ListBrowserView& ownerView) : owner (ownerView) {}

    int getNumRows() override { return static_cast<int> (owner.categories.size()); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (row < 0 || row >= getNumRows())
            return;

        const auto& [index, name] = owner.categories[static_cast<size_t> (row)];
        const auto colour = index == allResultsCategory ? theme::text : theme::categoryColour (index);
        const auto count = owner.programsInCategory (row).size();
        paintRowBackground (g, width, height, selected, colour);

        g.setColour (count == 0 ? colour.withAlpha (0.3f) : colour);
        if (index == allResultsCategory)
            g.drawEllipse (15.0f, static_cast<float> (height) * 0.5f - 4.0f, 8.0f, 8.0f, 1.4f);
        else if (index == theme::favouritesCategory)
            g.fillPath (theme::heartPath ({ 14.0f, static_cast<float> (height) * 0.5f - 5.0f, 10.0f, 10.0f }));
        else
            g.fillEllipse (16.0f, static_cast<float> (height) * 0.5f - 3.0f, 6.0f, 6.0f);

        g.setFont (theme::labelFont (12.0f));
        g.setColour (selected ? theme::text : (count == 0 ? theme::textDim.withAlpha (0.4f) : theme::textDim));
        g.drawText (name.toUpperCase(), 32, 0, width - 72, height, juce::Justification::centredLeft);
        g.drawText (juce::String (count), width - 44, 0, 32, height, juce::Justification::centredRight);
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        if (! owner.updating && lastRowSelected >= 0)
            owner.showCategory (lastRowSelected);
    }

private:
    ListBrowserView& owner;
};

class ListBrowserView::ProgramModel final : public juce::ListBoxModel
{
public:
    explicit ProgramModel (ListBrowserView& ownerView) : owner (ownerView) {}

    int getNumRows() override { return static_cast<int> (programs.size()); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (row < 0 || row >= getNumRows())
            return;

        const auto& item = *programs[static_cast<size_t> (row)];
        paintRowBackground (g, width, height, selected, theme::categoryColour (item.entry.categoryIndex));

        const auto tagWidth = owner.showsMixedCategories() ? categoryTagWidth : 0;

        g.setFont (juce::FontOptions (14.0f));
        g.setColour (selected ? theme::text : theme::text.withAlpha (0.85f));
        g.drawText (item.entry.name, heartColumnWidth, 0, width - heartColumnWidth - 80 - tagWidth, height, juce::Justification::centredLeft);

        if (tagWidth > 0)
        {
            const auto tagX = static_cast<float> (width - 80 - tagWidth);
            g.setColour (theme::categoryColour (item.entry.categoryIndex));
            g.fillEllipse (tagX, static_cast<float> (height) * 0.5f - 2.5f, 5.0f, 5.0f);
            g.setColour (theme::textDim);
            g.setFont (theme::labelFont (10.0f));
            g.drawText (item.entry.category.toUpperCase(), juce::roundToInt (tagX) + 10, 0, tagWidth - 10, height, juce::Justification::centredLeft);
            g.setFont (juce::FontOptions (14.0f));
        }

        if (item.profile)
        {
            g.setColour (theme::textDim);
            g.drawText (theme::formatSeconds (item.profile->midT60), width - 76, 0, 64, height, juce::Justification::centredRight);
        }
    }

    juce::Component* refreshComponentForRow (int row, bool, juce::Component* existing) override;

    void selectedRowsChanged (int lastRowSelected) override
    {
        if (! owner.updating && lastRowSelected >= 0 && lastRowSelected < getNumRows())
        {
            owner.selectedId = programs[static_cast<size_t> (lastRowSelected)]->entry.id();
            owner.onSelect (owner.selectedId);
        }
    }

    std::vector<const BrowserItem*> programs;

private:
    ListBrowserView& owner;
};

/** Transparent overlay for a program row: draws the favourite heart and routes clicks to the heart or the row. */
class ListBrowserView::ProgramRow final : public juce::Component
{
public:
    explicit ProgramRow (ListBrowserView& ownerView) : owner (ownerView) {}

    void update (int newRow, const BrowserItem* newItem)
    {
        row = newRow;
        item = newItem;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (item == nullptr || ! (item->favourite || isMouseOver()))
            return;

        const auto heart = theme::heartPath (heartArea().withSizeKeepingCentre (12.0f, 11.0f));
        if (item->favourite)
        {
            g.setColour (theme::categoryColour (item->entry.categoryIndex));
            g.fillPath (heart);
        }
        else
        {
            g.setColour (overHeart ? theme::text : theme::textDim.withAlpha (0.6f));
            g.strokePath (heart, juce::PathStrokeType (1.2f));
        }
    }

    void mouseEnter (const juce::MouseEvent& event) override { mouseMove (event); }
    void mouseExit (const juce::MouseEvent&) override { overHeart = false; repaint(); }

    void mouseMove (const juce::MouseEvent& event) override
    {
        overHeart = heartArea().contains (event.position);
        setMouseCursor (overHeart ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        if (item == nullptr)
            return;

        owner.programList.grabKeyboardFocus();
        if (heartArea().contains (event.position))
            owner.onToggleFavourite (item->entry.id());
        else
            owner.programList.selectRow (row);
    }

private:
    juce::Rectangle<float> heartArea() const
    {
        return getLocalBounds().toFloat().withWidth (static_cast<float> (heartColumnWidth)).withTrimmedLeft (8.0f);
    }

    ListBrowserView& owner;
    int row = -1;
    const BrowserItem* item = nullptr;
    bool overHeart = false;
};

juce::Component* ListBrowserView::ProgramModel::refreshComponentForRow (int row, bool, juce::Component* existing)
{
    auto* rowComponent = dynamic_cast<ProgramRow*> (existing);
    if (rowComponent == nullptr)
    {
        delete existing;
        rowComponent = new ProgramRow (owner);
    }

    rowComponent->update (row, row >= 0 && row < getNumRows() ? programs[static_cast<size_t> (row)] : nullptr);
    return rowComponent;
}

ListBrowserView::ListBrowserView (SelectionCallback callback, SelectionCallback favouriteCallback)
    : onSelect (std::move (callback)),
      onToggleFavourite (std::move (favouriteCallback)),
      categoryModel (std::make_unique<CategoryModel> (*this)),
      programModel (std::make_unique<ProgramModel> (*this))
{
    for (auto* list : { &categoryList, &programList })
    {
        list->setRowHeight (rowHeight);
        list->setOutlineThickness (0);
        list->setWantsKeyboardFocus (true);
        list->addKeyListener (this);
        addAndMakeVisible (*list);
    }

    categoryList.setModel (categoryModel.get());
    programList.setModel (programModel.get());

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
    searchBox.onReturnKey = [this]
    {
        if (programModel->getNumRows() > 0)
        {
            programList.grabKeyboardFocus();
            programList.selectRow (0);
        }
    };
    searchBox.addKeyListener (this);
    addAndMakeVisible (searchBox);

    clearSearchButton.setButtonText (juce::String::fromUTF8 ("\xc3\x97"));
    clearSearchButton.getProperties().set ("round", true);
    clearSearchButton.setTooltip ("Clear search");
    clearSearchButton.onClick = [this] { searchBox.clear(); applySearch(); searchBox.grabKeyboardFocus(); };
    addChildComponent (clearSearchButton);
}

ListBrowserView::~ListBrowserView()
{
    categoryList.removeKeyListener (this);
    programList.removeKeyListener (this);
    searchBox.removeKeyListener (this);
}

bool ListBrowserView::keyPressed (const juce::KeyPress& key, juce::Component* originator)
{
    if (originator == &searchBox)
    {
        if (key == juce::KeyPress::downKey && programModel->getNumRows() > 0)
        {
            programList.grabKeyboardFocus();
            programList.selectRow (0);
            return true;
        }
        return false;
    }

    if (key == juce::KeyPress::rightKey && originator == &categoryList)
    {
        programList.grabKeyboardFocus();
        if (programList.getSelectedRow() < 0 && programModel->getNumRows() > 0)
            programList.selectRow (0);
        return true;
    }

    if (key == juce::KeyPress::leftKey && originator == &programList)
    {
        categoryList.grabKeyboardFocus();
        return true;
    }

    return false;
}

void ListBrowserView::setItems (std::vector<BrowserItem> newItems)
{
    items = std::move (newItems);
    std::sort (items.begin(), items.end(), alphabeticalOrder);

    rebuildCategories();
    setSelectedId (selectedId);
}

void ListBrowserView::rebuildCategories()
{
    categories.clear();
    if (! searchTerms.isEmpty())
        categories.emplace_back (allResultsCategory, "All results");

    categories.emplace_back (theme::favouritesCategory, "Favourites");
    for (const auto& item : items)
        if (categories.back().first != item.entry.categoryIndex)
            categories.emplace_back (item.entry.categoryIndex, item.entry.category);

    categoryList.updateContent();
    categoryList.repaint();
}

void ListBrowserView::applySearch()
{
    const auto previousCategory = juce::isPositiveAndBelow (categoryList.getSelectedRow(), static_cast<int> (categories.size()))
                                      ? categories[static_cast<size_t> (categoryList.getSelectedRow())].first
                                      : allResultsCategory;

    searchTerms = juce::StringArray::fromTokens (searchBox.getText().toLowerCase(), true);
    searchTerms.removeEmptyStrings();
    clearSearchButton.setVisible (searchBox.getText().isNotEmpty());
    rebuildCategories();

    if (! searchTerms.isEmpty())
    {
        selectCategoryRow (0);
        return;
    }

    if (previousCategory == allResultsCategory)
    {
        categoryList.deselectAllRows();
        setSelectedId (selectedId);
        return;
    }

    const auto row = std::find_if (categories.begin(), categories.end(), [&] (const auto& c) { return c.first == previousCategory; });
    selectCategoryRow (static_cast<int> (std::distance (categories.begin(), row)));
}

void ListBrowserView::selectCategoryRow (int categoryRow)
{
    {
        const juce::ScopedValueSetter<bool> guard (updating, true);
        categoryList.selectRow (categoryRow);
    }
    showCategory (categoryRow);
}

bool ListBrowserView::matchesSearch (const BrowserItem& item) const
{
    if (searchTerms.isEmpty())
        return true;

    const auto haystack = (item.entry.name + " " + item.entry.category).toLowerCase();
    for (const auto& term : searchTerms)
        if (! haystack.contains (term))
            return false;
    return true;
}

bool ListBrowserView::showsMixedCategories() const
{
    const auto row = categoryList.getSelectedRow();
    if (! juce::isPositiveAndBelow (row, static_cast<int> (categories.size())))
        return false;

    const auto index = categories[static_cast<size_t> (row)].first;
    return index == allResultsCategory || index == theme::favouritesCategory;
}

void ListBrowserView::setSelectedId (const juce::String& id)
{
    selectedId = id;
    const juce::ScopedValueSetter<bool> guard (updating, true);

    const auto inRow = [&] (int row)
    {
        const auto programs = programsInCategory (row);
        return std::any_of (programs.begin(), programs.end(), [&] (const auto* item) { return item->entry.id() == id; });
    };

    auto categoryRow = categoryList.getSelectedRow();
    if (categoryRow < 0 || ! inRow (categoryRow))
    {
        const auto selected = std::find_if (items.begin(), items.end(), [&] (const auto& item) { return item.entry.id() == id; });
        const auto categoryIndex = selected != items.end() ? selected->entry.categoryIndex : 1;
        categoryRow = static_cast<int> (std::distance (categories.begin(),
            std::find_if (categories.begin(), categories.end(), [&] (const auto& c) { return c.first == categoryIndex; })));
    }

    categoryList.selectRow (categoryRow);
    programModel->programs = programsInCategory (categoryRow);
    programList.updateContent();

    for (size_t row = 0; row < programModel->programs.size(); ++row)
        if (programModel->programs[row]->entry.id() == id)
            programList.selectRow (static_cast<int> (row));

    programList.repaint();
}

void ListBrowserView::showCategory (int categoryRow)
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    programModel->programs = programsInCategory (categoryRow);
    programList.updateContent();
    programList.deselectAllRows();

    for (size_t row = 0; row < programModel->programs.size(); ++row)
        if (programModel->programs[row]->entry.id() == selectedId)
            programList.selectRow (static_cast<int> (row));

    programList.repaint();
}

std::vector<const BrowserItem*> ListBrowserView::programsInCategory (int categoryRow) const
{
    std::vector<const BrowserItem*> result;
    if (categoryRow < 0 || categoryRow >= static_cast<int> (categories.size()))
        return result;

    const auto categoryIndex = categories[static_cast<size_t> (categoryRow)].first;
    for (const auto& item : items)
    {
        const auto inCategory = categoryIndex == allResultsCategory     ? true
                              : categoryIndex == theme::favouritesCategory ? item.favourite
                                                                           : item.entry.categoryIndex == categoryIndex;
        if (inCategory && matchesSearch (item))
            result.push_back (&item);
    }

    return result;
}

void ListBrowserView::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
    g.setColour (theme::outline);
    g.drawVerticalLine (categoryColumnWidth + 12, static_cast<float> (categoryList.getY()), static_cast<float> (getHeight() - 12));
}

void ListBrowserView::paintOverChildren (juce::Graphics& g)
{
    const auto icon = searchBox.getBounds().toFloat().removeFromLeft (30.0f).withSizeKeepingCentre (11.0f, 11.0f);
    g.setColour (theme::textDim);
    g.drawEllipse (icon.withTrimmedRight (3.0f).withTrimmedBottom (3.0f), 1.3f);
    g.drawLine (icon.getRight() - 4.0f, icon.getBottom() - 4.0f, icon.getRight(), icon.getBottom(), 1.3f);

    if (programModel->getNumRows() == 0 && ! searchTerms.isEmpty())
    {
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (12.0f));
        g.drawText ("NO MATCHES", programList.getBounds().removeFromTop (60), juce::Justification::centred);
    }
}

void ListBrowserView::resized()
{
    auto bounds = getLocalBounds().reduced (0, 12);
    auto searchRow = bounds.removeFromTop (searchHeight).withTrimmedLeft (8).withTrimmedRight (12);
    searchBox.setBounds (searchRow);
    clearSearchButton.setBounds (searchRow.removeFromRight (searchHeight).reduced (3));
    bounds.removeFromTop (10);

    categoryList.setBounds (bounds.removeFromLeft (categoryColumnWidth).withTrimmedLeft (8));
    bounds.removeFromLeft (25);
    programList.setBounds (bounds.withTrimmedRight (12));
}

} // namespace lucky
