#pragma once

#include "ir/IrAnalysis.h"
#include "ir/IrLibrary.h"
#include <functional>

namespace lucky
{

/** A program as shown in the browser, with its analysis once available. */
struct BrowserItem
{
    IrEntry entry;
    std::optional<DecayProfile> profile;
    bool favourite = false;
};

/** Browse order shared by the list view and the prev/next buttons: category A-Z, then program A-Z. */
inline bool alphabeticalOrder (const BrowserItem& a, const BrowserItem& b)
{
    if (a.entry.category != b.entry.category)
        return a.entry.category.compareNatural (b.entry.category) < 0;
    return a.entry.name.compareNatural (b.entry.name) < 0;
}

/** Splits search text into lower-case words; every word must appear in a program's name or category to match. */
inline juce::StringArray parseSearch (const juce::String& text)
{
    auto terms = juce::StringArray::fromTokens (text.toLowerCase(), true);
    terms.removeEmptyStrings();
    return terms;
}

/** True if every search word appears in the program's name or category (always true with no words). */
inline bool matchesSearch (const BrowserItem& item, const juce::StringArray& terms)
{
    if (terms.isEmpty())
        return true;

    const auto haystack = (item.entry.name + " " + item.entry.category).toLowerCase();
    for (const auto& term : terms)
        if (! haystack.contains (term))
            return false;
    return true;
}

using SelectionCallback = std::function<void (const juce::String& id)>;

} // namespace lucky
