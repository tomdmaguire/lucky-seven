#include "LookAndFeel.h"
#include "Theme.h"

namespace lucky
{

LuckyLookAndFeel::LuckyLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, theme::background);
    setColour (juce::ListBox::backgroundColourId, theme::panel);
    setColour (juce::ListBox::outlineColourId, theme::outline);
    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::TextButton::buttonColourId, theme::panelRaised);
    setColour (juce::TextButton::buttonOnColourId, theme::accent);
    setColour (juce::TextButton::textColourOffId, theme::textDim);
    setColour (juce::TextButton::textColourOnId, theme::background);
    setColour (juce::ScrollBar::thumbColourId, theme::outline);
}

void LuckyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const auto radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto thickness = 4.0f;
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - thickness, radius - thickness, 0.0f, startAngle, endAngle, true);
    g.setColour (theme::outline);
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - thickness, radius - thickness, 0.0f, startAngle, angle, true);
    g.setColour (theme::accent);
    g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto knobRadius = radius - thickness * 3.0f;
    g.setColour (theme::panelRaised);
    g.fillEllipse (juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre));

    const auto pointer = centre.getPointOnCircumference (knobRadius * 0.75f, angle);
    g.setColour (theme::text);
    g.drawLine (juce::Line<float> (centre.getPointOnCircumference (knobRadius * 0.25f, angle), pointer), 2.5f);
}

void LuckyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                             bool isMouseOver, bool isButtonDown)
{
    auto colour = button.getToggleState() ? button.findColour (juce::TextButton::buttonOnColourId) : backgroundColour;
    if (isButtonDown)
        colour = colour.darker (0.2f);
    else if (isMouseOver)
        colour = colour.brighter (0.1f);

    g.setColour (colour);
    g.fillRoundedRectangle (button.getLocalBounds().toFloat().reduced (1.0f), 6.0f);
}

juce::Font LuckyLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions (std::min (14.0f, static_cast<float> (buttonHeight) * 0.55f));
}

} // namespace lucky
