#include "ir/IrDownloader.h"
#include "ir/IrLibrary.h"

class IrDownloaderTests final : public juce::UnitTest
{
public:
    IrDownloaderTests() : juce::UnitTest ("IrDownloader", "Lucky Seven") {}

    void runTest() override
    {
        const auto workspace = juce::File::createTempFile ("lucky-seven-download");
        workspace.createDirectory();
        const auto archive = workspace.getChildFile ("archive.zip");
        const auto destination = workspace.getChildFile ("IRs");

        writeArchive (archive, {
            "Samplicity quad channel files, 48 Khz/1 Halls 01 Large Hall, 48K.wav",
            "Samplicity left-right files, 48 Khz/1 Halls 01 Large Hall, 48K L.wav",
            "Samplicity left-right files, 48 Khz/1 Halls 01 Large Hall, 48K R.wav",
            "__MACOSX/Samplicity quad channel files, 48 Khz/._1 Halls 01 Large Hall, 48K.wav",
        });

        beginTest ("downloads and unpacks an archive, skipping split and Mac metadata files");
        lucky::IrDownloader downloader;
        downloader.start (juce::URL (archive), destination);

        const auto deadline = juce::Time::getMillisecondCounter() + 20000;
        while (downloader.isBusy() && juce::Time::getMillisecondCounter() < deadline)
            juce::Thread::sleep (20);

        const auto status = downloader.getStatus();
        expect (status.stage == lucky::IrDownloader::Stage::finished, status.detail);
        expect (destination.getChildFile ("Samplicity quad channel files, 48 Khz/1 Halls 01 Large Hall, 48K.wav").existsAsFile());
        expect (! destination.getChildFile ("Samplicity left-right files, 48 Khz").exists());
        expect (! destination.getChildFile ("__MACOSX").exists());
        expect (! destination.getChildFile (".lucky-seven-download.zip").exists());

        beginTest ("reports a failure for a missing archive");
        lucky::IrDownloader failing;
        failing.start (juce::URL (workspace.getChildFile ("missing.zip")), destination);
        const auto failDeadline = juce::Time::getMillisecondCounter() + 20000;
        while (failing.isBusy() && juce::Time::getMillisecondCounter() < failDeadline)
            juce::Thread::sleep (20);
        expect (failing.getStatus().stage == lucky::IrDownloader::Stage::failed);

        workspace.deleteRecursively();

        const auto realTarget = juce::SystemStats::getEnvironmentVariable ("LUCKY_SEVEN_REAL_DOWNLOAD", {});
        if (realTarget.isEmpty())
            return;

        beginTest ("downloads the real Bricasti archive and finds all 134 programs");
        const juce::File realDestination (realTarget);
        lucky::IrDownloader real;
        real.start (lucky::IrDownloader::bricastiArchive, realDestination);
        const auto start = juce::Time::getMillisecondCounter();
        while (real.isBusy() && juce::Time::getMillisecondCounter() - start < 900000)
            juce::Thread::sleep (200);

        expect (real.getStatus().stage == lucky::IrDownloader::Stage::finished, real.getStatus().detail);
        const auto entries = lucky::scanLibrary (realDestination);
        logMessage ("downloaded in " + juce::String ((juce::Time::getMillisecondCounter() - start) / 1000) + " s, "
                    + juce::String (static_cast<int> (entries.size())) + " programs");
        expectEquals (static_cast<int> (entries.size()), 134);
    }

private:
    static void writeArchive (const juce::File& archive, std::initializer_list<const char*> entries)
    {
        juce::ZipFile::Builder builder;
        for (const auto* name : entries)
        {
            juce::MemoryOutputStream content;
            content << "not really audio: " << name;
            builder.addEntry (std::make_unique<juce::MemoryInputStream> (content.getMemoryBlock(), true), 0, name, juce::Time::getCurrentTime());
        }

        juce::FileOutputStream output (archive);
        builder.writeToStream (output, nullptr);
    }
};

static IrDownloaderTests irDownloaderTests;
