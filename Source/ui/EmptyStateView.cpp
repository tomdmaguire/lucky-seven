#include "EmptyStateView.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr int contentWidth = 520;
constexpr int contentHeight = 250;
constexpr int buttonHeight = 30;
} // namespace

EmptyStateView::EmptyStateView (Actions actions)
{
    downloadButton.onClick = [] { downloadUrl.launchInDefaultBrowser(); };
    openButton.onClick = actions.openFolder;
    chooseButton.onClick = actions.chooseFolder;
    rescanButton.onClick = actions.rescan;

    downloadButton.setTooltip (downloadUrl.toString (false) + " - the 'Bricasti M7 impulse response archive' at the bottom of the page");
    rescanButton.setTooltip ("Look for IRs again after adding files");

    for (auto* button : { &downloadButton, &openButton, &chooseButton, &rescanButton })
    {
        button->getProperties().set ("outline", true);
        addAndMakeVisible (*button);
    }
    downloadButton.getProperties().set ("primary", true);
}

void EmptyStateView::setFolder (const juce::File& newFolder)
{
    folder = newFolder;
    repaint();
}

juce::Rectangle<int> EmptyStateView::contentArea() const
{
    return getLocalBounds().withSizeKeepingCentre (contentWidth, contentHeight);
}

void EmptyStateView::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    auto area = contentArea();
    g.setColour (theme::text);
    g.setFont (theme::labelFont (16.0f).withStyle ("Bold"));
    g.drawText ("NO IRS FOUND", area.removeFromTop (28), juce::Justification::centred);

    area.removeFromTop (8);
    g.setColour (theme::textDim);
    g.setFont (juce::FontOptions (13.5f));
    g.drawFittedText ("Lucky Seven plays impulse responses from this folder:", area.removeFromTop (20), juce::Justification::centred, 1);

    g.setColour (theme::text);
    g.setFont (juce::FontOptions (13.0f));
    g.drawFittedText (folder.getFullPathName(), area.removeFromTop (22), juce::Justification::centred, 1, 0.8f);

    area.removeFromTop (6);
    g.setColour (theme::textDim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawFittedText ("Download the free Samplicity Bricasti M7 IRs and unzip the whole archive into it, "
                      "or add your own WAV, AIFF or FLAC files - each subfolder becomes a category.",
                      area.removeFromTop (40), juce::Justification::centred, 3);
}

void EmptyStateView::resized()
{
    auto area = contentArea().withTrimmedTop (140);
    downloadButton.setBounds (area.removeFromTop (buttonHeight).withSizeKeepingCentre (260, buttonHeight));
    area.removeFromTop (12);

    auto row = area.removeFromTop (buttonHeight);
    const auto widths = std::array<int, 3> { 140, 200, 100 };
    const auto gap = 10;
    auto x = row.getCentreX() - (widths[0] + widths[1] + widths[2] + gap * 2) / 2;
    for (auto [button, width] : { std::pair { &openButton, widths[0] }, std::pair { &chooseButton, widths[1] }, std::pair { &rescanButton, widths[2] } })
    {
        button->setBounds (x, row.getY(), width, buttonHeight);
        x += width + gap;
    }
}

} // namespace lucky
