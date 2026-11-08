#include "IrDownloader.h"
#include "IrLibrary.h"

namespace lucky
{

namespace
{
constexpr int connectionTimeoutMs = 20000;
constexpr int chunkSize = 1 << 16;
constexpr juce::uint32 notifyIntervalMs = 100;
constexpr juce::int64 minimumFreeBytes = 3LL * 1024 * 1024 * 1024;
const juce::String partialFileName { ".lucky-seven-download.zip" };

juce::String megabytes (juce::int64 bytes)
{
    return juce::String (juce::roundToInt (static_cast<double> (bytes) / (1024.0 * 1024.0)));
}

bool shouldExtract (const juce::String& entryName)
{
    const auto fileName = entryName.fromLastOccurrenceOf ("/", false, false);
    return ! entryName.startsWith ("__MACOSX") && ! fileName.startsWithChar ('.') && ! isSamplicitySplitFile (fileName);
}
} // namespace

IrDownloader::IrDownloader() : juce::Thread ("Lucky Seven IR download") {}

IrDownloader::~IrDownloader()
{
    stopThread (10000);
}

void IrDownloader::start (const juce::URL& archive, const juce::File& destination)
{
    if (isBusy())
        return;

    stopThread (1000);
    source = archive;
    target = destination;
    setStatus (Stage::downloading, 0.0f, "Connecting...");
    startThread();
}

void IrDownloader::cancel()
{
    stopThread (10000);
    if (target != juce::File())
        target.getChildFile (partialFileName).deleteFile();
    setStatus (Stage::idle, 0.0f, {});
}

IrDownloader::Status IrDownloader::getStatus() const
{
    const juce::ScopedLock scope (lock);
    return status;
}

bool IrDownloader::isBusy() const
{
    const auto stage = getStatus().stage;
    return stage == Stage::downloading || stage == Stage::unpacking;
}

void IrDownloader::setStatus (Stage stage, float progress, const juce::String& detail)
{
    {
        const juce::ScopedLock scope (lock);
        status = { stage, progress, detail };
    }

    const auto now = juce::Time::getMillisecondCounter();
    const auto stageChange = stage != Stage::downloading && stage != Stage::unpacking;
    if (stageChange || now - lastNotifyMs >= notifyIntervalMs)
    {
        lastNotifyMs = now;
        sendChangeMessage();
    }
}

void IrDownloader::run()
{
    if (! target.createDirectory())
    {
        setStatus (Stage::failed, 0.0f, "Couldn't create " + target.getFullPathName());
        return;
    }

    if (target.getBytesFreeOnVolume() < minimumFreeBytes)
    {
        setStatus (Stage::failed, 0.0f, "Not enough free disk space: the IRs need about 3 GB while unpacking.");
        return;
    }

    auto archiveFile = target.getChildFile (partialFileName);
    const auto ok = download (archiveFile) && unpack (archiveFile);
    archiveFile.deleteFile();

    if (ok)
        setStatus (Stage::finished, 1.0f, "Done");
}

bool IrDownloader::download (juce::File& archiveFile)
{
    auto stream = source.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                .withConnectionTimeoutMs (connectionTimeoutMs));
    if (stream == nullptr)
    {
        setStatus (Stage::failed, 0.0f, "Couldn't reach Samplicity. Check your internet connection and try again.");
        return false;
    }

    archiveFile.deleteFile();
    juce::FileOutputStream output (archiveFile);
    if (! output.openedOk())
    {
        setStatus (Stage::failed, 0.0f, "Couldn't write to " + archiveFile.getParentDirectory().getFullPathName());
        return false;
    }

    const auto total = stream->getTotalLength();
    juce::HeapBlock<char> buffer (chunkSize);
    juce::int64 received = 0;

    while (! stream->isExhausted())
    {
        if (threadShouldExit())
            return false;

        const auto read = stream->read (buffer.get(), chunkSize);
        if (read < 0)
            break;
        if (read == 0)
        {
            if (stream->isExhausted())
                break;
            continue;
        }

        if (! output.write (buffer.get(), static_cast<size_t> (read)))
        {
            setStatus (Stage::failed, 0.0f, "Ran out of disk space while downloading.");
            return false;
        }

        received += read;
        const auto progress = total > 0 ? static_cast<float> (static_cast<double> (received) / static_cast<double> (total)) : 0.0f;
        setStatus (Stage::downloading, progress,
                   "Downloading " + megabytes (received) + (total > 0 ? " of " + megabytes (total) : juce::String()) + " MB");
    }

    output.flush();
    if (received == 0 || (total > 0 && received < total))
    {
        setStatus (Stage::failed, 0.0f, "The download stopped early. Check your connection and try again.");
        return false;
    }

    return true;
}

bool IrDownloader::unpack (const juce::File& archiveFile)
{
    juce::ZipFile zip (archiveFile);
    const auto count = zip.getNumEntries();
    if (count == 0)
    {
        setStatus (Stage::failed, 0.0f, "The downloaded file isn't a valid archive. Try again later.");
        return false;
    }

    for (int i = 0; i < count; ++i)
    {
        if (threadShouldExit())
            return false;

        if (const auto* entry = zip.getEntry (i); entry != nullptr && shouldExtract (entry->filename))
        {
            if (const auto result = zip.uncompressEntry (i, target, true); result.failed())
            {
                setStatus (Stage::failed, 0.0f, "Couldn't unpack the IRs: " + result.getErrorMessage());
                return false;
            }
        }

        setStatus (Stage::unpacking, static_cast<float> (i + 1) / static_cast<float> (count), "Unpacking IRs...");
    }

    return true;
}

} // namespace lucky
