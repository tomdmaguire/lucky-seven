#pragma once

#include "PluginProcessor.h"
#include "ui/BrowserPanel.h"
#include "ui/LookAndFeel.h"

class LuckySevenEditor final : public juce::AudioProcessorEditor,
                               private juce::ChangeListener,
                               private juce::Timer
{
public:
    explicit LuckySevenEditor (LuckySevenProcessor&);
    ~LuckySevenEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshItems();
    void chooseFolder();
    void setupKnob (Knob& knob, const juce::String& parameterId, const juce::String& name);
    juce::String headerText() const;

    LuckySevenProcessor& audioProcessor;
    lucky::LuckyLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 400 };
    lucky::BrowserPanel browser;
    juce::TextButton folderButton { "IR folder..." };
    Knob predelay, decay, mix;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<lucky::BrowserItem> items;
    juce::String shownSelection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckySevenEditor)
};
