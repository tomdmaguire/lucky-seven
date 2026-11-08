#include "EqView.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr float minHz = 20.0f;
constexpr float maxHz = 20000.0f;
constexpr float topDb = 6.0f;
constexpr float bottomDb = -30.0f;
constexpr int filterOrder = 2;
constexpr float handleRadius = 7.0f;
constexpr float pixelsPerOctave = 14.0f;
constexpr int toggleHeight = 20;
constexpr int readoutHeight = 16;

const std::array<float, 9> gridFrequencies { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };

juce::String formatHz (float hertz)
{
    return hertz >= 1000.0f ? juce::String (hertz / 1000.0f, hertz >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (hertz)) + " Hz";
}

bool isOn (const juce::RangedAudioParameter& parameter)
{
    return parameter.getValue() >= 0.5f;
}
} // namespace

EqView::EqView (Parameters params)
    : parameters (params),
      frequencyAttachment (params.frequency, [this] (float value) { centreHz = value; repaint(); }),
      widthAttachment (params.width, [this] (float value) { widthOctaves = value; repaint(); }),
      lowCutAttachment (params.lowCutOn, lowCutButton),
      highCutAttachment (params.highCutOn, highCutButton)
{
    frequencyAttachment.sendInitialUpdate();
    widthAttachment.sendInitialUpdate();

    for (auto* button : { &lowCutButton, &highCutButton })
    {
        styleToggle (*button);
        button->onStateChange = [this] { repaint(); };
        addAndMakeVisible (*button);
    }

    setTooltip ("Drag the handle sideways for frequency, up/down for width. Double-click to reset.");
}

std::pair<float, float> EqView::cornersFor (float centre, float width)
{
    const auto half = std::exp2 (width * 0.5f);
    return { juce::jlimit (minHz, maxHz, centre / half), juce::jlimit (minHz, maxHz, centre * half) };
}

void EqView::styleToggle (juce::TextButton& button)
{
    button.setClickingTogglesState (true);
    button.getProperties().set ("pill", true);
    button.setColour (juce::Slider::rotarySliderFillColourId, accent);
}

void EqView::setAccent (juce::Colour colour)
{
    if (colour == accent)
        return;

    accent = colour;
    for (auto* button : { &lowCutButton, &highCutButton })
    {
        button->setColour (juce::Slider::rotarySliderFillColourId, accent);
        button->repaint();
    }
    repaint();
}

juce::Rectangle<float> EqView::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedTop (toggleHeight + 6.0f).withTrimmedBottom (readoutHeight + 2.0f);
}

float EqView::xForFrequency (float hertz) const
{
    const auto area = plotArea();
    return area.getX() + std::log (hertz / minHz) / std::log (maxHz / minHz) * area.getWidth();
}

float EqView::frequencyForX (float x) const
{
    const auto area = plotArea();
    return minHz * std::pow (maxHz / minHz, (x - area.getX()) / area.getWidth());
}

float EqView::yForDb (float decibels) const
{
    const auto area = plotArea().reduced (0.0f, 4.0f);
    return juce::jmap (decibels, topDb, bottomDb, area.getY(), area.getBottom());
}

float EqView::responseDb (float hertz) const
{
    const auto [low, high] = cornersFor (centreHz, widthOctaves);
    auto power = 1.0f;
    if (isOn (parameters.lowCutOn))
        power *= 1.0f / (1.0f + std::pow (low / hertz, 2.0f * filterOrder));
    if (isOn (parameters.highCutOn))
        power *= 1.0f / (1.0f + std::pow (hertz / high, 2.0f * filterOrder));
    return juce::Decibels::gainToDecibels (std::sqrt (power), bottomDb - 10.0f);
}

juce::Point<float> EqView::handlePosition() const
{
    const auto area = plotArea();
    return { xForFrequency (centreHz), juce::jlimit (area.getY() + handleRadius, area.getBottom() - handleRadius, yForDb (responseDb (centreHz))) };
}

void EqView::paint (juce::Graphics& g)
{
    const auto area = plotArea();
    const auto active = isOn (parameters.lowCutOn) || isOn (parameters.highCutOn);

    g.setColour (theme::background);
    g.fillRoundedRectangle (area, 4.0f);

    for (const auto hertz : gridFrequencies)
    {
        g.setColour (theme::grid);
        g.drawVerticalLine (juce::roundToInt (xForFrequency (hertz)), area.getY(), area.getBottom());
    }
    for (const auto decibels : { 0.0f, -12.0f, -24.0f })
        g.drawHorizontalLine (juce::roundToInt (yForDb (decibels)), area.getX(), area.getRight());

    juce::Path curve;
    for (auto x = area.getX(); x <= area.getRight(); x += 1.0f)
    {
        const juce::Point<float> point { x, juce::jlimit (area.getY(), area.getBottom(), yForDb (responseDb (frequencyForX (x)))) };
        if (curve.isEmpty())
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }

    const auto curveColour = active ? accent : theme::textDim;
    g.setColour (curveColour);
    g.strokePath (curve, juce::PathStrokeType (1.8f));

    const auto handle = handlePosition();
    const auto handleBounds = juce::Rectangle<float> (handleRadius * 2.0f, handleRadius * 2.0f).withCentre (handle);
    if (dragging || hovering)
    {
        g.setColour (curveColour);
        g.fillEllipse (handleBounds);
    }
    else
    {
        g.setColour (theme::background);
        g.fillEllipse (handleBounds);
        g.setColour (curveColour);
        g.drawEllipse (handleBounds.reduced (0.75f), 1.5f);
    }

    auto readouts = getLocalBounds().removeFromBottom (readoutHeight).toFloat();
    g.setFont (theme::labelFont (11.0f));
    g.setColour (active ? accent : theme::textDim);
    g.drawText (formatHz (centreHz), readouts, juce::Justification::centredLeft);
    g.drawText (juce::String (widthOctaves, 1) + " oct", readouts, juce::Justification::centredRight);
}

void EqView::resized()
{
    auto toggles = getLocalBounds().removeFromTop (toggleHeight);
    const auto half = toggles.getWidth() / 2;
    lowCutButton.setBounds (toggles.removeFromLeft (half).withTrimmedRight (3));
    highCutButton.setBounds (toggles.withTrimmedLeft (3));
}

void EqView::mouseMove (const juce::MouseEvent& event)
{
    const auto over = handlePosition().getDistanceFrom (event.position) < handleRadius * 2.0f;
    if (over != hovering)
    {
        hovering = over;
        repaint();
    }
}

void EqView::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    repaint();
}

void EqView::mouseDown (const juce::MouseEvent&)
{
    dragging = true;
    dragStartLog2 = std::log2 (centreHz);
    dragStartWidth = widthOctaves;
    frequencyAttachment.beginGesture();
    widthAttachment.beginGesture();
    repaint();
}

void EqView::mouseDrag (const juce::MouseEvent& event)
{
    const auto octavesPerPixel = std::log2 (maxHz / minHz) / plotArea().getWidth();
    const auto log2Hz = dragStartLog2 + static_cast<float> (event.getDistanceFromDragStartX()) * octavesPerPixel;
    const auto width = dragStartWidth - static_cast<float> (event.getDistanceFromDragStartY()) / pixelsPerOctave;

    frequencyAttachment.setValueAsPartOfGesture (juce::jlimit (minHz, maxHz, std::exp2 (log2Hz)));
    widthAttachment.setValueAsPartOfGesture (parameters.width.getNormalisableRange().snapToLegalValue (width));
}

void EqView::mouseUp (const juce::MouseEvent&)
{
    frequencyAttachment.endGesture();
    widthAttachment.endGesture();
    dragging = false;
    repaint();
}

void EqView::mouseDoubleClick (const juce::MouseEvent&)
{
    frequencyAttachment.setValueAsCompleteGesture (parameters.frequency.convertFrom0to1 (parameters.frequency.getDefaultValue()));
    widthAttachment.setValueAsCompleteGesture (parameters.width.convertFrom0to1 (parameters.width.getDefaultValue()));
}

} // namespace lucky
