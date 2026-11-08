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
constexpr float minWidthOctaves = 1.0f;
constexpr int filterOrder = 4;
constexpr float dotRadius = 7.0f;
constexpr float pixelsPerOctaveOfWidth = 12.0f;

const float maxWidthOctaves = std::log2 (maxHz / minHz);
const std::array<float, 9> gridFrequencies { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };

juce::String formatHz (float hertz)
{
    return hertz >= 1000.0f ? juce::String (hertz / 1000.0f, hertz >= 10000.0f ? 0 : 1) + "k" : juce::String (juce::roundToInt (hertz));
}
} // namespace

EqView::EqView (juce::RangedAudioParameter& lowCut, juce::RangedAudioParameter& highCut)
    : lowParameter (lowCut),
      highParameter (highCut),
      lowAttachment (lowCut, [this] (float value) { lowHz = value; repaint(); }),
      highAttachment (highCut, [this] (float value) { highHz = value; repaint(); })
{
    lowAttachment.sendInitialUpdate();
    highAttachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Drag sideways to move the band, up/down to widen or narrow it. Double-click to reset.");
}

juce::Rectangle<float> EqView::plotArea() const
{
    return getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (14.0f);
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
    const auto area = plotArea();
    return juce::jmap (decibels, topDb, bottomDb, area.getY(), area.getBottom());
}

float EqView::responseDb (float hertz) const
{
    const auto highPass = 1.0f / (1.0f + std::pow (lowHz / hertz, 2.0f * filterOrder));
    const auto lowPass = 1.0f / (1.0f + std::pow (hertz / highHz, 2.0f * filterOrder));
    return juce::Decibels::gainToDecibels (std::sqrt (highPass * lowPass), bottomDb - 10.0f);
}

juce::Point<float> EqView::dotPosition() const
{
    const auto area = plotArea();
    const auto centre = std::sqrt (lowHz * highHz);
    const auto width = std::log2 (highHz / lowHz);
    const auto y = juce::jmap (width, minWidthOctaves, maxWidthOctaves, area.getBottom() - dotRadius * 2.0f, yForDb (0.0f) + dotRadius * 2.0f);
    return { xForFrequency (centre), y };
}

void EqView::paint (juce::Graphics& g)
{
    const auto area = plotArea();
    g.setColour (theme::background);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    g.setFont (juce::FontOptions (10.0f));
    for (const auto hertz : gridFrequencies)
    {
        const auto x = xForFrequency (hertz);
        g.setColour (theme::outline.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
        if (hertz == 100.0f || hertz == 1000.0f || hertz == 10000.0f)
        {
            g.setColour (theme::textDim);
            g.drawText (formatHz (hertz), juce::Rectangle<float> (30.0f, 12.0f).withCentre ({ x, area.getBottom() + 7.0f }), juce::Justification::centred);
        }
    }

    for (const auto decibels : { 0.0f, -12.0f, -24.0f })
    {
        g.setColour (theme::outline.withAlpha (decibels == 0.0f ? 0.9f : 0.5f));
        g.drawHorizontalLine (juce::roundToInt (yForDb (decibels)), area.getX(), area.getRight());
    }

    juce::Path curve;
    for (auto x = area.getX(); x <= area.getRight(); x += 1.0f)
    {
        const juce::Point<float> point { x, juce::jlimit (area.getY(), area.getBottom(), yForDb (responseDb (frequencyForX (x)))) };
        if (curve.isEmpty())
            curve.startNewSubPath (point);
        else
            curve.lineTo (point);
    }

    auto fill = curve;
    fill.lineTo (area.getBottomRight());
    fill.lineTo (area.getBottomLeft());
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.1f));
    g.fillPath (fill);

    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (1.8f));

    const auto dot = dotPosition();
    g.setColour (accent.withAlpha (0.2f));
    g.fillEllipse (juce::Rectangle<float> (dotRadius * 3.0f, dotRadius * 3.0f).withCentre (dot));
    g.setColour (dragging ? accent.brighter (0.3f) : accent);
    g.fillEllipse (juce::Rectangle<float> (dotRadius * 2.0f, dotRadius * 2.0f).withCentre (dot));

    g.setColour (theme::textDim);
    g.setFont (theme::labelFont (10.0f));
    const auto readout = (lowHz <= minHz + 0.5f ? juce::String ("--") : formatHz (lowHz)) + " - "
                       + (highHz >= maxHz - 1.0f ? juce::String ("--") : formatHz (highHz)) + " Hz";
    g.drawText (readout, area.reduced (6.0f, 3.0f), juce::Justification::topRight);
}

void EqView::setAccent (juce::Colour colour)
{
    if (colour != accent)
    {
        accent = colour;
        repaint();
    }
}

void EqView::setBand (float centreLog2, float widthOctaves)
{
    const auto minLog = std::log2 (minHz), maxLog = std::log2 (maxHz);
    const auto width = juce::jlimit (minWidthOctaves, maxWidthOctaves, widthOctaves);
    const auto centre = juce::jlimit (minLog + width * 0.5f, maxLog - width * 0.5f, centreLog2);

    lowAttachment.setValueAsPartOfGesture (std::exp2 (centre - width * 0.5f));
    highAttachment.setValueAsPartOfGesture (std::exp2 (centre + width * 0.5f));
}

void EqView::mouseDown (const juce::MouseEvent&)
{
    dragging = true;
    dragStartCentre = 0.5f * (std::log2 (lowHz) + std::log2 (highHz));
    dragStartWidth = std::log2 (highHz / lowHz);
    lowAttachment.beginGesture();
    highAttachment.beginGesture();
    repaint();
}

void EqView::mouseDrag (const juce::MouseEvent& event)
{
    const auto area = plotArea();
    const auto octavesPerPixel = std::log2 (maxHz / minHz) / area.getWidth();
    const auto centre = dragStartCentre + static_cast<float> (event.getDistanceFromDragStartX()) * octavesPerPixel;
    const auto width = dragStartWidth - static_cast<float> (event.getDistanceFromDragStartY()) / pixelsPerOctaveOfWidth;
    setBand (centre, width);
}

void EqView::mouseUp (const juce::MouseEvent&)
{
    lowAttachment.endGesture();
    highAttachment.endGesture();
    dragging = false;
    repaint();
}

void EqView::mouseDoubleClick (const juce::MouseEvent&)
{
    lowAttachment.setValueAsCompleteGesture (lowParameter.convertFrom0to1 (lowParameter.getDefaultValue()));
    highAttachment.setValueAsCompleteGesture (highParameter.convertFrom0to1 (highParameter.getDefaultValue()));
}

void EqView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const auto centre = 0.5f * (std::log2 (lowHz) + std::log2 (highHz));
    const auto width = std::log2 (highHz / lowHz) + wheel.deltaY * 4.0f;
    lowAttachment.beginGesture();
    highAttachment.beginGesture();
    setBand (centre, width);
    lowAttachment.endGesture();
    highAttachment.endGesture();
}

} // namespace lucky
