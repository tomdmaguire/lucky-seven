#include "IrCatalogue.h"

namespace lucky
{

namespace
{
const juce::Identifier rootKey { "irRoot" };

juce::File cacheFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Lucky Seven")
        .getChildFile ("analysis.json");
}

juce::var profileToVar (const DecayProfile& profile)
{
    auto* object = new juce::DynamicObject();
    juce::Array<juce::var> bands;
    for (auto t60 : profile.bandT60)
        bands.add (t60);

    object->setProperty ("bandT60", bands);
    object->setProperty ("midT60", profile.midT60);
    object->setProperty ("brightnessHz", profile.brightnessHz);
    object->setProperty ("onsetSample", profile.onsetSample);
    return juce::var (object);
}

std::optional<DecayProfile> profileFromVar (const juce::var& value)
{
    const auto* bands = value["bandT60"].getArray();
    if (bands == nullptr || bands->size() != static_cast<int> (analysisBands.size()))
        return std::nullopt;

    DecayProfile profile;
    for (size_t i = 0; i < analysisBands.size(); ++i)
        profile.bandT60[i] = static_cast<float> ((*bands)[static_cast<int> (i)]);

    profile.midT60 = static_cast<float> (value["midT60"]);
    profile.brightnessHz = static_cast<float> (value["brightnessHz"]);
    profile.onsetSample = static_cast<int> (value["onsetSample"]);
    return profile;
}
} // namespace

IrCatalogue::IrCatalogue() : juce::Thread ("Lucky Seven IR analysis")
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Lucky Seven";
    options.filenameSuffix = ".settings";
    options.folderName = "Lucky Seven";
    options.osxLibrarySubFolder = "Application Support";
    settings.setStorageParameters (options);

    loadCache();
    rescan();
}

IrCatalogue::~IrCatalogue()
{
    stopThread (5000);
}

juce::File IrCatalogue::defaultRoot()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Lucky Seven").getChildFile ("IRs");
}

juce::File IrCatalogue::getRoot() const
{
    const juce::ScopedLock scope (lock);
    const auto stored = settings.getUserSettings()->getValue (rootKey.toString());
    return stored.isNotEmpty() ? juce::File (stored) : defaultRoot();
}

void IrCatalogue::setRoot (const juce::File& newRoot)
{
    {
        const juce::ScopedLock scope (lock);
        settings.getUserSettings()->setValue (rootKey.toString(), newRoot.getFullPathName());
        settings.saveIfNeeded();
    }
    rescan();
}

std::vector<IrEntry> IrCatalogue::getEntries() const
{
    const juce::ScopedLock scope (lock);
    return entries;
}

std::optional<DecayProfile> IrCatalogue::getProfile (const juce::String& id) const
{
    const juce::ScopedLock scope (lock);
    const auto it = profiles.find (id);
    return it != profiles.end() ? std::optional<DecayProfile> (it->second) : std::nullopt;
}

std::optional<IrEntry> IrCatalogue::findEntry (const juce::String& id) const
{
    const juce::ScopedLock scope (lock);
    for (const auto& entry : entries)
        if (entry.id() == id)
            return entry;
    return std::nullopt;
}

void IrCatalogue::rescan()
{
    stopThread (5000);
    auto scanned = scanLibrary (getRoot());

    {
        const juce::ScopedLock scope (lock);
        entries = std::move (scanned);
        profiles.clear();
    }

    sendChangeMessage();
    startThread (juce::Thread::Priority::low);
}

void IrCatalogue::run()
{
    const auto snapshot = getEntries();
    auto cacheChanged = false;

    for (const auto& entry : snapshot)
    {
        if (threadShouldExit())
            return;

        const auto file = entry.fileForRate (analysisRate);
        const auto key = cacheKey (file);
        std::optional<DecayProfile> profile;

        {
            const juce::ScopedLock scope (lock);
            if (const auto it = cache.find (key); it != cache.end())
                profile = it->second;
        }

        if (! profile)
        {
            double sampleRate = 0.0;
            const auto buffer = readIrFile (file, sampleRate);
            if (buffer.getNumSamples() == 0)
                continue;

            profile = analyse (buffer, sampleRate);
            cacheChanged = true;
        }

        {
            const juce::ScopedLock scope (lock);
            cache[key] = *profile;
            profiles[entry.id()] = *profile;
        }

        sendChangeMessage();
    }

    if (cacheChanged)
        saveCache();
}

juce::String IrCatalogue::cacheKey (const juce::File& file)
{
    return file.getFullPathName() + "|" + juce::String (file.getSize()) + "|"
         + juce::String (file.getLastModificationTime().toMilliseconds());
}

void IrCatalogue::loadCache()
{
    const auto parsed = juce::JSON::parse (cacheFile());
    const auto* object = parsed.getDynamicObject();
    if (object == nullptr)
        return;

    const juce::ScopedLock scope (lock);
    for (const auto& property : object->getProperties())
        if (auto profile = profileFromVar (property.value))
            cache[property.name.toString()] = *profile;
}

void IrCatalogue::saveCache() const
{
    auto* object = new juce::DynamicObject();
    {
        const juce::ScopedLock scope (lock);
        for (const auto& [key, profile] : cache)
            object->setProperty (juce::Identifier (key), profileToVar (profile));
    }

    const auto file = cacheFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (juce::var (object)));
}

} // namespace lucky
