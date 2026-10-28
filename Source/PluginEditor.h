#pragma once

// ---------------------------------------------------------------------------
// SuccoAudioProcessorEditor - main UI. Hosts:
//   - top tab bar (module navigation)
//   - central display (logo + preset name + meter)
//   - module-specific panel area (knobs swap with the tab)
//   - bottom row (Input / Punch / Mix / Output)
// ---------------------------------------------------------------------------

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "gui/SuccoLookAndFeel.h"
#include "gui/KnobComponent.h"
#include "gui/MeterComponent.h"
#include "gui/CurveDisplayComponent.h"
#include "gui/ModuleTabBar.h"
#include "gui/DisplayComponent.h"
#include <array>
#include <vector>

namespace succo
{

// A panel holds a set of knobs for one module tab.
// Layout (top to bottom):
//   1. Title bar (22 px) - drawn in paint()
//   2. Optional preset bar (~30 px) - preset dropdown + A/B buttons
//   3. Optional top component (curve display, etc.)
//   4. Knob grid
// Optional right component (e.g. vertical GR meter) on the RIGHT side.
class ModulePanel : public juce::Component
{
public:
    ModulePanel (SuccoLookAndFeel* lf, const juce::String& title)
        : lookAndFeel (lf), panelTitle (title) {}

    void addKnob (std::unique_ptr<KnobComponent> knob)
    {
        addAndMakeVisible (*knob);
        knobs.push_back (std::move (knob));
    }

    // Top component (curve display, etc.)
    void setTopComponent (std::unique_ptr<juce::Component> comp, int height)
    {
        topHeight = height;
        topComponent = std::move (comp);
        if (topComponent != nullptr)
            addAndMakeVisible (*topComponent);
    }

    // Right component (vertical GR meter, etc.)
    void setRightComponent (std::unique_ptr<juce::Component> comp, int width)
    {
        rightWidth = width;
        rightComponent = std::move (comp);
        if (rightComponent != nullptr)
            addAndMakeVisible (*rightComponent);
    }

    // Backwards-compatible alias
    void setExtraComponent (std::unique_ptr<juce::Component> comp, int w) { setRightComponent (std::move (comp), w); }

    template <typename T>
    T* addOwned (std::unique_ptr<T> comp)
    {
        auto* raw = comp.get();
        addAndMakeVisible (*raw);
        ownedComponents.push_back (std::move (comp));
        return raw;
    }

    void paint (juce::Graphics& g) override
    {
        if (lookAndFeel != nullptr)
            lookAndFeel->drawModulePanel (g, getLocalBounds(), panelTitle, true);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (8);
        b.removeFromTop (22); // title bar

        // Top component (curve display)
        if (topComponent != nullptr && topHeight > 0)
        {
            auto topArea = b.removeFromTop (topHeight).reduced (2);
            topComponent->setBounds (topArea);
            b.removeFromTop (4);
        }

        // Right component (meter)
        if (rightComponent != nullptr && rightWidth > 0)
        {
            auto rightArea = b.removeFromRight (rightWidth).reduced (2);
            rightComponent->setBounds (rightArea);
            b.removeFromRight (4);
        }

        // Knob grid
        const int cols = juce::jmax (1, b.getWidth() / 100);
        const int rows = (int) std::ceil ((float) knobs.size() / (float) cols);
        const int itemW = b.getWidth() / cols;
        const int itemH = juce::jmin (b.getHeight() / std::max (1, rows), 100);

        for (size_t i = 0; i < knobs.size(); ++i)
        {
            const int r = (int) i / cols;
            const int c = (int) i % cols;
            const auto cell = juce::Rectangle<int> (b.getX() + c * itemW,
                                                     b.getY() + r * itemH,
                                                     itemW, itemH).reduced (4);
            knobs[i]->setBounds (cell);
        }

        if (! ownedComponents.empty() && topComponent == nullptr && rightComponent == nullptr)
            for (auto& c : ownedComponents)
                c->setBounds (b);
    }

    SuccoLookAndFeel* getLookAndFeel() const { return lookAndFeel; }
    const juce::String& getTitle() const { return panelTitle; }

    std::vector<std::unique_ptr<KnobComponent>> knobs;
    std::vector<std::unique_ptr<juce::Component>> ownedComponents;
    std::unique_ptr<juce::Component> topComponent;
    std::unique_ptr<juce::Component> rightComponent;
    int topHeight = 0;
    int rightWidth = 0;
    SuccoLookAndFeel* lookAndFeel = nullptr;
    juce::String panelTitle;
};

class SuccoAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer,
                                  private ModuleTabBar::Listener
{
public:
    SuccoAudioProcessorEditor (SuccoAudioProcessor&);
    ~SuccoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static const std::vector<juce::String>& getTabNames();

private:
    void timerCallback() override;
    void tabSelected (int moduleIndex) override;
    void buildModulePanels();
    void showPanelForCurrentModule();
    void refreshPresetLabel();
    void refreshPresetCombo();

    SuccoAudioProcessor& processor;
    SuccoLookAndFeel lookAndFeel;

    ModuleTabBar tabBar;
    DisplayComponent display;

    // Bottom-row macro knobs
    std::unique_ptr<KnobComponent> knobInput;
    std::unique_ptr<KnobComponent> knobPunch;
    std::unique_ptr<KnobComponent> knobMix;
    std::unique_ptr<KnobComponent> knobOutput;

    // 13 module panels (HOME, TUNE, GATE, EQ, COMP, DE-ESS, DRIVE, DOUBLE,
    // CHORUS, DELAY, VERB, LIMIT, PRESETS)
    std::array<std::unique_ptr<ModulePanel>, 13> modulePanels;

    // GR meters (live-updated from processor)
    MeterComponent* compGRMeter = nullptr;
    MeterComponent* limiterGRMeter = nullptr;

    juce::Label presetLabel;
    juce::TextButton prevPreset { "<" };
    juce::TextButton nextPreset { ">" };
    juce::ComboBox presetCombo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuccoAudioProcessorEditor)
};

} // namespace succo
