#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace forgeui
{
    const juce::Colour bg       { 0xff121213 };
    const juce::Colour panel    { 0xff1c1c1f };
    const juce::Colour panelHi  { 0xff26262a };
    const juce::Colour ember    { 0xffff6a1a };
    const juce::Colour emberHot { 0xffffb347 };
    const juce::Colour steel    { 0xff59a5d8 };
    const juce::Colour text     { 0xffe8e2d9 };
    const juce::Colour dim      { 0xff77716a };

    class LookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
        void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
        void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }
        juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (12.0f, juce::Font::bold)); }
    };
}

//==============================================================================
class PatternGrid : public juce::Component
{
public:
    void setDisplay (const ForgeDrumProcessor::Display& d) { display = d; repaint(); }
    void paint (juce::Graphics&) override;
    static constexpr int numRows = forge::NumLanes + 2;   // melody + lanes + knots
private:
    ForgeDrumProcessor::Display display;
};

//==============================================================================
class ForgeDrumEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ForgeDrumEditor (ForgeDrumProcessor&);
    ~ForgeDrumEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;
    };
    struct Combo
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<ComboAtt> att;
    };

    void setupKnob (Knob&, const juce::String& id, const juce::String& name);
    void setupCombo (Combo&, const juce::String& id, const juce::String& name);
    void setupToggle (juce::ToggleButton&, std::unique_ptr<ButtonAtt>&, const juce::String& id, const juce::String& name);
    void layoutKnob (Knob&, juce::Rectangle<int>);
    void layoutCombo (Combo&, juce::Rectangle<int>);

    ForgeDrumProcessor& proc;
    forgeui::LookAndFeel lnf;
    PatternGrid grid;

    Combo style, output, clave, cresc, bassStyle, bassKey, section;
    juce::Label sectionReadout;
    Knob follow, density, accent, variation, swing, evolve, gate, lenHat, lenPerc, lenRim, lenRide, bassOct, seed;
    juce::ToggleButton tresillo, polymeter, thru;
    std::unique_ptr<ButtonAtt> tresilloAtt, polymeterAtt, thruAtt;
    juce::TextButton dice { "NEW GROOVE" };

    std::array<juce::TextButton, forge::NumLanes> laneButtons;
    std::array<std::unique_ptr<ButtonAtt>, forge::NumLanes> laneAtts;
    std::array<juce::Slider, forge::NumLanes> noteBoxes;
    std::array<std::unique_ptr<SliderAtt>, forge::NumLanes> noteAtts;

    juce::Rectangle<int> grooveArea, mathsArea, bassArea, laneArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ForgeDrumEditor)
};
