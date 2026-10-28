#pragma once

// ---------------------------------------------------------------------------
// Delay - stereo delay with tempo sync (host BPM), feedback, ping-pong, mix.
// Time can be set in seconds (free) or as a sync fraction (1/4, 1/8 dotted...).
// Sync is supported in v1 via the host AudioPlayHead + UI ratio slider.
// ---------------------------------------------------------------------------

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace succo
{

class Delay
{
public:
    struct Params
    {
        bool  enabled   = true;
        float timeMs    = 250.0f;
        float feedback  = 0.35f;    // 0..0.95
        bool  pingPong  = false;
        float mix       = 0.25f;
        float hpFreq    = 200.0f;    // damping HP on feedback path
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        const auto maxDelaySamples = (size_t) std::ceil (4.0 * sampleRate); // 4s max
        delayL.setMaximumDelayInSamples (maxDelaySamples);
        delayR.setMaximumDelayInSamples (maxDelaySamples);
        delayL.prepare (spec);
        delayR.prepare (spec);
        hpFilter.prepare (spec);
        hpFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 200.0, 0.707f);
        reset();
    }

    void reset()
    {
        delayL.reset();
        delayR.reset();
        hpFilter.reset();
        fbL = fbR = 0.0f;
    }

    void setParams (const Params& p)
    {
        params = p;
        hpFilter.state = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, params.hpFreq, 0.707f);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! params.enabled)
            return;

        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numSamples == 0) return;

        const double delaySamples = params.timeMs * 0.001 * sampleRate;
        const float fb = juce::jlimit (0.0f, 0.95f, params.feedback);

        // Pre-ensure stereo: if mono, duplicate
        if (numChannels < 2)
        {
            // Treat as mono → L = R
            for (int n = 0; n < numSamples; ++n)
            {
                const float inL = buffer.getSample (0, n);
                const float inR = buffer.getSample (0, n);
                delayL.pushSample (0, inL + fbL);
                delayR.pushSample (0, inR + fbR);

                const float wL = (float) delayL.popSample (0, delaySamples);
                const float wR = (float) delayR.popSample (0, delaySamples);

                // Damping through HP
                float fbSignal = (wL + wR) * 0.5f;
                fbSignal = applyHp (fbSignal);
                fbL = fbR = fbSignal;

                const float dry = buffer.getSample (0, n);
                buffer.setSample (0, n, dry * (1.0f - params.mix) + wL * params.mix);
            }
            return;
        }

        for (int n = 0; n < numSamples; ++n)
        {
            const float inL = buffer.getSample (0, n);
            const float inR = buffer.getSample (1, n);

            if (params.pingPong)
            {
                delayL.pushSample (0, inR + fbR);
                delayR.pushSample (0, inL + fbL);
            }
            else
            {
                delayL.pushSample (0, inL + fbL);
                delayR.pushSample (0, inR + fbR);
            }

            const float wL = (float) delayL.popSample (0, delaySamples);
            const float wR = (float) delayR.popSample (0, delaySamples);

            fbL = applyHp (wL) * fb;
            fbR = applyHp (wR) * fb;

            buffer.setSample (0, n, inL * (1.0f - params.mix) + wL * params.mix);
            buffer.setSample (1, n, inR * (1.0f - params.mix) + wR * params.mix);
        }
    }

private:
    float applyHp (float x)
    {
        // Single-channel filter application
        const float* in = &x;
        float* out = &x;
        juce::dsp::AudioBlock<const float> inBlock (&in, 1, 1);
        juce::dsp::AudioBlock<float> outBlock (&out, 1, 1);
        juce::dsp::ProcessContextNonReplacing<float> ctx (inBlock, outBlock);
        hpFilter.process (ctx);
        return *out;
    }

    Params params;
    double sampleRate = 44100.0;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL { 1 << 18 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR { 1 << 18 };
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    juce::dsp::ProcessorDuplicator<Filter, Coeffs> hpFilter;
    float fbL = 0.0f, fbR = 0.0f;
};

} // namespace succo
