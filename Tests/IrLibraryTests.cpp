#include "ir/IrLibrary.h"

class IrLibraryTests final : public juce::UnitTest
{
public:
    IrLibraryTests() : juce::UnitTest ("IrLibrary", "Lucky Seven") {}

    void runTest() override
    {
        beginTest ("parses quad file names");
        const auto parsed = lucky::parseIrFileName ("1 Halls 04 Large & Near, 48K.wav");
        expect (parsed.has_value());
        expectEquals (parsed->categoryIndex, 1);
        expectEquals (parsed->category, juce::String ("Halls"));
        expectEquals (parsed->number, 4);
        expectEquals (parsed->name, juce::String ("Large & Near"));
        expectEquals (parsed->sampleRate, 48000);

        const auto rate441 = lucky::parseIrFileName ("5 Ambiences 14 Bass XXL, 44.1K.wav");
        expect (rate441.has_value());
        expectEquals (rate441->sampleRate, 44100);
        expectEquals (rate441->category, juce::String ("Ambiences"));

        const auto samplicity441 = lucky::parseIrFileName ("1 Halls 01 Large Hall, 44K.wav");
        expect (samplicity441.has_value());
        expectEquals (samplicity441->sampleRate, 44100);

        beginTest ("rejects left/right split files and unrelated files");
        expect (! lucky::parseIrFileName ("1 Halls 01 Large Hall, 48K L.wav").has_value());
        expect (! lucky::parseIrFileName ("readme.wav").has_value());

        beginTest ("builds stable ids");
        lucky::IrEntry entry { 1, "Halls", 4, "Large & Near", {}, {}, {} };
        expectEquals (entry.id(), juce::String ("Halls/04 Large & Near"));

        beginTest ("picks the closest sample rate");
        entry.filesByRate[44100] = juce::File ("/tmp/a.wav");
        entry.filesByRate[48000] = juce::File ("/tmp/b.wav");
        expectEquals (entry.fileForRate (96000.0).getFileName(), juce::String ("b.wav"));
        expectEquals (entry.fileForRate (44100.0).getFileName(), juce::String ("a.wav"));

        beginTest ("splits quad channels as LL LR RL RR");
        juce::AudioBuffer<float> quad (4, 1);
        for (int ch = 0; ch < 4; ++ch)
            quad.setSample (ch, 0, static_cast<float> (ch + 1));
        const auto split = lucky::splitTrueStereo (quad, 48000.0);
        expectEquals (split.fromLeft.getSample (0, 0), 1.0f);
        expectEquals (split.fromLeft.getSample (1, 0), 2.0f);
        expectEquals (split.fromRight.getSample (0, 0), 3.0f);
        expectEquals (split.fromRight.getSample (1, 0), 4.0f);
    }
};

static IrLibraryTests irLibraryTests;

class CustomLibraryTests final : public juce::UnitTest
{
public:
    CustomLibraryTests() : juce::UnitTest ("CustomLibrary", "Lucky Seven") {}

    void runTest() override
    {
        const auto root = juce::File::createTempFile ("lucky-seven-scan");
        root.createDirectory();

        writeWav (root.getChildFile ("Samplicity quad/1 Halls 01 Large Hall, 48K.wav"), 4);
        writeWav (root.getChildFile ("Samplicity quad/1 Halls 01 Large Hall, 44K.wav"), 4);
        writeWav (root.getChildFile ("Samplicity lr/1 Halls 01 Large Hall, 48K L.wav"), 2);
        writeWav (root.getChildFile ("My Rooms/Kitchen.wav"), 2);
        writeWav (root.getChildFile ("My Rooms/Small/Bathroom.wav"), 1);
        writeWav (root.getChildFile ("Halls/Garage Hall.wav"), 2);
        writeWav (root.getChildFile ("Loose.wav"), 2);

        const auto entries = lucky::scanLibrary (root);
        juce::StringArray ids;
        for (const auto& entry : entries)
            ids.add (entry.id());

        beginTest ("merges Samplicity rates, skips split files and adds custom folders");
        expectEquals (ids.joinIntoString (" | "),
                      "Halls/01 Large Hall | Halls/Garage Hall | My Rooms/Kitchen | My Rooms/Small/Bathroom | " + root.getFileName() + "/Loose");

        beginTest ("custom folders get custom category indices; matching names join the Bricasti category");
        expectEquals (entries[0].filesByRate.size(), static_cast<size_t> (2));
        expectEquals (entries[1].categoryIndex, 1);
        expectGreaterOrEqual (entries[2].categoryIndex, lucky::firstCustomCategoryIndex);
        expect (entries[2].fileForRate (48000.0).existsAsFile());

        root.deleteRecursively();

        testMultipleRootsAndPairs();
    }

private:
    void testMultipleRootsAndPairs()
    {
        const auto workspace = juce::File::createTempFile ("lucky-seven-roots");
        const auto bricasti = workspace.getChildFile ("Bricasti");
        const auto ableton = workspace.getChildFile ("Ableton IRs");
        const auto mine = workspace.getChildFile ("Mine");

        writeWav (bricasti.getChildFile ("quad/1 Halls 01 Large Hall, 48K.wav"), 4);
        writeWav (ableton.getChildFile ("05 Halls/German Concert Hall L.aif"), 2);
        writeWav (ableton.getChildFile ("05 Halls/German Concert Hall R.aif"), 2);
        writeWav (ableton.getChildFile ("01 Real Places/Kitchen.wav"), 2);
        writeWav (ableton.getChildFile ("01 Real Places/Orphan L.wav"), 2);
        writeWav (ableton.getChildFile ("09 Springs/Mono Spring_L.wav"), 1);
        writeWav (ableton.getChildFile ("09 Springs/Mono Spring_R.wav"), 1);
        writeWav (mine.getChildFile ("Real Places/Kitchen.wav"), 2);

        const auto entries = lucky::scanLibrary (juce::Array<juce::File> { bricasti, ableton, mine });
        juce::StringArray ids;
        for (const auto& entry : entries)
            ids.add (entry.id());

        beginTest ("strips numbering, merges categories by name across folders and pairs L/R files");
        expectEquals (ids.joinIntoString (" | "),
                      juce::String ("Halls/01 Large Hall | Halls/German Concert Hall | Real Places/Kitchen | Real Places/Kitchen (2) | "
                                    "Real Places/Orphan L | Springs/Mono Spring"));

        const auto find = [&] (const juce::String& id)
        {
            return *std::find_if (entries.begin(), entries.end(), [&] (const auto& e) { return e.id() == id; });
        };

        beginTest ("an L/R pair loads as one 4-channel true-stereo program");
        double rate = 0.0;
        const auto hall = lucky::readIrEntry (find ("Halls/German Concert Hall"), 48000.0, rate);
        expectEquals (hall.getNumChannels(), 4);

        beginTest ("a mono L/R pair loads as stereo");
        expectEquals (lucky::readIrEntry (find ("Springs/Mono Spring"), 48000.0, rate).getNumChannels(), 2);

        beginTest ("keeps the original folder order for custom categories");
        expectLessThan (find ("Real Places/Kitchen").categoryIndex, find ("Springs/Mono Spring").categoryIndex);
        expectEquals (find ("Halls/German Concert Hall").categoryIndex, 1);

        workspace.deleteRecursively();
    }

    static void writeWav (const juce::File& file, int channels)
    {
        file.getParentDirectory().createDirectory();
        juce::AudioBuffer<float> buffer (channels, 64);
        buffer.clear();
        buffer.setSample (0, 0, 1.0f);

        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        const auto options = juce::AudioFormatWriterOptions().withSampleRate (48000.0).withNumChannels (channels).withBitsPerSample (24);
        if (auto writer = format.createWriterFor (stream, options))
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }
};

static CustomLibraryTests customLibraryTests;
