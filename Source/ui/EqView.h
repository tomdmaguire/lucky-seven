#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace lucky
{

/** Ableton-style band display for the reverb's low and high cut: drag the dot sideways to move the band, up/down to widen it. */
class EqView final : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    EqView (juce::RangedAudioParameter& lowCut, juce::RangedAudioParameter& highCut);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    juce::Rectangle<float> plotArea() const;
    float xForFrequency (float hertz) const;
    float frequencyForX (float x) const;
    float yForDb (float decibels) const;
    juce::Point<float> dotPosition() const;
    float responseDb (float hertz) const;
    void setBand (float centreLog2, float widthOctaves);

    juce::RangedAudioParameter& lowParameter;
    juce::RangedAudioParameter& highParameter;
    juce::ParameterAttachment lowAttachment, highAttachment;
    float lowHz = 20.0f, highHz = 20000.0f;
    float dragStartCentre = 0.0f, dragStartWidth = 0.0f;
    bool dragging = false;
};

} // namespace lucky
