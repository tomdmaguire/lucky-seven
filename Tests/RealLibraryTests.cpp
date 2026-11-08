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
