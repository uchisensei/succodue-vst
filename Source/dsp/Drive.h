#pragma once

// ---------------------------------------------------------------------------
// Drive - soft / hard clipping waveshaper with oversampling, tone (post-tilt)
// and output makeup. Curve: tanh-style saturation with shape parameter (0..1)
// interpolating between soft clip and hard clip.
// ---------------------------------------------------------------------------

#include "Oversampler.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace succo
{

class Drive
{
public:
    struct Params
    {
        bool  enabled    = true;
        float driveDb   = 0.0f;        // input drive (dB)
        float bias      = 0.0f;        // DC bias added to waveshaper input
        float shape     = 0.5f;        // 0 = soft tanh, 1 = hard clip
        float tone      = 0.0f;        // -1..+1, post-tilt toward treble or bass
        float outputDb  = 0.0f;        // output makeup
        int   oversample = 2;          // 1 / 2 / 4
    };

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        maxBlock   = spec.maximumBlockSize;
        // Allocate tone filters
        const auto numCh = static_cast<juce::uint32> (spec.numChannels);
        toneLow.prepare  ({ spec.sampleRate, spec.maximumBlockSize, numCh });
        toneHigh.prepare ({ spec.sampleRate, spec.maximumBlockSize, numCh });

        toneLow.state  = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (spec.sampleRate, 800.0, 0.707f, 1.0f);
        toneHigh.state = juce::dsp::IIR::Coefficients<float>::makeHighShelf (spec.sampleRate, 4000.0, 0.707f, 1.0f);

        oversampler.prepare (spec, params.oversample);
        updateTone();
    }

    void reset()
    {
        oversampler.reset();
        toneLow.reset();
        toneHigh.reset();
    }

    void setParams (const Params& p)
    {
        bool osChanged = (p.oversample != params.oversample);
        params = p;
        if (osChanged)
        {
            // Re-init oversampler with stereo config
            juce::dsp::ProcessSpec spec { sampleRate, juce::jmax<juce::uint32>(maxBlock, 64), 2 };
            oversampler.prepare (spec, params.oversample);
        }
        updateTone();
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (! params.enabled)
            return;

        const float driveLin = std::pow (10.0f, params.driveDb / 20.0f);
        const float outLin  = std::pow (10.0f, params.outputDb / 20.0f);
        const float bias    = params.bias;
        const float shape   = params.shape;
        const float k = 2.0f;  // tanh scale

        oversampler.process (buffer, [&] (juce::AudioBuffer<float>& up)
        {
            for (int c = 0; c < up.getNumChannels(); ++c)
            {
                auto* data = up.getWritePointer (c);
                for (int n = 0; n < up.getNumSamples(); ++n)
                {
                    float x = data[n] * driveLin + bias;

                    // === 4 saturation modes blended smoothly via shape param ===
                    // shape 0.00 -> Tube (warm tanh)
                    // shape 0.33 -> Tape (asymmetric soft clip)
                    // shape 0.67 -> Transformer (bias-shifted, even harmonics)
                    // shape 1.00 -> Fuzz (hard asymmetric clip)
                    auto tube = [k](float v) { return std::tanh (k * v) / std::tanh (k); };
                    auto tape = [](float v) {
                        return (v >= 0.0f) ? std::tanh (1.5f * v) * 0.95f
                                            : std::tanh (1.2f * v) * 0.92f;
                    };
                    auto transformer = [](float v) {
                        const float bias2 = 0.15f;
                        const float y1 = std::tanh (2.0f * (v + bias2));
                        const float yAtBias = std::tanh (2.0f * bias2);
                        return y1 - yAtBias;  // remove DC offset
                    };
                    auto fuzz = [](float v) {
                        return juce::jlimit (-1.0f, 1.0f, v * 1.5f);
                    };

                    float y;
                    if (shape < 0.333f) {
                        const float t = shape / 0.333f;
                        y = (1.0f - t) * tube (x) + t * tape (x);
                    } else if (shape < 0.667f) {
                        const float t = (shape - 0.333f) / 0.334f;
                        y = (1.0f - t) * tape (x) + t * transformer (x);
                    } else {
                        const float t = (shape - 0.667f) / 0.333f;
                        y = (1.0f - t) * transformer (x) + t * fuzz (x);
                    }

                    data[n] = y * outLin;
                }
            }
        });

        // Tone (post-tilt)
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        toneLow.process (ctx);
        toneHigh.process (ctx);
    }

private:
    void updateTone()
    {
        // Map -1..+1 tone to filter gains: +1 = boost high + cut low; -1 = boost low + cut high
        const float t = juce::jlimit (-1.0f, 1.0f, params.tone);
        const float highGainDb = std::abs (t) * 6.0f * (t >= 0.0f ? 1.0f : -0.2f);
        const float lowGainDb  = std::abs (t) * 6.0f * (t <= 0.0f ? 1.0f : -0.2f);

        const double sr = sampleRate;
        toneLow.state  = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sr, 800.0, 0.707f,
            juce::Decibels::decibelsToGain (lowGainDb));
        toneHigh.state = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 4000.0, 0.707f,
            juce::Decibels::decibelsToGain (highGainDb));
    }

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    Params params;
    double sampleRate = 44100.0;
    juce::uint32 maxBlock = 0;
    Oversampler oversampler;
    juce::dsp::ProcessorDuplicator<Filter, Coeffs> toneLow;
    juce::dsp::ProcessorDuplicator<Filter, Coeffs> toneHigh;
};

} // namespace succo
