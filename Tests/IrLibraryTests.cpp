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
        lucky::IrEntry entry { 1, "Halls", 4, "Large & Near", {} };
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
