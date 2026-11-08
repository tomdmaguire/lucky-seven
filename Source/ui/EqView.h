#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace lucky
{

/** Ableton-style reverb filter: Lo Cut / Hi Cut toggles and one handle on the curve for frequency (sideways) and width (up/down). */
class EqView final : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    struct Parameters
    {
        juce::RangedAudioParameter& frequency;
        juce::RangedAudioParameter& width;
        juce::RangedAudioParameter& lowCutOn;
        juce::RangedAudioParameter& highCutOn;
    };

    explicit EqView (Parameters parameters);

    /** Colour of the curve, handle, toggles and readouts. */
    void setAccent (juce::Colour colour);

    /** Corner frequencies for a band centre and width in octaves. */
    static std::pair<float, float> cornersFor (float centreHz, float widthOctaves);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const;
    float xForFrequency (float hertz) const;
    float frequencyForX (float x) const;
    float yForDb (float decibels) const;
    float responseDb (float hertz) const;
    juce::Point<float> handlePosition() const;
    void styleToggle (juce::TextButton& button);

    Parameters parameters;
    juce::ParameterAttachment frequencyAttachment, widthAttachment;
    juce::TextButton lowCutButton { "LO CUT" }, highCutButton { "HI CUT" };
    juce::ButtonParameterAttachment lowCutAttachment, highCutAttachment;
    float centreHz = 1000.0f, widthOctaves = 6.0f;
    float dragStartLog2 = 0.0f, dragStartWidth = 0.0f;
    bool dragging = false, hovering = false;
    juce::Colour accent { 0xffff4d6d };
};

} // namespace lucky
