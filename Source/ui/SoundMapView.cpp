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
constexpr float legendRowHeight = 24.0f;
constexpr float legendPadding = 6.0f;
constexpr float legendGap = 22.0f;
constexpr float legendDotSpace = 14.0f;
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
    setComponentID ("soundMap");
    setWantsKeyboardFocus (true);
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
    updateLegendLayout();

    layoutDots();
    repaint();
}

void SoundMapView::setSelectedId (const juce::String& id)
{
    selectedId = id;
    repaint();
}

void SoundMapView::setFilter (std::set<int> categories)
{
    categoryFilter = std::move (categories);
    staticLayerDirty = true;
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
    area.removeFromBottom (legendHeight);
    area.removeFromBottom (axisMargin * 0.6f);
    area.removeFromLeft (axisMargin);
    return area.reduced (18.0f, 0.0f).withTrimmedTop (14.0f).withTrimmedBottom (8.0f);
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
    const auto inFilter = categoryFilter.empty() || categoryFilter.count (dot.item->entry.categoryIndex) > 0
                       || (dot.item->favourite && categoryFilter.count (theme::favouritesCategory) > 0);
    return inFilter && matchesSearch (*dot.item, searchTerms);
}

void SoundMapView::setSearch (const juce::StringArray& terms)
{
    searchTerms = terms;
    updateLegendCounts();
    staticLayerDirty = true;
    hoveredDot = -1;
    repaint();
}

std::optional<juce::Point<float>> SoundMapView::positionOf (const juce::String& id) const
{
    for (const auto& dot : dots)
        if (dot.item->entry.id() == id)
            return dot.position;
    return std::nullopt;
}

bool SoundMapView::selectFirstVisible()
{
    for (const auto& dot : dots)
    {
        if (! isShown (dot))
            continue;

        grabKeyboardFocus();
        selectedId = dot.item->entry.id();
        onSelect (selectedId);
        repaint();
        return true;
    }
    return false;
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

    if (! searchTerms.isEmpty() && std::none_of (dots.begin(), dots.end(), [this] (const Dot& dot) { return isShown (dot); }))
    {
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (12.0f));
        g.drawText ("NO MATCHES", plotArea(), juce::Justification::centred);
    }

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

    paintHoverCard (g);

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

void SoundMapView::updateLegendLayout()
{
    legendLayout = { { theme::favouritesCategory, "FAVOURITES", {}, 0 } };
    for (const auto& item : items)
        if (legendLayout.back().categoryIndex != item.entry.categoryIndex)
            legendLayout.push_back ({ item.entry.categoryIndex, item.entry.category.toUpperCase(), {}, 0 });

    const juce::Font font (theme::labelFont (10.0f));
    const auto countWidth = juce::GlyphArrangement::getStringWidth (font, "  999");
    const auto available = static_cast<float> (getWidth()) - 24.0f;

    std::vector<std::vector<size_t>> rows (1);
    std::vector<float> rowWidths (1, 0.0f);
    for (size_t i = 0; i < legendLayout.size(); ++i)
    {
        const auto width = legendDotSpace + juce::GlyphArrangement::getStringWidth (font, legendLayout[i].text) + countWidth;
        legendLayout[i].bounds.setWidth (width);
        const auto needed = rowWidths.back() + (rows.back().empty() ? 0.0f : legendGap) + width;
        if (! rows.back().empty() && needed > available)
        {
            rows.emplace_back();
            rowWidths.push_back (0.0f);
        }
        rowWidths.back() += (rows.back().empty() ? 0.0f : legendGap) + width;
        rows.back().push_back (i);
    }

    legendHeight = static_cast<float> (rows.size()) * legendRowHeight + legendPadding;
    auto y = static_cast<float> (getHeight()) - legendHeight + legendPadding * 0.5f;
    for (size_t row = 0; row < rows.size(); ++row)
    {
        auto x = static_cast<float> (getWidth()) * 0.5f - rowWidths[row] * 0.5f;
        for (const auto index : rows[row])
        {
            auto& bounds = legendLayout[index].bounds;
            bounds = { x, y, bounds.getWidth(), legendRowHeight };
            x += bounds.getWidth() + legendGap;
        }
        y += legendRowHeight;
    }

    updateLegendCounts();
}

void SoundMapView::updateLegendCounts()
{
    for (auto& entry : legendLayout)
    {
        entry.matches = 0;
        for (const auto& item : items)
        {
            const auto inEntry = entry.categoryIndex == theme::favouritesCategory ? item.favourite
                                                                                  : item.entry.categoryIndex == entry.categoryIndex;
            if (inEntry && matchesSearch (item, searchTerms))
                ++entry.matches;
        }
    }
}

void SoundMapView::paintLegend (juce::Graphics& g) const
{
    g.setFont (theme::labelFont (10.0f));

    const auto searching = ! searchTerms.isEmpty();
    for (const auto& entry : legendLayout)
    {
        const auto active = (categoryFilter.empty() || categoryFilter.count (entry.categoryIndex) > 0) && (! searching || entry.matches > 0);
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
        g.drawText (searching ? entry.text + "  " + juce::String (entry.matches) : entry.text,
                    entry.bounds.withTrimmedLeft (12.0f), juce::Justification::centredLeft);
    }
}

void SoundMapView::resized()
{
    updateLegendLayout();
    layoutDots();
}

void SoundMapView::mouseMove (const juce::MouseEvent& event)
{
    const auto dot = dotAt (event.position);
    const auto overLegend = std::any_of (legendLayout.begin(), legendLayout.end(),
                                         [&] (const LegendEntry& entry) { return entry.bounds.expanded (4.0f, 0.0f).contains (event.position); });
    setMouseCursor (dot >= 0 || overLegend ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);

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

    for (const auto& entry : legendLayout)
    {
        if (entry.bounds.expanded (4.0f, 0.0f).contains (event.position))
        {
            if (! categoryFilter.erase (entry.categoryIndex))
                categoryFilter.insert (entry.categoryIndex);

            staticLayerDirty = true;
            if (onFilterChanged != nullptr)
                onFilterChanged (categoryFilter);
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

int SoundMapView::nearestAlongDecay (int from, bool longer) const
{
    const auto origin = dots[static_cast<size_t> (from)].target;
    const auto height = plotArea().getHeight();

    for (const auto band : { 0.06f, 0.15f, 1.0f })
    {
        auto best = -1;
        auto bestDistance = std::numeric_limits<float>::max();

        for (size_t i = 0; i < dots.size(); ++i)
        {
            const auto delta = dots[i].target - origin;
            const auto along = longer ? delta.x : -delta.x;
            if (static_cast<int> (i) == from || ! isShown (dots[i]) || along <= 0.5f || std::abs (delta.y) > band * height)
                continue;

            if (along < bestDistance)
            {
                bestDistance = along;
                best = static_cast<int> (i);
            }
        }

        if (best >= 0)
            return best;
    }

    return -1;
}

bool SoundMapView::keyPressed (const juce::KeyPress& key)
{
    juce::Point<float> direction;
    const auto code = key.getKeyCode();
    if (code == juce::KeyPress::leftKey)
        direction = { -1.0f, 0.0f };
    else if (code == juce::KeyPress::rightKey)
        direction = { 1.0f, 0.0f };
    else if (code == juce::KeyPress::upKey)
        direction = { 0.0f, -1.0f };
    else if (code == juce::KeyPress::downKey)
        direction = { 0.0f, 1.0f };
    else
        return false;

    const auto current = selectedDot();
    if (current < 0)
        return true;

    if (key.getModifiers().isShiftDown() && juce::exactlyEqual (direction.y, 0.0f))
    {
        if (const auto target = nearestAlongDecay (current, direction.x > 0.0f); target >= 0)
        {
            selectedId = dots[static_cast<size_t> (target)].item->entry.id();
            onSelect (selectedId);
            repaint();
        }
        return true;
    }

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

void SoundMapView::paintHoverCard (juce::Graphics& g) const
{
    if (! juce::isPositiveAndBelow (hoveredDot, static_cast<int> (dots.size())) || ! isShown (dots[static_cast<size_t> (hoveredDot)]))
        return;

    const auto& dot = dots[static_cast<size_t> (hoveredDot)];
    const auto& entry = dot.item->entry;
    const auto colour = theme::categoryColour (entry.categoryIndex);

    const juce::Font nameFont (juce::FontOptions (14.0f));
    const juce::Font detailFont (theme::labelFont (10.0f));
    const juce::Font hintFont (theme::labelFont (9.0f));

    const auto details = entry.category.toUpperCase() + (entry.number > 0 ? " " + juce::String (entry.number).paddedLeft ('0', 2) : juce::String())
                       + (dot.item->profile ? juce::String::fromUTF8 ("  \xc2\xb7  ") + theme::formatSeconds (dot.item->profile->midT60) : juce::String());
    const auto hint = juce::String::fromUTF8 ("SHIFT \xe2\x86\x90 \xe2\x86\x92  SHORTER / LONGER, SIMILAR TONE");

    constexpr auto padding = 10.0f;
    const auto width = padding * 2.0f + std::max ({ juce::GlyphArrangement::getStringWidth (nameFont, entry.name) + 14.0f,
                                                    juce::GlyphArrangement::getStringWidth (detailFont, details),
                                                    juce::GlyphArrangement::getStringWidth (hintFont, hint) });
    constexpr auto height = 72.0f;

    auto card = juce::Rectangle<float> (width, height).withCentre (dot.position).withY (dot.position.y - height - 16.0f);
    if (card.getY() < 4.0f)
        card.setY (dot.position.y + 16.0f);
    card = card.constrainedWithin (getLocalBounds().toFloat().reduced (4.0f));

    g.setColour (theme::panelRaised.withAlpha (0.97f));
    g.fillRoundedRectangle (card, 6.0f);
    g.setColour (colour.withAlpha (0.45f));
    g.drawRoundedRectangle (card.reduced (0.5f), 6.0f, 1.0f);

    auto content = card.reduced (padding, 8.0f);
    auto nameRow = content.removeFromTop (20.0f);
    g.setColour (colour);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ nameRow.getX() + 3.5f, nameRow.getCentreY() }));
    g.setColour (theme::text);
    g.setFont (nameFont);
    g.drawText (entry.name, nameRow.withTrimmedLeft (14.0f), juce::Justification::centredLeft);

    g.setColour (theme::textDim);
    g.setFont (detailFont);
    g.drawText (details, content.removeFromTop (16.0f), juce::Justification::centredLeft);

    content.removeFromTop (4.0f);
    g.setColour (theme::outline);
    g.fillRect (content.removeFromTop (1.0f));
    content.removeFromTop (3.0f);
    g.setColour (theme::textDim.withAlpha (0.6f));
    g.setFont (hintFont);
    g.drawText (hint, content, juce::Justification::centredLeft);
}

} // namespace lucky
