#pragma once

#include "Theme.h"

namespace lucky
{

/** Heart toggle: outlined when off, filled in the slider fill colour when on. */
class HeartButton final : public juce::Button
{
public:
    HeartButton() : juce::Button ("Favourite") { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown) override
    {
        const auto bounds = getLocalBounds().toFloat().withSizeKeepingCentre (14.0f, 13.0f).translated (0.0f, isButtonDown ? 1.0f : 0.0f);
        const auto heart = theme::heartPath (bounds);
        const auto colour = findColour (juce::Slider::rotarySliderFillColourId);

        if (getToggleState())
        {
            g.setColour (colour);
            g.fillPath (heart);
        }
        else
        {
            g.setColour (isMouseOver ? theme::text : theme::textDim);
            g.strokePath (heart, juce::PathStrokeType (1.3f));
        }
    }
};

} // namespace lucky
