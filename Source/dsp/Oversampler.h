#pragma once

// ---------------------------------------------------------------------------
// Oversampler - 2x/4x oversampling helper using juce::dsp::Oversampling
// Used by Drive and Limiter to reduce aliasing on nonlinear stages.
// ---------------------------------------------------------------------------

#include <juce_dsp/juce_dsp.h>

namespace succo
{

class Oversampler
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec, int factor)
    {
        const auto numChannels = static_cast<juce::uint32> (spec.numChannels);
        os.clear();

        if (factor == 2)
        {
            os.push_back (std::make_unique<juce::dsp::Oversampling<float>>
                (numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true));
        }
        else if (factor == 4)
        {
            os.push_back (std::make_unique<juce::dsp::Oversampling<float>>
                (numChannels, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true));
        }
        else
        {
            os.push_back (std::make_unique<juce::dsp::Oversampling<float>>
                (numChannels, 0, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true));
        }

        for (auto& o : os)
            o->initProcessing (spec.maximumBlockSize);

        upSpec = spec;
        upSpec.maximumBlockSize = os.front()->getOversamplingFactor() * spec.maximumBlockSize;
        upSpec.sampleRate = spec.sampleRate * os.front()->getOversamplingFactor();
    }

    void reset()
    {
        for (auto& o : os)
            o->reset();
    }

    // Up-sample a stereo block, process with f, then down-sample.
    template <typename ProcessFn>
    void process (juce::AudioBuffer<float>& buffer, ProcessFn&& processUp)
    {
        if (os.empty())
        {
            processUp (buffer);
            return;
        }

        juce::dsp::AudioBlock<float> inBlock (buffer);
        // processSamplesUp returns a block aliasing internal oversampled memory
        auto upBlock = os.front()->processSamplesUp (inBlock);

        // Copy to a temp buffer the lambda can safely modify
        juce::AudioBuffer<float> upBuffer ((int) upBlock.getNumChannels(),
                                           (int) upBlock.getNumSamples());
        juce::dsp::AudioBlock<float> upBufBlock (upBuffer);
        upBufBlock.copyFrom (upBlock);

        processUp (upBuffer);

        // Copy the modified up-sampled data back into the oversampler's internal memory
        upBlock.copyFrom (upBufBlock);

        // processSamplesDown in JUCE 8 takes ONLY the output block (single arg).
        // It reads from the internal oversampled memory (which we just updated).
        os.front()->processSamplesDown (inBlock);
    }

    double getUpSampleRate() const { return upSpec.sampleRate; }
    int getFactor() const { return os.empty() ? 1 : os.front()->getOversamplingFactor(); }

private:
    std::vector<std::unique_ptr<juce::dsp::Oversampling<float>>> os;
    juce::dsp::ProcessSpec upSpec {};
};

} // namespace succo
