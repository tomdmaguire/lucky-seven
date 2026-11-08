#pragma once

#include "BrowserItem.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky
{

/** Scatter of every program by decay time (x) and brightness (y); click a dot to load it. */
class SoundMapView final : public juce::Component,
                           public juce::TooltipClient
{
public:
    explicit SoundMapView (SelectionCallback onSelect);

    /** Replaces the programs shown; programs without analysis yet are left off the map. */
    void setItems (std::vector<BrowserItem> newItems);

    /** Marks the given program as current without firing the selection callback. */
    void setSelectedId (const juce::String& id);

    /** Moves the ghost marker that shows the effective decay of the current program. */
    void setDecayRatio (float ratio);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    juce::String getTooltip() override;

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

    juce::Rectangle<float> plotArea() const;
    float xForSeconds (float seconds) const;
    float yForBrightness (float hertz) const;
    void layoutDots();
    int dotAt (juce::Point<float> point) const;
    int selectedDot() const;
    void paintAxes (juce::Graphics&, juce::Rectangle<float> area) const;
    void paintLegend (juce::Graphics&) const;

    SelectionCallback onSelect;
    std::vector<BrowserItem> items;
    std::vector<Dot> dots;
    juce::String selectedId;
    float decayRatio = 1.0f;
    int hoveredDot = -1;
    AxisScale secondsScale, brightnessScale;
};

} // namespace lucky
