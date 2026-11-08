#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** Flat dark look with arc-style rotary knobs and the bundled Barlow typeface.
    Shared by every open editor via SharedResourcePointer, and installed as the default look while alive
    so that menus, tooltips and every default-named font also render in Barlow. */
class LuckyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LuckyLookAndFeel();
    ~LuckyLookAndFeel() override;

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOver, bool isButtonDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOver, bool isButtonDown) override;

private:
    juce::Typeface::Ptr regular, medium, semiBold;
};

} // namespace lucky
