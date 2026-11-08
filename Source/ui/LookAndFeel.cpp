#include "LookAndFeel.h"
#include "Theme.h"
#include <BinaryData.h>

namespace lucky
{

LuckyLookAndFeel::LuckyLookAndFeel()
    : regular (juce::Typeface::createSystemTypefaceFor (LuckySevenFonts::BarlowRegular_ttf, LuckySevenFonts::BarlowRegular_ttfSize)),
      medium (juce::Typeface::createSystemTypefaceFor (LuckySevenFonts::BarlowMedium_ttf, LuckySevenFonts::BarlowMedium_ttfSize)),
      semiBold (juce::Typeface::createSystemTypefaceFor (LuckySevenFonts::BarlowSemiBold_ttf, LuckySevenFonts::BarlowSemiBold_ttfSize))
{
    juce::LookAndFeel::setDefaultLookAndFeel (this);
    juce::Typeface::clearTypefaceCache();

    setColour (juce::ResizableWindow::backgroundColourId, theme::background);
    setColour (juce::ListBox::backgroundColourId, theme::background);
    setColour (juce::ListBox::outlineColourId, theme::outline);
    setColour (juce::Slider::rotarySliderFillColourId, theme::accent);
    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::textColourOffId, theme::textDim);
    setColour (juce::TextButton::textColourOnId, theme::text);
    setColour (juce::ScrollBar::thumbColourId, theme::outline);
    setColour (juce::TooltipWindow::backgroundColourId, theme::panelRaised);
    setColour (juce::TooltipWindow::textColourId, theme::text);
    setColour (juce::TooltipWindow::outlineColourId, theme::outline);
}

LuckyLookAndFeel::~LuckyLookAndFeel()
{
    if (&juce::LookAndFeel::getDefaultLookAndFeel() == this)
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    juce::Typeface::clearTypefaceCache();
}

juce::Typeface::Ptr LuckyLookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    if (font.getTypefaceName() != juce::Font::getDefaultSansSerifFontName())
        return juce::LookAndFeel_V4::getTypefaceForFont (font);

    if (font.isBold())
        return semiBold;
    if (font.getTypefaceStyle().containsIgnoreCase ("Medium"))
        return medium;
    return regular;
}

void LuckyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto cut = slider.getProperties()["cut"].toString();
    const auto fillsFromEnd = cut == "high";
    const auto isOff = (cut == "low" && sliderPos <= 0.001f) || (fillsFromEnd && sliderPos >= 0.999f);
    const auto colour = slider.isEnabled() && ! isOff ? slider.findColour (juce::Slider::rotarySliderFillColourId) : theme::textDim.withAlpha (0.5f);
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);

    g.setColour (colour.withAlpha (0.07f));
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));

    juce::Path ring;
    ring.addCentredArc (centre.x, centre.y, radius - 1.0f, radius - 1.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (colour.withAlpha (0.25f));
    g.strokePath (ring, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - 1.0f, radius - 1.0f, 0.0f,
                         fillsFromEnd ? angle : startAngle, fillsFromEnd ? endAngle : angle, true);
    g.setColour (colour);
    g.strokePath (value, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto tip = centre.getPointOnCircumference (radius - 1.0f, angle);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (tip));

    g.setColour (isOff ? theme::textDim : theme::text);
    g.setFont (theme::labelFont (13.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()), bounds.reduced (radius * 0.2f), juce::Justification::centred);
}

void LuckyLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool isMouseOver, bool)
{
    if (button.getProperties()["outline"])
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.75f);
        const auto colour = button.findColour (juce::Slider::rotarySliderFillColourId);
        if (button.getProperties()["primary"])
        {
            g.setColour (isMouseOver ? colour.brighter (0.15f) : colour);
            g.fillRoundedRectangle (bounds, 4.0f);
        }
        else
        {
            g.setColour (isMouseOver ? theme::text.withAlpha (0.06f) : juce::Colours::transparentBlack);
            g.fillRoundedRectangle (bounds, 4.0f);
            g.setColour (theme::outline.brighter (isMouseOver ? 0.4f : 0.15f));
            g.drawRoundedRectangle (bounds, 4.0f, 1.0f);
        }
        return;
    }

    if (button.getProperties()["round"])
    {
        if (isMouseOver)
        {
            g.setColour (theme::text.withAlpha (0.08f));
            g.fillEllipse (button.getLocalBounds().toFloat().reduced (2.0f));
        }
        return;
    }

    if (button.getToggleState())
    {
        g.setColour (button.findColour (juce::Slider::rotarySliderFillColourId));
        g.fillRect (button.getLocalBounds().removeFromBottom (2));
    }
    else if (isMouseOver)
    {
        g.setColour (theme::text.withAlpha (0.04f));
        g.fillRect (button.getLocalBounds());
    }
}

juce::Font LuckyLookAndFeel::getTextButtonFont (juce::TextButton& button, int)
{
    const auto& properties = button.getProperties();
    return theme::labelFont (properties["small"] || properties["outline"] ? 11.0f : 13.0f);
}

void LuckyLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool isMouseOver, bool isButtonDown)
{
    if (! button.getProperties()["outline"])
    {
        juce::LookAndFeel_V4::drawButtonText (g, button, isMouseOver, isButtonDown);
        return;
    }

    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (button.getProperties()["primary"] ? theme::background : theme::text);
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
}

} // namespace lucky
