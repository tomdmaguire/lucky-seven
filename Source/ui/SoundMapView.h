#pragma once

#include "BrowserItem.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <set>

namespace lucky
{

/** Scatter of every program by decay time (x) and brightness (y); click a dot to load it. The legend (wrapping onto more
    rows as needed) filters by category, and a search narrows the dots further. */
class SoundMapView final : public juce::Component,
                           private juce::Timer
{
public:
    explicit SoundMapView (SelectionCallback onSelect);

    /** Replaces the programs shown; programs without analysis yet are left off the map. */
    void setItems (std::vector<BrowserItem> newItems);

    /** Marks the given program as current without firing the selection callback. */
    void setSelectedId (const juce::String& id);

    /** Moves the ghost marker that shows the effective decay of the current program. */
    void setDecayRatio (float ratio);

    /** Replaces the category filter (empty shows everything) without firing onFilterChanged. */
    void setFilter (std::set<int> categories);

    /** Called after the user toggles a legend entry. */
    std::function<void (const std::set<int>&)> onFilterChanged;

    /** Shows only programs matching every search word (combined with the legend filter); legend entries show match counts. */
    void setSearch (const juce::StringArray& terms);

    /** Where a program's dot is drawn, if it is on the map. */
    std::optional<juce::Point<float>> positionOf (const juce::String& id) const;

    /** Selects the first program that is currently visible; returns false if none is. */
    bool selectFirstVisible();

    /** Source of (input peak, reverb peak) since the last call; drives the ripples and the glow on the current dot. */
    void setLevelSource (std::function<std::pair<float, float>()> source);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void visibilityChanged() override;

private:
    struct Dot
    {
        const BrowserItem* item = nullptr;
        juce::Point<float> target, position;
    };

    /** Maps values onto 0..1 by blending their rank among all programs with their log position, so clusters spread out. */
    struct AxisScale
    {
        std::vector<float> sortedLogValues;

        void build (std::vector<float> logValues);
        float normalise (float value) const;
    };

    struct Ripple
    {
        float age, life, strength;
    };

    void timerCallback() override;
    void updateActivityTimer();
    float effectiveDecaySeconds() const;
    void paintActivity (juce::Graphics&, juce::Point<float> centre, juce::Colour colour) const;

    struct LegendEntry
    {
        int categoryIndex;
        juce::String text;
        juce::Rectangle<float> bounds;
        int matches;
    };

    void updateLegendLayout();
    void updateLegendCounts();
    bool isShown (const Dot& dot) const;
    juce::Rectangle<float> plotArea() const;
    float xForSeconds (float seconds) const;
    float yForBrightness (float hertz) const;
    void layoutDots();
    int dotAt (juce::Point<float> point) const;
    int selectedDot() const;

    void paintAxes (juce::Graphics&, juce::Rectangle<float> area) const;
    void paintLegend (juce::Graphics&) const;
    void paintHoverCard (juce::Graphics&) const;
    void renderStaticLayer (float scale);

    SelectionCallback onSelect;
    std::vector<BrowserItem> items;
    std::vector<Dot> dots;
    juce::String selectedId;
    float decayRatio = 1.0f;
    int hoveredDot = -1;
    AxisScale secondsScale, brightnessScale;
    std::set<int> categoryFilter;
    juce::StringArray searchTerms;
    std::vector<LegendEntry> legendLayout;
    float legendHeight = 30.0f;
    juce::Image staticLayer;
    float staticLayerScale = 1.0f;
    bool staticLayerDirty = true;
    std::function<std::pair<float, float>()> levelSource;
    std::vector<Ripple> ripples;
    float glow = 0.0f, inputEnvelope = 0.0f, secondsSinceRipple = 1.0f;
};

} // namespace lucky
