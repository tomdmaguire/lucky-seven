#include "IrCatalogue.h"

namespace lucky
{

namespace
{
const juce::String legacyRootKey { "irRoot" };
const juce::String extraRootsKey { "irRoots" };
const juce::String favouritesKey { "favourites" };
const juce::String uiScaleKey { "uiScale" };

juce::File cacheFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Application Support")
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
    options.millisecondsBeforeSaving = 1000;
    settings.setStorageParameters (options);

    for (const auto& id : juce::StringArray::fromLines (settings.getUserSettings()->getValue (favouritesKey)))
        if (id.isNotEmpty())
            favourites.insert (id);

    if (const auto legacy = settings.getUserSettings()->getValue (legacyRootKey); legacy.isNotEmpty())
    {
        if (juce::File (legacy) != defaultRoot())
            storeExtraRoots ({ juce::File (legacy) });
        settings.getUserSettings()->removeValue (legacyRootKey);
        settings.saveIfNeeded();
    }

    loadCache();
    rescan();
    downloader.addChangeListener (this);
}

IrCatalogue::~IrCatalogue()
{
    downloader.removeChangeListener (this);
    stopThread (5000);
}

juce::File IrCatalogue::defaultRoot()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Lucky Seven").getChildFile ("IRs");
}

juce::File IrCatalogue::getRoot() const
{
    if (const auto override = juce::SystemStats::getEnvironmentVariable ("LUCKY_SEVEN_IR_ROOT_OVERRIDE", {}); override.isNotEmpty())
        return juce::File (override);
    return defaultRoot();
}

juce::Array<juce::File> IrCatalogue::getExtraRoots() const
{
    if (juce::SystemStats::getEnvironmentVariable ("LUCKY_SEVEN_IR_ROOT_OVERRIDE", {}).isNotEmpty())
        return {};

    if (const auto extra = juce::SystemStats::getEnvironmentVariable ("LUCKY_SEVEN_EXTRA_IR_ROOT", {}); extra.isNotEmpty())
        return { juce::File (extra) };

    const juce::ScopedLock scope (lock);
    juce::Array<juce::File> folders;
    for (const auto& path : juce::StringArray::fromLines (settings.getUserSettings()->getValue (extraRootsKey)))
        if (path.isNotEmpty())
            folders.addIfNotAlreadyThere (juce::File (path));
    return folders;
}

juce::Array<juce::File> IrCatalogue::getRoots() const
{
    auto roots = getExtraRoots();
    roots.insert (0, getRoot());
    return roots;
}

void IrCatalogue::storeExtraRoots (const juce::Array<juce::File>& folders)
{
    juce::StringArray paths;
    for (const auto& folder : folders)
        paths.add (folder.getFullPathName());

    const juce::ScopedLock scope (lock);
    settings.getUserSettings()->setValue (extraRootsKey, paths.joinIntoString ("\n"));
    settings.saveIfNeeded();
}

void IrCatalogue::addRoot (const juce::File& folder)
{
    auto folders = getExtraRoots();
    if (folder == getRoot() || folder.isAChildOf (getRoot()) || folders.contains (folder))
        return;

    folders.add (folder);
    storeExtraRoots (folders);
    rescan();
}

void IrCatalogue::removeRoot (const juce::File& folder)
{
    auto folders = getExtraRoots();
    folders.removeAllInstancesOf (folder);
    storeExtraRoots (folders);
    rescan();
}

void IrCatalogue::downloadBricastiIrs()
{
    downloader.start (IrDownloader::bricastiArchive, getRoot());
}

void IrCatalogue::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (downloader.getStatus().stage == IrDownloader::Stage::finished)
        rescan();
}

float IrCatalogue::getUiScale() const
{
    const juce::ScopedLock scope (lock);
    return static_cast<float> (settings.getUserSettings()->getDoubleValue (uiScaleKey, 1.0));
}

void IrCatalogue::setUiScale (float scale)
{
    const juce::ScopedLock scope (lock);
    if (! juce::approximatelyEqual (static_cast<float> (settings.getUserSettings()->getDoubleValue (uiScaleKey, 1.0)), scale))
        settings.getUserSettings()->setValue (uiScaleKey, scale);
}

bool IrCatalogue::isFavourite (const juce::String& id) const
{
    const juce::ScopedLock scope (lock);
    return favourites.count (id) > 0;
}

void IrCatalogue::toggleFavourite (const juce::String& id)
{
    {
        const juce::ScopedLock scope (lock);
        if (! favourites.erase (id))
            favourites.insert (id);

        juce::StringArray lines;
        for (const auto& favourite : favourites)
            lines.add (favourite);

        settings.getUserSettings()->setValue (favouritesKey, lines.joinIntoString ("\n"));
        settings.saveIfNeeded();
    }
    sendChangeMessage();
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

    const auto root = getRoot();
    if (root == defaultRoot() && ! root.exists())
        root.createDirectory();

    auto scanned = scanLibrary (getRoots());

    {
        const juce::ScopedLock scope (lock);
        entries = std::move (scanned);
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

        const auto key = cacheKey (entry);
        std::optional<DecayProfile> profile;

        {
            const juce::ScopedLock scope (lock);
            if (const auto it = cache.find (key); it != cache.end())
                profile = it->second;
        }

        if (! profile)
        {
            double sampleRate = 0.0;
            const auto buffer = readIrEntry (entry, analysisRate, sampleRate);
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

juce::String IrCatalogue::cacheKey (const IrEntry& entry)
{
    const auto describe = [] (const juce::File& file)
    {
        return file.getFullPathName() + "|" + juce::String (file.getSize()) + "|"
             + juce::String (file.getLastModificationTime().toMilliseconds());
    };

    const auto key = "v2|" + describe (entry.fileForRate (analysisRate));
    return entry.isPair() ? key + "|" + describe (entry.rightFile) : key;
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
