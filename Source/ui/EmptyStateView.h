#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** First-run panel shown when the IR folder has no usable files: explains where IRs go and offers the next steps. */
class EmptyStateView final : public juce::Component
{
public:
    struct Actions
    {
        std::function<void()> openFolder, chooseFolder, rescan;
    };

    explicit EmptyStateView (Actions actions);

    /** Folder the plugin is currently scanning, shown in the explanation. */
    void setFolder (const juce::File& folder);

    void paint (juce::Graphics&) override;
    void resized() override;

    static inline const juce::URL downloadUrl { "https://www.samplicity.com/downloads/" };

private:
    juce::Rectangle<int> contentArea() const;

    juce::File folder;
    juce::TextButton downloadButton { "GET THE FREE BRICASTI IRS" }, openButton { "OPEN IR FOLDER" },
        chooseButton { "CHOOSE ANOTHER FOLDER..." }, rescanButton { "RESCAN" };
};

} // namespace lucky
