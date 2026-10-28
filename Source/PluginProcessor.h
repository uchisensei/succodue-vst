#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/ChainProcessor.h"
#include "params/ParameterManager.h"
#include "presets/PresetManager.h"

// ---------------------------------------------------------------------------
// SuccoAudioProcessor - top-level JUCE AudioProcessor.
// Owns the APVTS, the DSP chain, and the preset manager.
// ---------------------------------------------------------------------------

namespace succo
{

class SuccoAudioProcessor : public juce::AudioProcessor
{
public:
    SuccoAudioProcessor();
    ~SuccoAudioProcessor() override = default;

    // -------- AudioProcessor interface --------
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SUCCO"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // -------- Public accessors for UI --------
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    ChainProcessor& getChain() { return chain; }
    PresetManager& getPresetManager() { return presets; }

    // Last measured output level (for UI meter)
    float getOutputLevelLin() const { return outputLevelLin.load(); }

    // Gain reduction data for UI meters
    float getCompressorGR() const { return compressorGR.load(); }
    float getLimiterGR() const   { return limiterGR.load(); }

private:
    juce::AudioProcessorValueTreeState apvts;
    ChainProcessor chain;
    PresetManager presets;

    std::atomic<float> outputLevelLin { 0.0f };
    std::atomic<float> compressorGR { 0.0f };
    std::atomic<float> limiterGR { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuccoAudioProcessor)
};

} // namespace succo
