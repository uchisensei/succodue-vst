#pragma once

// ---------------------------------------------------------------------------
// MeterComponent - reusable vertical level / gain-reduction meter.
// Renders a bar from bottom to top based on level (0..1).
// Supports peak hold, GR mode (yellow), and label.
// ---------------------------------------------------------------------------

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include "SuccoLookAndFeel.h"

namespace succo
{

class MeterComponent : public juce::Component,
                       private juce::Timer
{
public:
    // Mode: OUTPUT = level meter (silent at bottom), GR = gain reduction meter
    //       (zero at top, bar grows downward when reducing gain)
    enum class Mode { Output, GainReduction };

    MeterComponent (SuccoLookAndFeel* lf, Mode m, const juce::String& label)
        : lookAndFeel (lf), mode (m), labelText (label)
    {
        startTimerHz (60);
    }

    void setLevel (float level01)
    {
        // Atomic store for thread-safe UI update from audio thread
        currentLevel.store (level01);
        // Decay peak hold slowly
        float peak = peakLevel.load();
        if (level01 > peak)
            peakLevel.store (level01);
        else
            peakLevel.store (peak * 0.95f);
    }

    void setRange (float minVal, float maxVal)
    {
        minRange = minVal;
        maxRange = maxVal;
    }

    void paint (juce::Graphics& g) override
    {
        if (lookAndFeel == nullptr) return;
        auto b = getLocalBounds().toFloat();

        // Label at top
        auto labelArea = b.removeFromTop (14.0f);
        g.setColour (lookAndFeel->getTextDim());
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawText (labelText, labelArea.toType<int>(),
                    juce::Justification::centred, true);

        // Meter area
        auto meterArea = b.reduced (2.0f, 0.0f).removeFromTop (b.getHeight() - 8.0f);

        // Convert level (minRange..maxRange, dB) to 0..1
        const float level = currentLevel.load();
        const float peak = peakLevel.load();
        float levelNorm = juce::jmap (level, minRange, maxRange, 0.0f, 1.0f);
        float peakNorm = juce::jmap (peak, minRange, maxRange, 0.0f, 1.0f);
        levelNorm = juce::jlimit (0.0f, 1.0f, levelNorm);
        peakNorm = juce::jlimit (0.0f, 1.0f, peakNorm);

        // Background
        g.setColour (lookAndFeel->getBackgroundColour());
        g.fillRoundedRectangle (meterArea, 3.0f);

        // Tick marks (every 6 dB)
        g.setColour (lookAndFeel->getYellow().withAlpha (0.15f));
        const int numTicks = (int) ((maxRange - minRange) / 6.0f);
        for (int i = 1; i < numTicks; ++i)
        {
            const float y = meterArea.getBottom() - (float) i * (meterArea.getHeight() / numTicks);
            g.drawHorizontalLine ((int) y, meterArea.getX() + 1.0f, meterArea.getRight() - 1.0f);
        }

        // Bar
        const bool isGR = (mode == Mode::GainReduction);
        const float barH = meterArea.getHeight() * levelNorm;
        auto barArea = meterArea.withY (meterArea.getBottom() - barH).withHeight (barH);

        // Color: yellow for GR, gradient for output (yellow -> orange -> red at top)
        juce::Colour barCol;
        if (isGR)
            barCol = lookAndFeel->getYellow();
        else
        {
            if (levelNorm > 0.85f) barCol = lookAndFeel->getDanger();
            else if (levelNorm > 0.7f) barCol = juce::Colour (0xffffa040); // orange
            else barCol = lookAndFeel->getYellowBright();
        }

        juce::ColourGradient grad (barCol.darker (0.4f), barArea.getX(), barArea.getY(),
                                    barCol.brighter (0.3f), barArea.getX(), barArea.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (barArea, 2.0f);

        // Peak hold marker
        if (peakNorm > 0.02f)
        {
            const float peakY = meterArea.getBottom() - meterArea.getHeight() * peakNorm;
            g.setColour (juce::Colours::white.withAlpha (0.85f));
            g.drawHorizontalLine ((int) peakY, meterArea.getX() + 1.0f, meterArea.getRight() - 1.0f);
        }

        // Border
        g.setColour (lookAndFeel->getYellowDim().withAlpha (0.6f));
        g.drawRoundedRectangle (meterArea, 3.0f, 1.0f);

        // Value text at bottom
        auto valueArea = b.removeFromBottom (12.0f);
        g.setColour (lookAndFeel->getTextDim());
        g.setFont (juce::Font (9.0f));
        const juce::String valStr = juce::String (level, 1) + " dB";
        g.drawText (valStr, valueArea.toType<int>(),
                    juce::Justification::centred, true);
    }

private:
    void timerCallback() override
    {
        // Smooth decay for visual
        float level = currentLevel.load();
        if (mode == Mode::Output)
            currentLevel.store (juce::jmax (0.0f, level * 0.92f));
        repaint();
    }

    SuccoLookAndFeel* lookAndFeel;
    Mode mode;
    juce::String labelText;
    std::atomic<float> currentLevel { 0.0f };
    std::atomic<float> peakLevel { 0.0f };
    float minRange = -60.0f;
    float maxRange = 0.0f;
};

} // namespace succo
