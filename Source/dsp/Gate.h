#pragma once

// ---------------------------------------------------------------------------
// Gate - expander-style noise gate with hysteresis to avoid chattering.
// Parameters: threshold (dB), range (dB attenuation when closed),
// attack (ms), hold (ms), release (ms).
// ---------------------------------------------------------------------------

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace succo
{

class Gate
{
public:
    struct Params
    {
        float thresholdDb = -60.0f;
        float rangeDb      = 40.0f;   // max attenuation when gate is closed
        float attackMs     = 1.0f;
        float holdMs       = 20.0f;
        float releaseMs    = 80.0f;
        bool  enabled      = true;
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        maxBlock   = spec.maximumBlockSize;
        envState = 0.0f;
        holdCounter = 0.0;
        smoothCoeff = 0.0f;
        reset();
    }

    void reset()
    {
        envState = 0.0f;
        holdCounter = 0.0;
        currentGainLin = std::pow (10.0f, -params.rangeDb / 20.0f);
    }

    void setParams (const Params& p) { params = p; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! params.enabled)
            return;

        const auto numChannels = buffer.getNumChannels();
        const auto numSamples  = buffer.getNumSamples();
        if (numSamples == 0) return;

        // Compute per-sample gain envelope using max channel level (linked)
        const double attackCoef  = std::exp (-1.0 / (std::max (0.0001, params.attackMs  * 0.001 * sampleRate)));
        const double releaseCoef = std::exp (-1.0 / (std::max (0.0001, params.releaseMs * 0.001 * sampleRate)));
        const double holdSamples = params.holdMs * 0.001 * sampleRate;

        const float threshLin = std::pow (10.0f, params.thresholdDb / 20.0f);
        const float closedLin = std::pow (10.0f, -params.rangeDb / 20.0f);

        for (int n = 0; n < numSamples; ++n)
        {
            // Peak-detector: max abs across channels
            float peak = 0.0f;
            for (int c = 0; c < numChannels; ++c)
                peak = std::max (peak, std::abs (buffer.getSample (c, n)));

            // Env follower
            if (peak > envState)
                envState = (float)(peak - (peak - envState) * attackCoef);
            else
                envState = (float)(peak - (peak - envState) * releaseCoef);

            // Hysteresis state machine: open / hold / close
            if (envState > threshLin)
            {
                holdCounter = holdSamples;
                targetLin = 1.0f;
            }
            else if (holdCounter > 0.0)
            {
                holdCounter -= 1.0;
                targetLin = 1.0f;
            }
            else
            {
                targetLin = closedLin;
            }

            // Smooth gain
            const float coef = (targetLin > currentGainLin) ? attackCoef : releaseCoef;
            currentGainLin = (float)(targetLin - (targetLin - currentGainLin) * coef);

            for (int c = 0; c < numChannels; ++c)
            {
                const float s = buffer.getSample (c, n);
                buffer.setSample (c, n, s * currentGainLin);
            }
        }
    }

private:
    Params params;
    double sampleRate = 44100.0;
    juce::uint32 maxBlock = 0;
    float envState = 0.0f;
    double holdCounter = 0.0;
    float currentGainLin = 1.0f;
    float targetLin = 1.0f;
    float smoothCoeff = 0.0f;
};

} // namespace succo
