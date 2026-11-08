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

using SelectionCallback = std::function<void (const juce::String& id)>;

} // namespace lucky
