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
inline const juce::Colour accent { 0xffd6b785 };

/** Pseudo-category index used for the favourites filter and list entry. */
inline constexpr int favouritesCategory = 0;

/** Colour for a Bricasti category index (1 = Halls ... 6 = Spaces; 0 = favourites). */
inline juce::Colour categoryColour (int categoryIndex)
{
    if (categoryIndex == favouritesCategory)
        return text;

    if (categoryIndex > 6)
    {
        const auto hue = std::fmod (0.08f + static_cast<float> (categoryIndex) * 0.61803f, 1.0f);
        return juce::Colour::fromHSV (hue, 0.38f, 0.84f, 1.0f);
    }

    static const juce::Colour colours[] {
        juce::Colour (0xffd6b785), juce::Colour (0xffd685be), juce::Colour (0xff85d68f),
        juce::Colour (0xff85d6d6), juce::Colour (0xff85aad6), juce::Colour (0xffa785d6),
    };
    return colours[static_cast<size_t> (juce::jlimit (1, 6, categoryIndex) - 1)];
}

/** Heart outline fitted to the given bounds. */
inline juce::Path heartPath (juce::Rectangle<float> r)
{
    const auto x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    juce::Path path;
    path.startNewSubPath (x + w * 0.5f, y + h * 0.95f);
    path.cubicTo (x - w * 0.1f, y + h * 0.5f, x + w * 0.1f, y - h * 0.1f, x + w * 0.5f, y + h * 0.25f);
    path.cubicTo (x + w * 0.9f, y - h * 0.1f, x + w * 1.1f, y + h * 0.5f, x + w * 0.5f, y + h * 0.95f);
    path.closeSubPath();
    return path;
}

/** Small uppercase label font used across the UI. */
inline juce::FontOptions labelFont (float height = 11.0f)
{
    return juce::FontOptions (height).withStyle ("Medium").withKerningFactor (0.08f);
}

/** Formats a duration in seconds as e.g. "2.4 s" or "850 ms". */
inline juce::String formatSeconds (float seconds)
{
    return seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms"
                          : juce::String (seconds, 1) + " s";
}

} // namespace lucky::theme
