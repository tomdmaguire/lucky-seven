#include "dsp/DecayShaper.h"
#include "ir/IrLibrary.h"

class RealLibraryTests final : public juce::UnitTest
{
public:
    RealLibraryTests() : juce::UnitTest ("RealLibrary", "Lucky Seven") {}

    void runTest() override
    {
        const auto rootPath = juce::SystemStats::getEnvironmentVariable ("LUCKY_SEVEN_IR_ROOT", {});
        if (rootPath.isEmpty())
            return;

        beginTest ("scans all 134 Bricasti programs at both sample rates");
        const auto entries = lucky::scanLibrary (juce::File (rootPath));
        expectEquals (static_cast<int> (entries.size()), 134);
        for (const auto& entry : entries)
            expectEquals (static_cast<int> (entry.filesByRate.size()), 2, entry.id());

        for (const auto& id : { juce::String ("Halls/01 Large Hall"), juce::String ("Plates/06 Vocal Plate"), juce::String ("Rooms/02 Studio B Close") })
        {
            beginTest ("halves the decay of " + id);
            const auto entry = std::find_if (entries.begin(), entries.end(), [&] (const auto& e) { return e.id() == id; });
            expect (entry != entries.end());
            if (entry == entries.end())
                continue;

            for (const auto fileRate : { 44100.0, 48000.0 })
            {
                double rate = 0.0;
                auto ir = lucky::readIrFile (entry->fileForRate (fileRate), rate);
                expectEquals (rate, fileRate);
                expectEquals (ir.getNumChannels(), 4);
                lucky::normaliseEnergy (ir);

                const auto profile = lucky::analyse (ir, rate);
                const auto shortened = lucky::analyse (lucky::shortenDecay (ir, rate, profile, 0.5f), rate);
                logMessage (id + " @ " + juce::String (rate) + ": mid T60 " + juce::String (profile.midT60, 2) + " s -> " + juce::String (shortened.midT60, 2)
                            + " s, brightness " + juce::String (profile.brightnessHz, 0) + " Hz");
                expectWithinAbsoluteError (shortened.midT60, profile.midT60 * 0.5f, profile.midT60 * 0.1f);
            }
        }
    }
};

static RealLibraryTests realLibraryTests;

class ExtraLibraryTests final : public juce::UnitTest
{
public:
    ExtraLibraryTests() : juce::UnitTest ("ExtraLibrary", "Lucky Seven") {}

    void runTest() override
    {
        const auto mainRoot = juce::SystemStats::getEnvironmentVariable ("LUCKY_SEVEN_IR_ROOT", {});
        const auto extraRoot = juce::SystemStats::getEnvironmentVariable ("LUCKY_SEVEN_EXTRA_IR_ROOT", {});
        if (mainRoot.isEmpty() || extraRoot.isEmpty())
            return;

        const auto entries = lucky::scanLibrary (juce::Array<juce::File> { juce::File (mainRoot), juce::File (extraRoot) });

        beginTest ("ids are unique and category names carry no numbering");
        juce::StringArray ids, categories;
        for (const auto& entry : entries)
        {
            ids.add (entry.id());
            categories.addIfNotAlreadyThere (entry.category);
        }
        juce::StringArray uniqueIds (ids);
        uniqueIds.removeDuplicates (false);
        expectEquals (uniqueIds.size(), ids.size());
        for (const auto& category : categories)
            expect (! juce::CharacterFunctions::isDigit (category[0]), category);
        logMessage (juce::String (static_cast<int> (entries.size())) + " programs in " + juce::String (categories.size())
                    + " categories: " + categories.joinIntoString (", "));

        beginTest ("every program loads and gets a sensible decay time");
        int pairs = 0, failures = 0;
        float slowest = 0.0f;
        juce::String slowestId;
        for (const auto& entry : entries)
        {
            double rate = 0.0;
            auto ir = lucky::readIrEntry (entry, 44100.0, rate);
            if (ir.getNumSamples() == 0)
            {
                ++failures;
                logMessage ("failed to load " + entry.id());
                continue;
            }

            if (entry.isPair())
            {
                ++pairs;
                expectEquals (ir.getNumChannels(), 4, entry.id());
            }

            lucky::trimNoiseFloor (ir, rate);
            const auto profile = lucky::analyse (ir, rate);
            const auto isClickOnly = entry.name.startsWith ("ABLCR M2S");
            expect (isClickOnly || (profile.midT60 > 0.01f && profile.midT60 < 60.0f), entry.id() + ": " + juce::String (profile.midT60));
            if (entry.name.contains ("Empty Indoor Pool") || entry.name.contains ("Wood Plate"))
                logMessage (entry.id() + " now measures " + juce::String (profile.midT60, 2) + " s");
            if (profile.midT60 > slowest)
            {
                slowest = profile.midT60;
                slowestId = entry.id();
            }

            if (entry.id().endsWith ("BM7 Studio A"))
                logMessage ("BM7 Studio A now measures " + juce::String (profile.midT60, 2) + " s");
        }

        expectEquals (failures, 0);
        expectEquals (pairs, 55);
        logMessage (juce::String (pairs) + " L/R pairs; longest decay " + juce::String (slowest, 1) + " s (" + slowestId + ")");
    }
};

static ExtraLibraryTests extraLibraryTests;
