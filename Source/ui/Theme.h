#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace lucky::theme
{

inline const juce::Colour background { 0xff1b1c20 };
inline const juce::Colour topBar { 0xff26272c };
inline const juce::Colour strip { 0xff222328 };
inline const juce::Colour panel { 0xff1b1c20 };
inline const juce::Colour panelRaised { 0xff2e3036 };
inline const juce::Colour outline { 0xff34363d };
inline const juce::Colour grid { 0xff26282e };
inline const juce::Colour text { 0xffe6e6e8 };
inline const juce::Colour textDim { 0xff83858e };
inline const juce::Colour accent { 0xffff4d6d };

/** Colour for a Bricasti category index (1 = Halls ... 6 = Spaces). */
inline juce::Colour categoryColour (int categoryIndex)
{
    static const juce::Colour colours[] {
        juce::Colour (0xff2fa8ff), juce::Colour (0xffffc23d), juce::Colour (0xff38d98a),
        juce::Colour (0xffb67cff), juce::Colour (0xff22d6e0), juce::Colour (0xffff4d6d),
    };
    return colours[static_cast<size_t> (juce::jlimit (1, 6, categoryIndex) - 1)];
}

/** Small uppercase label font used across the UI. */
inline juce::FontOptions labelFont (float height = 11.0f)
{
    return juce::FontOptions (height).withKerningFactor (0.08f);
}

/** Formats a duration in seconds as e.g. "2.4 s" or "850 ms". */
inline juce::String formatSeconds (float seconds)
{
    return seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms"
                          : juce::String (seconds, 1) + " s";
}

} // namespace lucky::theme
