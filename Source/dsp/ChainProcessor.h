#pragma once

// ---------------------------------------------------------------------------
// ChainProcessor - holds all 11 DSP modules and routes audio through them
// in a fixed order. Modules can be bypassed individually. Also implements
// overall input/output gain and the global dry/wet mix.
// ---------------------------------------------------------------------------

#include "Gate.h"
#include "EQ.h"
#include "Compressor.h"
#include "DeEsser.h"
#include "Drive.h"
#include "Doubler.h"
#include "Chorus.h"
#include "Delay.h"
#include "Reverb.h"
#include "Limiter.h"
#include "PitchShift.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace succo
{

struct ChainParams
{
    // Master
    float inputGainDb = 0.0f;
    float outputGainDb = 0.0f;
    float mix = 1.0f;             // global dry/wet (0=full dry, 1=full wet)
    float punch = 0.5f;          // parallel-comp style "punch" blend

    // Module bypass flags (mirrored from each module's 'enabled' flag)
    bool tuneBypass   = false;
    bool gateBypass   = false;
    bool eqBypass     = false;
    bool compBypass   = false;
    bool deEssBypass  = false;
    bool driveBypass  = false;
    bool doubleBypass = false;
    bool chorusBypass = false;
    bool delayBypass  = false;
    bool reverbBypass = false;
    bool limitBypass  = false;

    // Each module's params
    PitchShift::Params  tune;
    Gate::Params        gate;
    EQ::Params          eq;
    Compressor::Params  comp;
    DeEsser::Params     deEss;
    Drive::Params       drive;
    Doubler::Params     doubler;
    Chorus::Params      chorus;
    Delay::Params       delay;
    Reverb::Params      reverb;
    Limiter::Params     limiter;
};

class ChainProcessor
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        maxBlock   = spec.maximumBlockSize;
        numChannels = spec.numChannels;

        // Allocate dry buffer for global mix
        dryBuffer.setSize ((int) numChannels, (int) maxBlock);

        pitchShift.prepare (spec);
        gate.prepare (spec);
        eq.prepare (spec);
        comp.prepare (spec);
        deEsser.prepare (spec);
        drive.prepare (spec);
        doubler.prepare (spec);
        chorus.prepare (spec);
        delay.prepare (spec);
        reverb.prepare (spec);
        limiter.prepare (spec);
    }

    void reset()
    {
        dryBuffer.clear();
        pitchShift.reset();
        gate.reset();
        eq.reset();
        comp.reset();
        deEsser.reset();
        drive.reset();
        doubler.reset();
        chorus.reset();
        delay.reset();
        reverb.reset();
        limiter.reset();
    }

    void setParams (const ChainParams& p)
    {
        params = p;

        pitchShift.setParams (p.tune);
        gate.setParams (p.gate);
        eq.setParams (p.eq);
        comp.setParams (p.comp);
        deEsser.setParams (p.deEss);
        drive.setParams (p.drive);
        doubler.setParams (p.doubler);
        chorus.setParams (p.chorus);
        delay.setParams (p.delay);
        reverb.setParams (p.reverb);
        limiter.setParams (p.limiter);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumSamples() == 0)
            return;

        // Ensure dry buffer is sized properly
        if (dryBuffer.getNumChannels() < buffer.getNumChannels()
            || dryBuffer.getNumSamples() < buffer.getNumSamples())
            dryBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, false, true);

        // 1) Input gain
        const float inGainLin = std::pow (10.0f, params.inputGainDb / 20.0f);
        buffer.applyGain (inGainLin);

        // 2) Save dry signal for global mix
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            dryBuffer.copyFrom (c, 0, buffer, c, 0, buffer.getNumSamples());

        // 3) Apply module bypass flags
        pitchShift.setParams ([&]{ auto p = params.tune;   p.enabled = p.enabled && !params.tuneBypass;   return p; }());
        gate.setParams       ([&]{ auto p = params.gate;   p.enabled = p.enabled && !params.gateBypass;   return p; }());
        eq.setParams         ([&]{ auto p = params.eq;     p.enabled = p.enabled && !params.eqBypass;     return p; }());
        comp.setParams       ([&]{ auto p = params.comp;   p.enabled = p.enabled && !params.compBypass;   return p; }());
        deEsser.setParams    ([&]{ auto p = params.deEss;  p.enabled = p.enabled && !params.deEssBypass;  return p; }());
        drive.setParams      ([&]{ auto p = params.drive;  p.enabled = p.enabled && !params.driveBypass;  return p; }());
        doubler.setParams    ([&]{ auto p = params.doubler;p.enabled = p.enabled && !params.doubleBypass;return p; }());
        chorus.setParams      ([&]{ auto p = params.chorus; p.enabled = p.enabled && !params.chorusBypass;return p; }());
        delay.setParams      ([&]{ auto p = params.delay;  p.enabled = p.enabled && !params.delayBypass;  return p; }());
        reverb.setParams     ([&]{ auto p = params.reverb; p.enabled = p.enabled && !params.reverbBypass;return p; }());
        limiter.setParams    ([&]{ auto p = params.limiter;p.enabled = p.enabled && !params.limitBypass;  return p; }());

        // 4) Run the chain in fixed order
        gate.process (buffer);
        eq.process (buffer);
        comp.process (buffer);
        deEsser.process (buffer);
        drive.process (buffer);
        doubler.process (buffer);
        chorus.process (buffer);
        delay.process (buffer);
        reverb.process (buffer);
        limiter.process (buffer);
        pitchShift.process (buffer); // last, so pitch shifting doesn't shift artifacts

        // 5) Global mix: dry/wet
        const float wet = params.mix;
        const float dry = 1.0f - params.mix;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            auto* out = buffer.getWritePointer (c);
            auto* dr  = dryBuffer.getReadPointer (c);
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                out[n] = out[n] * wet + dr[n] * dry;
        }

        // 6) Output gain
        const float outGainLin = std::pow (10.0f, params.outputGainDb / 20.0f);
        buffer.applyGain (outGainLin);
    }

    // Provide read access for UI (e.g., gain-reduction meters)
    const Compressor& getCompressor() const { return comp; }
    const Limiter&    getLimiter() const    { return limiter; }

private:
    ChainParams params;
    double sampleRate = 44100.0;
    juce::uint32 maxBlock = 0;
    juce::uint32 numChannels = 2;
    juce::AudioBuffer<float> dryBuffer;

    PitchShift  pitchShift;
    Gate        gate;
    EQ          eq;
    Compressor  comp;
    DeEsser     deEsser;
    Drive       drive;
    Doubler     doubler;
    Chorus      chorus;
    Delay       delay;
    Reverb      reverb;
    Limiter     limiter;
};

} // namespace succo
