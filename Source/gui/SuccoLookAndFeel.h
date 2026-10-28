#pragma once

// ---------------------------------------------------------------------------
// SuccoLookAndFeel - dark purple + yellow accent theme for SUCCO.
// Custom 3D knob rendering with depth shadow + LED dots, inspired by
// professional audio plugins (Soundtoys / u-He / FabFilter aesthetics).
// All rendering code is original.
// ---------------------------------------------------------------------------

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace succo
{

class SuccoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SuccoLookAndFeel()
    {
        // ---- Dark purple base palette ----
        colBackground     = juce::Colour (0xff1a0d24);
        colBackgroundAlt  = juce::Colour (0xff2a1438);
        colPanel          = juce::Colour (0xff341a4a);
        colPanelAlt       = juce::Colour (0xff42225e);
        colAccent         = juce::Colour (0xffb975ff);
        colAccentBright   = juce::Colour (0xffe0a3ff);
        colAccentDim      = juce::Colour (0xff6b3da3);
        colText           = juce::Colour (0xfff0e6ff);
        colTextDim        = juce::Colour (0xff9b85b8);
        colKnob           = juce::Colour (0xff1f1030);
        colKnobFill       = juce::Colour (0xffb975ff);
        colDisplay        = juce::Colour (0xff2a0d3a);
        colDisplayBorder  = juce::Colour (0xff6b3da3);

        // ---- Yellow accent palette (NEW) ----
        colYellow        = juce::Colour (0xffffd93d);   // warm yellow
        colYellowBright  = juce::Colour (0xffffeb70);   // brighter yellow
        colYellowDim     = juce::Colour (0xff8c6b1f);   // dim yellow
        colYellowGlow    = juce::Colour (0x66ffd93d);   // semi-transparent yellow glow

        // ---- Status colors ----
        colGood          = juce::Colour (0xff4dd599);
        colWarn          = juce::Colour (0xffffcc44);
        colDanger        = juce::Colour (0xffff5577);
        colOff           = juce::Colour (0xff3a2a4a);

        setColour (juce::ResizableWindow::backgroundColourId, colBackground);
        setColour (juce::DocumentWindow::backgroundColourId, colBackground);
        setColour (juce::ComboBox::backgroundColourId, colPanel);
        setColour (juce::ComboBox::textColourId, colText);
        setColour (juce::ComboBox::arrowColourId, colYellow);
        setColour (juce::PopupMenu::backgroundColourId, colPanel);
        setColour (juce::PopupMenu::textColourId, colText);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colAccentDim);
        setColour (juce::PopupMenu::highlightedTextColourId, colText);
        setColour (juce::TextButton::buttonColourId, colPanel);
        setColour (juce::TextButton::buttonOnColourId, colYellow);
        setColour (juce::TextButton::textColourOffId, colText);
        setColour (juce::TextButton::textColourOnId, colBackground);
        setColour (juce::Label::textColourId, colText);
        setColour (juce::Label::textWhenEditingColourId, colYellowBright);
        setColour (juce::Slider::textBoxTextColourId, colText);
        setColour (juce::Slider::textBoxBackgroundColourId, colPanel);
        setColour (juce::Slider::textBoxOutlineColourId, colAccentDim);
        setColour (juce::Slider::thumbColourId, colYellow);
        setColour (juce::Slider::rotarySliderFillColourId, colYellow);
        setColour (juce::Slider::rotarySliderOutlineColourId, colKnob);

        // JUCE 8: this is a member of LookAndFeel, not Font
        setDefaultSansSerifTypefaceName ("Verdana");
    }

    // -------- Custom 3D knob drawing --------
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                            float pos, const float startAngle, const float endAngle,
                            juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y,
                                                     (float) width, (float) height).reduced (4.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = startAngle + pos * (endAngle - startAngle);

        // === 1) Drop shadow under knob (3D depth) ===
        const float shadowOffset = radius * 0.08f;
        juce::Colour shadowCol = juce::Colours::black.withAlpha (0.35f);
        g.setColour (shadowCol);
        g.fillEllipse (centre.x - radius * 0.85f + shadowOffset,
                       centre.y - radius * 0.85f + shadowOffset,
                       radius * 1.7f, radius * 1.7f);

        // === 2) Track ring (background, unfilled portion) ===
        const float trackWidth = radius * 0.18f;
        const float trackRadius = radius * 0.82f;
        juce::Path bgRing;
        bgRing.addCentredArc (centre.x, centre.y, trackRadius, trackRadius,
            0.0f, startAngle, endAngle, true);
        g.setColour (colKnob);
        g.strokePath (bgRing, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved));

        // === 3) Filled arc (yellow, up to current position) ===
        juce::Path fillRing;
        fillRing.addCentredArc (centre.x, centre.y, trackRadius, trackRadius,
            0.0f, startAngle, angle, true);
        // Glow under filled arc
        juce::ColourGradient fillGrad (colYellowBright,
                                       centre.x, centre.y - trackRadius,
                                       colYellow,
                                       centre.x, centre.y + trackRadius, true);
        g.setGradientFill (fillGrad);
        g.strokePath (fillRing, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved));

        // === 4) Inner knob disc with 3D radial gradient ===
        const float knobRadius = radius * 0.6f;
        juce::ColourGradient knobGrad (
            colPanelAlt.withAlpha (0.9f),
            centre.x - knobRadius * 0.35f, centre.y - knobRadius * 0.35f,
            colKnob.darker (0.4f),
            centre.x + knobRadius * 0.35f, centre.y + knobRadius * 0.35f,
            true);
        g.setGradientFill (knobGrad);
        g.fillEllipse (centre.x - knobRadius, centre.y - knobRadius,
                       knobRadius * 2.0f, knobRadius * 2.0f);

        // Knob rim (yellow accent)
        g.setColour (colYellow.withAlpha (0.85f));
        g.drawEllipse (centre.x - knobRadius, centre.y - knobRadius,
                       knobRadius * 2.0f, knobRadius * 2.0f, 1.2f);

        // Inner highlight (top-left, gives "lit from above" feel)
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillEllipse (centre.x - knobRadius * 0.7f, centre.y - knobRadius * 0.85f,
                       knobRadius * 0.7f, knobRadius * 0.45f);

        // === 5) Pointer line (bright yellow) ===
        const float pointerLen = knobRadius * 0.85f;
        const float pointerX = centre.x + std::sin (angle) * pointerLen;
        const float pointerY = centre.y - std::cos (angle) * pointerLen;
        juce::Path pointer;
        pointer.startNewSubPath (centre.x, centre.y);
        pointer.lineTo (pointerX, pointerY);
        g.setColour (colYellowBright);
        g.strokePath (pointer, juce::PathStrokeType (2.8f));

        // === 6) Center cap dot ===
        g.setColour (colYellowBright);
        g.fillEllipse (centre.x - 2.5f, centre.y - 2.5f, 5.0f, 5.0f);
    }

    // -------- Display panel background --------
    void drawDisplayBackground (juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        const auto f = bounds.toFloat();
        juce::ColourGradient grad (colPanel, f.getCentreX(), f.getCentreY(),
            colDisplay, f.getX(), f.getBottom(), true);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (f, 8.0f);

        // Yellow accent border
        g.setColour (colYellow.withAlpha (0.6f));
        g.drawRoundedRectangle (f, 8.0f, 1.5f);

        // Subtle inner grid (yellow)
        g.setColour (colYellow.withAlpha (0.05f));
        for (int i = 1; i < 8; ++i)
        {
            const float x = f.getX() + (float) i * f.getWidth() / 8.0f;
            g.drawVerticalLine ((int) x, f.getY() + 4.0f, f.getBottom() - 4.0f);
        }
    }

    // -------- Module panel background (with yellow title bar) --------
    void drawModulePanel (juce::Graphics& g, juce::Rectangle<int> bounds,
                          const juce::String& title, bool isActive)
    {
        const auto f = bounds.toFloat();
        // Background
        juce::ColourGradient bg (colPanel, f.getCentreX(), f.getY(),
                                  colBackgroundAlt, f.getCentreX(), f.getBottom(), false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (f, 6.0f);

        // Border
        const juce::Colour borderCol = isActive ? colYellow : colAccentDim;
        g.setColour (borderCol.withAlpha (0.5f));
        g.drawRoundedRectangle (f, 6.0f, 1.0f);

        // Title bar at top (yellow strip)
        const float titleH = 22.0f;
        auto titleArea = f.withHeight (titleH);
        g.setColour (colYellow.withAlpha (isActive ? 0.25f : 0.10f));
        g.fillRoundedRectangle (titleArea, 6.0f);

        // Yellow accent line below title bar
        g.setColour (colYellow);
        g.drawHorizontalLine ((int) titleH, f.getX() + 4.0f, f.getRight() - 4.0f);

        // Title text
        g.setColour (isActive ? colYellowBright : colTextDim);
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText (title.toUpperCase(), titleArea.reduced (8, 0).toType<int>(),
                    juce::Justification::centredLeft, true);
    }

    // -------- LED indicator dot --------
    void drawLED (juce::Graphics& g, float x, float y, float size,
                  bool on, const juce::Colour& color = juce::Colour())
    {
        const juce::Colour& c = on ? (color == juce::Colour() ? colYellow : color)
                                    : colOff;
        // Outer glow when on
        if (on)
        {
            juce::ColourGradient glow (c.withAlpha (0.4f), x + size * 0.5f, y + size * 0.5f,
                                        juce::Colours::transparentBlack,
                                        x + size, y + size, true);
            g.setGradientFill (glow);
            g.fillEllipse (x - size * 0.5f, y - size * 0.5f, size * 2.0f, size * 2.0f);
        }
        // LED dot
        g.setColour (c);
        g.fillEllipse (x, y, size, size);
        // Highlight
        g.setColour (juce::Colours::white.withAlpha (on ? 0.5f : 0.15f));
        g.fillEllipse (x + size * 0.2f, y + size * 0.15f, size * 0.35f, size * 0.25f);
    }

    // -------- Level / GR meter bar (vertical) --------
    void drawMeterBar (juce::Graphics& g, juce::Rectangle<float> bounds,
                       float level01, float peak01, bool isGR)
    {
        // Background
        g.setColour (colKnob);
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (colKnob.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds, 2.0f, 1.0f);

        if (level01 <= 0.0f) return;
        const float barH = bounds.getHeight() * juce::jlimit (0.0f, 1.0f, level01);
        auto barArea = bounds.withY (bounds.getBottom() - barH).withHeight (barH);

        // Color depends on level (and GR vs output meter)
        juce::Colour barCol;
        if (isGR)
            barCol = colYellow;  // GR meters are yellow
        else
        {
            if (level01 > 0.85f) barCol = colDanger;
            else if (level01 > 0.7f) barCol = colWarn;
            else barCol = colYellowBright;
        }

        juce::ColourGradient grad (barCol.darker (0.4f), barArea.getX(), barArea.getY(),
                                    barCol.brighter (0.3f), barArea.getX(), barArea.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (barArea, 2.0f);

        // Peak hold marker
        if (peak01 > 0.0f && peak01 >= level01)
        {
            const float peakY = bounds.getBottom() - bounds.getHeight() * juce::jlimit (0.0f, 1.0f, peak01);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawHorizontalLine ((int) peakY, bounds.getX() + 1.0f, bounds.getRight() - 1.0f);
        }
    }

    // -------- Curve display background (for EQ response, drive curve, etc.) --------
    void drawCurveBackground (juce::Graphics& g, juce::Rectangle<int> bounds,
                              const juce::String& title)
    {
        const auto f = bounds.toFloat();
        g.setColour (colDisplay);
        g.fillRoundedRectangle (f, 4.0f);
        g.setColour (colYellow.withAlpha (0.3f));
        g.drawRoundedRectangle (f, 4.0f, 1.0f);

        // Horizontal grid lines
        g.setColour (colYellow.withAlpha (0.08f));
        for (int i = 1; i < 4; ++i)
        {
            const float y = f.getY() + (float) i * f.getHeight() / 4.0f;
            g.drawHorizontalLine ((int) y, f.getX() + 2.0f, f.getRight() - 2.0f);
        }

        // Title
        g.setColour (colYellow.withAlpha (0.7f));
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawText (title, bounds.reduced (6, 2), juce::Justification::topLeft, true);
    }

    // -------- Getters --------
    juce::Colour getBackgroundColour() const { return colBackground; }
    juce::Colour getPanelColour() const     { return colPanel; }
    juce::Colour getAccentColour() const    { return colAccent; }
    juce::Colour getAccentBright() const   { return colAccentBright; }
    juce::Colour getAccentDim() const      { return colAccentDim; }
    juce::Colour getTextColour() const     { return colText; }
    juce::Colour getTextDim() const         { return colTextDim; }
    juce::Colour getDanger() const          { return colDanger; }
    juce::Colour getGood() const            { return colGood; }
    juce::Colour getYellow() const          { return colYellow; }
    juce::Colour getYellowBright() const    { return colYellowBright; }
    juce::Colour getYellowDim() const       { return colYellowDim; }
    juce::Colour getOff() const             { return colOff; }

private:
    juce::Colour colBackground, colBackgroundAlt, colPanel, colPanelAlt;
    juce::Colour colAccent, colAccentBright, colAccentDim;
    juce::Colour colText, colTextDim;
    juce::Colour colKnob, colKnobFill;
    juce::Colour colDisplay, colDisplayBorder;
    juce::Colour colYellow, colYellowBright, colYellowDim, colYellowGlow;
    juce::Colour colGood, colWarn, colDanger, colOff;
};

} // namespace succo
