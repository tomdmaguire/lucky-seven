#include "ListBrowserView.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr int rowHeight = 28;
constexpr int categoryColumnWidth = 190;

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
        paintRowBackground (g, width, height, selected, theme::categoryColour (index));

        g.setColour (theme::categoryColour (index));
        if (index == theme::favouritesCategory)
            g.fillPath (theme::heartPath ({ 14.0f, static_cast<float> (height) * 0.5f - 5.0f, 10.0f, 10.0f }));
        else
            g.fillEllipse (16.0f, static_cast<float> (height) * 0.5f - 3.0f, 6.0f, 6.0f);

        g.setFont (theme::labelFont (12.0f));
        g.setColour (selected ? theme::text : theme::textDim);
        g.drawText (name.toUpperCase(), 32, 0, width - 72, height, juce::Justification::centredLeft);
        g.drawText (juce::String (owner.programsInCategory (row).size()), width - 44, 0, 32, height, juce::Justification::centredRight);
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

        g.setFont (juce::FontOptions (14.0f));
        g.setColour (selected ? theme::text : theme::text.withAlpha (0.85f));
        g.drawText (item.entry.name, 16, 0, width - 110, height, juce::Justification::centredLeft);

        if (item.favourite)
        {
            g.setColour (theme::categoryColour (item.entry.categoryIndex));
            g.fillPath (theme::heartPath ({ static_cast<float> (width) - 92.0f, static_cast<float> (height) * 0.5f - 5.0f, 10.0f, 10.0f }));
        }

        if (item.profile)
        {
            g.setColour (theme::textDim);
            g.drawText (theme::formatSeconds (item.profile->midT60), width - 76, 0, 64, height, juce::Justification::centredRight);
        }
    }

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

ListBrowserView::ListBrowserView (SelectionCallback callback)
    : onSelect (std::move (callback)),
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
}

ListBrowserView::~ListBrowserView()
{
    categoryList.removeKeyListener (this);
    programList.removeKeyListener (this);
}

bool ListBrowserView::keyPressed (const juce::KeyPress& key, juce::Component* originator)
{
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

    categories.clear();
    categories.emplace_back (theme::favouritesCategory, "Favourites");
    for (const auto& item : items)
        if (categories.empty() || categories.back().first != item.entry.categoryIndex)
            categories.emplace_back (item.entry.categoryIndex, item.entry.category);

    categoryList.updateContent();
    setSelectedId (selectedId);
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
        if (categoryIndex == theme::favouritesCategory ? item.favourite : item.entry.categoryIndex == categoryIndex)
            result.push_back (&item);

    return result;
}

void ListBrowserView::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
    g.setColour (theme::outline);
    g.drawVerticalLine (categoryColumnWidth + 12, 12.0f, static_cast<float> (getHeight() - 12));
}

void ListBrowserView::resized()
{
    auto bounds = getLocalBounds().reduced (0, 12);
    categoryList.setBounds (bounds.removeFromLeft (categoryColumnWidth).withTrimmedLeft (8));
    bounds.removeFromLeft (25);
    programList.setBounds (bounds.withTrimmedRight (12));
}

} // namespace lucky
