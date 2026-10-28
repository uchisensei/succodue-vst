#pragma once

// ---------------------------------------------------------------------------
// ModuleTabBar - the top horizontal tab bar (HOME, TUNE, GATE, EQ, COMP,
// DE-ESS, DRIVE, DOUBLE, CHORUS, DELAY, VERB, LIMIT, PRESETS).
// Clicking a tab sets the APVTS "ui_module" parameter, which the editor
// watches to swap the panel below.
// ---------------------------------------------------------------------------

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "../params/ParameterManager.h"
#include "SuccoLookAndFeel.h"

namespace succo
{

class ModuleTabBar : public juce::Component,
                     private juce::ValueTree::Listener
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void tabSelected (int moduleIndex) = 0;
    };

    ModuleTabBar (juce::AudioProcessorValueTreeState& apvts, SuccoLookAndFeel* lf)
        : lookAndFeel (lf), apvtsRef (apvts)
    {
        tabNames = { "HOME", "TUNE", "GATE", "EQ", "COMP", "DE-ESS",
                     "DRIVE", "DOUBLE", "CHORUS", "DELAY", "VERB", "LIMIT", "PRESETS" };

        // Listen to ui_module parameter - it's AudioParameterChoice with 13 options,
        // so normalized 0..1 maps to index 0..12 via *12 and round.
        auto param = apvts.getParameter ("ui_module");
        if (param != nullptr)
            currentIdx = juce::roundToInt (param->getValue() * (float) (tabNames.size() - 1));
    }

    void setListener (Listener* l) { listener = l; }

    void paint (juce::Graphics& g) override
    {
        if (lookAndFeel == nullptr) return;
        g.fillAll (lookAndFeel->getBackgroundColour());

        const auto b = getLocalBounds().toFloat();
        const float tabW = b.getWidth() / (float) tabNames.size();

        for (size_t i = 0; i < tabNames.size(); ++i)
        {
            const auto tabBounds = juce::Rectangle<float> ((float) i * tabW, 0.0f, tabW, b.getHeight());
            const bool selected = ((int) i == currentIdx);
            const bool bypassed = isBypassed (i);

            if (selected)
            {
                g.setColour (lookAndFeel->getAccentColour().withAlpha (0.25f));
                g.fillRoundedRectangle (tabBounds.reduced (2.0f), 4.0f);
            }

            juce::Colour textCol = selected ? lookAndFeel->getAccentBright() : lookAndFeel->getTextDim();
            if (bypassed) textCol = textCol.withAlpha (0.45f);

            g.setColour (textCol);
            g.setFont (juce::Font (12.0f, juce::Font::bold));
            g.drawText (tabNames[i], tabBounds, juce::Justification::centred, true);

            if (bypassed && !selected)
            {
                g.setColour (lookAndFeel->getDanger().withAlpha (0.4f));
                g.fillEllipse (tabBounds.getRight() - 10.0f, tabBounds.getY() + 4.0f, 4.0f, 4.0f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const float tabW = (float) getWidth() / (float) tabNames.size();
        const int idx = (int) ((float) e.x / tabW);
        if (idx >= 0 && idx < (int) tabNames.size())
        {
            currentIdx = idx;
            // Push to APVTS
            if (auto* p = apvtsRef.getParameter ("ui_module"))
                p->setValueNotifyingHost ((float) idx / (float) (tabNames.size() - 1));
            if (listener != nullptr)
                listener->tabSelected (idx);
            repaint();
        }
    }

    void setCurrentIndex (int idx)
    {
        if (idx != currentIdx)
        {
            currentIdx = idx;
            repaint();
        }
    }

private:
    SuccoLookAndFeel* lookAndFeel;
    juce::AudioProcessorValueTreeState& apvtsRef;
    std::vector<juce::String> tabNames;
    int currentIdx = 0;
    Listener* listener = nullptr;

    bool isBypassed (size_t tabIndex) const
    {
        using namespace ParamIDs;
        // Map tab index -> bypass parameter name (nullptr = no bypass control)
        static constexpr std::array<const char*, 13> bypassParams = {
            nullptr,           // HOME
            tuneEnable, gateEnable, eqEnable, compEnable, deEssEnable,
            driveEnable, doublerEnable, chorusEnable, delayEnable,
            reverbEnable, limitEnable, nullptr  // PRESETS
        };
        if (tabIndex < bypassParams.size())
        {
            if (auto* p = bypassParams[tabIndex])
            {
                if (auto* param = apvtsRef.getParameter (p))
                    return param->getValue() < 0.5f;
            }
        }
        return false;
    }
};

} // namespace succo
