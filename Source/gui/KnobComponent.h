#pragma once

// ---------------------------------------------------------------------------
// KnobComponent - labeled rotary knob + value box + optional bypass LED.
// Wraps a juce::Slider driven by an APVTS attachment.
// ---------------------------------------------------------------------------

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "SuccoLookAndFeel.h"

namespace succo
{

class KnobComponent : public juce::Component
{
public:
    KnobComponent (juce::AudioProcessorValueTreeState& apvts,
                   const juce::String& paramID,
                   const juce::String& label,
                   const juce::String& tooltipIT)
        : parameterID (paramID),
          labelText (label),
          tooltipText (tooltipIT)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                    juce::MathConstants<float>::pi * 2.8f, true);
        slider.setColour (juce::Slider::textBoxTextColourId, findColour (juce::Slider::textBoxTextColourId));
        addAndMakeVisible (slider);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, paramID, slider);

        // Label below
        labelLabel.setText (label, juce::dontSendNotification);
        labelLabel.setJustificationType (juce::Justification::centred);
        labelLabel.setFont (juce::Font (12.0f, juce::Font::bold));
        addAndMakeVisible (labelLabel);

        // Tooltip in IT
        slider.setTooltip (tooltipIT);
    }

    void setLookAndFeelToUse (SuccoLookAndFeel* lf)
    {
        slider.setLookAndFeel (lf);
        lookAndFeel = lf;
    }

    void paint (juce::Graphics& g) override
    {
        if (lookAndFeel != nullptr)
            g.fillAll (lookAndFeel->getBackgroundColour());
        else
            g.fillAll (juce::Colours::transparentBlack);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        // Reserve 16px at bottom for label
        auto labelArea = b.removeFromBottom (16);
        labelLabel.setBounds (labelArea);
        slider.setBounds (b);
    }

    juce::Slider& getSlider() { return slider; }

private:
    juce::String parameterID;
    juce::String labelText;
    juce::String tooltipText;

    juce::Slider slider;
    juce::Label labelLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    SuccoLookAndFeel* lookAndFeel = nullptr;
};

} // namespace succo
