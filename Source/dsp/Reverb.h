#pragma once

// ---------------------------------------------------------------------------
// Reverb - tuned Schroeder/Moorer algorithmic reverb.
// Uses a parallel comb-filter bank feeding a series of allpass filters.
// Parameters: size (room scale), decay (s), damp (Hz LP on feedback path),
// predelay (ms), lowCut (Hz HP on input), mix (0..1).
// ---------------------------------------------------------------------------

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>
#include <vector>

namespace succo
{

class Reverb
{
public:
    struct Params
    {
        bool  enabled   = true;
        float size      = 0.5f;     // 0..1 (room scale factor)
        float decay     = 1.2f;     // seconds
        float damp      = 4000.0f;  // Hz
        float predelayMs = 0.0f;
        float lowCut    = 100.0f;   // Hz
        float mix       = 0.25f;
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        const auto maxPredelaySamples = (size_t) std::ceil (0.5 * sampleRate);
        predelay.setMaximumDelayInSamples (maxPredelaySamples);
        predelay.prepare (spec);
        predelay.reset();

        for (auto& c : combs) { c.prepare (spec); c.reset(); }
        for (auto& a : allpass) { a.prepare (spec); a.reset(); }

        dampFilter.prepare (spec);
        lowcutFilter.prepare (spec);
        updateParams();
        reset();
    }

    void reset()
    {
        predelay.reset();
        for (auto& c : combs) c.reset();
        for (auto& a : allpass) a.reset();
        dampFilter.reset();
        lowcutFilter.reset();
    }

    void setParams (const Params& p)
    {
        params = p;
        updateParams();
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! params.enabled)
            return;

        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numSamples == 0) return;

        const double predelaySamp = params.predelayMs * 0.001 * sampleRate;

        for (int n = 0; n < numSamples; ++n)
        {
            // Per-sample: take mono sum (L + R) / 2 for the reverb tail input
            const float inL = buffer.getSample (0, n);
            const float inR = (numChannels >= 2) ? buffer.getSample (1, n) : inL;
            const float mono = (inL + inR) * 0.5f;

            // HP on input (single-channel IIR::Filter via processSample)
            const float inHpL = lowcutFilter.processSample (mono);

            // Predelay
            predelay.pushSample (0, inHpL);
            const float delayed = (float) predelay.popSample (0, std::max (1.0, predelaySamp));

            // Process through 4 combs in parallel
            float sum = 0.0f;
            for (size_t i = 0; i < combs.size(); ++i)
                sum += combs[i].process (delayed);

            // Process through 2 allpass in series
            for (size_t i = 0; i < allpass.size(); ++i)
                sum = allpass[i].process (sum);

            // Damp on feedback (single-channel LP via processSample)
            const float dampedWet = dampFilter.processSample (sum);

            const float wet = dampedWet;
            const float dryL = inL;
            const float dryR = inR;
            buffer.setSample (0, n, dryL * (1.0f - params.mix) + wet * params.mix);
            if (numChannels >= 2)
                buffer.setSample (1, n, dryR * (1.0f - params.mix) + wet * params.mix);
        }
    }

private:
    // ---------- Comb filter ----------
    struct Comb
    {
        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            maxLen = (size_t) (0.1 * spec.sampleRate) + 16;
            buf.assign (maxLen, 0.0f);
            writePos = 0;
        }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); writePos = 0; }
        void setParams (size_t delaySamples, float feedbackVal) { length = delaySamples; fb = feedbackVal; }
        float process (float in)
        {
            const float out = buf[writePos];
            buf[writePos] = in + out * fb;
            if (++writePos >= length) writePos = 0;
            return out;
        }
        std::vector<float> buf;
        size_t length = 1, maxLen = 1, writePos = 0;
        float fb = 0.5f;
    };

    // ---------- Allpass filter ----------
    struct Allpass
    {
        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            maxLen = (size_t) (0.05 * spec.sampleRate) + 16;
            buf.assign (maxLen, 0.0f);
            writePos = 0;
        }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); writePos = 0; }
        void setParams (size_t delaySamples, float feedbackVal) { length = delaySamples; fb = feedbackVal; }
        float process (float in)
        {
            const float delayed = buf[writePos];
            const float out = -in + delayed;
            buf[writePos] = in + delayed * fb;
            if (++writePos >= length) writePos = 0;
            return out;
        }
        std::vector<float> buf;
        size_t length = 1, maxLen = 1, writePos = 0;
        float fb = 0.5f;
    };

    void updateParams()
    {
        const double srScale = sampleRate / 44100.0;
        const float sizeScale = 0.5f + 1.5f * params.size;
        const float decayScale = juce::jlimit (0.1f, 4.0f, params.decay);

        auto setComb = [&](Comb& c, int baseSamp)
        {
            const size_t delaySamp = (size_t) std::max ((int) 1, (int) std::ceil (baseSamp * srScale * sizeScale));
            const double dt = (double) delaySamp / sampleRate;
            const double rt60 = decayScale;
            const float feedback = (float) std::pow (10.0, -3.0 * dt / rt60);
            c.setParams (delaySamp, juce::jlimit (0.0f, 0.999f, feedback));
        };

        setComb (combs[0], 1116);
        setComb (combs[1], 1188);
        setComb (combs[2], 1277);
        setComb (combs[3], 1356);

        auto setAp = [&](Allpass& a, int baseSamp)
        {
            const size_t delaySamp = (size_t) std::max ((int) 1, (int) std::ceil (baseSamp * srScale * 0.7));
            a.setParams (delaySamp, 0.7f);
        };
        setAp (allpass[0], 556);
        setAp (allpass[1], 441);

        // Damping / low-cut filters (use direct IIR::Filter<float> for single-sample processing)
        dampFilter.coefficients   = juce::dsp::IIR::Coefficients<float>::makeLowPass  (sampleRate, params.damp, 0.707f);
        lowcutFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, params.lowCut, 0.707f);
    }

    Params params;
    double sampleRate = 44100.0;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> predelay { 1 << 17 };

    std::array<Comb, 4> combs;
    std::array<Allpass, 2> allpass;

    // Direct single-channel IIR::Filter (NOT ProcessorDuplicator — those lack processSample)
    juce::dsp::IIR::Filter<float> dampFilter;
    juce::dsp::IIR::Filter<float> lowcutFilter;
};

} // namespace succo
