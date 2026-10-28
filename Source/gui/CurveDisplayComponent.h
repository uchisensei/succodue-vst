#pragma once

// ---------------------------------------------------------------------------
// CurveDisplayComponent - generic curve display with yellow grid + curve.
// Takes a std::function that draws the curve content given a Graphics+bounds.
// Used by EQ (magnitude response), Drive (waveshaper), Reverb (decay envelope),
// Delay (echo timeline), Chorus (LFO waveform), Comp (transfer curve), etc.
// Each module passes its own custom draw lambda -> each panel looks different.
// ---------------------------------------------------------------------------

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <cmath>
#include "SuccoLookAndFeel.h"

namespace succo
{

class CurveDisplayComponent : public juce::Component,
                              private juce::Timer
{
public:
    using DrawFn = std::function<void (juce::Graphics& g, juce::Rectangle<int> area)>;

    CurveDisplayComponent (SuccoLookAndFeel* lf, const juce::String& title, DrawFn fn)
        : lookAndFeel (lf), titleText (title), drawFn (std::move (fn))
    {
        startTimerHz (30); // 30 FPS refresh
    }

    void paint (juce::Graphics& g) override
    {
        if (lookAndFeel == nullptr) return;
        auto b = getLocalBounds();

        // Background with yellow border
        lookAndFeel->drawCurveBackground (g, b, titleText);

        // Curve area (reduced for title)
        auto curveArea = b.reduced (4, 4).withTrimmedTop (14);

        // Grid lines (light yellow)
        g.setColour (lookAndFeel->getYellow().withAlpha (0.06f));
        for (int i = 1; i < 4; ++i)
        {
            const int y = curveArea.getY() + i * curveArea.getHeight() / 4;
            g.drawHorizontalLine (y, (float) curveArea.getX() + 2.0f, (float) curveArea.getRight() - 2.0f);
        }
        for (int i = 1; i < 8; ++i)
        {
            const int x = curveArea.getX() + i * curveArea.getWidth() / 8;
            g.drawVerticalLine (x, (float) curveArea.getY() + 2.0f, (float) curveArea.getBottom() - 2.0f);
        }

        // Module-specific curve content
        if (drawFn)
            drawFn (g, curveArea);
    }

private:
    void timerCallback() override { repaint(); }

    SuccoLookAndFeel* lookAndFeel;
    juce::String titleText;
    DrawFn drawFn;
};

} // namespace succo
