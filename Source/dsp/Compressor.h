#pragma once

// ---------------------------------------------------------------------------
// Compressor - feed-forward RMS compressor with soft knee, gain-reduction
// sidechain (no external sidechain input in v1), and makeup gain.
// Parameters: threshold (dB), ratio (n:1), attack (ms), release (ms),
// knee (dB), makeup (dB), mix (0..1).
// ---------------------------------------------------------------------------

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>

namespace succo
{

class Compressor
{
public:
    struct Params
    {
        bool  enabled     = true;
        float thresholdDb = -20.0f;
        float ratio       = 3.0f;
        float attackMs    = 5.0f;
        float releaseMs   = 100.0f;
        float kneeDb      = 6.0f;
        float makeupDb    = 0.0f;
        float mix         = 1.0f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        reset();
    }

    void reset()
    {
        envState = 0.0f;
        smoothState = 0.0f;
    }

    void setParams (const Params& p) { params = p; }

    float getGainReductionDb() const { return lastGrDb; }

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
        const double rmsWindowSec = 0.005;
        const double rmsCoef = std::exp (-1.0 / std::max (0.0001, rmsWindowSec * sr));

        const float thresh = params.thresholdDb;
        const float ratio  = params.ratio;
        const float knee   = params.kneeDb;
        const float kneeHalf = knee * 0.5f;
        const float makeup  = std::pow (10.0f, params.makeupDb / 20.0f);

        for (int n = 0; n < numSamples; ++n)
        {
            // Stereo-linked detector: max |sample| across channels
            float peak = 0.0f;
            for (int c = 0; c < numChannels; ++c)
                peak = std::max (peak, std::abs (buffer.getSample (c, n)));

            // RMS-ish envelope
            envState = (float)(peak - (peak - envState) * rmsCoef);
            const float envDb = 20.0f * std::log10 (std::max (envState, 1e-7f));

            // Soft-knee gain computer
            float grDb;
            if (envDb > thresh - kneeHalf && envDb < thresh + kneeHalf)
            {
                // inside knee zone: quadratic curve
                const float x = envDb - (thresh - kneeHalf);
                const float r = ratio - 1.0f;
                grDb = (x * x * r) / (2.0f * knee) * (1.0f / (ratio > 0 ? ratio : 1.0f));
                grDb = -grDb;
            }
            else if (envDb >= thresh + kneeHalf)
            {
                // above knee
                grDb = (thresh - envDb) * (1.0f - 1.0f / std::max (1.0f, ratio));
            }
            else
            {
                grDb = 0.0f;
            }
            grDb = std::max (grDb, -40.0f); // limit GR

            // Smooth GR with attack/release
            const float target = grDb;
            const float coef = (target < smoothState) ? (float) attackCoef : (float) releaseCoef;
            smoothState = (float)(target - (target - smoothState) * coef);

            const float gainLin = std::pow (10.0f, smoothState / 20.0f);

            for (int c = 0; c < numChannels; ++c)
            {
                const float s = buffer.getSample (c, n);
                const float dry = s * makeup * (1.0f - params.mix);
                const float wet = s * gainLin * makeup * params.mix;
                buffer.setSample (c, n, dry + wet);
            }

            lastGrDb = smoothState;
        }
    }

private:
    Params params;
    double sampleRate = 44100.0;
    float envState = 0.0f;
    float smoothState = 0.0f;
    float lastGrDb = 0.0f;
};

} // namespace succo
