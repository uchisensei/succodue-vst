#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace succo
{

SuccoAudioProcessor::SuccoAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SUCCO_PARAMS", ParameterManager::buildLayout()),
      presets (apvts)
{
}

bool SuccoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Support mono->mono, mono->stereo, stereo->stereo
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (in == out)
        return true;

    if (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo())
        return true;

    return false;
}

void SuccoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    chain.prepare (spec);
}

void SuccoAudioProcessor::releaseResources()
{
    chain.reset();
}

void SuccoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples == 0) return;

    // Clear extra channels
    for (int c = juce::jmax (1, numChannels); c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    // Mono to stereo if needed
    if (numChannels == 1 && getMainBusNumOutputChannels() == 2)
    {
        // We'll keep mono processing as the chain supports it
    }

    // Update chain params from APVTS
    auto cp = ParameterManager::toChainParams (apvts);
    chain.setParams (cp);

    // Process
    chain.process (buffer);

    // Compute output level for meter
    float peak = 0.0f;
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int n = 0; n < buffer.getNumSamples(); ++n)
            peak = juce::jmax (peak, std::abs (buffer.getSample (c, n)));
    outputLevelLin.store (peak);

    // Push GR data to UI meters
    compressorGR.store (chain.getCompressor().getGainReductionDb());
    limiterGR.store (chain.getLimiter().getGainReductionDb());
}

juce::AudioProcessorEditor* SuccoAudioProcessor::createEditor()
{
    return new SuccoAudioProcessorEditor (*this);
}

int SuccoAudioProcessor::getNumPrograms()
{
    return presets.getNumPresets();
}

int SuccoAudioProcessor::getCurrentProgram()
{
    return presets.getCurrentPresetIndex();
}

void SuccoAudioProcessor::setCurrentProgram (int index)
{
    presets.loadPreset (index);
}

const juce::String SuccoAudioProcessor::getProgramName (int index)
{
    return presets.getPresetName (index);
}

void SuccoAudioProcessor::changeProgramName (int, const juce::String&)
{
    // Not supported - presets are managed by PresetManager
}

void SuccoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("presetIndex", presets.getCurrentPresetIndex());
    copyXmlToBinary (*xml, destData);
}

void SuccoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        int presetIdx = xml->getIntAttribute ("presetIndex", -1);
        if (presetIdx >= 0)
            presets.loadPreset (presetIdx);
    }
}

} // namespace succo

// ---------------------------------------------------------------------------
// JUCE plugin entry points
// ---------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new succo::SuccoAudioProcessor();
}
