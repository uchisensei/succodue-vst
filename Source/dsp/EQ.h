#pragma once

// ---------------------------------------------------------------------------
// EQ - 4-band parametric + optional high-pass and low-pass filters.
// Uses juce::dsp::IIR::Coefficients with RBJ biquad filters.
// Bands: HPF (12 dB/oct), Low Shelf, Low-Mid Bell, High-Mid Bell, High Shelf, LPF.
// ---------------------------------------------------------------------------

#include <juce_dsp/juce_dsp.h>

namespace succo
{

class EQ
{
public:
    struct Params
    {
        bool  enabled       = true;
        bool  hpfOn        = false;
        float hpfFreq      = 80.0f;    // Hz
        float lowShelfFreq = 120.0f;
        float lowShelfGain = 0.0f;      // dB
        float lowMidFreq   = 350.0f;
        float lowMidQ      = 0.9f;
        float lowMidGain   = 0.0f;      // dB
        float highMidFreq  = 3000.0f;
        float highMidQ     = 0.9f;
        float highMidGain  = 0.0f;      // dB
        float highShelfFreq = 8000.0f;
        float highShelfGain = 0.0f;     // dB
        bool  lpfOn        = false;
        float lpfFreq      = 16000.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        for (auto& f : filters)
            f.prepare (spec);
        updateCoefficients();
    }

    void reset()
    {
        for (auto& f : filters)
            f.reset();
    }

    void setParams (const Params& p)
    {
        params = p;
        updateCoefficients();
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! params.enabled)
            return;

        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        if (params.hpfOn)
            filters[0].process (ctx);
        filters[1].process (ctx); // low shelf
        filters[2].process (ctx); // low-mid bell
        filters[3].process (ctx); // high-mid bell
        filters[4].process (ctx); // high shelf
        if (params.lpfOn)
            filters[5].process (ctx); // low-pass
    }

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    void updateCoefficients()
    {
        const double sr = sampleRate;
        // HPF
        filters[0].state = Coeffs::makeHighPass (sr, params.hpfFreq, 0.707f);
        // Low shelf
        filters[1].state = Coeffs::makeLowShelf (sr, params.lowShelfFreq, 0.707f,
            juce::Decibels::decibelsToGain (params.lowShelfGain));
        // Low-mid bell
        filters[2].state = Coeffs::makePeakFilter (sr, params.lowMidFreq, params.lowMidQ,
            juce::Decibels::decibelsToGain (params.lowMidGain));
        // High-mid bell
        filters[3].state = Coeffs::makePeakFilter (sr, params.highMidFreq, params.highMidQ,
            juce::Decibels::decibelsToGain (params.highMidGain));
        // High shelf
        filters[4].state = Coeffs::makeHighShelf (sr, params.highShelfFreq, 0.707f,
            juce::Decibels::decibelsToGain (params.highShelfGain));
        // LPF
        filters[5].state = Coeffs::makeLowPass (sr, params.lpfFreq, 0.707f);
    }

    Params params;
    double sampleRate = 44100.0;
    std::array<juce::dsp::ProcessorDuplicator<Filter, Coeffs>, 6> filters;
};

} // namespace succo
