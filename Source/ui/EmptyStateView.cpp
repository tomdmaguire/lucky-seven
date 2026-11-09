#include "EmptyStateView.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr int contentWidth = 540;
constexpr int contentHeight = 290;
constexpr int buttonHeight = 30;
constexpr int primaryWidth = 300;
constexpr int cancelWidth = 80;
} // namespace

EmptyStateView::EmptyStateView (Actions actions)
{
    downloadButton.onClick = actions.download;
    cancelButton.onClick = actions.cancelDownload;
    openButton.onClick = actions.openFolder;
    chooseButton.onClick = actions.chooseFolder;
    rescanButton.onClick = actions.rescan;

    downloadButton.setTooltip ("Downloads Samplicity's free Bricasti M7 archive and unpacks it into the IR folder");
    rescanButton.setTooltip ("Look for IRs again after adding files");

    for (auto* button : { &downloadButton, &cancelButton, &openButton, &chooseButton, &rescanButton })
    {
        button->getProperties().set ("outline", true);
        addAndMakeVisible (*button);
    }
    downloadButton.getProperties().set ("primary", true);
    setDownloadStatus ({});
}

void EmptyStateView::setFolder (const juce::File& newFolder)
{
    folder = newFolder;
    repaint();
}

bool EmptyStateView::downloading() const
{
    return downloadStatus.stage == IrDownloader::Stage::downloading || downloadStatus.stage == IrDownloader::Stage::unpacking;
}

void EmptyStateView::setDownloadStatus (const IrDownloader::Status& status)
{
    downloadStatus = status;
    downloadButton.setButtonText (status.stage == IrDownloader::Stage::failed
                                      ? juce::String ("TRY AGAIN")
                                      : "DOWNLOAD BRICASTI IRS (" + juce::String (IrDownloader::bricastiArchiveMegabytes) + " MB)");
    downloadButton.setVisible (! downloading());
    cancelButton.setVisible (downloading());
    resized();
    repaint();
}

juce::Rectangle<int> EmptyStateView::contentArea() const
{
    return getLocalBounds().withSizeKeepingCentre (contentWidth, contentHeight);
}

juce::Rectangle<int> EmptyStateView::primaryArea() const
{
    return contentArea().withTrimmedTop (150).removeFromTop (buttonHeight);
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
    g.drawFittedText ("Download Samplicity's free Bricasti M7 IRs, or add a folder of your own.",
                      area.removeFromTop (40), juce::Justification::centred, 3);

    const auto primary = primaryArea();
    if (downloading())
    {
        const auto bar = primary.withTrimmedRight (cancelWidth + 10).toFloat().withSizeKeepingCentre (static_cast<float> (primary.getWidth() - cancelWidth - 10), 8.0f);
        g.setColour (theme::panelRaised);
        g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, downloadStatus.progress)), 4.0f);
    }

    if (downloading() || downloadStatus.stage == IrDownloader::Stage::failed)
    {
        g.setColour (downloadStatus.stage == IrDownloader::Stage::failed ? theme::categoryColour (2) : theme::textDim);
        g.setFont (juce::FontOptions (12.5f));
        g.drawFittedText (downloadStatus.detail, primary.translated (0, buttonHeight + 4).withHeight (18), juce::Justification::centred, 1);
    }
}

void EmptyStateView::resized()
{
    const auto primary = primaryArea();
    downloadButton.setBounds (primary.withSizeKeepingCentre (primaryWidth, buttonHeight));
    cancelButton.setBounds (primary.withLeft (primary.getRight() - cancelWidth));

    auto row = primary.translated (0, buttonHeight + 34);
    const auto widths = std::array<int, 3> { 140, 150, 100 };
    const auto gap = 10;
    auto x = row.getCentreX() - (widths[0] + widths[1] + widths[2] + gap * 2) / 2;
    for (auto [button, width] : { std::pair { &openButton, widths[0] }, std::pair { &chooseButton, widths[1] }, std::pair { &rescanButton, widths[2] } })
    {
        button->setBounds (x, row.getY(), width, buttonHeight);
        x += width + gap;
    }
}

} // namespace lucky
