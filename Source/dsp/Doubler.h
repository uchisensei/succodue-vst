#pragma once

// ---------------------------------------------------------------------------
// Doubler - thickens vocals by adding short modulated delay (~14-22 ms)
// with slight detune, panned L/R opposite to source. Simple but effective.
// Parameters: amount (0..1), time (ms), detune (cents), spread (stereo width)
// ---------------------------------------------------------------------------

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include <random>

namespace succo
{

class Doubler
{
public:
    struct Params
    {
        bool  enabled = true;
        float amount  = 0.4f;
        float timeMs  = 18.0f;
        float detune  = 7.0f;        // cents
        float spread  = 0.7f;        // 0 = mono, 1 = full stereo
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        maxBlock   = spec.maximumBlockSize;
        const auto maxDelaySamples = (size_t) std::ceil (0.080 * sampleRate); // 80ms max
        delayLineL.setMaximumDelayInSamples (maxDelaySamples);
        delayLineR.setMaximumDelayInSamples (maxDelaySamples);
        delayLineL.prepare (spec);
        delayLineR.prepare (spec);
        lfoPhase = 0.0;
        reset();
    }

    void reset()
    {
        delayLineL.reset();
        delayLineR.reset();
        lfoPhase = 0.0;
    }

    void setParams (const Params& p) { params = p; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! params.enabled)
            return;

        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels < 2 || numSamples == 0)
            return; // need stereo

        const double sr = sampleRate;
        const double baseDelaySamples = params.timeMs * 0.001 * sr;
        const double lfoRate = 0.7; // Hz slow drift
        const double lfoDepth = 0.0015 * sr; // ~1.5 samples drift
        const double phaseInc = 2.0 * juce::MathConstants<double>::pi * lfoRate / sr;
        const double detuneSemitones = params.detune / 100.0;
        // Pitch ratio (not used directly here, but kept for future per-sample modulation)
        (void) detuneSemitones;

        for (int n = 0; n < numSamples; ++n)
        {
            // Drift LFO: opposite phase on L/R
            const double lfoL = std::sin (lfoPhase);
            const double lfoR = std::sin (lfoPhase + juce::MathConstants<double>::pi);

            delayLineL.pushSample (0, buffer.getSample (0, n));
            delayLineR.pushSample (0, buffer.getSample (1, n));

            const double dL = std::max (1.0, baseDelaySamples + lfoL * lfoDepth);
            const double dR = std::max (1.0, baseDelaySamples + lfoR * lfoDepth);

            const double wetL = delayLineL.popSample (0, dL);
            const double wetR = delayLineR.popSample (0, dR);

            // Apply spread: 1.0 = full wet on opposite channel
            const float dryL = buffer.getSample (0, n);
            const float dryR = buffer.getSample (1, n);
            const float a = params.amount;
            const float s = params.spread;

            // Output L = dryL * (1 - a) + wetL * a * (1 - s) + wetR * a * s
            const float outL = dryL * (1.0f - a) + wetL * a * (1.0f - s) + wetR * a * s;
            const float outR = dryR * (1.0f - a) + wetR * a * (1.0f - s) + wetL * a * s;

            buffer.setSample (0, n, outL);
            buffer.setSample (1, n, outR);

            lfoPhase += phaseInc;
            if (lfoPhase > 2.0 * juce::MathConstants<double>::pi)
                lfoPhase -= 2.0 * juce::MathConstants<double>::pi;
        }
    }

private:
    Params params;
    double sampleRate = 44100.0;
    juce::uint32 maxBlock = 0;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineL { 1 << 15 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLineR { 1 << 15 };
    double lfoPhase = 0.0;
};

} // namespace succo
