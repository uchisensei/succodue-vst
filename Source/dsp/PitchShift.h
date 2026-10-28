#pragma once

// ---------------------------------------------------------------------------
// PitchShift - simple pitch shifter using a dual-buffer modulated delay-line
// crossfade (basic implementation). NOT a full automatic pitch-correction
// engine (that would require a scale-quantizer + YIN pitch detection + PSOLA).
// In v1 we expose two knobs: semitones (-12..+12) and "auto-tune amount"
// which scales the semitones toward 0 (to mimic the visual concept).
//
// Future work: integrate a real pitch tracking + PSOLA engine for
// scale-locked correction.
// ---------------------------------------------------------------------------

#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace succo
{

class PitchShift
{
public:
    struct Params
    {
        bool  enabled    = true;
        float semitones = 0.0f;     // -12..+12
        float amount     = 0.0f;     // 0..1 (visual: "auto-tune amount")
        float formant   = 0.0f;     // -1..+1 (placeholder - not implemented in v1)
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        const auto maxDelay = (size_t) std::ceil (0.080 * sampleRate);
        delayA.setMaximumDelayInSamples (maxDelay);
        delayB.setMaximumDelayInSamples (maxDelay);
        delayA.prepare (spec);
        delayB.prepare (spec);
        reset();
    }

    void reset()
    {
        delayA.reset();
        delayB.reset();
        phaseA = 0.0;
        phaseB = 0.0;
        readA = 0.0;
        readB = 0.0;
    }

    void setParams (const Params& p) { params = p; }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! params.enabled || std::abs (params.semitones) < 0.005f)
            return;

        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numSamples == 0) return;

        // Effective pitch ratio (semitones * amount)
        const float eff = params.semitones * juce::jlimit (0.0f, 1.0f, params.amount);
        const double ratio = std::pow (2.0, eff / 12.0);

        // For dual-buffer modulated delay crossfade pitch shifter:
        // - we have two delay lines each reading at 'ratio' speed
        // - each cycle they crossfade to avoid discontinuity
        // - phase increments at 1/cycleLength samples
        const double cycleSec = 0.040; // 40ms
        const double cycleSamples = cycleSec * sampleRate;
        const double phaseInc = 1.0 / cycleSamples;

        for (int n = 0; n < numSamples; ++n)
        {
            // Advance both phases
            phaseA += phaseInc;
            phaseB += phaseInc;

            if (phaseA >= 1.0) phaseA -= 1.0;
            if (phaseB >= 1.0) phaseB -= 1.0;

            // B is offset by half cycle for crossfade
            double pB = phaseB + 0.5;
            if (pB >= 1.0) pB -= 1.0;

            // Read positions advance at (1 - ratio) per sample
            readA = std::fmod (readA + (1.0 - ratio), cycleSamples);
            readB = std::fmod (readB + (1.0 - ratio), cycleSamples);

            // Windowed envelope: cosine window centered at phase 0.5
            auto env = [] (double p)
            {
                return 0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * p);
            };

            const float envA = (float) env (phaseA);
            const float envB = (float) env (pB);

            for (int c = 0; c < numChannels; ++c)
            {
                const float in = buffer.getSample (c, n);
                delayA.pushSample (c, in);
                delayB.pushSample (c, in);

                // Read at current read offset
                const float a = (float) delayA.popSample (c, std::max (1.0, readA + 1.0));
                const float b = (float) delayB.popSample (c, std::max (1.0, readB + 1.0));

                const float out = a * envA + b * envB;
                buffer.setSample (c, n, out);
            }
        }
    }

private:
    Params params;
    double sampleRate = 44100.0;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayA { 1 << 14 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayB { 1 << 14 };
    double phaseA = 0.0, phaseB = 0.0;
    double readA = 0.0, readB = 0.0;
};

} // namespace succo
