#include "IrLibrary.h"

#include <regex>

namespace lucky
{

juce::String IrEntry::id() const
{
    return number > 0 ? category + "/" + juce::String (number).paddedLeft ('0', 2) + " " + name
                      : category + "/" + name;
}

juce::File IrEntry::fileForRate (double sampleRate) const
{
    jassert (! filesByRate.empty());

    auto best = filesByRate.begin();
    for (auto it = filesByRate.begin(); it != filesByRate.end(); ++it)
        if (std::abs (it->first - sampleRate) < std::abs (best->first - sampleRate))
            best = it;

    return best->second;
}

std::optional<ParsedIrName> parseIrFileName (const juce::String& fileName)
{
    static const std::regex pattern (R"(^(\d+) ([A-Za-z]+) (\d+) (.+), (44(?:\.1)?|48|88(?:\.2)?|96)K\.wav$)", std::regex::icase);

    std::smatch match;
    const auto text = fileName.toStdString();
    if (! std::regex_match (text, match, pattern))
        return std::nullopt;

    ParsedIrName parsed;
    parsed.categoryIndex = std::stoi (match[1].str());
    parsed.category = juce::String (match[2].str());
    parsed.number = std::stoi (match[3].str());
    parsed.name = juce::String (match[4].str()).trim();
    const auto kiloHertz = std::stod (match[5].str());
    parsed.sampleRate = kiloHertz < 45.0 ? 44100 : kiloHertz < 50.0 ? 48000 : kiloHertz < 90.0 ? 88200 : 96000;
    return parsed;
}

bool isSamplicitySplitFile (const juce::String& fileName)
{
    static const std::regex pattern (R"(, (44(?:\.1)?|48|88(?:\.2)?|96)K [LR]\.wav$)", std::regex::icase);
    return std::regex_search (fileName.toStdString(), pattern);
}

std::vector<IrEntry> scanLibrary (const juce::File& root)
{
    std::map<juce::String, IrEntry> byId;
    std::map<juce::String, int> builtInIndexByName;
    std::vector<std::pair<juce::String, juce::File>> customFiles;

    for (const auto& item : juce::RangedDirectoryIterator (root, true, supportedIrWildcard, juce::File::findFiles))
    {
        const auto file = item.getFile();
        const auto fileName = file.getFileName();

        if (const auto parsed = parseIrFileName (fileName))
        {
            IrEntry candidate { parsed->categoryIndex, parsed->category, parsed->number, parsed->name, {} };
            auto& entry = byId.try_emplace (candidate.id(), candidate).first->second;
            entry.filesByRate[parsed->sampleRate] = file;
            builtInIndexByName[parsed->category.toLowerCase()] = parsed->categoryIndex;
            continue;
        }

        if (isSamplicitySplitFile (fileName) || fileName.startsWithChar ('.'))
            continue;

        const auto relative = file.getRelativePathFrom (root);
        const auto separator = relative.indexOfChar (juce::File::getSeparatorChar());
        const auto category = separator > 0 ? relative.substring (0, separator) : juce::String ("User");
        customFiles.emplace_back (category, file);
    }

    juce::StringArray customCategories;
    for (const auto& [category, file] : customFiles)
        if (! builtInIndexByName.count (category.toLowerCase()))
            customCategories.addIfNotAlreadyThere (category);
    customCategories.sortNatural();

    for (const auto& [category, file] : customFiles)
    {
        const auto builtIn = builtInIndexByName.find (category.toLowerCase());
        const auto index = builtIn != builtInIndexByName.end() ? builtIn->second : firstCustomCategoryIndex + customCategories.indexOf (category);
        const auto categoryFolder = root.getChildFile (category);
        const auto name = (file.isAChildOf (categoryFolder) ? file.getRelativePathFrom (categoryFolder) : file.getFileName())
                              .upToLastOccurrenceOf (".", false, false)
                              .replaceCharacter (juce::File::getSeparatorChar(), '/');

        IrEntry candidate { index, category, 0, name, {} };
        byId.try_emplace (candidate.id(), candidate).first->second.filesByRate[0] = file;
    }

    std::vector<IrEntry> entries;
    entries.reserve (byId.size());
    for (auto& [id, entry] : byId)
        entries.push_back (std::move (entry));

    std::sort (entries.begin(), entries.end(), [] (const IrEntry& a, const IrEntry& b)
    {
        if (a.categoryIndex != b.categoryIndex)
            return a.categoryIndex < b.categoryIndex;
        if (a.number != b.number)
            return (a.number == 0 ? std::numeric_limits<int>::max() : a.number) < (b.number == 0 ? std::numeric_limits<int>::max() : b.number);
        return a.name.compareNatural (b.name) < 0;
    });

    return entries;
}

juce::AudioBuffer<float> readIrFile (const juce::File& file, double& sampleRateOut)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    const std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
        return {};

    juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));
    reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);
    sampleRateOut = reader->sampleRate;
    return buffer;
}

TrueStereoIr splitTrueStereo (const juce::AudioBuffer<float>& ir, double sampleRate)
{
    TrueStereoIr result;
    result.sampleRate = sampleRate;

    const auto numSamples = ir.getNumSamples();
    const auto copyPair = [&] (juce::AudioBuffer<float>& destination, int first, int second)
    {
        destination.setSize (2, numSamples);
        destination.copyFrom (0, 0, ir, first, 0, numSamples);
        destination.copyFrom (1, 0, ir, second, 0, numSamples);
    };

    if (ir.getNumChannels() >= 4)
    {
        copyPair (result.fromLeft, 0, 1);
        copyPair (result.fromRight, 2, 3);
    }
    else if (ir.getNumChannels() > 0)
    {
        result.fromLeft.setSize (2, numSamples);
        result.fromRight.setSize (2, numSamples);
        result.fromLeft.clear();
        result.fromRight.clear();
        result.fromLeft.copyFrom (0, 0, ir, 0, 0, numSamples);
        result.fromRight.copyFrom (1, 0, ir, std::min (1, ir.getNumChannels() - 1), 0, numSamples);
    }

    return result;
}

} // namespace lucky
