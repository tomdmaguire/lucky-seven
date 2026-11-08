#include "PluginEditor.h"
#include "ui/Theme.h"

namespace
{
constexpr int editorWidth = 860;
constexpr int editorHeight = 560;
constexpr int headerHeight = 52;
constexpr int knobRowHeight = 140;
constexpr int eqWidth = 300;
constexpr int margin = 16;
} // namespace

LuckySevenEditor::LuckySevenEditor (LuckySevenProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      browser ([this] (const juce::String& id) { audioProcessor.selectIr (id); },
               [this] (lucky::BrowserPanel::View view)
               {
                   audioProcessor.getParameters().state.setProperty (LuckySevenProcessor::viewProperty,
                                                                     lucky::BrowserPanel::viewToString (view), nullptr);
               }),
      eq (*p.getParameters().getParameter (LuckySevenProcessor::lowCutId),
          *p.getParameters().getParameter (LuckySevenProcessor::highCutId))
{
    setLookAndFeel (&lookAndFeel);

    browser.setView (lucky::BrowserPanel::viewFromString (
        audioProcessor.getParameters().state.getProperty (LuckySevenProcessor::viewProperty, "map").toString()));
    addAndMakeVisible (browser);

    folderButton.onClick = [this] { chooseFolder(); };
    folderButton.setTooltip (audioProcessor.getCatalogue().getRoot().getFullPathName());
    addAndMakeVisible (folderButton);

    setupKnob (predelay, LuckySevenProcessor::predelayId, "PRE-DELAY");
    setupKnob (decay, LuckySevenProcessor::decayId, "DECAY");
    setupKnob (mix, LuckySevenProcessor::mixId, "MIX");

    eqLabel.setText ("REVERB EQ", juce::dontSendNotification);
    eqLabel.setJustificationType (juce::Justification::centred);
    eqLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    eqLabel.setColour (juce::Label::textColourId, lucky::theme::textDim);
    addAndMakeVisible (eqLabel);
    addAndMakeVisible (eq);

    predelay.slider.textFromValueFunction = [] (double ms) { return juce::String (juce::roundToInt (ms)) + " ms"; };
    mix.slider.textFromValueFunction = [] (double percent) { return juce::String (juce::roundToInt (percent)) + "%"; };
    decay.slider.textFromValueFunction = [this] (double percent)
    {
        const auto original = audioProcessor.getOriginalT60();
        if (original <= 0.0f)
            return juce::String (juce::roundToInt (percent)) + "%";
        return lucky::theme::formatSeconds (original * static_cast<float> (percent) / 100.0f)
             + (percent >= 99.95 ? juce::String() : " of " + lucky::theme::formatSeconds (original));
    };
    for (auto* knob : { &predelay, &decay, &mix })
        knob->slider.updateText();

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

void LuckySevenEditor::setupKnob (Knob& knob, const juce::String& parameterId, const juce::String& name)
{
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 110, 20);
    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getParameters(), parameterId, knob.slider);

    knob.label.setText (name, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    knob.label.setColour (juce::Label::textColourId, lucky::theme::textDim);

    addAndMakeVisible (knob.slider);
    addAndMakeVisible (knob.label);
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
    repaint();
}

void LuckySevenEditor::timerCallback()
{
    browser.setDecayRatio (static_cast<float> (decay.slider.getValue() / 100.0));
    decay.slider.updateText();

    if (const auto selected = audioProcessor.getSelectedIrId(); selected != shownSelection)
    {
        shownSelection = selected;
        browser.setSelectedId (selected);
        repaint (getLocalBounds().removeFromTop (headerHeight));
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
                              folderButton.setTooltip (folder.getFullPathName());
                              audioProcessor.selectIr (audioProcessor.getSelectedIrId());
                          });
}

juce::String LuckySevenEditor::headerText() const
{
    for (const auto& item : items)
        if (item.entry.id() == shownSelection)
            return item.entry.category + juce::String::fromUTF8 ("  \xc2\xb7  ") + juce::String (item.entry.number).paddedLeft ('0', 2) + " " + item.entry.name;
    return "No IR loaded";
}

void LuckySevenEditor::paint (juce::Graphics& g)
{
    g.fillAll (lucky::theme::background);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (margin, 0);
    g.setColour (lucky::theme::accent);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("LUCKY SEVEN", header.removeFromLeft (170), juce::Justification::centredLeft);

    g.setColour (lucky::theme::text);
    g.setFont (juce::FontOptions (16.0f));
    g.drawText (headerText(), header.withTrimmedRight (130), juce::Justification::centredLeft);
}

void LuckySevenEditor::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop (headerHeight).reduced (margin, 12);
    folderButton.setBounds (header.removeFromRight (110));

    auto knobRow = bounds.removeFromBottom (knobRowHeight).reduced (margin, 8);
    const auto knobWidth = (knobRow.getWidth() - eqWidth) / 3;
    const auto placeKnob = [&] (Knob& knob)
    {
        auto slot = knobRow.removeFromLeft (knobWidth);
        knob.label.setBounds (slot.removeFromTop (18));
        knob.slider.setBounds (slot.withSizeKeepingCentre (130, slot.getHeight()));
    };

    placeKnob (predelay);
    placeKnob (decay);
    auto eqSlot = knobRow.removeFromLeft (eqWidth);
    eqLabel.setBounds (eqSlot.removeFromTop (18));
    eq.setBounds (eqSlot.reduced (0, 4));
    placeKnob (mix);

    browser.setBounds (bounds.reduced (margin, 0));
}
