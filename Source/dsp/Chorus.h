#pragma once

// ---------------------------------------------------------------------------
// Chorus - multi-voice modulated delay with feedback (mild).
// Parameters: rate (Hz), depth (ms), voices (1-4), mix (0..1)
// ---------------------------------------------------------------------------

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

namespace succo
{

class Chorus
{
public:
    struct Params
    {
        bool  enabled  = true;
        float rate     = 0.6f;     // Hz
        float depth    = 2.5f;     // ms
        int   voices   = 2;
        float feedback = 0.0f;     // 0..0.9
        float mix      = 0.3f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        const auto maxDelaySamples = (size_t) std::ceil (0.050 * sampleRate);
        for (auto& d : delayLines)
        {
            d.setMaximumDelayInSamples (maxDelaySamples);
            d.prepare (spec);
            d.reset();
        }
        for (auto& p : lfoPhases)
            p = 0.0;
        feedbackBuf.fill (0.0f);
        reset();
    }

    void reset()
    {
        for (auto& d : delayLines)
            d.reset();
        for (auto& p : lfoPhases)
            p = 0.0;
        feedbackBuf.fill (0.0f);
    }

    void setParams (const Params& p) { params = p; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! params.enabled)
            return;

        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numSamples == 0) return;

        const double sr = sampleRate;
        const double baseDelay = 0.015 * sr;  // 15ms base
        const double phaseInc = 2.0 * juce::MathConstants<double>::pi * params.rate / sr;
        const int voices = juce::jlimit (1, 4, params.voices);
        const float depthSamp = params.depth * 0.001f * (float) sr;
        const float fb = juce::jlimit (0.0f, 0.95f, params.feedback);

        for (int n = 0; n < numSamples; ++n)
        {
            float wetSum = 0.0f;

            for (int v = 0; v < voices; ++v)
            {
                const double phaseOffset = (double) v * (2.0 * juce::MathConstants<double>::pi / std::max (1, voices));
                const double lfo = std::sin (lfoPhases[v] + phaseOffset);
                const double delaySamp = std::max (1.0, baseDelay + lfo * depthSamp);

                // Use input from either L or R alternating
                const int ch = (numChannels >= 2 && v % 2 == 1) ? 1 : 0;
                const float input = buffer.getSample (ch, n) + feedbackBuf[v] * fb;

                delayLines[v].pushSample (0, input);
                wetSum += (float) delayLines[v].popSample (0, delaySamp);
                lfoPhases[v] += phaseInc;
                if (lfoPhases[v] > 2.0 * juce::MathConstants<double>::pi)
                    lfoPhases[v] -= 2.0 * juce::MathConstants<double>::pi;
            }
            wetSum /= (float) voices;

            // Mix dry/wet on every channel
            const float dryGain = 1.0f - params.mix;
            const float wetGain = params.mix;
            for (int c = 0; c < numChannels; ++c)
            {
                const float dry = buffer.getSample (c, n);
                buffer.setSample (c, n, dry * dryGain + wetSum * wetGain);
            }
            feedbackBuf.fill (wetSum);
        }
    }

private:
    Params params;
    double sampleRate = 44100.0;
    std::array<juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>, 4> delayLines;
    std::array<double, 4> lfoPhases { 0.0, 0.0, 0.0, 0.0 };
    std::array<float, 4> feedbackBuf { 0.0f, 0.0f, 0.0f, 0.0f };
};

} // namespace succo
