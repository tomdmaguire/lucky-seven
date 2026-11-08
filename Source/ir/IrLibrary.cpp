#include "IrLibrary.h"

#include <regex>

namespace lucky
{

juce::String IrEntry::id() const
{
    return category + "/" + juce::String (number).paddedLeft ('0', 2) + " " + name;
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

std::vector<IrEntry> scanLibrary (const juce::File& root)
{
    std::map<juce::String, IrEntry> byId;

    for (const auto& item : juce::RangedDirectoryIterator (root, true, "*.wav", juce::File::findFiles))
    {
        const auto parsed = parseIrFileName (item.getFile().getFileName());
        if (! parsed)
            continue;

        IrEntry candidate { parsed->categoryIndex, parsed->category, parsed->number, parsed->name, {} };
        auto& entry = byId.try_emplace (candidate.id(), candidate).first->second;
        entry.filesByRate[parsed->sampleRate] = item.getFile();
    }

    std::vector<IrEntry> entries;
    entries.reserve (byId.size());
    for (auto& [id, entry] : byId)
        entries.push_back (std::move (entry));

    std::sort (entries.begin(), entries.end(), [] (const IrEntry& a, const IrEntry& b)
    {
        return std::tie (a.categoryIndex, a.number) < std::tie (b.categoryIndex, b.number);
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
