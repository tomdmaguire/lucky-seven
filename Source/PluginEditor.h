#pragma once

#include "PluginProcessor.h"
#include "ui/BrowserPanel.h"
#include "ui/LookAndFeel.h"
#include "ui/Theme.h"

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
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        juce::Label label, minLabel, maxLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshItems();
    void chooseFolder();
    void stepProgram (int delta);
    void setView (lucky::BrowserPanel::View view);
    void applyAccent();
    void setupLabel (juce::Label& label, const juce::String& text);
    void setupKnob (Knob& knob, const juce::String& parameterId, const juce::String& name);
    void setupRangeLabels (Knob& knob, const juce::String& minText, const juce::String& maxText);
    const lucky::BrowserItem* selectedItem() const;

    LuckySevenProcessor& audioProcessor;
    lucky::LuckyLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 400 };
    lucky::BrowserPanel browser;
    juce::TextButton mapTab { "MAP" }, listTab { "LIST" };
    juce::TextButton previousButton { juce::String::fromUTF8 ("\xe2\x80\xb9") }, nextButton { juce::String::fromUTF8 ("\xe2\x80\xba") };
    juce::DrawableButton folderButton { "IR folder", juce::DrawableButton::ImageFitted };
    Knob predelay, decay, lowCut, highCut, mix;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<lucky::BrowserItem> items;
    juce::String shownSelection;
    float shownOriginalT60 = -1.0f;
    juce::Colour accent { lucky::theme::accent };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckySevenEditor)
};
