#include "SoundMapView.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr float dotRadius = 5.5f;
constexpr float minimumSpacing = 12.0f;
constexpr int relaxIterations = 60;
constexpr float hitRadius = 10.0f;
constexpr int legendHeight = 26;
constexpr float axisMargin = 34.0f;

const std::array<float, 8> secondsTicks { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f };
} // namespace

SoundMapView::SoundMapView (SelectionCallback callback) : onSelect (std::move (callback))
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SoundMapView::setItems (std::vector<BrowserItem> newItems)
{
    items = std::move (newItems);

    auto secondsMin = std::numeric_limits<float>::max(), secondsMax = 0.0f;
    auto brightMin = std::numeric_limits<float>::max(), brightMax = 0.0f;

    for (const auto& item : items)
    {
        if (! item.profile || item.profile->midT60 <= 0.0f || item.profile->brightnessHz <= 0.0f)
            continue;

        secondsMin = std::min (secondsMin, std::log (item.profile->midT60));
        secondsMax = std::max (secondsMax, std::log (item.profile->midT60));
        brightMin = std::min (brightMin, std::log (item.profile->brightnessHz));
        brightMax = std::max (brightMax, std::log (item.profile->brightnessHz));
    }

    if (secondsMax > secondsMin)
    {
        const auto secondsPad = (secondsMax - secondsMin) * 0.05f;
        const auto brightPad = std::max ((brightMax - brightMin) * 0.08f, 0.05f);
        logSecondsRange = { secondsMin - secondsPad, secondsMax + secondsPad };
        logBrightnessRange = { brightMin - brightPad, brightMax + brightPad };
    }

    layoutDots();
    repaint();
}

void SoundMapView::setSelectedId (const juce::String& id)
{
    selectedId = id;
    repaint();
}

void SoundMapView::setDecayRatio (float ratio)
{
    if (! juce::approximatelyEqual (ratio, decayRatio))
    {
        decayRatio = ratio;
        repaint();
    }
}

juce::Rectangle<float> SoundMapView::plotArea() const
{
    auto area = getLocalBounds().toFloat();
    area.removeFromBottom (static_cast<float> (legendHeight));
    area.removeFromBottom (axisMargin * 0.6f);
    area.removeFromLeft (axisMargin);
    return area.reduced (12.0f);
}

float SoundMapView::xForSeconds (float seconds) const
{
    const auto area = plotArea();
    const auto normalised = (std::log (std::max (seconds, 0.01f)) - logSecondsRange.getStart()) / std::max (logSecondsRange.getLength(), 1.0e-3f);
    return area.getX() + normalised * area.getWidth();
}

float SoundMapView::yForBrightness (float hertz) const
{
    const auto area = plotArea();
    const auto normalised = (std::log (std::max (hertz, 1.0f)) - logBrightnessRange.getStart()) / std::max (logBrightnessRange.getLength(), 1.0e-3f);
    return area.getBottom() - normalised * area.getHeight();
}

void SoundMapView::layoutDots()
{
    dots.clear();
    for (const auto& item : items)
    {
        if (! item.profile || item.profile->midT60 <= 0.0f)
            continue;

        const juce::Point<float> target { xForSeconds (item.profile->midT60), yForBrightness (item.profile->brightnessHz) };
        dots.push_back ({ &item, target, target });
    }

    const auto area = plotArea();
    for (int iteration = 0; iteration < relaxIterations; ++iteration)
    {
        for (size_t a = 0; a < dots.size(); ++a)
        {
            for (size_t b = a + 1; b < dots.size(); ++b)
            {
                auto delta = dots[b].position - dots[a].position;
                auto distance = delta.getDistanceFromOrigin();
                if (distance >= minimumSpacing)
                    continue;

                if (distance < 1.0e-3f)
                {
                    delta = { 0.0f, 1.0f };
                    distance = 1.0f;
                }

                const auto push = delta / distance * ((minimumSpacing - distance) * 0.5f);
                dots[a].position -= push;
                dots[b].position += push;
            }
        }

        for (auto& dot : dots)
        {
            dot.position += (dot.target - dot.position) * 0.05f;
            dot.position = area.getConstrainedPoint (dot.position);
        }
    }
}

int SoundMapView::dotAt (juce::Point<float> point) const
{
    auto best = -1;
    auto bestDistance = hitRadius;
    for (size_t i = 0; i < dots.size(); ++i)
    {
        const auto distance = dots[i].position.getDistanceFrom (point);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = static_cast<int> (i);
        }
    }
    return best;
}

int SoundMapView::selectedDot() const
{
    for (size_t i = 0; i < dots.size(); ++i)
        if (dots[i].item->entry.id() == selectedId)
            return static_cast<int> (i);
    return -1;
}

void SoundMapView::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    const auto area = plotArea();

    if (dots.empty())
    {
        g.setColour (theme::textDim);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText (items.empty() ? "No IRs found - choose the IR folder above" : "Analysing IRs...",
                    getLocalBounds(), juce::Justification::centred);
        return;
    }

    paintAxes (g, area);

    for (size_t i = 0; i < dots.size(); ++i)
    {
        const auto& dot = dots[i];
        const auto hovered = static_cast<int> (i) == hoveredDot;
        const auto colour = theme::categoryColour (dot.item->entry.categoryIndex);
        const auto radius = hovered ? dotRadius + 1.5f : dotRadius;
        g.setColour (colour.withAlpha (hovered ? 1.0f : 0.8f));
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (dot.position));
    }

    if (const auto selected = selectedDot(); selected >= 0)
    {
        const auto& dot = dots[static_cast<size_t> (selected)];
        const auto ringBounds = juce::Rectangle<float> (dotRadius * 4.0f, dotRadius * 4.0f).withCentre (dot.position);
        g.setColour (theme::text);
        g.drawEllipse (ringBounds, 2.0f);

        if (decayRatio < 0.999f && dot.item->profile)
        {
            const auto shift = xForSeconds (dot.item->profile->midT60 * decayRatio) - xForSeconds (dot.item->profile->midT60);
            const auto ghost = dot.position.translated (shift, 0.0f);
            g.setColour (theme::accent.withAlpha (0.7f));
            const float dashes[] { 4.0f, 3.0f };
            g.drawDashedLine ({ dot.position.translated (-dotRadius * 2.0f, 0.0f), ghost.translated (dotRadius * 1.5f, 0.0f) }, dashes, 2, 1.5f);
            g.drawEllipse (juce::Rectangle<float> (dotRadius * 3.0f, dotRadius * 3.0f).withCentre (ghost), 1.5f);
        }

        g.setColour (theme::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        const auto label = dot.item->entry.name;
        const auto labelWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label) + 8.0f;
        auto labelBounds = juce::Rectangle<float> (labelWidth, 18.0f).withCentre (dot.position.translated (0.0f, -dotRadius * 2.0f - 12.0f));
        labelBounds = labelBounds.constrainedWithin (getLocalBounds().toFloat());
        g.drawText (label, labelBounds, juce::Justification::centred);
    }

    paintLegend (g);
}

void SoundMapView::paintAxes (juce::Graphics& g, juce::Rectangle<float> area) const
{
    g.setFont (juce::FontOptions (11.0f));

    for (const auto seconds : secondsTicks)
    {
        const auto x = xForSeconds (seconds);
        if (x < area.getX() || x > area.getRight())
            continue;

        g.setColour (theme::outline.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
        g.setColour (theme::textDim);
        g.drawText (theme::formatSeconds (seconds), juce::Rectangle<float> (60.0f, 14.0f).withCentre ({ x, area.getBottom() + 14.0f }),
                    juce::Justification::centred);
    }

    g.setColour (theme::textDim);
    g.drawText ("decay", juce::Rectangle<float> (area.getRight() - 60.0f, area.getBottom() + 7.0f, 60.0f, 14.0f), juce::Justification::centredRight);

    const auto labelArea = juce::Rectangle<float> (4.0f, area.getY(), axisMargin, area.getHeight());
    g.drawText ("bright", labelArea.withHeight (14.0f), juce::Justification::centredLeft);
    g.drawText ("dark", labelArea.withTrimmedTop (area.getHeight() - 14.0f), juce::Justification::centredLeft);
}

void SoundMapView::paintLegend (juce::Graphics& g) const
{
    std::vector<std::pair<int, juce::String>> categories;
    for (const auto& item : items)
        if (categories.empty() || categories.back().first != item.entry.categoryIndex)
            categories.emplace_back (item.entry.categoryIndex, item.entry.category);

    auto legend = getLocalBounds().removeFromBottom (legendHeight).reduced (12, 4);
    g.setFont (juce::FontOptions (12.0f));
    const auto slotWidth = categories.empty() ? 0 : legend.getWidth() / static_cast<int> (categories.size());

    for (const auto& [index, name] : categories)
    {
        auto slot = legend.removeFromLeft (slotWidth);
        g.setColour (theme::categoryColour (index));
        g.fillEllipse (slot.removeFromLeft (14).withSizeKeepingCentre (8, 8).toFloat());
        g.setColour (theme::textDim);
        g.drawText (name, slot, juce::Justification::centredLeft);
    }
}

void SoundMapView::resized()
{
    layoutDots();
}

void SoundMapView::mouseMove (const juce::MouseEvent& event)
{
    const auto dot = dotAt (event.position);
    if (dot != hoveredDot)
    {
        hoveredDot = dot;
        repaint();
    }
}

void SoundMapView::mouseExit (const juce::MouseEvent&)
{
    hoveredDot = -1;
    repaint();
}

void SoundMapView::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    if (const auto dot = dotAt (event.position); dot >= 0)
    {
        selectedId = dots[static_cast<size_t> (dot)].item->entry.id();
        onSelect (selectedId);
        repaint();
    }
}

bool SoundMapView::keyPressed (const juce::KeyPress& key)
{
    juce::Point<float> direction;
    if (key == juce::KeyPress::leftKey)
        direction = { -1.0f, 0.0f };
    else if (key == juce::KeyPress::rightKey)
        direction = { 1.0f, 0.0f };
    else if (key == juce::KeyPress::upKey)
        direction = { 0.0f, -1.0f };
    else if (key == juce::KeyPress::downKey)
        direction = { 0.0f, 1.0f };
    else
        return false;

    const auto current = selectedDot();
    if (current < 0)
        return true;

    const auto origin = dots[static_cast<size_t> (current)].position;
    auto best = -1;
    auto bestScore = std::numeric_limits<float>::max();

    for (size_t i = 0; i < dots.size(); ++i)
    {
        const auto delta = dots[i].position - origin;
        const auto along = delta.x * direction.x + delta.y * direction.y;
        if (static_cast<int> (i) == current || along <= 0.5f)
            continue;

        const auto across = std::abs (delta.x * direction.y - delta.y * direction.x);
        const auto score = along + across * 2.0f;
        if (score < bestScore)
        {
            bestScore = score;
            best = static_cast<int> (i);
        }
    }

    if (best >= 0)
    {
        selectedId = dots[static_cast<size_t> (best)].item->entry.id();
        onSelect (selectedId);
        repaint();
    }

    return true;
}

juce::String SoundMapView::getTooltip()
{
    if (hoveredDot < 0 || hoveredDot >= static_cast<int> (dots.size()))
        return {};

    const auto& item = *dots[static_cast<size_t> (hoveredDot)].item;
    return item.entry.category + " " + juce::String (item.entry.number).paddedLeft ('0', 2) + "  " + item.entry.name
         + "\n" + theme::formatSeconds (item.profile->midT60) + " decay";
}

} // namespace lucky
