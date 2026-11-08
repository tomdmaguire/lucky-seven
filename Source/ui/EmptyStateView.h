#pragma once

#include "ir/IrDownloader.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** First-run panel shown when the IR folder has no usable files: offers a one-click Bricasti download and the manual routes. */
class EmptyStateView final : public juce::Component
{
public:
    struct Actions
    {
        std::function<void()> download, cancelDownload, openFolder, chooseFolder, rescan;
    };

    explicit EmptyStateView (Actions actions);

    /** Folder the plugin is currently scanning, shown in the explanation. */
    void setFolder (const juce::File& folder);

    /** Shows download progress, or the error from a failed attempt. */
    void setDownloadStatus (const IrDownloader::Status& status);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Rectangle<int> contentArea() const;
    juce::Rectangle<int> primaryArea() const;
    bool downloading() const;

    juce::File folder;
    IrDownloader::Status downloadStatus;
    juce::TextButton downloadButton, cancelButton { "CANCEL" }, openButton { "OPEN IR FOLDER" },
        chooseButton { "CHOOSE ANOTHER FOLDER..." }, rescanButton { "RESCAN" };
};

} // namespace lucky
