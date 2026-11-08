#pragma once

#include <juce_events/juce_events.h>

namespace lucky
{

/** Downloads an IR archive and unpacks it into a folder on a background thread, reporting progress. */
class IrDownloader final : public juce::ChangeBroadcaster,
                           private juce::Thread
{
public:
    /** Samplicity's free Bricasti M7 archive. */
    static inline const juce::URL bricastiArchive {
        "https://cdn.samplicity.com/downloads/Samplicity%20-%20Bricasti%20IRs%20version%202023-10.zip"
    };

    /** Approximate download size shown to the user before starting. */
    static constexpr int bricastiArchiveMegabytes = 964;

    enum class Stage { idle, downloading, unpacking, finished, failed };

    struct Status
    {
        Stage stage = Stage::idle;
        float progress = 0.0f;
        juce::String detail;
    };

    IrDownloader();
    ~IrDownloader() override;

    /** Starts downloading the archive into a temporary file inside destination, then unpacks it there. Ignored while busy. */
    void start (const juce::URL& archive, const juce::File& destination);

    /** Stops a download or unpack in progress and removes the partial download. */
    void cancel();

    /** Current stage, progress (0-1) and a human-readable detail line. */
    Status getStatus() const;

    /** True while downloading or unpacking. */
    bool isBusy() const;

private:
    void run() override;
    bool download (juce::File& archiveFile);
    bool unpack (const juce::File& archiveFile);
    void setStatus (Stage stage, float progress, const juce::String& detail);

    mutable juce::CriticalSection lock;
    Status status;
    juce::URL source;
    juce::File target;
    juce::uint32 lastNotifyMs = 0;
};

} // namespace lucky
