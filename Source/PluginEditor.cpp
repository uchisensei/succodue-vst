#include "PluginEditor.h"
#include "params/ParameterManager.h"

namespace succo
{

// Static table of tab names
const std::vector<juce::String>& SuccoAudioProcessorEditor::getTabNames()
{
    static const std::vector<juce::String> names = {
        "HOME", "TUNE", "GATE", "EQ", "COMP", "DE-ESS",
        "DRIVE", "DOUBLE", "CHORUS", "DELAY", "VERB", "LIMIT", "PRESETS"
    };
    return names;
}

SuccoAudioProcessorEditor::SuccoAudioProcessorEditor (SuccoAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      processor (p),
      lookAndFeel(),
      tabBar (p.getAPVTS(), &lookAndFeel),
      display (&lookAndFeel, [this]() { return processor.getOutputLevelLin(); })
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (tabBar);
    tabBar.setListener (this);

    addAndMakeVisible (display);

    // ---- Bottom-row macro knobs ----
    using namespace ParamIDs;
    knobInput  = std::make_unique<KnobComponent> (processor.getAPVTS(), inputGain,  "INPUT",  "Guadagno in ingresso (dB)");
    knobPunch  = std::make_unique<KnobComponent> (processor.getAPVTS(), punch,      "PUNCH",  "Mix di compressione parallela per dare impatto");
    knobMix    = std::make_unique<KnobComponent> (processor.getAPVTS(), mix,        "MIX",    "Bilanciamento segnale secco/processato");
    knobOutput = std::make_unique<KnobComponent> (processor.getAPVTS(), outputGain, "OUTPUT", "Guadagno in uscita (dB)");

    for (auto* k : { knobInput.get(), knobPunch.get(), knobMix.get(), knobOutput.get() })
    {
        k->setLookAndFeelToUse (&lookAndFeel);
        addAndMakeVisible (*k);
    }

    // ---- Preset bar ----
    presetLabel.setColour (juce::Label::textColourId, lookAndFeel.getTextColour());
    presetLabel.setJustificationType (juce::Justification::centredLeft);
    presetLabel.setFont (juce::Font (13.0f));
    addAndMakeVisible (presetLabel);

    addAndMakeVisible (prevPreset);
    prevPreset.onClick = [this]
    {
        const int n = processor.getPresetManager().getNumPresets();
        const int i = processor.getPresetManager().getCurrentPresetIndex();
        const int next = (i - 1 + n) % n;
        processor.getPresetManager().loadPreset (next);
        refreshPresetLabel();
    };

    addAndMakeVisible (nextPreset);
    nextPreset.onClick = [this]
    {
        const int n = processor.getPresetManager().getNumPresets();
        const int i = processor.getPresetManager().getCurrentPresetIndex();
        const int next = (i + 1) % n;
        processor.getPresetManager().loadPreset (next);
        refreshPresetLabel();
    };

    presetCombo.setTextWhenNothingSelected ("Select preset...");
    presetCombo.onChange = [this]
    {
        const int idx = presetCombo.getSelectedItemIndex();
        if (idx >= 0)
        {
            processor.getPresetManager().loadPreset (idx);
            refreshPresetLabel();
        }
    };
    addAndMakeVisible (presetCombo);
    refreshPresetCombo();
    refreshPresetLabel();

    buildModulePanels();
    showPanelForCurrentModule();

    // Window size
    setSize (1100, 720);
    setResizable (true, true);
    setResizeLimits (900, 600, 1600, 1000);

    startTimerHz (30);
}

SuccoAudioProcessorEditor::~SuccoAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void SuccoAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (lookAndFeel.getBackgroundColour());

    // Title bar
    auto top = getLocalBounds().removeFromTop (40);
    g.setColour (lookAndFeel.getPanelColour());
    g.fillRect (top);

    g.setColour (lookAndFeel.getTextColour());
    g.setFont (juce::Font (18.0f, juce::Font::bold));
    g.drawText ("SUCCO", top.reduced (12, 4), juce::Justification::centredLeft, true);

    g.setColour (lookAndFeel.getTextDim());
    g.setFont (juce::Font (11.0f));
    g.drawText ("Vocal Chain  |  v1.0",
                top.reduced (12, 4), juce::Justification::centredRight, true);
}

void SuccoAudioProcessorEditor::resized()
{
    auto b = getLocalBounds();
    b.removeFromTop (40);  // title bar painted over

    // Top tab bar
    tabBar.setBounds (b.removeFromTop (28).reduced (2, 2));

    // Preset bar
    auto presetArea = b.removeFromTop (28).reduced (8, 2);
    prevPreset.setBounds (presetArea.removeFromLeft (24));
    nextPreset.setBounds (presetArea.removeFromLeft (24));
    presetCombo.setBounds (presetArea.removeFromLeft (200));
    presetLabel.setBounds (presetArea);

    // Main content area: display on left, panel on right
    auto content = b.removeFromTop (440);
    auto displayArea = content.removeFromLeft (520).reduced (8);
    display.setBounds (displayArea);

    auto rightArea = content.reduced (8);
    for (auto& panel : modulePanels)
        if (panel != nullptr)
            panel->setBounds (rightArea);

    // Bottom macro row
    auto bottom = b.removeFromTop (110).reduced (8);
    const int knobCount = 4;
    const int kw = bottom.getWidth() / knobCount;
    knobInput->setBounds  (bottom.removeFromLeft (kw).reduced (4));
    knobPunch->setBounds  (bottom.removeFromLeft (kw).reduced (4));
    knobMix->setBounds    (bottom.removeFromLeft (kw).reduced (4));
    knobOutput->setBounds (bottom.removeFromLeft (kw).reduced (4));
}

void SuccoAudioProcessorEditor::timerCallback()
{
    refreshPresetLabel();

    // Push GR data to UI meters (sign-flip so positive dB = gain reduction = bar grows)
    if (compGRMeter != nullptr)
        compGRMeter->setLevel (-processor.getCompressorGR());   // GR dB is positive when reducing; meter expects negative dB
    if (limiterGRMeter != nullptr)
        limiterGRMeter->setLevel (-processor.getLimiterGR());
}

void SuccoAudioProcessorEditor::tabSelected (int moduleIndex)
{
    showPanelForCurrentModule();
    const auto& names = getTabNames();
    if (moduleIndex >= 0 && moduleIndex < (int) names.size())
        display.setModuleName (names[(size_t) moduleIndex]);
}

void SuccoAudioProcessorEditor::showPanelForCurrentModule()
{
    // ui_module is AudioParameterChoice: normalized 0..1, must convert to integer
    const float norm = processor.getAPVTS().getRawParameterValue ("ui_module")->load();
    const int idx = juce::roundToInt (norm * 12.0f);  // 13 options -> 0..12
    for (size_t i = 0; i < modulePanels.size(); ++i)
        if (modulePanels[i] != nullptr)
            modulePanels[i]->setVisible ((int) i == idx);
    repaint();
}

void SuccoAudioProcessorEditor::refreshPresetLabel()
{
    const int i = processor.getPresetManager().getCurrentPresetIndex();
    const juce::String name = processor.getPresetManager().getPresetName (i);
    presetLabel.setText ("PRESET: " + name, juce::dontSendNotification);
    display.setPresetName (name);
    const auto& names = getTabNames();
    const float norm = processor.getAPVTS().getRawParameterValue ("ui_module")->load();
    const int modIdx = juce::roundToInt (norm * 12.0f);
    if (modIdx >= 0 && modIdx < (int) names.size())
        display.setModuleName (names[(size_t) modIdx]);
}

void SuccoAudioProcessorEditor::refreshPresetCombo()
{
    presetCombo.clear();
    const int n = processor.getPresetManager().getNumPresets();
    for (int i = 0; i < n; ++i)
        presetCombo.addItem (processor.getPresetManager().getPresetName (i), i + 1);
    presetCombo.setSelectedItemIndex (processor.getPresetManager().getCurrentPresetIndex(),
                                       juce::dontSendNotification);
}

// ---- Build per-module knob panels ----
void SuccoAudioProcessorEditor::buildModulePanels()
{
    using namespace ParamIDs;
    auto& apvts = processor.getAPVTS();

    auto newPanel = [this](int idx, const juce::String& title) -> ModulePanel*
    {
        auto p = std::make_unique<ModulePanel>(&lookAndFeel, title);
        auto* raw = p.get();
        modulePanels[(size_t) idx] = std::move (p);
        addAndMakeVisible (*raw);
        raw->setVisible (false);
        return raw;
    };

    auto addKnobTo = [this, &apvts](ModulePanel* panel, const char* pid,
                                    const char* label, const char* tipIT)
    {
        auto k = std::make_unique<KnobComponent> (apvts, pid, label, tipIT);
        k->setLookAndFeelToUse (&lookAndFeel);
        panel->addKnob (std::move (k));
    };

    // TUNE (1)
    {
        auto* p = newPanel (1, "TUNE");
        addKnobTo (p, tuneEnable,     "ENABLE",    "Attiva il modulo Tune (pitch shift)");
        addKnobTo (p, tuneSemitones, "SEMITONES", "Shift in semitoni (-12..+12)");
        addKnobTo (p, tuneAmount,    "AMOUNT",    "Quantita applicata (0..100%)");
        // Per-module preset bar
        // PITCH SHIFT bar
        auto curve = std::make_unique<CurveDisplayComponent> (&lookAndFeel, "PITCH SHIFT",
            [this](juce::Graphics& g, juce::Rectangle<int> area)
            {
                auto& a = processor.getAPVTS();
                const float semi = a.getRawParameterValue(ParamIDs::tuneSemitones)->load();
                const float amount = a.getRawParameterValue(ParamIDs::tuneAmount)->load();
                const float eff = semi * amount;
                // Center line
                g.setColour(juce::Colours::white.withAlpha(0.2f));
                g.drawVerticalLine(area.getCentreX(), (float)area.getY() + 4, (float)area.getBottom() - 4);
                // Bar
                const float t = (eff + 12.0f) / 24.0f;
                const int barX = area.getX() + (int)(t * area.getWidth());
                g.setColour(lookAndFeel.getYellow());
                const int x0 = juce::jmin(barX, area.getCentreX());
                const int w = juce::jmax(2, std::abs(barX - area.getCentreX()));
                g.fillRect(juce::Rectangle<int>(x0, area.getY() + 8, w, area.getHeight() - 16));
                g.setColour(juce::Colours::white);
                g.drawVerticalLine(barX, (float)area.getY() + 4, (float)area.getBottom() - 4);
                g.setColour(lookAndFeel.getYellowBright());
                g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
                g.drawText(juce::String(eff, 1) + " st", area.reduced(8, 2), juce::Justification::topLeft, true);
            });
        p->setTopComponent(std::move(curve), 60);
    }

    // GATE (2)
    {
        auto* p = newPanel (2, "GATE");
        addKnobTo (p, gateEnable, "ENABLE",  "Attiva il gate");
        addKnobTo (p, gateThresh, "THRESH",  "Soglia (dB sotto la quale il gate si chiude)");
        addKnobTo (p, gateRange,  "RANGE",   "Attenuazione a gate chiuso (dB)");
        addKnobTo (p, gateAttack, "ATTACK",  "Tempo di attacco (ms)");
        addKnobTo (p, gateHold,    "HOLD",    "Tempo di hold (ms)");
        addKnobTo (p, gateRelease,"RELEASE", "Tempo di rilascio (ms)");
        // GATE envelope visualization: open region (green) above threshold, closed region (red) below
        auto curve = std::make_unique<CurveDisplayComponent> (&lookAndFeel, "ENVELOPE",
            [this](juce::Graphics& g, juce::Rectangle<int> area)
            {
                auto& a = processor.getAPVTS();
                const float thresh = a.getRawParameterValue(ParamIDs::gateThresh)->load(); // -80..0
                const float range = a.getRawParameterValue(ParamIDs::gateRange)->load();
                // Map: thresh 0 dB = top, thresh -80 dB = bottom
                const int threshY = area.getY() + (int)((1.0f - (-thresh / 80.0f)) * area.getHeight());
                // Closed region (below threshold) - red
                g.setColour(juce::Colour(0x66ff5577));
                g.fillRect(juce::Rectangle<int>(area.getX(), threshY, area.getWidth(), area.getBottom() - threshY));
                // Open region (above threshold) - green
                g.setColour(juce::Colour(0x664dd599));
                g.fillRect(juce::Rectangle<int>(area.getX(), area.getY(), area.getWidth(), threshY - area.getY()));
                // Threshold line (yellow)
                g.setColour(lookAndFeel.getYellow());
                g.drawHorizontalLine(threshY, (float)area.getX() + 4, (float)area.getRight() - 4);
                // Range marker
                const int rangeY = juce::jlimit(area.getY(), area.getBottom() - 1, threshY + (int)(range * 4));
                g.setColour(juce::Colours::white.withAlpha(0.5f));
                g.drawHorizontalLine(rangeY, (float)area.getX() + 4, (float)area.getRight() - 4);
            });
        p->setTopComponent(std::move(curve), 70);
    }

    // EQ (3)
    {
        auto* p = newPanel (3, "EQ");
        addKnobTo (p, eqEnable,        "ENABLE",   "Attiva l'equalizzatore");
        addKnobTo (p, eqHpfOn,         "HPF ON",   "Abilita il filtro passaalto");
        addKnobTo (p, eqHpfFreq,       "HPF",      "Frequenza del passaalto (Hz)");
        addKnobTo (p, eqLowShelfFreq,  "LS FREQ",  "Frequenza low shelf (Hz)");
        addKnobTo (p, eqLowShelfGain,  "LS GAIN",  "Guadagno low shelf (dB)");
        addKnobTo (p, eqLowMidFreq,    "LM FREQ",  "Frequenza banda low-mid (Hz)");
        addKnobTo (p, eqLowMidQ,       "LM Q",     "Fattore Q low-mid");
        addKnobTo (p, eqLowMidGain,    "LM GAIN",  "Guadagno low-mid (dB)");
        addKnobTo (p, eqHighMidFreq,   "HM FREQ",  "Frequenza banda high-mid (Hz)");
        addKnobTo (p, eqHighMidQ,      "HM Q",     "Fattore Q high-mid");
        addKnobTo (p, eqHighMidGain,   "HM GAIN",  "Guadagno high-mid (dB)");
        addKnobTo (p, eqHighShelfFreq, "HS FREQ", "Frequenza high shelf (Hz)");
        addKnobTo (p, eqHighShelfGain, "HS GAIN", "Guadagno high shelf (dB)");
        addKnobTo (p, eqLpfOn,         "LPF ON",   "Abilita il filtro passabasso");
        addKnobTo (p, eqLpfFreq,       "LPF",      "Frequenza del passabasso (Hz)");
        // FREQUENCY RESPONSE curve (sum of all 6 filters magnitude)
        auto curve = std::make_unique<CurveDisplayComponent> (&lookAndFeel, "FREQUENCY RESPONSE",
            [this](juce::Graphics& g, juce::Rectangle<int> area)
            {
                auto& a = processor.getAPVTS();
                bool hpfOn = a.getRawParameterValue(ParamIDs::eqHpfOn)->load() > 0.5f;
                bool lpfOn = a.getRawParameterValue(ParamIDs::eqLpfOn)->load() > 0.5f;
                float hpfFreq = a.getRawParameterValue(ParamIDs::eqHpfFreq)->load();
                float lsFreq = a.getRawParameterValue(ParamIDs::eqLowShelfFreq)->load();
                float lsGain = a.getRawParameterValue(ParamIDs::eqLowShelfGain)->load();
                float lmFreq = a.getRawParameterValue(ParamIDs::eqLowMidFreq)->load();
                float lmQ = a.getRawParameterValue(ParamIDs::eqLowMidQ)->load();
                float lmGain = a.getRawParameterValue(ParamIDs::eqLowMidGain)->load();
                float hmFreq = a.getRawParameterValue(ParamIDs::eqHighMidFreq)->load();
                float hmQ = a.getRawParameterValue(ParamIDs::eqHighMidQ)->load();
                float hmGain = a.getRawParameterValue(ParamIDs::eqHighMidGain)->load();
                float hsFreq = a.getRawParameterValue(ParamIDs::eqHighShelfFreq)->load();
                float hsGain = a.getRawParameterValue(ParamIDs::eqHighShelfGain)->load();
                float lpfFreq = a.getRawParameterValue(ParamIDs::eqLpfFreq)->load();
                const double sr = 44100.0;
                auto hpfC = hpfOn ? juce::dsp::IIR::Coefficients<float>::makeHighPass(sr, hpfFreq, 0.707f) : nullptr;
                auto lsC  = juce::dsp::IIR::Coefficients<float>::makeLowShelf(sr, lsFreq, 0.707f, juce::Decibels::decibelsToGain(lsGain));
                auto lmC  = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, lmFreq, lmQ, juce::Decibels::decibelsToGain(lmGain));
                auto hmC  = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sr, hmFreq, hmQ, juce::Decibels::decibelsToGain(hmGain));
                auto hsC  = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sr, hsFreq, 0.707f, juce::Decibels::decibelsToGain(hsGain));
                auto lpfC = lpfOn ? juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, lpfFreq, 0.707f) : nullptr;
                juce::Path curvePath;
                const int N = 200;
                for (int i = 0; i < N; ++i)
                {
                    const float t = (float)i / (float)(N - 1);
                    const double freq = 20.0 * std::pow(1000.0, t);
                    double magDb = 0.0;
                    if (hpfC != nullptr) magDb += juce::Decibels::gainToDecibels(hpfC->getMagnitudeForFrequency(freq, sr));
                    magDb += juce::Decibels::gainToDecibels(lsC->getMagnitudeForFrequency(freq, sr));
                    magDb += juce::Decibels::gainToDecibels(lmC->getMagnitudeForFrequency(freq, sr));
                    magDb += juce::Decibels::gainToDecibels(hmC->getMagnitudeForFrequency(freq, sr));
                    magDb += juce::Decibels::gainToDecibels(hsC->getMagnitudeForFrequency(freq, sr));
                    if (lpfC != nullptr) magDb += juce::Decibels::gainToDecibels(lpfC->getMagnitudeForFrequency(freq, sr));
                    const float y = juce::jmap(magDb, -18.0, 18.0, 1.0, 0.0);
                    const float px = (float)area.getX() + t * area.getWidth();
                    const float py = (float)area.getY() + y * area.getHeight();
                    if (i == 0) curvePath.startNewSubPath(px, py);
                    else        curvePath.lineTo(px, py);
                }
                // Diagonal reference line (linear 0 dB)
                const int midY = area.getCentreY();
                g.setColour(juce::Colours::white.withAlpha(0.15f));
                g.drawHorizontalLine(midY, (float)area.getX() + 2, (float)area.getRight() - 2);
                g.setColour(lookAndFeel.getYellowBright());
                g.strokePath(curvePath, juce::PathStrokeType(2.0f));
            });
        p->setTopComponent(std::move(curve), 110);
    }

    // COMP (4)
    {
        auto* p = newPanel (4, "COMP");
        addKnobTo (p, compEnable,  "ENABLE",  "Attiva il compressore");
        addKnobTo (p, compThresh,  "THRESH",  "Soglia (dB)");
        addKnobTo (p, compRatio,   "RATIO",   "Rapporto (n:1)");
        addKnobTo (p, compAttack,  "ATTACK",  "Tempo di attacco (ms)");
        addKnobTo (p, compRelease, "RELEASE", "Tempo di rilascio (ms)");
        addKnobTo (p, compKnee,    "KNEE",    "Ammorbidimento del ginocchio (dB)");
        addKnobTo (p, compMakeup,  "MAKEUP",  "Guadagno di compensazione (dB)");
        // INPUT/OUTPUT transfer curve
        auto curve = std::make_unique<CurveDisplayComponent> (&lookAndFeel, "INPUT / OUTPUT",
            [this](juce::Graphics& g, juce::Rectangle<int> area)
            {
                auto& a = processor.getAPVTS();
                const float thresh = a.getRawParameterValue(ParamIDs::compThresh)->load();
                const float ratio = a.getRawParameterValue(ParamIDs::compRatio)->load();
                const float knee = a.getRawParameterValue(ParamIDs::compKnee)->load();
                const float kneeHalf = knee * 0.5f;
                const float r = juce::jmax(1.0f, ratio);
                juce::Path curvePath;
                const int N = 200;
                for (int i = 0; i < N; ++i)
                {
                    const float t = (float)i / (float)(N - 1);
                    const float inDb = juce::jmap(t, -60.0f, 0.0f);
                    float outDb;
                    if (inDb < thresh - kneeHalf)
                        outDb = inDb;
                    else if (inDb > thresh + kneeHalf)
                        outDb = thresh + (inDb - thresh) / r;
                    else
                        outDb = inDb - (1.0f - 1.0f / r) * (inDb - (thresh - kneeHalf)) * (inDb - (thresh - kneeHalf)) / (2.0f * juce::jmax(0.01f, knee));
                    const float px = area.getX() + juce::jmap(inDb, -60.0f, 0.0f, 0.0f, 1.0f) * area.getWidth();
                    const float py = area.getY() + juce::jmap(outDb, -60.0f, 0.0f, 1.0f, 0.0f) * area.getHeight();
                    if (i == 0) curvePath.startNewSubPath(px, py);
                    else        curvePath.lineTo(px, py);
                }
                g.setColour(juce::Colours::white.withAlpha(0.15f));
                g.drawLine((float)area.getX(), (float)area.getBottom(), (float)area.getRight(), (float)area.getY(), 1.0f);
                g.setColour(lookAndFeel.getYellowBright());
                g.strokePath(curvePath, juce::PathStrokeType(2.0f));
                const int tX = area.getX() + (int)(juce::jmap(thresh, -60.0f, 0.0f, 0.0f, 1.0f) * area.getWidth());
                const int tY = area.getY() + (int)(juce::jmap(thresh, -60.0f, 0.0f, 1.0f, 0.0f) * area.getHeight());
                g.setColour(lookAndFeel.getYellow());
                g.drawVerticalLine(tX, (float)area.getY() + 2, (float)area.getBottom() - 2);
                g.drawHorizontalLine(tY, (float)area.getX() + 2, (float)area.getRight() - 2);
            });
        p->setTopComponent(std::move(curve), 90);

        // Yellow GR meter on the right side of the panel (vertical)
        auto meter = std::make_unique<MeterComponent> (&lookAndFeel,
                                                       MeterComponent::Mode::GainReduction,
                                                       "GR");
        meter->setRange (-24.0f, 0.0f);
        compGRMeter = meter.get();
        p->setExtraComponent (std::move (meter), 60);
    }

    // DE-ESS (5)
    {
        auto* p = newPanel (5, "DE-ESS");
        addKnobTo (p, deEssEnable, "ENABLE",  "Attiva il de-esser");
        addKnobTo (p, deEssFreq,   "FREQ",    "Frequenza di crossover (Hz)");
        addKnobTo (p, deEssThresh, "THRESH",  "Soglia (dB)");
        addKnobTo (p, deEssAmount, "AMOUNT",  "Quantita di riduzione (0..100%)");
        // FREQUENCY BAND visualization
        auto curve = std::make_unique<CurveDisplayComponent> (&lookAndFeel, "FREQUENCY BAND",
            [this](juce::Graphics& g, juce::Rectangle<int> area)
            {
                auto& a = processor.getAPVTS();
                const float freq = a.getRawParameterValue(ParamIDs::deEssFreq)->load();
                const float amount = a.getRawParameterValue(ParamIDs::deEssAmount)->load();
                const float t = std::log(freq / 100.0f) / std::log(200.0f);
                const int centerX = area.getX() + (int)(juce::jlimit(0.0f, 1.0f, t) * area.getWidth());
                const int bandWidth = juce::jmax(20, area.getWidth() / 12);
                const int bandHeight = (int)(amount * (area.getHeight() - 8) * 0.85f);
                juce::Path bell;
                const int baseY = area.getBottom() - 4;
                bell.startNewSubPath((float)(centerX - bandWidth), (float)baseY);
                bell.quadraticTo((float)centerX, (float)(baseY - bandHeight), (float)(centerX + bandWidth), (float)baseY);
                juce::ColourGradient grad(lookAndFeel.getYellowBright().withAlpha(0.5f),
                                          (float)centerX, (float)(baseY - bandHeight),
                                          lookAndFeel.getYellow().withAlpha(0.1f),
                                          (float)centerX, (float)baseY, false);
                g.setGradientFill(grad);
                g.fillPath(bell);
                g.setColour(lookAndFeel.getYellow());
                g.drawVerticalLine(centerX, (float)area.getY() + 2, (float)area.getBottom() - 2);
                g.setColour(lookAndFeel.getYellowBright());
                g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
                g.drawText(juce::String((int)freq) + " Hz", area.reduced(8, 2), juce::Justification::topLeft, true);
            });
        p->setTopComponent(std::move(curve), 70);
    }

    // DRIVE (6)
    {
        auto* p = newPanel (6, "DRIVE");
        addKnobTo (p, driveEnable, "ENABLE",  "Attiva il drive (saturation)");
        addKnobTo (p, driveDrive,  "DRIVE",   "Quantita di drive (dB)");
        addKnobTo (p, driveShape,  "SHAPE",   "Forma del clip (0=tanh soft, 1=hard)");
        addKnobTo (p, driveTone,   "TONE",    "Tono post-tilt (-1=basso, +1=alto)");
        addKnobTo (p, driveOutput, "OUTPUT",  "Guadagno in uscita (dB)");
        // WAVESHAPER curve
        auto curve = std::make_unique<CurveDisplayComponent> (&lookAndFeel, "WAVESHAPER",
            [this](juce::Graphics& g, juce::Rectangle<int> area)
            {
                auto& a = processor.getAPVTS();
                const float driveDb = a.getRawParameterValue(ParamIDs::driveDrive)->load();
                const float shape = a.getRawParameterValue(ParamIDs::driveShape)->load();
                const float driveLin = std::pow(10.0f, driveDb / 20.0f);
                const float k = 2.0f;

                // 4-mode waveshaper (matching Drive.h implementation)
                auto tube = [k](float v) { return std::tanh (k * v) / std::tanh (k); };
                auto tape = [](float v) {
                    return (v >= 0.0f) ? std::tanh (1.5f * v) * 0.95f : std::tanh (1.2f * v) * 0.92f;
                };
                auto transformer = [](float v) {
                    const float bias2 = 0.15f;
                    return std::tanh (2.0f * (v + bias2)) - std::tanh (2.0f * bias2);
                };
                auto fuzz = [](float v) { return juce::jlimit (-1.0f, 1.0f, v * 1.5f); };

                auto waveshape = [&](float v) {
                    if (shape < 0.333f) {
                        const float t = shape / 0.333f;
                        return (1.0f - t) * tube (v) + t * tape (v);
                    } else if (shape < 0.667f) {
                        const float t = (shape - 0.333f) / 0.334f;
                        return (1.0f - t) * tape (v) + t * transformer (v);
                    } else {
                        const float t = (shape - 0.667f) / 0.333f;
                        return (1.0f - t) * transformer (v) + t * fuzz (v);
                    }
                };

                g.setColour(juce::Colours::white.withAlpha(0.15f));
                g.drawLine((float)area.getX(), (float)area.getBottom(), (float)area.getRight(), (float)area.getY(), 1.0f);
                juce::Path curvePath;
                const int N = 200;
                for (int i = 0; i < N; ++i)
                {
                    const float t = (float)i / (float)(N - 1);
                    const float x_in = (t - 0.5f) * 2.0f;
                    const float x = x_in * driveLin;
                    const float y = waveshape(x);
                    const float px = area.getX() + ((x_in + 1.0f) * 0.5f) * area.getWidth();
                    const float py = area.getY() + ((1.0f - (y + 1.0f) * 0.5f)) * area.getHeight();
                    if (i == 0) curvePath.startNewSubPath(px, py);
                    else        curvePath.lineTo(px, py);
                }
                g.setColour(lookAndFeel.getYellowBright());
                g.strokePath(curvePath, juce::PathStrokeType(2.0f));

                // Mode label
                juce::String modeName;
                if (shape < 0.333f) modeName = "TUBE";
                else if (shape < 0.667f) modeName = "TAPE";
                else if (shape < 1.0f) modeName = "TRANSFORMER";
                else modeName = "FUZZ";
                g.setColour(lookAndFeel.getYellowBright());
                g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
                g.drawText("MODE: " + modeName, area.reduced(8, 2), juce::Justification::topLeft, true);
            });
        p->setTopComponent(std::move(curve), 80);
    }

    // DOUBLE (7)
    {
        auto* p = newPanel (7, "DOUBLE");
        addKnobTo (p, doublerEnable, "ENABLE",  "Attiva il doubler (raddoppio vocale)");
        addKnobTo (p, doublerAmount, "AMOUNT",  "Livello del raddoppio (0..100%)");
        addKnobTo (p, doublerTime,   "TIME",    "Tempo di ritardo (ms)");
        addKnobTo (p, doublerSpread, "SPREAD",  "Larghezza stereo (0..100%)");
        // STEREO WIDTH visualization
        auto curve = std::make_unique<CurveDisplayComponent> (&lookAndFeel, "STEREO WIDTH",
            [this](juce::Graphics& g, juce::Rectangle<int> area)
            {
                auto& a = processor.getAPVTS();
                const float amount = a.getRawParameterValue(ParamIDs::doublerAmount)->load();
                const float spread = a.getRawParameterValue(ParamIDs::doublerSpread)->load();
                const int pan = (int)(spread * amount * area.getWidth() * 0.4f);
                const int midX = area.getCentreX();
                g.setColour(juce::Colours::white.withAlpha(0.2f));
                g.drawVerticalLine(midX, (float)area.getY() + 4, (float)area.getBottom() - 4);
                g.setColour(lookAndFeel.getYellow());
                g.fillRect(juce::Rectangle<int>(midX - pan, area.getY() + 12, pan * 2, 18));
                g.fillRect(juce::Rectangle<int>(midX - pan, area.getBottom() - 30, pan * 2, 18));
                g.setColour(lookAndFeel.getYellowBright());
                g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
                g.drawText("L", area.withTrimmedTop(4).withTrimmedLeft(8).withHeight(12), juce::Justification::topLeft, true);
                g.drawText("R", area.withTrimmedBottom(4).withTrimmedLeft(8).withHeight(12), juce::Justification::bottomLeft, true);
            });
        p->setTopComponent(std::move(curve), 60);
    }

    // CHORUS (8)
    {
        auto* p = newPanel (8, "CHORUS");
        addKnobTo (p, chorusEnable, "ENABLE",  "Attiva il chorus");
        addKnobTo (p, chorusRate,   "RATE",    "Velocita del LFO (Hz)");
        addKnobTo (p, chorusDepth,  "DEPTH",   "Profondita di modulazione (ms)");
        addKnobTo (p, chorusVoices, "VOICES",  "Numero di voci (1..4)");
        addKnobTo (p, chorusMix,   "MIX",     "Mix dry/wet");
        // LFO WAVEFORM visualization
        auto curve = std::make_unique<CurveDisplayComponent> (&lookAndFeel, "LFO WAVEFORM",
            [this](juce::Graphics& g, juce::Rectangle<int> area)
            {
                auto& a = processor.getAPVTS();
                const float rate = a.getRawParameterValue(ParamIDs::chorusRate)->load();
                const float depth = a.getRawParameterValue(ParamIDs::chorusDepth)->load();
                const int voices = (int)a.getRawParameterValue(ParamIDs::chorusVoices)->load() + 1;
                const int midY = area.getCentreY();
                g.setColour(juce::Colours::white.withAlpha(0.15f));
                g.drawHorizontalLine(midY, (float)area.getX() + 2, (float)area.getRight() - 2);
                for (int v = 0; v < voices; ++v)
                {
                    juce::Path vp;
                    const float phaseOffset = (float)v / (float)voices * juce::MathConstants<float>::pi * 2.0f;
                    const int N = 100;
                    for (int i = 0; i <= N; ++i)
                    {
                        const float t = (float)i / (float)N;
                        const float phase = t * juce::MathConstants<float>::pi * 4.0f + phaseOffset;
                        const float y = std::sin(phase) * depth * 0.3f;
                        const float px = area.getX() + t * area.getWidth();
                        const float py = midY + y * (area.getHeight() * 0.4f);
                        if (i == 0) vp.startNewSubPath(px, py);
                        else        vp.lineTo(px, py);
                    }
                    const float alpha = 0.3f + 0.7f / (float)voices;
                    g.setColour(lookAndFeel.getYellow().withAlpha(alpha));
                    g.strokePath(vp, juce::PathStrokeType(1.5f));
                }
                g.setColour(lookAndFeel.getYellowBright());
                g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
                g.drawText(juce::String(rate, 2) + " Hz  x" + juce::String(voices), area.reduced(8, 2), juce::Justification::topLeft, true);
            });
        p->setTopComponent(std::move(curve), 60);
    }

    // DELAY (9)
    {
        auto* p = newPanel (9, "DELAY");
        addKnobTo (p, delayEnable,   "ENABLE",    "Attiva il delay");
        addKnobTo (p, delayTime,     "TIME",      "Tempo di ritardo (ms)");
        addKnobTo (p, delayFeedback, "FEEDBACK",  "Quantita di feedback (0..95%)");
        addKnobTo (p, delayPingPong, "PING-PONG", "Modalita ping-pong");
        addKnobTo (p, delayMix,      "MIX",       "Mix dry/wet");
        // ECHO TIMELINE visualization
        auto curve = std::make_unique<CurveDisplayComponent> (&lookAndFeel, "ECHO TIMELINE",
            [this](juce::Graphics& g, juce::Rectangle<int> area)
            {
                auto& a = processor.getAPVTS();
                const float timeMs = a.getRawParameterValue(ParamIDs::delayTime)->load();
                const float feedback = a.getRawParameterValue(ParamIDs::delayFeedback)->load();
                const bool pingPong = a.getRawParameterValue(ParamIDs::delayPingPong)->load() > 0.5f;
                const int lineY = area.getCentreY();
                g.setColour(juce::Colours::white.withAlpha(0.2f));
                g.drawHorizontalLine(lineY, (float)area.getX() + 4, (float)area.getRight() - 4);
                const int maxEchoes = 8;
                for (int i = 1; i <= maxEchoes; ++i)
                {
                    const float t = (float)i / (float)maxEchoes;
                    const float x = area.getX() + 10.0f + t * (area.getWidth() - 20);
                    const float amp = std::pow(feedback, (float)i);
                    if (amp < 0.05f) break;
                    const int radius = juce::jmax(3, 8 - (i - 1));
                    const int dotY = pingPong ? (lineY + (i % 2 == 0 ? -22 : 22)) : lineY;
                    const juce::Colour dotCol = lookAndFeel.getYellow().withAlpha(0.4f + amp * 0.6f);
                    g.setColour(dotCol);
                    g.fillEllipse(x - radius, dotY - radius, radius * 2, radius * 2);
                    g.setColour(dotCol.withAlpha(0.3f));
                    g.drawVerticalLine((int)x, (float)area.getY() + 4, (float)area.getBottom() - 4);
                }
                g.setColour(lookAndFeel.getYellowBright());
                g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
                g.drawText(juce::String((int)timeMs) + " ms  FB " + juce::String((int)(feedback * 100)) + "%", area.reduced(8, 2), juce::Justification::topLeft, true);
            });
        p->setTopComponent(std::move(curve), 70);
    }

    // VERB (10)
    {
        auto* p = newPanel (10, "VERB");
        addKnobTo (p, reverbEnable,   "ENABLE",   "Attiva la reverb");
        addKnobTo (p, reverbSize,     "SIZE",     "Dimensione della stanza (0..100%)");
        addKnobTo (p, reverbDecay,    "DECAY",    "Tempo di decadimento (s)");
        addKnobTo (p, reverbDamp,     "DAMP",     "Smorzamento (Hz)");
        addKnobTo (p, reverbPredelay, "PREDELAY", "Ritardo iniziale (ms)");
        addKnobTo (p, reverbLowCut,   "LOW CUT",  "Taglio basso (Hz)");
        addKnobTo (p, reverbMix,      "MIX",      "Mix dry/wet");
        // DECAY ENVELOPE visualization
        auto curve = std::make_unique<CurveDisplayComponent> (&lookAndFeel, "DECAY ENVELOPE",
            [this](juce::Graphics& g, juce::Rectangle<int> area)
            {
                auto& a = processor.getAPVTS();
                const float size = a.getRawParameterValue(ParamIDs::reverbSize)->load();
                const float decay = a.getRawParameterValue(ParamIDs::reverbDecay)->load();
                const float damp = a.getRawParameterValue(ParamIDs::reverbDamp)->load();
                juce::Path curvePath;
                juce::Path fillPath;
                const int N = 200;
                for (int i = 0; i < N; ++i)
                {
                    const float t = (float)i / (float)(N - 1);
                    const float tSec = t * 5.0f;
                    const float amp = std::exp(-3.0f * tSec / juce::jmax(0.1f, decay)) * (0.5f + 0.5f * size);
                    const float px = area.getX() + t * area.getWidth();
                    const float py = area.getY() + (1.0f - juce::jlimit(0.0f, 1.0f, amp)) * area.getHeight();
                    if (i == 0) {
                        curvePath.startNewSubPath(px, py);
                        fillPath.startNewSubPath((float)area.getX(), (float)area.getBottom());
                        fillPath.lineTo(px, py);
                    } else {
                        curvePath.lineTo(px, py);
                        fillPath.lineTo(px, py);
                    }
                }
                fillPath.lineTo((float)area.getRight(), (float)area.getBottom());
                fillPath.closeSubPath();
                juce::ColourGradient grad(lookAndFeel.getYellow().withAlpha(0.3f),
                                          (float)area.getX(), (float)area.getY(),
                                          lookAndFeel.getYellow().withAlpha(0.05f),
                                          (float)area.getX(), (float)area.getBottom(), false);
                g.setGradientFill(grad);
                g.fillPath(fillPath);
                g.setColour(lookAndFeel.getYellowBright());
                g.strokePath(curvePath, juce::PathStrokeType(2.0f));
                const float dampT = juce::jmap(damp, 500.0f, 20000.0f, 0.0f, 1.0f);
                const int dampX = area.getX() + (int)(juce::jlimit(0.0f, 1.0f, dampT) * area.getWidth());
                g.setColour(juce::Colours::white.withAlpha(0.3f));
                g.drawVerticalLine(dampX, (float)area.getY() + 2, (float)area.getBottom() - 2);
                g.setColour(lookAndFeel.getYellowBright());
                g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
                g.drawText("RT60: " + juce::String(decay, 2) + " s", area.reduced(8, 2), juce::Justification::topLeft, true);
            });
        p->setTopComponent(std::move(curve), 80);
    }

    // LIMIT (11)
    {
        auto* p = newPanel (11, "LIMIT");
        addKnobTo (p, limitEnable,  "ENABLE",    "Attiva il limitatore");
        addKnobTo (p, limitCeiling, "CEILING",   "Soglia massima di uscita (dB)");
        addKnobTo (p, limitRelease, "RELEASE",   "Tempo di rilascio (ms)");
        addKnobTo (p, limitTruePeak,"TRUE PEAK", "Controllo inter-sample peak (4x OS)");
        // INPUT/OUTPUT limiting curve
        auto curve = std::make_unique<CurveDisplayComponent> (&lookAndFeel, "INPUT / OUTPUT",
            [this](juce::Graphics& g, juce::Rectangle<int> area)
            {
                auto& a = processor.getAPVTS();
                const float ceiling = a.getRawParameterValue(ParamIDs::limitCeiling)->load();
                const float ceilingLin = std::pow(10.0f, ceiling / 20.0f);
                const int ceilingY = area.getY() + (int)(juce::jmap(ceilingLin, 0.0f, 1.0f, 1.0f, 0.0f) * area.getHeight());
                g.setColour(juce::Colours::white.withAlpha(0.15f));
                g.drawLine((float)area.getX(), (float)area.getBottom(), (float)area.getRight(), (float)area.getY(), 1.0f);
                juce::Path curvePath;
                const int N = 200;
                for (int i = 0; i < N; ++i)
                {
                    const float t = (float)i / (float)(N - 1);
                    const float in = t;
                    const float out = (in > ceilingLin) ? ceilingLin : in;
                    const float px = area.getX() + in * area.getWidth();
                    const float py = area.getY() + (1.0f - out) * area.getHeight();
                    if (i == 0) curvePath.startNewSubPath(px, py);
                    else        curvePath.lineTo(px, py);
                }
                g.setColour(lookAndFeel.getYellowBright());
                g.strokePath(curvePath, juce::PathStrokeType(2.0f));
                g.setColour(lookAndFeel.getDanger());
                g.drawHorizontalLine(ceilingY, (float)area.getX() + 2, (float)area.getRight() - 2);
                g.setColour(lookAndFeel.getYellowBright());
                g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
                g.drawText("Ceiling: " + juce::String(ceiling, 1) + " dB", area.reduced(8, 2), juce::Justification::topLeft, true);
            });
        p->setTopComponent(std::move(curve), 80);
        // Yellow GR meter on the right side (vertical)
        auto meter = std::make_unique<MeterComponent> (&lookAndFeel,
                                                       MeterComponent::Mode::GainReduction,
                                                       "GR");
        meter->setRange (-18.0f, 0.0f);
        limiterGRMeter = meter.get();
        p->setExtraComponent (std::move (meter), 60);
    }

    // HOME (0) - intro text
    {
        auto* p = newPanel (0, "HOME");
        auto hint = std::make_unique<juce::Label>("hint",
            "SUCCO Vocal Chain\n\n"
            "Seleziona un modulo dalla barra in alto per iniziare.\n"
            "I preset sono selezionabili dal menu a tendina in basso.\n"
            "Salva i tuoi preset in ~/Documents/SUCCO/Presets");
        hint->setColour (juce::Label::textColourId, lookAndFeel.getTextColour());
        hint->setJustificationType (juce::Justification::centred);
        hint->setFont (juce::Font (14.0f));
        p->addOwned (std::move (hint));  // transfers ownership to the panel
    }

    // PRESETS (12)
    {
        auto* p = newPanel (12, "PRESETS");
        auto saveBtn = std::make_unique<juce::TextButton>("Save Preset");
        saveBtn->setColour (juce::TextButton::buttonColourId, lookAndFeel.getAccentColour());
        saveBtn->setColour (juce::TextButton::textColourOnId, lookAndFeel.getBackgroundColour());
        saveBtn->setColour (juce::TextButton::textColourOffId, lookAndFeel.getTextColour());
        saveBtn->onClick = [this]()
        {
            juce::AlertWindow w ("Salva preset", "Inserisci il nome del preset:", juce::AlertWindow::NoIcon);
            w.addTextEditor ("name", "My Preset");
            w.addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
            w.addButton ("Annulla", 0, juce::KeyPress (juce::KeyPress::escapeKey));
            if (w.runModalLoop() == 1)
            {
                auto name = w.getTextEditorContents ("name");
                if (name.isNotEmpty())
                {
                    processor.getPresetManager().saveUserPreset (name);
                    refreshPresetCombo();
                    refreshPresetLabel();
                }
            }
        };
        p->addOwned (std::move (saveBtn));  // transfers ownership to the panel
    }
}

} // namespace succo
