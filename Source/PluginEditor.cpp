#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 900;
constexpr int editorHeight = 600;
constexpr int topBarHeight = 48;
constexpr int stripHeight = 156;
constexpr int logoWidth = 196;
constexpr int tabWidth = 96;
constexpr int pillWidth = 340;
constexpr int iconWidth = 52;
constexpr int knobSize = 84;

juce::String formatHz (double hertz)
{
    return hertz >= 1000.0 ? juce::String (hertz / 1000.0, hertz >= 10000.0 ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (hertz)) + " Hz";
}

std::unique_ptr<juce::Drawable> makeMenuIcon (juce::Colour colour)
{
    juce::Path path;
    for (const auto x : { 4.0f, 11.0f, 18.0f })
        path.addEllipse (x - 1.6f, 9.4f, 3.2f, 3.2f);

    auto drawable = std::make_unique<juce::DrawablePath>();
    drawable->setPath (path);
    drawable->setFill (colour);
    return drawable;
}
} // namespace

LuckySevenEditor::LuckySevenEditor (LuckySevenProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      browser ([this] (const juce::String& id) { audioProcessor.selectIr (id); },
               [this] (const juce::String& id) { audioProcessor.getCatalogue().toggleFavourite (id); })
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (browser);

    for (auto* tab : { &mapTab, &listTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (7001);
        addAndMakeVisible (*tab);
    }
    mapTab.onClick = [this] { if (mapTab.getToggleState()) setView (lucky::BrowserPanel::View::map); };
    listTab.onClick = [this] { if (listTab.getToggleState()) setView (lucky::BrowserPanel::View::list); };
    setView (lucky::BrowserPanel::viewFromString (
        audioProcessor.getParameters().state.getProperty (LuckySevenProcessor::viewProperty, "map").toString()));

    previousButton.onClick = [this] { stepProgram (-1); };
    nextButton.onClick = [this] { stepProgram (1); };
    previousButton.getProperties().set ("round", true);
    nextButton.getProperties().set ("round", true);
    previousButton.setTooltip ("Previous program");
    nextButton.setTooltip ("Next program");
    addAndMakeVisible (previousButton);
    addAndMakeVisible (nextButton);

    programButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    programButton.onClick = [this]
    {
        setView (listTab.getToggleState() ? lucky::BrowserPanel::View::map : lucky::BrowserPanel::View::list);
    };
    addAndMakeVisible (programButton);

    favouriteButton.onClick = [this] { audioProcessor.getCatalogue().toggleFavourite (shownSelection); };
    addAndMakeVisible (favouriteButton);

    const auto idleIcon = makeMenuIcon (lucky::theme::textDim);
    const auto hoverIcon = makeMenuIcon (lucky::theme::text);
    menuButton.setImages (idleIcon.get(), hoverIcon.get());
    menuButton.setEdgeIndent (16);
    menuButton.onClick = [this] { showMenu(); };
    menuButton.setTooltip ("IR folder options");
    addAndMakeVisible (menuButton);

    setupKnob (predelay, LuckySevenProcessor::predelayId, "PRE-DELAY");
    setupKnob (decay, LuckySevenProcessor::decayId, "DECAY");
    setupKnob (lowCut, LuckySevenProcessor::lowCutId, "LOW CUT");
    setupKnob (highCut, LuckySevenProcessor::highCutId, "HIGH CUT");
    setupKnob (mix, LuckySevenProcessor::mixId, "MIX");
    setupRangeLabels (lowCut, "20 Hz", "1 kHz");
    setupRangeLabels (highCut, "1 kHz", "20 kHz");

    lowCut.slider.textFromValueFunction = [] (double hertz) { return hertz <= lucky::ReverbEngine::minCutHz + 0.5 ? juce::String ("OFF") : formatHz (hertz); };
    highCut.slider.textFromValueFunction = [] (double hertz) { return hertz >= lucky::ReverbEngine::maxCutHz - 1.0 ? juce::String ("OFF") : formatHz (hertz); };
    lowCut.slider.getProperties().set ("cut", "low");
    highCut.slider.getProperties().set ("cut", "high");
    lowCut.slider.setTooltip ("Low cut on the reverb only, 12 dB/oct");
    highCut.slider.setTooltip ("High cut on the reverb only, 12 dB/oct");

    predelay.slider.textFromValueFunction = [] (double ms) { return juce::String (juce::roundToInt (ms)) + " ms"; };
    mix.label.setVisible (false);
    for (size_t i = 0; i < modeButtons.size(); ++i)
    {
        auto& button = modeButtons[i];
        button.setButtonText (LuckySevenProcessor::outputModes[static_cast<int> (i)].toUpperCase());
        button.getProperties().set ("small", true);
        button.setClickingTogglesState (false);
        button.onClick = [this, i] { modeAttachment->setValueAsCompleteGesture (static_cast<float> (i)); };
        addAndMakeVisible (button);
    }
    modeButtons[0].setTooltip ("Mix: crossfade between dry and reverb");
    modeButtons[1].setTooltip ("Blend: dry stays at 0 dB, the knob sets the reverb level");
    modeButtons[2].setTooltip ("Send: 100% reverb, for return tracks");

    modeAttachment = std::make_unique<juce::ParameterAttachment> (
        *audioProcessor.getParameters().getParameter (LuckySevenProcessor::outputModeId),
        [this] (float value) { showOutputMode (juce::roundToInt (value)); });
    modeAttachment->sendInitialUpdate();
    decay.slider.textFromValueFunction = [this] (double percent)
    {
        const auto original = audioProcessor.getOriginalT60();
        return original > 0.0f ? lucky::theme::formatSeconds (original * static_cast<float> (percent) / 100.0f)
                               : juce::String (juce::roundToInt (percent)) + "%";
    };
    decay.slider.setTooltip ("Shortens the reverb tail; 100% is the original decay");

    audioProcessor.getCatalogue().addChangeListener (this);
    audioProcessor.getCatalogue().rescan();
    refreshItems();

    setSize (editorWidth, editorHeight);
    startTimerHz (20);
}

LuckySevenEditor::~LuckySevenEditor()
{
    audioProcessor.getCatalogue().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void LuckySevenEditor::setupLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (lucky::theme::labelFont (11.0f));
    label.setColour (juce::Label::textColourId, lucky::theme::textDim);
    addAndMakeVisible (label);
}

void LuckySevenEditor::setupKnob (Knob& knob, const juce::String& parameterId, const juce::String& name)
{
    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getParameters(), parameterId, knob.slider);
    setupLabel (knob.label, name);
    addAndMakeVisible (knob.slider);
}

void LuckySevenEditor::setupRangeLabels (Knob& knob, const juce::String& minText, const juce::String& maxText)
{
    const auto style = [this] (juce::Label& label, const juce::String& text, juce::Justification justification)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (justification);
        label.setFont (lucky::theme::labelFont (10.0f));
        label.setColour (juce::Label::textColourId, lucky::theme::textDim.withAlpha (0.8f));
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
    };

    style (knob.minLabel, minText, juce::Justification::centredRight);
    style (knob.maxLabel, maxText, juce::Justification::centredLeft);
}

void LuckySevenEditor::setView (lucky::BrowserPanel::View view)
{
    browser.setView (view);
    mapTab.setToggleState (view == lucky::BrowserPanel::View::map, juce::dontSendNotification);
    programButton.setTooltip (view == lucky::BrowserPanel::View::map ? "Show the program list" : "Back to the sound map");
    listTab.setToggleState (view == lucky::BrowserPanel::View::list, juce::dontSendNotification);
    audioProcessor.getParameters().state.setProperty (LuckySevenProcessor::viewProperty,
                                                      lucky::BrowserPanel::viewToString (view), nullptr);
}

void LuckySevenEditor::showOutputMode (int mode)
{
    if (mode == outputMode)
        return;

    outputMode = mode;
    for (size_t i = 0; i < modeButtons.size(); ++i)
        modeButtons[i].setToggleState (static_cast<int> (i) == mode, juce::dontSendNotification);

    mix.attachment.reset();

    if (mode == 2)
    {
        mix.slider.setRange (0.0, 100.0);
        mix.slider.setValue (100.0, juce::dontSendNotification);
        mix.slider.textFromValueFunction = [] (double) { return juce::String ("100%"); };
    }
    else
    {
        const auto& parameterId = mode == 1 ? LuckySevenProcessor::blendId : LuckySevenProcessor::mixId;
        mix.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            audioProcessor.getParameters(), parameterId, mix.slider);

        if (mode == 1)
            mix.slider.textFromValueFunction = [] (double db)
            {
                return db <= LuckySevenProcessor::blendSilentDb ? juce::String ("-INF dB") : juce::String (db, 1) + " dB";
            };
        else
            mix.slider.textFromValueFunction = [] (double percent) { return juce::String (juce::roundToInt (percent)) + "%"; };
    }

    mix.slider.setEnabled (mode != 2);
    mix.slider.setTooltip (mode == 2 ? "Send mode: 100% reverb, set the level with the return fader" : juce::String());
    mix.slider.repaint();
}

void LuckySevenEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshItems();
}

void LuckySevenEditor::refreshItems()
{
    auto& catalogue = audioProcessor.getCatalogue();
    items.clear();
    for (auto& entry : catalogue.getEntries())
    {
        const auto profile = catalogue.getProfile (entry.id());
        const auto favourite = catalogue.isFavourite (entry.id());
        items.push_back ({ std::move (entry), profile, favourite });
    }

    browser.setItems (items);
    shownSelection = audioProcessor.getSelectedIrId();
    browser.setSelectedId (shownSelection);
    applyAccent();
    repaint();
}

const lucky::BrowserItem* LuckySevenEditor::selectedItem() const
{
    for (const auto& item : items)
        if (item.entry.id() == shownSelection)
            return &item;
    return nullptr;
}

void LuckySevenEditor::applyAccent()
{
    const auto* item = selectedItem();
    favouriteButton.setToggleState (item != nullptr && item->favourite, juce::dontSendNotification);
    favouriteButton.setTooltip (item != nullptr && item->favourite ? "Remove from favourites" : "Add to favourites");
    accent = item != nullptr ? lucky::theme::categoryColour (item->entry.categoryIndex) : lucky::theme::accent;

    for (auto* component : std::initializer_list<juce::Component*> { &predelay.slider, &decay.slider, &lowCut.slider, &highCut.slider,
                                                                     &mix.slider, &mapTab, &listTab })
    {
        component->setColour (juce::Slider::rotarySliderFillColourId, accent);
        component->repaint();
    }

    for (auto& button : modeButtons)
    {
        button.setColour (juce::Slider::rotarySliderFillColourId, accent);
        button.repaint();
    }

    favouriteButton.setColour (juce::Slider::rotarySliderFillColourId, accent);
    favouriteButton.repaint();
}

void LuckySevenEditor::stepProgram (int delta)
{
    if (items.empty())
        return;

    auto ordered = items;
    std::sort (ordered.begin(), ordered.end(), lucky::alphabeticalOrder);

    const auto current = std::find_if (ordered.begin(), ordered.end(), [this] (const auto& item) { return item.entry.id() == shownSelection; });
    const auto index = current == ordered.end() ? 0 : static_cast<int> (std::distance (ordered.begin(), current));
    const auto count = static_cast<int> (ordered.size());
    audioProcessor.selectIr (ordered[static_cast<size_t> ((index + delta + count) % count)].entry.id());
}

void LuckySevenEditor::timerCallback()
{
    browser.setDecayRatio (static_cast<float> (decay.slider.getValue() / 100.0));

    if (const auto original = audioProcessor.getOriginalT60(); ! juce::approximatelyEqual (original, shownOriginalT60))
    {
        shownOriginalT60 = original;
        decay.slider.repaint();
    }

    if (const auto selected = audioProcessor.getSelectedIrId(); selected != shownSelection)
    {
        shownSelection = selected;
        browser.setSelectedId (selected);
        applyAccent();
        repaint (getLocalBounds().removeFromTop (topBarHeight));
    }
}

void LuckySevenEditor::showMenu()
{
    enum { rescanItem = 1, chooseItem, revealItem };
    const auto root = audioProcessor.getCatalogue().getRoot();

    juce::PopupMenu menu;
    menu.addSectionHeader ("IR folder: " + root.getFullPathName());
    menu.addItem (rescanItem, "Rescan IR folder");
    menu.addItem (chooseItem, "Choose IR folder...");
    menu.addItem (revealItem, "Show in Finder", root.exists());

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (menuButton),
                        [this, root] (int result)
                        {
                            if (result == rescanItem)
                                audioProcessor.getCatalogue().rescan();
                            else if (result == chooseItem)
                                chooseFolder();
                            else if (result == revealItem)
                                root.revealToUser();
                        });
}

void LuckySevenEditor::chooseFolder()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose the Bricasti IR folder", audioProcessor.getCatalogue().getRoot());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fileChooser)
                          {
                              const auto folder = fileChooser.getResult();
                              if (folder == juce::File())
                                  return;

                              audioProcessor.getCatalogue().setRoot (folder);
                              audioProcessor.selectIr (audioProcessor.getSelectedIrId());
                          });
}

void LuckySevenEditor::paint (juce::Graphics& g)
{
    using namespace lucky;
    g.fillAll (theme::background);

    auto topBar = getLocalBounds().removeFromTop (topBarHeight);
    g.setColour (theme::topBar);
    g.fillRect (topBar);

    auto logo = topBar.removeFromLeft (logoWidth).reduced (16, 0).toFloat();
    const auto badge = logo.removeFromLeft (26.0f).withSizeKeepingCentre (26.0f, 26.0f);
    g.setColour (accent);
    g.drawEllipse (badge.reduced (1.0f), 1.6f);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("7", badge, juce::Justification::centred);
    g.setColour (theme::text);
    g.setFont (theme::labelFont (15.0f).withStyle ("Bold"));
    g.drawText ("LUCKY SEVEN", logo.withTrimmedLeft (10.0f), juce::Justification::centredLeft);

    g.setColour (theme::background);
    for (const auto x : { logoWidth, getWidth() - iconWidth - tabWidth * 2 - 1, getWidth() - iconWidth })
        g.fillRect (x, 0, 1, topBarHeight);

    const auto pill = previousButton.getBounds().getUnion (nextButton.getBounds()).toFloat();
    g.setColour (theme::background);
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);

    if (const auto* item = selectedItem())
    {
        const auto textArea = pill.withTrimmedLeft (32.0f).withTrimmedRight (62.0f);
        g.setColour (accent);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ textArea.getX() + 4.0f, textArea.getCentreY() }));
        g.setColour (theme::textDim);
        g.setFont (theme::labelFont (10.0f));
        const auto category = item->entry.category.toUpperCase();
        const auto categoryWidth = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), category) + 10.0f;
        g.drawText (category, textArea.withTrimmedLeft (14.0f).withWidth (categoryWidth), juce::Justification::centredLeft);
        g.setColour (theme::text);
        g.setFont (theme::labelFont (13.0f));
        g.drawText (item->entry.name, textArea.withTrimmedLeft (14.0f + categoryWidth), juce::Justification::centredLeft);
    }

    auto stripBounds = getLocalBounds().removeFromBottom (stripHeight);
    g.setColour (theme::strip);
    g.fillRect (stripBounds);
    g.setColour (theme::outline);
    g.fillRect (stripBounds.removeFromTop (1));
}

void LuckySevenEditor::resized()
{
    auto bounds = getLocalBounds();

    auto topBar = bounds.removeFromTop (topBarHeight);
    topBar.removeFromLeft (logoWidth + 1);
    menuButton.setBounds (topBar.removeFromRight (iconWidth));
    listTab.setBounds (topBar.removeFromRight (tabWidth));
    mapTab.setBounds (topBar.removeFromRight (tabWidth));
    topBar.removeFromRight (1);

    auto pill = topBar.withSizeKeepingCentre (pillWidth, 30);
    pill.setX (topBar.getX() + 16);
    previousButton.setBounds (pill.removeFromLeft (30));
    nextButton.setBounds (pill.removeFromRight (30));
    favouriteButton.setBounds (pill.removeFromRight (28));
    programButton.setBounds (pill);

    auto strip = bounds.removeFromBottom (stripHeight).reduced (24, 12);
    const auto knobSlot = strip.getWidth() / 5;
    for (auto* knob : { &predelay, &decay, &lowCut, &highCut, &mix })
    {
        auto slot = strip.removeFromLeft (knobSlot);
        knob->label.setBounds (slot.removeFromTop (18));
        const auto knobBounds = slot.withSizeKeepingCentre (knobSize, knobSize);
        knob->slider.setBounds (knobBounds);

        if (knob == &mix)
        {
            auto modeRow = knob->label.getBounds().withSizeKeepingCentre (150, 18);
            for (auto& button : modeButtons)
                button.setBounds (modeRow.removeFromLeft (50));
        }

        const auto rangeRow = juce::Rectangle<int> (knobBounds.getX() - 44, knobBounds.getBottom() - 12, knobBounds.getWidth() + 88, 14);
        knob->minLabel.setBounds (rangeRow.withWidth (50));
        knob->maxLabel.setBounds (rangeRow.withTrimmedLeft (rangeRow.getWidth() - 50));
    }

    browser.setBounds (bounds);
}
