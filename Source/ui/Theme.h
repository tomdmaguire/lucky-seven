#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky::theme
{

inline const juce::Colour background { 0xff141519 };
inline const juce::Colour panel { 0xff1c1e24 };
inline const juce::Colour panelRaised { 0xff252830 };
inline const juce::Colour outline { 0xff30333c };
inline const juce::Colour text { 0xffe8e6e1 };
inline const juce::Colour textDim { 0xff8b8d96 };
inline const juce::Colour accent { 0xffe8b44c };

/** Colour for a Bricasti category index (1 = Halls ... 6 = Spaces). */
inline juce::Colour categoryColour (int categoryIndex)
{
    static const juce::Colour colours[] {
        juce::Colour (0xff6fa8ff), juce::Colour (0xffe8b44c), juce::Colour (0xff6fd39a),
        juce::Colour (0xffb58cff), juce::Colour (0xff4fd1d9), juce::Colour (0xffff7f6e),
    };
    return colours[static_cast<size_t> (juce::jlimit (1, 6, categoryIndex) - 1)];
}

/** Formats a duration in seconds as e.g. "2.4 s" or "850 ms". */
inline juce::String formatSeconds (float seconds)
{
    return seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms"
                          : juce::String (seconds, 1) + " s";
}

} // namespace lucky::theme
