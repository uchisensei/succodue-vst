#pragma once

// ---------------------------------------------------------------------------
// DeEsser - dynamic high-band compressor focused on sibilance.
// Splits signal into low band (via LPF) and high band (via HPF) with shared
// crossover frequency. Detects energy in high band and applies GR to it only.
// Parameters: freq (Hz crossover), threshold (dB), amount (0..1 of reduction).
// ---------------------------------------------------------------------------

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

namespace succo
{

class DeEsser
{
public:
    struct Params
    {
        bool  enabled    = true;
        float freq       = 6500.0f;
        float thresholdDb = -28.0f;
        float amount     = 0.5f;       // 0..1, fraction of GR applied
        float attackMs   = 1.0f;
        float releaseMs  = 60.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        lpFilter.prepare (spec);
        hpFilter.prepare (spec);
        updateCoefficients();
        reset();
    }

    void reset()
    {
        lpFilter.reset();
        hpFilter.reset();
        envState = 0.0f;
        grState  = 0.0f;
    }

    void setParams (const Params& p)
    {
        if (p.freq != params.freq)
            updateCoefficients();
        params = p;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! params.enabled)
            return;

        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numSamples == 0) return;

        const double sr = sampleRate;
        const double attackCoef  = std::exp (-1.0 / std::max (0.0001, params.attackMs  * 0.001 * sr));
        const double releaseCoef = std::exp (-1.0 / std::max (0.0001, params.releaseMs * 0.001 * sr));

        // Process per channel through separate HP/LP duplicates (we want HP path on a temp buffer)
        // For simplicity, process the whole buffer through LP into "lowBuf", HP into "highBuf",
        // then re-sum with applied gain.
        // We'll process channels sequentially.

        const float threshLin = std::pow (10.0f, params.thresholdDb / 20.0f);

        for (int c = 0; c < numChannels; ++c)
        {
            auto* channelData = buffer.getWritePointer (c);

            // Make copies through the filters
            // LP path
            juce::AudioBuffer<float> lowPath (1, numSamples);
            lowPath.copyFrom (0, 0, channelData, numSamples);
            juce::dsp::AudioBlock<float> lpBlock (lowPath);
            juce::dsp::ProcessContextReplacing<float> lpCtx (lpBlock);
            lpFilter.process (lpCtx);

            // HP path
            juce::AudioBuffer<float> highPath (1, numSamples);
            highPath.copyFrom (0, 0, channelData, numSamples);
            juce::dsp::AudioBlock<float> hpBlock (highPath);
            juce::dsp::ProcessContextReplacing<float> hpCtx (hpBlock);
            hpFilter.process (hpCtx);

            auto* lpData = lowPath.getWritePointer (0);
            auto* hpData = highPath.getWritePointer (0);

            // Sidechain: envelope of high path
            for (int n = 0; n < numSamples; ++n)
            {
                const float hp = std::abs (hpData[n]);
                if (hp > envState)
                    envState = (float)(hp - (hp - envState) * attackCoef);
                else
                    envState = (float)(hp - (hp - envState) * releaseCoef);

                // Compute gain reduction: above threshold -> reduce high path by 'amount'
                float grLin = 1.0f;
                if (envState > threshLin)
                {
                    const float overDb = 20.0f * std::log10 (envState / std::max (threshLin, 1e-7f));
                    const float reductionDb = -params.amount * std::min (overDb * 1.0f, 24.0f);
                    grLin = std::pow (10.0f, reductionDb / 20.0f);
                }
                grState = (float)(grLin - (grLin - grState) * (grLin < grState ? attackCoef : releaseCoef));

                // Sum back: low + reduced high
                channelData[n] = lpData[n] + hpData[n] * grState;
            }
        }
    }

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    void updateCoefficients()
    {
        const double sr = sampleRate;
        lpFilter.state = Coeffs::makeLowPass (sr, params.freq, 0.707f);
        hpFilter.state = Coeffs::makeHighPass (sr, params.freq, 0.707f);
    }

    Params params;
    double sampleRate = 44100.0;
    juce::dsp::ProcessorDuplicator<Filter, Coeffs> lpFilter;
    juce::dsp::ProcessorDuplicator<Filter, Coeffs> hpFilter;
    float envState = 0.0f;
    float grState  = 1.0f;
};

} // namespace succo
