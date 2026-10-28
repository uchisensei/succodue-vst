#pragma once

// ---------------------------------------------------------------------------
// Limiter - lookahead brickwall limiter with true-peak awareness
// (inter-sample peak detection via 4x oversampling). Release + ceiling.
// ---------------------------------------------------------------------------

#include "Oversampler.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <algorithm>
#include <vector>

namespace succo
{

class Limiter
{
public:
    struct Params
    {
        bool  enabled    = true;
        float ceilingDb = -0.3f;     // output ceiling
        float releaseMs = 50.0f;
        bool  truePeak  = true;       // use 4x oversampling for ISP detection
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        maxBlock   = spec.maximumBlockSize;
        numChannels = static_cast<juce::uint32> (spec.numChannels);
        // Lookahead buffer (5ms)
        const size_t lookSamp = (size_t) std::ceil (0.005 * sampleRate);
        lookBuf.assign (numChannels, std::vector<float> (lookSamp, 0.0f));
        lookPos.assign (numChannels, 0);

        // Oversampler for ISP detection
        oversampler.prepare (spec, params.truePeak ? 4 : 1);

        gainState = 1.0f;
        targetGain = 1.0f;
        reset();
    }

    void reset()
    {
        for (auto& b : lookBuf) std::fill (b.begin(), b.end(), 0.0f);
        gainState = 1.0f;
        targetGain = 1.0f;
    }

    void setParams (const Params& p)
    {
        bool osChanged = (p.truePeak != params.truePeak);
        params = p;
        if (osChanged)
        {
            const auto numCh = lookBuf.size();
            juce::dsp::ProcessSpec spec { sampleRate, maxBlock, (juce::uint32) numCh };
            oversampler.prepare (spec, params.truePeak ? 4 : 1);
        }
    }

    // Expose current gain reduction for UI meter (dB reduction, 0 = no reduction)
    float getGainReductionDb() const { return lastGrDb; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! params.enabled)
            return;

        const int numSamples  = buffer.getNumSamples();
        if (numSamples == 0) return;

        const double sr = sampleRate;
        const double releaseCoef = std::exp (-1.0 / std::max (0.0001, params.releaseMs * 0.001 * sr));
        const double attackCoef = 0.0; // instant attack (we use lookahead)
        (void) attackCoef;

        const float ceiling = std::pow (10.0f, params.ceilingDb / 20.0f);
        const double lookSamples = lookBuf.empty() ? 0 : (double) lookBuf[0].size();

        for (int n = 0; n < numSamples; ++n)
        {
            // Compute input peak across channels
            float inputPeak = 0.0f;
            for (int c = 0; c < numChannels; ++c)
                inputPeak = std::max (inputPeak, std::abs (buffer.getSample (c, n)));

            // Push current sample into lookahead buffer (channel 0 only - we keep peak trace per channel)
            // For simplicity, we look ahead per channel separately.

            // Determine gain target based on the next 'lookSamples' samples' peaks
            // Compute by scanning lookahead buffer + current sample
            float maxAhead = inputPeak;
            for (int c = 0; c < numChannels; ++c)
            {
                for (size_t k = 0; k < lookSamples; ++k)
                {
                    const size_t idx = (lookPos[c] + k) % lookBuf[c].size();
                    maxAhead = std::max (maxAhead, std::abs (lookBuf[c][idx]));
                }
            }

            // If maxAhead > ceiling, we need gain = ceiling / maxAhead
            if (maxAhead > ceiling)
                targetGain = ceiling / maxAhead;
            else
                targetGain = 1.0f;

            // Smooth gain with fast attack (when decreasing) / release (when increasing)
            if (targetGain < gainState)
                gainState = targetGain; // instant attack (lookahead compensates)
            else
                gainState = (float)(targetGain - (targetGain - gainState) * releaseCoef);

            // Track gain reduction for UI meter
            lastGrDb = (gainState > 0.001f)
                ? juce::jlimit (0.0f, 24.0f, -20.0f * std::log10 (gainState))
                : 24.0f;

            // Apply gain, then push current input to lookahead (post-GC)
            for (int c = 0; c < numChannels; ++c)
            {
                lookBuf[c][lookPos[c]] = buffer.getSample (c, n);
                lookPos[c] = (lookPos[c] + 1) % lookBuf[c].size();
                // Output the delayed sample (one lookahead-length behind)
                const float delayed = lookBuf[c][lookPos[c]];
                buffer.setSample (c, n, delayed * gainState);
            }
        }

        // Optional: true-peak check via 4x oversampling and final clamp
        if (params.truePeak)
        {
            oversampler.process (buffer, [ceiling] (juce::AudioBuffer<float>& up)
            {
                for (int c = 0; c < up.getNumChannels(); ++c)
                {
                    auto* d = up.getWritePointer (c);
                    for (int n = 0; n < up.getNumSamples(); ++n)
                        d[n] = std::clamp (d[n], -ceiling, ceiling);
                }
            });
        }
    }

private:
    Params params;
    double sampleRate = 44100.0;
    juce::uint32 maxBlock = 0;
    juce::uint32 numChannels = 2;
    std::vector<std::vector<float>> lookBuf;
    std::vector<size_t> lookPos;
    Oversampler oversampler;
    float gainState = 1.0f;
    float targetGain = 1.0f;
    float lastGrDb = 0.0f;
};

} // namespace succo
