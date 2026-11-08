#include "ListBrowserView.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr int rowHeight = 26;
constexpr int categoryColumnWidth = 170;

void paintRowBackground (juce::Graphics& g, int width, int height, bool selected)
{
    if (selected)
    {
        g.setColour (theme::panelRaised);
        g.fillRect (0, 0, width, height);
        g.setColour (theme::accent);
        g.fillRect (0, 0, 3, height);
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

        paintRowBackground (g, width, height, selected);
        const auto& [index, name] = owner.categories[static_cast<size_t> (row)];

        g.setColour (theme::categoryColour (index));
        g.fillEllipse (14.0f, static_cast<float> (height) * 0.5f - 4.0f, 8.0f, 8.0f);

        g.setFont (juce::FontOptions (14.0f));
        g.setColour (selected ? theme::text : theme::textDim);
        g.drawText (name, 30, 0, width - 70, height, juce::Justification::centredLeft);
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

        paintRowBackground (g, width, height, selected);
        const auto& item = *programs[static_cast<size_t> (row)];

        g.setFont (juce::FontOptions (14.0f));
        g.setColour (selected ? theme::text : theme::text.withAlpha (0.85f));
        g.drawText (item.entry.name, 16, 0, width - 92, height, juce::Justification::centredLeft);

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
    std::sort (items.begin(), items.end(), [] (const BrowserItem& a, const BrowserItem& b)
    {
        if (a.entry.category != b.entry.category)
            return a.entry.category.compareNatural (b.entry.category) < 0;
        return a.entry.name.compareNatural (b.entry.name) < 0;
    });

    categories.clear();
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

    const auto selected = std::find_if (items.begin(), items.end(), [&] (const auto& item) { return item.entry.id() == id; });
    const auto categoryIndex = selected != items.end() ? selected->entry.categoryIndex : (categories.empty() ? 0 : categories.front().first);
    const auto categoryRow = static_cast<int> (std::distance (categories.begin(),
        std::find_if (categories.begin(), categories.end(), [&] (const auto& c) { return c.first == categoryIndex; })));

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
        if (item.entry.categoryIndex == categoryIndex)
            result.push_back (&item);

    return result;
}

void ListBrowserView::resized()
{
    auto bounds = getLocalBounds();
    categoryList.setBounds (bounds.removeFromLeft (categoryColumnWidth));
    bounds.removeFromLeft (1);
    programList.setBounds (bounds);
}

} // namespace lucky
