#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <map>
#include <optional>

namespace lucky
{

/** Fields parsed from a Samplicity quad-channel file name such as "1 Halls 04 Large & Near, 48K.wav". */
struct ParsedIrName
{
    int categoryIndex = 0;
    juce::String category;
    int number = 0;
    juce::String name;
    int sampleRate = 0;
};

/** One Bricasti program, available at one or more sample rates. */
struct IrEntry
{
    int categoryIndex = 0;
    juce::String category;
    int number = 0;
    juce::String name;
    std::map<int, juce::File> filesByRate;
    juce::File rightFile;
    juce::String uniqueSuffix;

    /** Stable identifier saved with the plugin state, e.g. "Halls/04 Large & Near" or "Real Places/Kitchen". */
    juce::String id() const;

    /** File whose sample rate is closest to the given rate (the left-input file for L/R pairs). */
    juce::File fileForRate (double sampleRate) const;

    /** True when this program is a pair of files holding the responses to the left and right inputs. */
    bool isPair() const { return rightFile != juce::File(); }
};

/** A true-stereo impulse response: the stereo response to the left input and to the right input. */
struct TrueStereoIr
{
    juce::AudioBuffer<float> fromLeft;
    juce::AudioBuffer<float> fromRight;
    double sampleRate = 0.0;
};

/** Category indices from here up are user folders; 1-6 are the Bricasti categories. */
inline constexpr int firstCustomCategoryIndex = 100;

/** Audio file types picked up when scanning the IR folder. */
inline const juce::String supportedIrWildcard { "*.wav;*.aif;*.aiff;*.flac" };

/** Parses a quad-channel file name; returns nothing for files that are not Bricasti quad IRs. */
std::optional<ParsedIrName> parseIrFileName (const juce::String& fileName);

/** True for Samplicity's left/right split files, which duplicate the quad files and are skipped. */
bool isSamplicitySplitFile (const juce::String& fileName);

/** Category name shown to the user: leading numbering such as "05 " or "07_" is removed. */
juce::String displayCategoryName (const juce::String& folderName);

/** Recursively scans every root. Samplicity quad IRs keep their categories and merge sample-rate variants. Any other
    audio file is categorised by its first subfolder (or by the root's own name if loose), files ending " L"/" R" are
    combined into one true-stereo program, and categories with the same display name merge across roots.
    Sorted by category, number, then name. */
std::vector<IrEntry> scanLibrary (const juce::Array<juce::File>& roots);

/** Single-root convenience overload of scanLibrary. */
std::vector<IrEntry> scanLibrary (const juce::File& root);

/** Reads an audio file into a buffer; returns an empty buffer on failure. */
juce::AudioBuffer<float> readIrFile (const juce::File& file, double& sampleRateOut);

/** Reads a program at the file closest to sampleRate; L/R pairs become one 4-channel LL/LR/RL/RR buffer. */
juce::AudioBuffer<float> readIrEntry (const IrEntry& entry, double sampleRate, double& sampleRateOut);

/** Splits a 4-channel LL/LR/RL/RR buffer into a true-stereo pair; 1-2 channel IRs become a channel-parallel pair. */
TrueStereoIr splitTrueStereo (const juce::AudioBuffer<float>& ir, double sampleRate);

} // namespace lucky
