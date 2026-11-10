#pragma once

#include "PluginProcessor.h"
#include "ui/BrowserPanel.h"
#include "ui/HeartButton.h"
#include "ui/LookAndFeel.h"
#include "ui/Theme.h"

/** The whole plugin interface, laid out at a fixed 900 x 600; LuckySevenEditor scales it to the window size. */
class LuckySevenView final : public juce::Component,
                             private juce::ChangeListener,
                             private juce::Timer
{
public:
    static constexpr int baseWidth = 900;
    static constexpr int baseHeight = 600;

    explicit LuckySevenView (LuckySevenProcessor&);
    ~LuckySevenView() override;

    /** Called when the user picks a window size from the menu; 1.0 is 900 x 600. */
    std::function<void (float scale)> onScaleRequested;

    /** Current window scale, used to tick the matching menu item. */
    std::function<float()> getScale;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Tab switches between the sound map and the program list. */
    bool keyPressed (const juce::KeyPress&) override;

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
    void showMenu();
    void stepProgram (int delta);
    void setView (lucky::BrowserPanel::View view);
    void showOutputMode (int mode);
    void applyAccent();
    void setupLabel (juce::Label& label, const juce::String& text);
    void setupKnob (Knob& knob, const juce::String& parameterId, const juce::String& name);
    void setupRangeLabels (Knob& knob, const juce::String& minText, const juce::String& maxText);
    const lucky::BrowserItem* selectedItem() const;
    static void paintSegmentSeven (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour);

    LuckySevenProcessor& audioProcessor;
    juce::SharedResourcePointer<lucky::LuckyLookAndFeel> lookAndFeel;
    juce::TooltipWindow tooltips { this, 400 };
    lucky::BrowserPanel browser;
    juce::TextButton mapTab { "MAP" }, listTab { "LIST" };
    juce::TextButton previousButton { juce::String::fromUTF8 ("\xe2\x80\xb9") }, nextButton { juce::String::fromUTF8 ("\xe2\x80\xba") };
    juce::TextButton programButton, logoButton;
    lucky::HeartButton favouriteButton;
    juce::DrawableButton menuButton { "Menu", juce::DrawableButton::ImageFitted };
    Knob predelay, decay, lowCut, damping, width, mix;
    std::array<juce::TextButton, 3> modeButtons;
    std::array<juce::TextButton, 2> slotButtons;
    std::unique_ptr<juce::ParameterAttachment> modeAttachment;
    int outputMode = -1;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<lucky::BrowserItem> items;
    juce::String shownSelection;
    float shownOriginalT60 = -1.0f;
    bool initialFocusGiven = false;
    juce::Colour accent { lucky::theme::accent };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckySevenView)
};

/** Resizable host window: keeps a 3:2 aspect ratio and scales LuckySevenView to fit, remembering the size globally. */
class LuckySevenEditor final : public juce::AudioProcessorEditor
{
public:
    explicit LuckySevenEditor (LuckySevenProcessor&);

    void resized() override;

private:
    float currentScale() const;

    LuckySevenProcessor& audioProcessor;
    LuckySevenView view;
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckySevenEditor)
};
