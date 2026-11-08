#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** Flat dark look with arc-style rotary knobs and pill buttons. */
class LuckyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LuckyLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOver, bool isButtonDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isMouseOver, bool isButtonDown) override;
};

} // namespace lucky
