#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 900;
constexpr int editorHeight = 600;
constexpr int topBarHeight = 48;
constexpr int stripHeight = 150;
constexpr int logoWidth = 196;
constexpr int tabWidth = 96;
constexpr int pillWidth = 340;
constexpr int iconWidth = 52;
constexpr int eqWidth = 300;
constexpr int knobSize = 88;

std::unique_ptr<juce::Drawable> makeFolderIcon (juce::Colour colour)
{
    juce::Path path;
    path.startNewSubPath (2.0f, 5.0f);
    path.lineTo (8.0f, 5.0f);
    path.lineTo (10.0f, 7.0f);
    path.lineTo (20.0f, 7.0f);
    path.lineTo (20.0f, 18.0f);
    path.lineTo (2.0f, 18.0f);
    path.closeSubPath();

    auto drawable = std::make_unique<juce::DrawablePath>();
    drawable->setPath (path);
    drawable->setFill (juce::Colours::transparentBlack);
    drawable->setStrokeFill (colour);
    drawable->setStrokeType (juce::PathStrokeType (1.4f));
    return drawable;
}
} // namespace

LuckySevenEditor::LuckySevenEditor (LuckySevenProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      browser ([this] (const juce::String& id) { audioProcessor.selectIr (id); }),
      eq (*p.getParameters().getParameter (LuckySevenProcessor::lowCutId),
          *p.getParameters().getParameter (LuckySevenProcessor::highCutId))
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
    previousButton.setTooltip ("Previous program");
    nextButton.setTooltip ("Next program");
    addAndMakeVisible (previousButton);
    addAndMakeVisible (nextButton);

    const auto idleIcon = makeFolderIcon (lucky::theme::textDim);
    const auto hoverIcon = makeFolderIcon (lucky::theme::text);
    folderButton.setImages (idleIcon.get(), hoverIcon.get());
    folderButton.setEdgeIndent (14);
    folderButton.onClick = [this] { chooseFolder(); };
    folderButton.setTooltip ("IR folder: " + audioProcessor.getCatalogue().getRoot().getFullPathName());
    addAndMakeVisible (folderButton);

    setupKnob (predelay, LuckySevenProcessor::predelayId, "PRE-DELAY");
    setupKnob (decay, LuckySevenProcessor::decayId, "DECAY");
    setupKnob (mix, LuckySevenProcessor::mixId, "MIX");
    setupLabel (eqLabel, "REVERB EQ");
    addAndMakeVisible (eq);

    predelay.slider.textFromValueFunction = [] (double ms) { return juce::String (juce::roundToInt (ms)) + " ms"; };
    mix.slider.textFromValueFunction = [] (double percent) { return juce::String (juce::roundToInt (percent)) + "%"; };
    decay.slider.textFromValueFunction = [this] (double percent)
    {
        const auto original = audioProcessor.getOriginalT60();
        return original > 0.0f ? lucky::theme::formatSeconds (original * static_cast<float> (percent) / 100.0f)
                               : juce::String (juce::roundToInt (percent)) + "%";
    };
    decay.slider.setTooltip ("Shortens the reverb tail; 100% is the original decay");

    audioProcessor.getCatalogue().addChangeListener (this);
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

void LuckySevenEditor::setView (lucky::BrowserPanel::View view)
{
    browser.setView (view);
    mapTab.setToggleState (view == lucky::BrowserPanel::View::map, juce::dontSendNotification);
    listTab.setToggleState (view == lucky::BrowserPanel::View::list, juce::dontSendNotification);
    audioProcessor.getParameters().state.setProperty (LuckySevenProcessor::viewProperty,
                                                      lucky::BrowserPanel::viewToString (view), nullptr);
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
        items.push_back ({ std::move (entry), profile });
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
    accent = item != nullptr ? lucky::theme::categoryColour (item->entry.categoryIndex) : lucky::theme::accent;

    for (auto* component : std::initializer_list<juce::Component*> { &predelay.slider, &decay.slider, &mix.slider, &mapTab, &listTab })
    {
        component->setColour (juce::Slider::rotarySliderFillColourId, accent);
        component->repaint();
    }

    eq.setAccent (accent);
}

void LuckySevenEditor::stepProgram (int delta)
{
    if (items.empty())
        return;

    const auto current = std::find_if (items.begin(), items.end(), [this] (const auto& item) { return item.entry.id() == shownSelection; });
    const auto index = current == items.end() ? 0 : static_cast<int> (std::distance (items.begin(), current));
    const auto count = static_cast<int> (items.size());
    audioProcessor.selectIr (items[static_cast<size_t> ((index + delta + count) % count)].entry.id());
}

void LuckySevenEditor::timerCallback()
{
    browser.setDecayRatio (static_cast<float> (decay.slider.getValue() / 100.0));
    decay.slider.repaint();

    if (const auto selected = audioProcessor.getSelectedIrId(); selected != shownSelection)
    {
        shownSelection = selected;
        browser.setSelectedId (selected);
        applyAccent();
        repaint (getLocalBounds().removeFromTop (topBarHeight));
    }
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
                              folderButton.setTooltip ("IR folder: " + folder.getFullPathName());
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
    for (const auto x : { logoWidth, logoWidth + tabWidth * 2, getWidth() - iconWidth })
        g.fillRect (x, 0, 1, topBarHeight);

    const auto pill = previousButton.getBounds().getUnion (nextButton.getBounds()).toFloat();
    g.setColour (theme::background);
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);

    if (const auto* item = selectedItem())
    {
        const auto textArea = pill.withTrimmedLeft (32.0f).withTrimmedRight (32.0f);
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
    mapTab.setBounds (topBar.removeFromLeft (tabWidth));
    listTab.setBounds (topBar.removeFromLeft (tabWidth));
    folderButton.setBounds (topBar.removeFromRight (iconWidth));

    auto pill = topBar.withSizeKeepingCentre (pillWidth, 30);
    pill.setX (topBar.getRight() - pillWidth - 16);
    previousButton.setBounds (pill.removeFromLeft (30));
    nextButton.setBounds (pill.removeFromRight (30));

    auto strip = bounds.removeFromBottom (stripHeight).reduced (24, 12);
    const auto knobSlot = (strip.getWidth() - eqWidth) / 3;
    const auto placeKnob = [&] (Knob& knob)
    {
        auto slot = strip.removeFromLeft (knobSlot);
        knob.label.setBounds (slot.removeFromTop (18));
        knob.slider.setBounds (slot.withSizeKeepingCentre (knobSize, knobSize));
    };

    placeKnob (predelay);
    placeKnob (decay);
    auto eqSlot = strip.removeFromLeft (eqWidth);
    eqLabel.setBounds (eqSlot.removeFromTop (18));
    eq.setBounds (eqSlot.reduced (0, 6));
    placeKnob (mix);

    browser.setBounds (bounds);
}
