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
};

/** Browse order shared by the list view and the prev/next buttons: category A-Z, then program A-Z. */
inline bool alphabeticalOrder (const BrowserItem& a, const BrowserItem& b)
{
    if (a.entry.category != b.entry.category)
        return a.entry.category.compareNatural (b.entry.category) < 0;
    return a.entry.name.compareNatural (b.entry.name) < 0;
}

using SelectionCallback = std::function<void (const juce::String& id)>;

} // namespace lucky
