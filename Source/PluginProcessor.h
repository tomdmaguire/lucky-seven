#pragma once

#include "dsp/IrLoader.h"
#include "dsp/ReverbEngine.h"
#include "ir/IrCatalogue.h"
#include <juce_audio_processors/juce_audio_processors.h>

class LuckySevenProcessor final : public juce::AudioProcessor
{
public:
    static inline const juce::String predelayId { "predelay" };
    static inline const juce::String decayId { "decay" };
    static inline const juce::String mixId { "mix" };
    static inline const juce::String blendId { "blend" };
    static inline const juce::String outputModeId { "outputMode" };
    static inline const juce::StringArray outputModes { "Mix", "Blend", "Send" };
    static constexpr float blendSilentDb = -60.0f;
    static inline const juce::String lowCutId { "lowCut" };
    // Keeps the ID of the High Cut parameter it replaced, so saved sessions restore their setting.
    static inline const juce::String dampingId { "highCut" };
    static inline const juce::String widthId { "width" };
    static constexpr float lowCutMaxHz = 1000.0f;
    static constexpr float dampingMinHz = 250.0f;
    static constexpr float legacyDampingMinHz = 1000.0f;
    static inline const juce::Identifier irIdProperty { "irId" };
    static inline const juce::Identifier viewProperty { "view" };
    static inline const juce::Identifier mapFilterProperty { "mapFilter" };
    static inline const juce::Identifier activeSlotProperty { "abSlot" };
    static inline const juce::Identifier slotType { "Slot" };
    static inline const juce::Identifier stateVersionProperty { "stateVersion" };
    static constexpr int stateVersion = 2;
    static inline const juce::Identifier slotIndexProperty { "index" };

    LuckySevenProcessor();
    ~LuckySevenProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Selects the program with the given catalogue id and starts loading it. */
    void selectIr (const juce::String& id);

    /** Index (0 = A, 1 = B) of the comparison slot currently loaded. */
    int getActiveSlot() const;

    /** Stores the current settings in the active slot and loads the other one; the first switch copies A to B. Message thread only. */
    void switchToSlot (int slot);

    /** Catalogue id of the selected program. */
    juce::String getSelectedIrId() const;

    /** Untouched mid-band T60 of the loaded program in seconds. */
    float getOriginalT60() const noexcept { return loader->getOriginalT60(); }

    /** Decay in percent that the reverb is currently playing, which lags the knob while an IR rebuilds; -1 before loading. */
    float getAppliedDecayPercent() const noexcept { return std::max (-1.0f, loader->getInstalledDecayRatio() * 100.0f); }

    /** Damping frequency that the reverb is currently playing, which lags the knob while an IR rebuilds; -1 before loading. */
    float getAppliedDampingHz() const noexcept { return loader->getInstalledDampingHz(); }

    /** Input and reverb peaks since the last call, for the UI's activity display. */
    std::pair<float, float> takePeaks() noexcept { return engine.takePeaks(); }

    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }
    lucky::IrCatalogue& getCatalogue() noexcept { return *catalogue; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void requestSelectedFile();
    juce::ValueTree captureSlot (int slot) const;
    void applySlot (const juce::ValueTree& snapshot);
    juce::ValueTree findSlot (int slot) const;

    juce::AudioProcessorValueTreeState parameters;
    juce::SharedResourcePointer<lucky::IrCatalogue> catalogue;
    lucky::ReverbEngine engine;
    std::unique_ptr<lucky::IrLoader> loader;
    std::atomic<float>* predelayMs = nullptr;
    std::atomic<float>* mixPercent = nullptr;
    std::atomic<float>* blendDb = nullptr;
    std::atomic<float>* outputMode = nullptr;
    std::atomic<float>* lowCutHz = nullptr;
    std::atomic<float>* widthPercent = nullptr;
    double hostSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LuckySevenProcessor)
};
