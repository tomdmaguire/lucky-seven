#include "IrLibrary.h"

#include <regex>

namespace lucky
{

juce::String IrEntry::id() const
{
    return (number > 0 ? category + "/" + juce::String (number).paddedLeft ('0', 2) + " " + name
                       : category + "/" + name)
         + uniqueSuffix;
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

juce::String displayCategoryName (const juce::String& folderName)
{
    static const std::regex numbering (R"(^\d+[ ._-]+)");
    const auto stripped = juce::String (std::regex_replace (folderName.toStdString(), numbering, "")).trim();
    return stripped.isNotEmpty() ? stripped : folderName;
}

namespace
{
struct CustomFile
{
    int rootIndex;
    juce::String folderName;
    juce::File folder;
    juce::File file, rightFile;
    juce::String baseName;
};

/** Splits "German Concert Hall L" into ("German Concert Hall", 'L'); returns 0 when there is no L/R suffix. */
std::pair<juce::String, char> splitChannelSuffix (const juce::String& nameWithoutExtension)
{
    static const std::regex suffix (R"(^(.*\S)[ _-]([LRlr])$)");
    std::smatch match;
    const auto text = nameWithoutExtension.toStdString();
    if (! std::regex_match (text, match, suffix))
        return { nameWithoutExtension, 0 };
    return { juce::String (match[1].str()), static_cast<char> (std::toupper (match[2].str()[0])) };
}

std::vector<CustomFile> pairChannelFiles (std::vector<CustomFile> files)
{
    std::map<juce::String, std::pair<int, int>> halves;
    for (size_t i = 0; i < files.size(); ++i)
    {
        const auto [base, side] = splitChannelSuffix (files[i].file.getFileNameWithoutExtension());
        if (side == 0)
            continue;

        const auto key = files[i].file.getParentDirectory().getFullPathName() + "|" + base.toLowerCase();
        auto& slot = halves.try_emplace (key, -1, -1).first->second;
        (side == 'L' ? slot.first : slot.second) = static_cast<int> (i);
    }

    std::vector<bool> consumed (files.size(), false);
    std::vector<CustomFile> result;
    for (const auto& [key, slot] : halves)
    {
        if (slot.first < 0 || slot.second < 0)
            continue;

        auto paired = files[static_cast<size_t> (slot.first)];
        paired.rightFile = files[static_cast<size_t> (slot.second)].file;
        paired.baseName = splitChannelSuffix (paired.file.getFileNameWithoutExtension()).first;
        consumed[static_cast<size_t> (slot.first)] = consumed[static_cast<size_t> (slot.second)] = true;
        result.push_back (std::move (paired));
    }

    for (size_t i = 0; i < files.size(); ++i)
        if (! consumed[i])
        {
            files[i].baseName = files[i].file.getFileNameWithoutExtension();
            result.push_back (std::move (files[i]));
        }

    return result;
}
} // namespace

std::vector<IrEntry> scanLibrary (const juce::File& root)
{
    return scanLibrary (juce::Array<juce::File> { root });
}

std::vector<IrEntry> scanLibrary (const juce::Array<juce::File>& roots)
{
    std::map<juce::String, IrEntry> byId;
    std::map<juce::String, std::pair<int, juce::String>> builtInCategories;
    std::vector<CustomFile> customFiles;

    for (int rootIndex = 0; rootIndex < roots.size(); ++rootIndex)
    {
        const auto& root = roots.getReference (rootIndex);
        if (! root.isDirectory())
            continue;

        for (const auto& item : juce::RangedDirectoryIterator (root, true, supportedIrWildcard, juce::File::findFiles))
        {
            const auto file = item.getFile();
            const auto fileName = file.getFileName();

            if (const auto parsed = parseIrFileName (fileName))
            {
                IrEntry candidate { parsed->categoryIndex, parsed->category, parsed->number, parsed->name, {}, {}, {} };
                auto& entry = byId.try_emplace (candidate.id(), candidate).first->second;
                entry.filesByRate[parsed->sampleRate] = file;
                builtInCategories[parsed->category.toLowerCase()] = { parsed->categoryIndex, parsed->category };
                continue;
            }

            if (isSamplicitySplitFile (fileName) || fileName.startsWithChar ('.'))
                continue;

            const auto relative = file.getRelativePathFrom (root);
            const auto separator = relative.indexOfChar (juce::File::getSeparatorChar());
            const auto folderName = separator > 0 ? relative.substring (0, separator) : root.getFileName();
            const auto folder = separator > 0 ? root.getChildFile (folderName) : root;
            customFiles.push_back ({ rootIndex, folderName, folder, file, {}, {} });
        }
    }

    customFiles = pairChannelFiles (std::move (customFiles));

    std::map<juce::String, juce::String> firstFolderForCategory;
    for (const auto& custom : customFiles)
    {
        const auto key = displayCategoryName (custom.folderName).toLowerCase();
        auto& first = firstFolderForCategory[key];
        if (first.isEmpty() || custom.folderName.compareNatural (first) < 0)
            first = custom.folderName;
    }

    juce::StringArray customOrder;
    for (const auto& [key, folderName] : firstFolderForCategory)
        if (! builtInCategories.count (key))
            customOrder.add (folderName);
    customOrder.sortNatural();

    std::map<juce::String, int> rootOfId;
    for (const auto& custom : customFiles)
    {
        const auto display = displayCategoryName (custom.folderName);
        const auto key = display.toLowerCase();
        const auto builtIn = builtInCategories.find (key);
        const auto index = builtIn != builtInCategories.end() ? builtIn->second.first
                                                              : firstCustomCategoryIndex + customOrder.indexOf (firstFolderForCategory[key]);
        const auto category = builtIn != builtInCategories.end() ? builtIn->second.second : display;

        auto relativeName = custom.file.isAChildOf (custom.folder) ? custom.file.getParentDirectory().getRelativePathFrom (custom.folder)
                                                                   : juce::String();
        relativeName = (relativeName.isEmpty() || relativeName == "." ? juce::String() : relativeName.replaceCharacter (juce::File::getSeparatorChar(), '/') + "/")
                     + custom.baseName;

        IrEntry candidate { index, category, 0, relativeName, {}, custom.rightFile, {} };
        for (int copy = 2; byId.count (candidate.id()) > 0 && rootOfId[candidate.id()] != custom.rootIndex; ++copy)
            candidate.uniqueSuffix = " (" + juce::String (copy) + ")";

        rootOfId[candidate.id()] = custom.rootIndex;
        byId.try_emplace (candidate.id(), candidate).first->second.filesByRate[0] = custom.file;
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

juce::AudioBuffer<float> readIrEntry (const IrEntry& entry, double sampleRate, double& sampleRateOut)
{
    auto left = readIrFile (entry.fileForRate (sampleRate), sampleRateOut);
    if (! entry.isPair() || left.getNumSamples() == 0)
        return left;

    double rightRate = 0.0;
    const auto right = readIrFile (entry.rightFile, rightRate);
    if (right.getNumSamples() == 0 || ! juce::approximatelyEqual (rightRate, sampleRateOut))
        return {};

    const auto length = std::max (left.getNumSamples(), right.getNumSamples());
    const auto monoPair = left.getNumChannels() == 1 && right.getNumChannels() == 1;
    juce::AudioBuffer<float> combined (monoPair ? 2 : 4, length);
    combined.clear();

    if (monoPair)
    {
        combined.copyFrom (0, 0, left, 0, 0, left.getNumSamples());
        combined.copyFrom (1, 0, right, 0, 0, right.getNumSamples());
        return combined;
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        combined.copyFrom (ch, 0, left, std::min (ch, left.getNumChannels() - 1), 0, left.getNumSamples());
        combined.copyFrom (2 + ch, 0, right, std::min (ch, right.getNumChannels() - 1), 0, right.getNumSamples());
    }
    return combined;
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
