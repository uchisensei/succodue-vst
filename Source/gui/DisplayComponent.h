#pragma once

// ---------------------------------------------------------------------------
// DisplayComponent - the central purple screen with:
//   - the SUCCO logo (yellow accents)
//   - the current preset name
//   - a BIG real-time output level meter (stereo L/R, yellow bars)
//   - the active module name
// ---------------------------------------------------------------------------

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <atomic>
#include <functional>
#include "SuccoLookAndFeel.h"

namespace succo
{

class DisplayComponent : public juce::Component,
                          private juce::Timer
{
public:
    DisplayComponent (SuccoLookAndFeel* lf, std::function<float()> getLevelFn)
        : lookAndFeel (lf), getLevel (std::move (getLevelFn))
    {
        startTimerHz (60);
    }

    void setPresetName (const juce::String& name) { presetName = name; repaint(); }
    void setModuleName (const juce::String& name) { moduleName = name; repaint(); }

    void paint (juce::Graphics& g) override
    {
        if (lookAndFeel == nullptr) return;
        auto b = getLocalBounds();
        lookAndFeel->drawDisplayBackground (g, b);

        // === Top bar: preset name + module name ===
        auto topArea = b.removeFromTop (24);
        g.setColour (lookAndFeel->getTextColour());
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText ("NOW POURING: " + presetName, topArea.reduced (10, 2),
                    juce::Justification::centredLeft, true);

        // Yellow accent dot
        lookAndFeel->drawLED (g, (float) topArea.getRight() - 100, (float) topArea.getCentreY() - 3, 6, true);

        g.setColour (lookAndFeel->getYellow());
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (moduleName, topArea.reduced (10, 2),
                    juce::Justification::centredRight, true);

        // === Middle: BIG stereo output meter ===
        auto meterArea = b.removeFromTop (b.getHeight() * 2 / 3).reduced (12, 6);
        drawStereoOutputMeter (g, meterArea);

        // === Bottom: logo ===
        drawLogo (g, b.reduced (4));
    }

    void timerCallback() override
    {
        const float level = getLevel ? getLevel() : 0.0f;
        // Smooth: hold peak, slow decay
        const float decay = 0.92f;
        smoothedLevel = juce::jmax (level, smoothedLevel * decay);
        peakHold = juce::jmax (level, peakHold * 0.995f);
        repaint();
    }

private:
    SuccoLookAndFeel* lookAndFeel;
    std::function<float()> getLevel;
    juce::String presetName { "<INIT>" };
    juce::String moduleName { "HOME" };
    float smoothedLevel = 0.0f;
    float peakHold = 0.0f;

    void drawStereoOutputMeter (juce::Graphics& g, juce::Rectangle<int> area)
    {
        // Background
        g.setColour (lookAndFeel->getBackgroundColour().withAlpha (0.5f));
        g.fillRoundedRectangle (area.toFloat(), 4.0f);

        // Title
        g.setColour (lookAndFeel->getTextDim());
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawText ("OUTPUT", area.reduced (8, 2).removeFromTop (12),
                    juce::Justification::centredLeft, true);

        // Meter area below title
        auto barArea = area.reduced (10, 4);
        barArea.removeFromTop (14); // remove space for title
        const int meterW = 40;
        const int gap = 12;
        auto leftMeter = barArea.removeFromLeft (meterW).reduced (0, 2);
        auto rightMeter = barArea.removeFromLeft (meterW).reduced (0, 2);

        // Tick marks (dB scale: -60 to 0)
        g.setColour (lookAndFeel->getYellow().withAlpha (0.1f));
        for (int i = 0; i <= 6; ++i)
        {
            const float y = barArea.getBottom() - (float) i * barArea.getHeight() / 6.0f;
            g.drawHorizontalLine ((int) y, leftMeter.getX() - 6, rightMeter.getRight() + 6);
        }

        // Yellow LED ladder (segment-style meter) on each channel
        drawSegmentMeter (g, leftMeter, smoothedLevel, "L");
        drawSegmentMeter (g, rightMeter, smoothedLevel, "R");

        // Scale labels on the right
        g.setColour (lookAndFeel->getTextDim());
        g.setFont (juce::Font (8.0f));
        for (int i = 0; i <= 6; ++i)
        {
            const float y = barArea.getBottom() - (float) i * barArea.getHeight() / 6.0f;
            const int db = -i * 10;
            g.drawText (juce::String (db), (int) (barArea.getRight() + 4), (int) y - 6, 28, 12,
                        juce::Justification::centredLeft, true);
        }
    }

    void drawSegmentMeter (juce::Graphics& g, juce::Rectangle<int> area, float level, const juce::String& label)
    {
        // Background
        g.setColour (lookAndFeel->getBackgroundColour());
        g.fillRoundedRectangle (area.toFloat(), 2.0f);

        // Channel label
        g.setColour (lookAndFeel->getTextDim());
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawText (label, area.withHeight (14), juce::Justification::centred, true);

        // Segments
        auto segArea = area.withTrimmedTop (16).reduced (2, 0);
        const int numSegs = 20;
        const float segHeight = segArea.getHeight() / (float) numSegs;
        const float segGap = 1.5f;

        for (int i = 0; i < numSegs; ++i)
        {
            const float segLevel = (float) (i + 1) / (float) numSegs;
            const bool active = level >= segLevel;
            juce::Colour segCol;

            if (segLevel > 0.85f)      segCol = lookAndFeel->getDanger();        // red (top 15%)
            else if (segLevel > 0.70f) segCol = juce::Colour (0xffffa040);          // orange
            else                       segCol = lookAndFeel->getYellowBright();  // yellow (default)

            const float y = segArea.getBottom() - (float) (i + 1) * segHeight;
            const auto segRect = juce::Rectangle<float> (
                (float) segArea.getX() + 2.0f, y + segGap / 2,
                (float) segArea.getWidth() - 4.0f, segHeight - segGap);

            if (active)
            {
                g.setColour (segCol);
                g.fillRoundedRectangle (segRect, 1.5f);
                // Subtle glow
                g.setColour (segCol.withAlpha (0.3f));
                g.fillRoundedRectangle (segRect.expanded (1.0f), 2.0f);
            }
            else
            {
                g.setColour (segCol.withAlpha (0.08f));
                g.fillRoundedRectangle (segRect, 1.5f);
            }
        }
    }

    void drawLogo (juce::Graphics& g, juce::Rectangle<int> area)
    {
        const auto bounds = area.toFloat().reduced (4.0f);

        // Soft yellow glow background
        juce::ColourGradient glow (lookAndFeel->getYellow().withAlpha (0.15f),
                                    bounds.getCentreX(), bounds.getCentreY(),
            juce::Colours::transparentBlack,
            bounds.getCentreX(), bounds.getBottom(), true);
        g.setGradientFill (glow);
        g.fillRect (bounds);

        // 3D-ish "SUCCO" text with layered shadow
        const juce::String logoText = "SUCCO Due 2";
        const juce::Font logoFont (28.0f, juce::Font::bold);

        // Drop shadow
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.setFont (logoFont);
        g.drawText (logoText, bounds.translated (2.0f, 2.0f),
                    juce::Justification::centred, true);

        // Main text (yellow gradient)
        g.setColour (lookAndFeel->getYellowBright());
        g.drawText (logoText, bounds, juce::Justification::centred, true);

        // Subtitle
        g.setColour (lookAndFeel->getTextDim());
        g.setFont (juce::Font (9.0f));
        g.drawText ("VOCAL CHAIN  v2.0", bounds.translated (0, 16),
                    juce::Justification::centred, true);

        // Yellow accent dots on each side of subtitle
        const auto dotY = bounds.getBottom() + 16;
        lookAndFeel->drawLED (g, bounds.getCentreX() - 50, dotY - 2, 4, true);
        lookAndFeel->drawLED (g, bounds.getCentreX() + 46, dotY - 2, 4, true);
    }
};

} // namespace succo
