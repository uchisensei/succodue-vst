#pragma once

// ---------------------------------------------------------------------------
// PresetManager - factory presets + load/save of user presets.
// Factory presets are baked-in defaults; user presets live in
// ~/Documents/SUCCO/Presets as .succo XML files.
// ---------------------------------------------------------------------------

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "../params/ParameterManager.h"

namespace succo
{

class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState& apvtsRef)
        : apvts (apvtsRef)
    {
        buildFactoryPresets();
        userFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                        .getChildFile ("SUCCO").getChildFile ("Presets");
        userFolder.createDirectory();
        refreshUserPresets();
    }

    int getNumPresets() const { return (int) allPresets.size(); }
    int getCurrentPresetIndex() const { return currentIdx; }
    const juce::String getPresetName (int idx) const
    {
        if (idx >= 0 && idx < (int) allPresets.size())
            return allPresets[(size_t) idx].name;
        return "Init";
    }

    void loadPreset (int idx)
    {
        if (idx < 0 || idx >= (int) allPresets.size())
            return;
        currentIdx = idx;
        const auto& p = allPresets[(size_t) idx];
        // Apply the ValueTree into the APVTS
        apvts.replaceState (p.state);
    }

    void saveUserPreset (const juce::String& name)
    {
        auto state = apvts.copyState();
        juce::XmlElement xml ("SUCCO_PRESET");
        xml.setAttribute ("name", name);
        xml.setAttribute ("version", "1.0");
        auto stateXml = state.createXml();
        xml.addChildElement (stateXml.release());

        userFolder.createDirectory();
        const auto file = userFolder.getChildFile (name + ".succo");
        xml.writeTo (file);

        refreshUserPresets();
        // Find and load it
        for (size_t i = 0; i < allPresets.size(); ++i)
            if (allPresets[i].name == name)
            {
                currentIdx = (int) i;
                break;
            }
    }

    void refreshUserPresets()
    {
        // Keep factory presets; clear user; reload
        allPresets = factoryPresets;
        auto files = userFolder.findChildFiles (juce::File::findFiles, false, "*.succo");
        for (auto& f : files)
        {
            std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (f));
            if (xml != nullptr && xml->hasTagName ("SUCCO_PRESET"))
            {
                Preset p;
                p.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
                auto* stateXml = xml->getChildByName (apvts.state.getType());
                if (stateXml != nullptr)
                    p.state = juce::ValueTree::fromXml (*stateXml);
                p.factory = false;
                if (p.state.isValid())
                    allPresets.push_back (std::move (p));
            }
        }
    }

private:
    struct Preset
    {
        juce::String name;
        juce::ValueTree state { juce::ValueTree ("SUCCO_PARAMS") };
        bool factory = false;
    };

    void buildFactoryPresets()
    {
        // ---- Init ----
        factoryPresets.push_back (makePreset ("Init",
            [](ChainParams&){}));

        // ---- Lead Vocal Pop ----
        factoryPresets.push_back (makePreset ("Pop Lead",
            [](ChainParams& cp)
            {
                cp.gateBypass = false;
                cp.eqBypass = false;
                cp.compBypass = false;
                cp.deEssBypass = false;
                cp.driveBypass = true;
                cp.doubleBypass = false;
                cp.chorusBypass = true;
                cp.delayBypass = true;
                cp.reverbBypass = false;
                cp.limitBypass = false;

                cp.gate.thresholdDb = -55.0f;
                cp.gate.rangeDb = 30.0f;
                cp.gate.attackMs = 1.0f;
                cp.gate.releaseMs = 50.0f;
                cp.gate.holdMs = 10.0f;

                cp.eq.hpfOn = true;
                cp.eq.hpfFreq = 90.0f;
                cp.eq.highShelfGain = 2.0f;
                cp.eq.lowMidFreq = 300.0f;
                cp.eq.lowMidGain = -2.0f;

                cp.comp.thresholdDb = -22.0f;
                cp.comp.ratio = 4.0f;
                cp.comp.attackMs = 6.0f;
                cp.comp.releaseMs = 80.0f;
                cp.comp.kneeDb = 6.0f;
                cp.comp.makeupDb = 3.0f;

                cp.deEss.freq = 7000.0f;
                cp.deEss.thresholdDb = -28.0f;
                cp.deEss.amount = 0.4f;

                cp.doubler.amount = 0.25f;
                cp.doubler.spread = 0.6f;
                cp.doubler.timeMs = 16.0f;

                cp.reverb.size = 0.5f;
                cp.reverb.decay = 1.0f;
                cp.reverb.damp = 6000.0f;
                cp.reverb.mix = 0.18f;

                cp.limiter.ceilingDb = -1.0f;
                cp.outputGainDb = 0.0f;
            }));

        // ---- Aggressive Rap ----
        factoryPresets.push_back (makePreset ("Rap / Trap",
            [](ChainParams& cp)
            {
                cp.gate.thresholdDb = -50.0f;
                cp.gate.rangeDb = 40.0f;
                cp.comp.thresholdDb = -18.0f;
                cp.comp.ratio = 6.0f;
                cp.comp.attackMs = 3.0f;
                cp.comp.releaseMs = 50.0f;
                cp.comp.makeupDb = 4.0f;
                cp.deEss.amount = 0.6f;
                cp.deEss.freq = 8000.0f;
                cp.drive.driveDb = 4.0f;
                cp.drive.shape = 0.3f;
                cp.doubler.amount = 0.15f;
                cp.reverb.mix = 0.08f;
                cp.reverb.decay = 0.6f;
                cp.limiter.ceilingDb = -0.5f;
            }));

        // ---- Dreamy Lead (chorus + reverb) ----
        factoryPresets.push_back (makePreset ("Dreamy Lead",
            [](ChainParams& cp)
            {
                cp.chorusBypass = false;
                cp.chorus.rate = 0.4f;
                cp.chorus.depth = 3.0f;
                cp.chorus.mix = 0.25f;
                cp.delayBypass = false;
                cp.delay.timeMs = 350.0f;
                cp.delay.feedback = 0.4f;
                cp.delay.pingPong = true;
                cp.delay.mix = 0.18f;
                cp.reverb.decay = 2.0f;
                cp.reverb.size = 0.7f;
                cp.reverb.mix = 0.35f;
                cp.doubler.amount = 0.3f;
            }));

        // ---- Podcaster Clean ----
        factoryPresets.push_back (makePreset ("Podcast Clean",
            [](ChainParams& cp)
            {
                cp.gate.thresholdDb = -45.0f;
                cp.comp.thresholdDb = -18.0f;
                cp.comp.ratio = 3.0f;
                cp.comp.attackMs = 10.0f;
                cp.comp.releaseMs = 150.0f;
                cp.deEss.amount = 0.3f;
                cp.eq.hpfOn = true;
                cp.eq.hpfFreq = 100.0f;
                cp.eq.highShelfGain = 1.5f;
                cp.reverb.mix = 0.06f;
                cp.reverb.decay = 0.6f;
                cp.doubleBypass = true;
                cp.chorusBypass = true;
                cp.delayBypass = true;
                cp.driveBypass = true;
                cp.limiter.ceilingDb = -1.5f;
            }));

        // ---- Lo-Fi / Hazy ----
        factoryPresets.push_back (makePreset ("Lo-Fi Hazy",
            [](ChainParams& cp)
            {
                cp.drive.driveDb = 6.0f;
                cp.drive.shape = 0.7f;
                cp.drive.tone = -0.4f;
                cp.chorusBypass = false;
                cp.chorus.rate = 0.3f;
                cp.chorus.depth = 4.0f;
                cp.chorus.mix = 0.4f;
                cp.eq.lpfOn = true;
                cp.eq.lpfFreq = 8000.0f;
                cp.eq.lowMidFreq = 250.0f;
                cp.eq.lowMidGain = 2.0f;
                cp.reverb.decay = 1.5f;
                cp.reverb.mix = 0.3f;
            }));
    }

    Preset makePreset (const juce::String& name, std::function<void (ChainParams&)> configurator)
    {
        // Start from default chain params
        ChainParams cp;
        configurator (cp);

        // Build a ValueTree from the chain params by mapping back to parameter IDs
        juce::ValueTree vt ("SUCCO_PARAMS");

        using namespace ParamIDs;
        auto set = [&](const char* id, float v) { vt.setProperty (id, v, nullptr); };

        set (inputGain, cp.inputGainDb);
        set (outputGain, cp.outputGainDb);
        set (mix, cp.mix);
        set (punch, cp.punch);

        set (tuneEnable, cp.tuneBypass ? 0.0f : 1.0f);
        set (tuneSemitones, cp.tune.semitones);
        set (tuneAmount, cp.tune.amount);

        set (gateEnable, cp.gateBypass ? 0.0f : 1.0f);
        set (gateThresh, cp.gate.thresholdDb);
        set (gateRange, cp.gate.rangeDb);
        set (gateAttack, cp.gate.attackMs);
        set (gateHold, cp.gate.holdMs);
        set (gateRelease, cp.gate.releaseMs);

        set (eqEnable, cp.eqBypass ? 0.0f : 1.0f);
        set (eqHpfOn, cp.eq.hpfOn ? 1.0f : 0.0f);
        set (eqHpfFreq, cp.eq.hpfFreq);
        set (eqLowShelfFreq, cp.eq.lowShelfFreq);
        set (eqLowShelfGain, cp.eq.lowShelfGain);
        set (eqLowMidFreq, cp.eq.lowMidFreq);
        set (eqLowMidQ, cp.eq.lowMidQ);
        set (eqLowMidGain, cp.eq.lowMidGain);
        set (eqHighMidFreq, cp.eq.highMidFreq);
        set (eqHighMidQ, cp.eq.highMidQ);
        set (eqHighMidGain, cp.eq.highMidGain);
        set (eqHighShelfFreq, cp.eq.highShelfFreq);
        set (eqHighShelfGain, cp.eq.highShelfGain);
        set (eqLpfOn, cp.eq.lpfOn ? 1.0f : 0.0f);
        set (eqLpfFreq, cp.eq.lpfFreq);

        set (compEnable, cp.compBypass ? 0.0f : 1.0f);
        set (compThresh, cp.comp.thresholdDb);
        set (compRatio, cp.comp.ratio);
        set (compAttack, cp.comp.attackMs);
        set (compRelease, cp.comp.releaseMs);
        set (compKnee, cp.comp.kneeDb);
        set (compMakeup, cp.comp.makeupDb);

        set (deEssEnable, cp.deEssBypass ? 0.0f : 1.0f);
        set (deEssFreq, cp.deEss.freq);
        set (deEssThresh, cp.deEss.thresholdDb);
        set (deEssAmount, cp.deEss.amount);

        set (driveEnable, cp.driveBypass ? 0.0f : 1.0f);
        set (driveDrive, cp.drive.driveDb);
        set (driveShape, cp.drive.shape);
        set (driveTone, cp.drive.tone);
        set (driveOutput, cp.drive.outputDb);

        set (doublerEnable, cp.doubleBypass ? 0.0f : 1.0f);
        set (doublerAmount, cp.doubler.amount);
        set (doublerTime, cp.doubler.timeMs);
        set (doublerSpread, cp.doubler.spread);

        set (chorusEnable, cp.chorusBypass ? 0.0f : 1.0f);
        set (chorusRate, cp.chorus.rate);
        set (chorusDepth, cp.chorus.depth);
        set (chorusVoices, (float) (cp.chorus.voices - 1));
        set (chorusMix, cp.chorus.mix);

        set (delayEnable, cp.delayBypass ? 0.0f : 1.0f);
        set (delayTime, cp.delay.timeMs);
        set (delayFeedback, cp.delay.feedback);
        set (delayPingPong, cp.delay.pingPong ? 1.0f : 0.0f);
        set (delayMix, cp.delay.mix);

        set (reverbEnable, cp.reverbBypass ? 0.0f : 1.0f);
        set (reverbSize, cp.reverb.size);
        set (reverbDecay, cp.reverb.decay);
        set (reverbDamp, cp.reverb.damp);
        set (reverbPredelay, cp.reverb.predelayMs);
        set (reverbLowCut, cp.reverb.lowCut);
        set (reverbMix, cp.reverb.mix);

        set (limitEnable, cp.limitBypass ? 0.0f : 1.0f);
        set (limitCeiling, cp.limiter.ceilingDb);
        set (limitRelease, cp.limiter.releaseMs);
        set (limitTruePeak, cp.limiter.truePeak ? 1.0f : 0.0f);

        Preset p;
        p.name = name;
        p.state = vt;
        p.factory = true;
        return p;
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Preset> factoryPresets;
    std::vector<Preset> allPresets;
    int currentIdx = 0;
    juce::File userFolder;
};

} // namespace succo
