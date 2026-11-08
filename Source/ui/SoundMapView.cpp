#include "SoundMapView.h"
#include "Theme.h"

namespace lucky
{

namespace
{
constexpr float dotRadius = 4.0f;
constexpr float glowRadius = 9.0f;
constexpr float selectionRingRadius = 17.0f;
constexpr float minimumSpacing = 11.0f;
constexpr int relaxIterations = 60;
constexpr float hitRadius = 10.0f;
constexpr int legendHeight = 30;
constexpr float axisMargin = 40.0f;

constexpr float rankWeight = 0.7f;
constexpr float edgePadding = 0.03f;
constexpr float minimumTickSpacing = 44.0f;

constexpr int activityFrameRate = 30;
constexpr float activityDt = 1.0f / activityFrameRate;
constexpr size_t maxRipples = 8;
constexpr float floorDb = -54.0f;
constexpr float onsetJump = 0.1f;
constexpr float minRippleGapSeconds = 0.09f;

/** Maps a linear peak onto 0..1 over the last 54 dB. */
float normalisedLevel (float peak)
{
    return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (peak, floorDb) - floorDb) / -floorDb);
}

const std::array<float, 12> secondsTicks { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 6.0f, 8.0f, 16.0f };
} // namespace

SoundMapView::SoundMapView (SelectionCallback callback) : onSelect (std::move (callback))
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SoundMapView::setItems (std::vector<BrowserItem> newItems)
{
    items = std::move (newItems);

    std::vector<float> logSeconds, logBrightness;
    for (const auto& item : items)
    {
        if (! item.profile || item.profile->midT60 <= 0.0f || item.profile->brightnessHz <= 0.0f)
            continue;

        logSeconds.push_back (std::log (item.profile->midT60));
        logBrightness.push_back (std::log (item.profile->brightnessHz));
    }

    secondsScale.build (std::move (logSeconds));
    brightnessScale.build (std::move (logBrightness));

    layoutDots();
    repaint();
}

void SoundMapView::setSelectedId (const juce::String& id)
{
    selectedId = id;
    repaint();
}

void SoundMapView::setLevelSource (std::function<std::pair<float, float>()> source)
{
    levelSource = std::move (source);
    updateActivityTimer();
}

void SoundMapView::visibilityChanged()
{
    updateActivityTimer();
}

void SoundMapView::updateActivityTimer()
{
    if (levelSource != nullptr && isVisible())
        startTimerHz (activityFrameRate);
    else
        stopTimer();
}

float SoundMapView::effectiveDecaySeconds() const
{
    const auto selected = selectedDot();
    if (selected < 0 || ! dots[static_cast<size_t> (selected)].item->profile)
        return 1.0f;
    return dots[static_cast<size_t> (selected)].item->profile->midT60 * decayRatio;
}

void SoundMapView::timerCallback()
{
    const auto [inputPeak, wetPeak] = levelSource();
    const auto input = normalisedLevel (inputPeak);
    const auto wet = normalisedLevel (wetPeak);
    const auto wasActive = glow > 0.001f || ! ripples.empty();

    glow = wet > glow ? glow + (wet - glow) * 0.6f : glow * 0.9f;
    if (glow < 0.002f)
        glow = 0.0f;

    secondsSinceRipple += activityDt;
    if (input - inputEnvelope > onsetJump && input > 0.2f && secondsSinceRipple > minRippleGapSeconds)
    {
        if (ripples.size() >= maxRipples)
            ripples.erase (ripples.begin());

        ripples.push_back ({ 0.0f, juce::jlimit (0.35f, 6.0f, effectiveDecaySeconds()), input });
        secondsSinceRipple = 0.0f;
    }
    inputEnvelope = std::max (input, inputEnvelope * 0.82f);

    for (auto& ripple : ripples)
        ripple.age += activityDt;
    ripples.erase (std::remove_if (ripples.begin(), ripples.end(), [] (const Ripple& r) { return r.age >= r.life; }), ripples.end());

    if (wasActive || glow > 0.0f || ! ripples.empty())
        repaint();
}

void SoundMapView::paintActivity (juce::Graphics& g, juce::Point<float> centre, juce::Colour colour) const
{
    for (const auto& ripple : ripples)
    {
        const auto progress = ripple.age / ripple.life;
        const auto reach = juce::jmap (std::min (ripple.life, 4.0f), 0.35f, 4.0f, 70.0f, 240.0f);
        const auto radius = 14.0f + reach * (1.0f - std::pow (1.0f - progress, 2.4f));
        const auto alpha = ripple.strength * std::pow (1.0f - progress, 1.6f) * 0.6f;
        g.setColour (colour.withAlpha (alpha));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 0.8f + 1.6f * (1.0f - progress));
    }

    if (glow > 0.0f)
    {
        const auto outer = 10.0f + 30.0f * glow;
        g.setColour (colour.withAlpha (0.06f + 0.16f * glow));
        g.fillEllipse (juce::Rectangle<float> (outer * 2.0f, outer * 2.0f).withCentre (centre));
        const auto inner = 8.0f + 14.0f * glow;
        g.setColour (colour.withAlpha (0.12f + 0.3f * glow));
        g.fillEllipse (juce::Rectangle<float> (inner * 2.0f, inner * 2.0f).withCentre (centre));
    }
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
    return area.reduced (18.0f, 30.0f);
}

void SoundMapView::AxisScale::build (std::vector<float> logValues)
{
    std::sort (logValues.begin(), logValues.end());
    sortedLogValues = std::move (logValues);
}

float SoundMapView::AxisScale::normalise (float value) const
{
    if (sortedLogValues.size() < 2)
        return 0.5f;

    const auto logValue = std::log (std::max (value, 1.0e-3f));
    const auto low = sortedLogValues.front();
    const auto high = sortedLogValues.back();
    const auto linear = (logValue - low) / std::max (high - low, 1.0e-3f);

    const auto upper = std::lower_bound (sortedLogValues.begin(), sortedLogValues.end(), logValue);
    float rank;
    if (upper == sortedLogValues.begin())
        rank = 0.0f;
    else if (upper == sortedLogValues.end())
        rank = 1.0f;
    else
    {
        const auto index = static_cast<float> (std::distance (sortedLogValues.begin(), upper));
        const auto previous = *(upper - 1);
        const auto fraction = (logValue - previous) / std::max (*upper - previous, 1.0e-6f);
        rank = (index - 1.0f + fraction) / static_cast<float> (sortedLogValues.size() - 1);
    }

    return edgePadding + (1.0f - 2.0f * edgePadding) * (rankWeight * rank + (1.0f - rankWeight) * linear);
}

float SoundMapView::xForSeconds (float seconds) const
{
    const auto area = plotArea();
    return area.getX() + secondsScale.normalise (seconds) * area.getWidth();
}

float SoundMapView::yForBrightness (float hertz) const
{
    const auto area = plotArea();
    return area.getBottom() - brightnessScale.normalise (hertz) * area.getHeight();
}

void SoundMapView::layoutDots()
{
    staticLayerDirty = true;
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
        if (! isShown (dots[i]))
            continue;

        const auto distance = dots[i].position.getDistanceFrom (point);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = static_cast<int> (i);
        }
    }
    return best;
}

bool SoundMapView::isShown (const Dot& dot) const
{
    return categoryFilter.empty() || categoryFilter.count (dot.item->entry.categoryIndex) > 0
        || (dot.item->favourite && categoryFilter.count (theme::favouritesCategory) > 0);
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
    g.fillAll (theme::background);
    const auto area = plotArea();

    if (dots.empty())
    {
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (13.0f));
        g.drawText (items.empty() ? "NO IRS FOUND - USE THE MENU AT TOP RIGHT TO CHOOSE A FOLDER" : "ANALYSING IRS...",
                    getLocalBounds(), juce::Justification::centred);
        return;
    }

    const auto scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (staticLayerDirty || staticLayer.isNull() || ! juce::approximatelyEqual (scale, staticLayerScale)
        || staticLayer.getWidth() != juce::roundToInt (static_cast<float> (getWidth()) * scale))
        renderStaticLayer (scale);

    g.drawImage (staticLayer, getLocalBounds().toFloat());

    if (const auto active = selectedDot(); active >= 0 && isShown (dots[static_cast<size_t> (active)]))
        paintActivity (g, dots[static_cast<size_t> (active)].position,
                       theme::categoryColour (dots[static_cast<size_t> (active)].item->entry.categoryIndex));

    if (juce::isPositiveAndBelow (hoveredDot, static_cast<int> (dots.size())) && isShown (dots[static_cast<size_t> (hoveredDot)]))
    {
        const auto& dot = dots[static_cast<size_t> (hoveredDot)];
        const auto radius = dotRadius + 2.0f;
        g.setColour (theme::categoryColour (dot.item->entry.categoryIndex));
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (dot.position));
    }

    if (const auto selected = selectedDot(); selected >= 0 && isShown (dots[static_cast<size_t> (selected)]))
    {
        const auto& dot = dots[static_cast<size_t> (selected)];
        const auto colour = theme::categoryColour (dot.item->entry.categoryIndex);

        if (decayRatio < 0.999f && dot.item->profile)
        {
            const auto shift = xForSeconds (dot.item->profile->midT60 * decayRatio) - xForSeconds (dot.item->profile->midT60);
            const auto ghost = dot.position.translated (shift, 0.0f);
            g.setColour (colour.withAlpha (0.8f));
            const float dashes[] { 3.0f, 3.0f };
            g.drawDashedLine ({ dot.position.translated (-selectionRingRadius, 0.0f), ghost.translated (7.0f, 0.0f) }, dashes, 2, 1.2f);
            g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (ghost), 1.2f);
        }

        g.setColour (colour);
        g.fillEllipse (juce::Rectangle<float> (dotRadius * 2.5f, dotRadius * 2.5f).withCentre (dot.position));
        g.setColour (theme::text);
        g.drawEllipse (juce::Rectangle<float> (selectionRingRadius * 2.0f, selectionRingRadius * 2.0f).withCentre (dot.position), 1.5f);

        g.setFont (theme::labelFont (12.0f));
        const auto label = dot.item->entry.name.toUpperCase();
        const auto labelWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), label) + 20.0f;
        const auto above = dot.position.y - area.getY() > 40.0f;
        auto labelBounds = juce::Rectangle<float> (labelWidth, 16.0f)
                               .withCentre (dot.position.translated (0.0f, (selectionRingRadius + 11.0f) * (above ? -1.0f : 1.0f)));
        labelBounds = labelBounds.constrainedWithin (getLocalBounds().toFloat().reduced (4.0f)).expanded (0.0f, 3.0f);
        g.setColour (theme::background.withAlpha (0.92f));
        g.fillRoundedRectangle (labelBounds, labelBounds.getHeight() * 0.5f);
        g.setColour (colour.withAlpha (0.5f));
        g.drawRoundedRectangle (labelBounds.reduced (0.5f), labelBounds.getHeight() * 0.5f, 1.0f);
        g.setColour (theme::text);
        g.drawText (label, labelBounds, juce::Justification::centred);
    }

}

void SoundMapView::renderStaticLayer (float scale)
{
    staticLayerScale = scale;
    staticLayerDirty = false;
    staticLayer = juce::Image (juce::Image::ARGB, std::max (1, juce::roundToInt (static_cast<float> (getWidth()) * scale)),
                               std::max (1, juce::roundToInt (static_cast<float> (getHeight()) * scale)), true);

    juce::Graphics g (staticLayer);
    g.addTransform (juce::AffineTransform::scale (scale));
    g.fillAll (theme::background);
    paintAxes (g, plotArea());

    for (const auto& dot : dots)
    {
        if (! isShown (dot))
            continue;

        const auto colour = theme::categoryColour (dot.item->entry.categoryIndex);
        g.setColour (colour.withAlpha (0.12f));
        g.fillEllipse (juce::Rectangle<float> (glowRadius * 2.0f, glowRadius * 2.0f).withCentre (dot.position));
        g.setColour (colour.withAlpha (0.9f));
        g.fillEllipse (juce::Rectangle<float> (dotRadius * 2.0f, dotRadius * 2.0f).withCentre (dot.position));
    }

    paintLegend (g);
}

void SoundMapView::paintAxes (juce::Graphics& g, juce::Rectangle<float> area) const
{
    g.setFont (theme::labelFont (10.0f));

    auto lastTickX = -minimumTickSpacing;
    for (const auto seconds : secondsTicks)
    {
        const auto x = xForSeconds (seconds);
        if (x < area.getX() || x > area.getRight() - 70.0f || x - lastTickX < minimumTickSpacing)
            continue;

        lastTickX = x;
        g.setColour (theme::grid);
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
        g.setColour (theme::textDim.withAlpha (0.7f));
        g.drawText (theme::formatSeconds (seconds), juce::Rectangle<float> (60.0f, 14.0f).withCentre ({ x, area.getBottom() + 14.0f }),
                    juce::Justification::centred);
    }

    g.setColour (theme::textDim);
    g.drawText ("DECAY", juce::Rectangle<float> (area.getRight() - 60.0f, area.getBottom() + 7.0f, 60.0f, 14.0f), juce::Justification::centredRight);

    const auto labelArea = juce::Rectangle<float> (14.0f, area.getY(), axisMargin, area.getHeight());
    g.drawText ("BRIGHT", labelArea.withHeight (14.0f), juce::Justification::centredLeft);
    g.drawText ("DARK", labelArea.withTrimmedTop (area.getHeight() - 14.0f), juce::Justification::centredLeft);
}

std::vector<SoundMapView::LegendEntry> SoundMapView::legendEntries() const
{
    std::vector<LegendEntry> entries { { theme::favouritesCategory, "FAVOURITES", {} } };
    for (const auto& item : items)
        if (entries.back().categoryIndex != item.entry.categoryIndex)
            entries.push_back ({ item.entry.categoryIndex, item.entry.category.toUpperCase(), {} });

    const juce::Font font (theme::labelFont (10.0f));
    constexpr auto gap = 22.0f;
    constexpr auto dotSpace = 14.0f;
    auto totalWidth = 0.0f;
    for (const auto& entry : entries)
        totalWidth += dotSpace + juce::GlyphArrangement::getStringWidth (font, entry.text) + gap;

    const auto legend = getLocalBounds().removeFromBottom (legendHeight).toFloat();
    auto x = legend.getCentreX() - (totalWidth - gap) * 0.5f;
    for (auto& entry : entries)
    {
        const auto width = dotSpace + juce::GlyphArrangement::getStringWidth (font, entry.text) + 4.0f;
        entry.bounds = { x, legend.getY(), width, legend.getHeight() };
        x += width - 4.0f + gap;
    }

    return entries;
}

void SoundMapView::paintLegend (juce::Graphics& g) const
{
    g.setFont (theme::labelFont (10.0f));

    for (const auto& entry : legendEntries())
    {
        const auto active = categoryFilter.empty() || categoryFilter.count (entry.categoryIndex) > 0;
        const auto selected = categoryFilter.count (entry.categoryIndex) > 0;
        const auto colour = theme::categoryColour (entry.categoryIndex);
        const auto dotCentre = juce::Point<float> { entry.bounds.getX() + 3.0f, entry.bounds.getCentreY() };

        g.setColour (active ? colour : colour.withAlpha (0.3f));
        if (entry.categoryIndex == theme::favouritesCategory)
            g.fillPath (theme::heartPath (juce::Rectangle<float> (9.0f, 9.0f).withCentre (dotCentre)));
        else
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (dotCentre));
        if (selected)
            g.drawEllipse (juce::Rectangle<float> (11.0f, 11.0f).withCentre (dotCentre), 1.0f);

        g.setColour (selected ? theme::text : (active ? theme::textDim : theme::textDim.withAlpha (0.4f)));
        g.drawText (entry.text, entry.bounds.withTrimmedLeft (12.0f), juce::Justification::centredLeft);
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

    for (const auto& entry : legendEntries())
    {
        if (entry.bounds.expanded (4.0f, 0.0f).contains (event.position))
        {
            if (! categoryFilter.erase (entry.categoryIndex))
                categoryFilter.insert (entry.categoryIndex);

            staticLayerDirty = true;
            hoveredDot = -1;
            repaint();
            return;
        }
    }

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
        if (static_cast<int> (i) == current || along <= 0.5f || ! isShown (dots[i]))
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
