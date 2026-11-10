#include "PluginEditor.h"

using namespace forgeui;

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, panelHi);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId, ember);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ember.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::Label::textColourId, dim);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, panelHi);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, panelHi);
    setColour (juce::TextButton::buttonOnColourId, ember);
    setColour (juce::TextButton::textColourOffId, dim);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float start, float end, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float size = std::min (r.getWidth(), r.getHeight());
    r = r.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float rad = size * 0.5f, thick = 4.0f;
    const float angle = start + pos * (end - start);

    juce::Path track, value;
    track.addCentredArc (c.x, c.y, rad - thick, rad - thick, 0.0f, start, end, true);
    value.addCentredArc (c.x, c.y, rad - thick, rad - thick, 0.0f, start, angle, true);
    g.setColour (panelHi);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (ember, c.x - rad, c.y + rad, emberHot, c.x + rad, c.y - rad, false));
    g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float inner = rad - thick * 2.6f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e2e33), c.x, c.y - inner, juce::Colour (0xff17171a), c.x, c.y + inner, false));
    g.fillEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2);

    juce::Path tick;
    tick.addRoundedRectangle (-1.5f, -inner + 3.0f, 3.0f, inner * 0.45f, 1.5f);
    g.setColour (emberHot);
    g.fillPath (tick, juce::AffineTransform::rotation (angle).translated (c.x, c.y));

    g.setColour (text);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawFittedText (s.getTextFromValue (s.getValue()), r.withSizeKeepingCentre (inner * 1.6f, 14.0f).toNearestInt().translated (0, (int) (inner * 0.35f)),
                      juce::Justification::centred, 1);
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? ember : panelHi.brighter (highlighted ? 0.08f : 0.0f));
    g.fillRoundedRectangle (r, 5.0f);
    auto led = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.32f);
    g.setColour (on ? juce::Colours::black.withAlpha (0.75f) : dim);
    g.fillEllipse (led);
    g.setColour (on ? juce::Colours::black : text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawFittedText (b.getButtonText(), r.toNearestInt().withTrimmedRight (4), juce::Justification::centredLeft, 1);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& colour, bool highlighted, bool down)
{
    auto c = colour;
    if (b.getToggleState()) c = b.findColour (juce::TextButton::buttonOnColourId);
    if (highlighted) c = c.brighter (0.1f);
    if (down) c = c.darker (0.2f);
    g.setColour (c);
    g.fillRoundedRectangle (b.getLocalBounds().toFloat().reduced (1.0f), 4.0f);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    g.setColour (panelHi);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 5.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (ember);
    g.fillPath (arrow);
}

//==============================================================================
void PatternGrid::paint (juce::Graphics& g)
{
    using namespace forge;
    auto area = getLocalBounds().toFloat();
    const float rowH = area.getHeight() / (float) numRows;
    const float gap = 8.0f;
    const float cellW = (area.getWidth() - gap * 3.0f) / (float) kSteps;
    auto cellX = [&] (int i) { return (float) i * cellW + (float) (i / 4) * gap; };

    // Playhead column
    if (display.playing && display.step >= 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (cellX (display.step), 0.0f, cellW, area.getHeight(), 4.0f);
        g.setColour (ember);
        g.fillRect (cellX (display.step) + 2.0f, 0.0f, cellW - 4.0f, 2.0f);
    }

    // Row 0: what the melody played (drives the follow)
    {
        const float y = 0.0f;
        if (! display.melody.valid)
        {
            g.setColour (dim);
            g.setFont (juce::FontOptions (11.0f, juce::Font::italic));
            g.drawText ("listening for your melody - put a MIDI clip on this track", juce::Rectangle<float> (0.0f, y, area.getWidth(), rowH),
                        juce::Justification::centred);
        }
        else
            for (int i = 0; i < kSteps; ++i)
            {
                const float o = display.melody.onset[(size_t) i];
                auto cell = juce::Rectangle<float> (cellX (i), y, cellW, rowH).reduced (3.0f, 4.0f);
                g.setColour (panelHi.withAlpha (0.6f));
                g.fillRoundedRectangle (cell, 3.0f);
                if (o > 0.02f)
                {
                    g.setColour (emberHot.withAlpha (0.35f + 0.6f * o));
                    g.fillRoundedRectangle (cell.withTrimmedTop (cell.getHeight() * (1.0f - o)), 3.0f);
                }
            }
    }

    // Lanes
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int lane = 0; lane < NumLanes; ++lane)
    {
        const float y = rowH * (float) (lane + 1);
        const auto laneCol = lane == Bass ? steel : ember;
        for (int i = 0; i < kSteps; ++i)
        {
            auto cell = juce::Rectangle<float> (cellX (i), y, cellW, rowH).reduced (2.0f);
            const auto& h = display.bar.hits[(size_t) lane][(size_t) i];
            if (! h.on())
            {
                g.setColour (i % 4 == 0 ? panelHi.brighter (0.04f) : panelHi);
                g.fillRoundedRectangle (cell, 3.0f);
                continue;
            }
            const float v = (float) h.vel / 127.0f;
            g.setColour (panelHi.interpolatedWith (laneCol, 0.3f + 0.7f * v));
            g.fillRoundedRectangle (cell, 3.0f);
            if (h.vel >= 105)
            {
                g.setColour (emberHot.withAlpha (lane == Bass ? 0.0f : 0.9f));
                g.drawRoundedRectangle (cell.reduced (0.5f), 3.0f, 1.2f);
            }
            if (lane == Bass && h.note >= 0)
            {
                g.setColour (juce::Colours::black.withAlpha (0.8f));
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
                g.drawText (juce::String (noteNames[h.note % 12]) + juce::String (h.note / 12 - 1), cell, juce::Justification::centred);
            }
        }
    }

    // Knots
    const float ky = rowH * (float) (NumLanes + 1);
    for (int i = 0; i < kSteps; ++i)
    {
        if (! display.bar.knot[(size_t) i]) continue;
        const auto c = juce::Rectangle<float> (cellX (i), ky, cellW, rowH).getCentre();
        juce::Path d;
        d.addQuadrilateral (c.x, c.y - 6.0f, c.x + 6.0f, c.y, c.x, c.y + 6.0f, c.x - 6.0f, c.y);
        g.setColour (emberHot);
        g.fillPath (d);
    }
}

//==============================================================================
ForgeDrumEditor::ForgeDrumEditor (ForgeDrumProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (grid);

    setupCombo (style, "style", "STYLE");
    setupCombo (output, "output", "OUTPUT");
    setupCombo (clave, "clave", "SON CLAVE");
    setupCombo (cresc, "cresc", "CRESCENDO");
    setupCombo (bassStyle, "bassStyle", "BASS RHYTHM");
    setupCombo (bassKey, "bassKey", "KEY IF NO MELODY");
    setupCombo (section, "section", "EVOLVE EVERY");
    section.label.setJustificationType (juce::Justification::centredRight);
    sectionReadout.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    sectionReadout.setColour (juce::Label::textColourId, emberHot);
    sectionReadout.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (sectionReadout);

    setupKnob (follow, "follow", "MELODY FOLLOW");
    setupKnob (density, "density", "DENSITY");
    setupKnob (accent, "accent", "ACCENT");
    setupKnob (variation, "variation", "VARIATION");
    setupKnob (swing, "swing", "SWING");
    setupKnob (evolve, "evolve", "EVOLVE");
    setupKnob (gate, "gate", "GATE");
    setupKnob (lenHat, "lenHat", "HAT LOOP");
    setupKnob (lenPerc, "lenPerc", "PERC LOOP");
    setupKnob (lenRim, "lenRim", "RIM LOOP");
    setupKnob (lenRide, "lenRide", "RIDE LOOP");
    setupKnob (bassOct, "bassOct", "OCTAVE");
    setupKnob (seed, "seed", "GROOVE SEED");
    seed.slider.setSliderStyle (juce::Slider::IncDecButtons);
    seed.slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 24);

    setupToggle (tresillo, tresilloAtt, "tresillo", "TRESILLO 3+3+2");
    setupToggle (polymeter, polymeterAtt, "polymeter", "POLYMETER");
    setupToggle (thru, thruAtt, "thru", "MELODY THRU");

    dice.onClick = [this]
    {
        if (auto* prm = proc.apvts.getParameter ("seed"))
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) juce::Random::getSystemRandom().nextInt ({ 1, 1000 })));
    };
    addAndMakeVisible (dice);

    for (int lane = 0; lane < forge::NumLanes; ++lane)
    {
        auto& b = laneButtons[(size_t) lane];
        b.setButtonText (forge::laneName (lane));
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonOnColourId, lane == forge::Bass ? steel : ember);
        b.setTooltip ("Click to mute / unmute");
        addAndMakeVisible (b);
        laneAtts[(size_t) lane] = std::make_unique<ButtonAtt> (proc.apvts, "on_" + ForgeDrumProcessor::laneId (lane), b);

        if (lane == forge::Bass) continue;
        auto& n = noteBoxes[(size_t) lane];
        n.setSliderStyle (juce::Slider::LinearBar);
        n.setColour (juce::Slider::trackColourId, juce::Colours::transparentBlack);
        n.setTooltip ("MIDI note for this drum - drag or double-click to type");
        addAndMakeVisible (n);
        noteAtts[(size_t) lane] = std::make_unique<SliderAtt> (proc.apvts, "note_" + ForgeDrumProcessor::laneId (lane), n);
    }

    setSize (1000, 660);
    startTimerHz (30);
}

void ForgeDrumEditor::timerCallback()
{
    const auto d = proc.getDisplay();
    grid.setDisplay (d);

    const float ev = proc.apvts.getRawParameterValue ("evolve")->load();
    juce::String txt;
    if (ev <= 0.001f) txt = "EVOLVE OFF";
    else
    {
        const int secBars = 16 << juce::jlimit (0, 2, (int) std::lround (proc.apvts.getRawParameterValue ("section")->load()));
        const auto bar = d.bar.index;
        const auto sec = bar >= 0 ? bar / secBars : 0;
        txt << "SECTION " << (int) sec + 1 << "   BAR " << (int) (bar - sec * secBars) + 1 << " / " << secBars;
    }
    if (sectionReadout.getText() != txt) sectionReadout.setText (txt, juce::dontSendNotification);
}

ForgeDrumEditor::~ForgeDrumEditor() { stopTimer(); setLookAndFeel (nullptr); }

void ForgeDrumEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void ForgeDrumEditor::setupCombo (Combo& c, const juce::String& id, const juce::String& name)
{
    if (auto* prm = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c.box.addItemList (prm->choices, 1);
    c.label.setText (name, juce::dontSendNotification);
    c.label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    addAndMakeVisible (c.box);
    addAndMakeVisible (c.label);
    c.att = std::make_unique<ComboAtt> (proc.apvts, id, c.box);
}

void ForgeDrumEditor::setupToggle (juce::ToggleButton& b, std::unique_ptr<ButtonAtt>& att, const juce::String& id, const juce::String& name)
{
    b.setButtonText (name);
    addAndMakeVisible (b);
    att = std::make_unique<ButtonAtt> (proc.apvts, id, b);
}

void ForgeDrumEditor::layoutKnob (Knob& k, juce::Rectangle<int> r)
{
    k.label.setBounds (r.removeFromBottom (16));
    k.slider.setBounds (r);
}

void ForgeDrumEditor::layoutCombo (Combo& c, juce::Rectangle<int> r)
{
    c.label.setBounds (r.removeFromTop (16));
    c.box.setBounds (r.removeFromTop (28));
}

void ForgeDrumEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    // Header
    auto header = getLocalBounds().removeFromTop (64).reduced (20, 0);
    auto titleRow = header.removeFromTop (44);
    g.setColour (ember);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("FORGE", titleRow.removeFromLeft (100).withTrimmedTop (8), juce::Justification::centredLeft);
    g.setColour (text);
    g.drawText ("DRUM", titleRow.removeFromLeft (90).withTrimmedTop (8), juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("TECHNO PATTERN ENGINE", header.removeFromTop (14), juce::Justification::centredLeft);
    g.setColour (ember.withAlpha (0.6f));
    g.fillRect (20, 63, getWidth() - 40, 1);

    // Panels
    auto drawPanel = [&] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (ember);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (title, r.reduced (14, 8).removeFromTop (16), juce::Justification::centredLeft);
    };
    drawPanel (laneArea, "PATTERN");
    drawPanel (grooveArea, "GROOVE");
    drawPanel (mathsArea, "MATHS");
    drawPanel (bassArea, "BASS LINE");

    // Row labels that aren't buttons
    const auto gb = grid.getBounds();
    const float rowH = (float) gb.getHeight() / (float) PatternGrid::numRows;
    g.setColour (dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("MELODY HEARD", laneArea.getX() + 14, gb.getY(), 120, (int) rowH, juce::Justification::centredLeft);
    g.drawText ("KNOTS", laneArea.getX() + 14, gb.getBottom() - (int) rowH, 120, (int) rowH, juce::Justification::centredLeft);
}

void ForgeDrumEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (64).reduced (20, 10);
    dice.setBounds (header.removeFromRight (110).withSizeKeepingCentre (110, 30));
    header.removeFromRight (8);
    seed.slider.setBounds (header.removeFromRight (110).withSizeKeepingCentre (110, 28));
    seed.label.setBounds (header.removeFromRight (90).withSizeKeepingCentre (90, 28));
    seed.label.setJustificationType (juce::Justification::centredRight);
    header.removeFromRight (16);
    output.box.setBounds (header.removeFromRight (170).withSizeKeepingCentre (170, 28));
    output.label.setBounds (header.removeFromRight (60).withSizeKeepingCentre (60, 28));
    header.removeFromRight (10);
    style.box.setBounds (header.removeFromRight (120).withSizeKeepingCentre (120, 28));
    style.label.setBounds (header.removeFromRight (46).withSizeKeepingCentre (46, 28));

    r.reduce (16, 10);
    laneArea = r.removeFromTop (330);
    r.removeFromTop (10);

    // Pattern grid with lane buttons on the left
    auto inner = laneArea.reduced (14, 10);
    inner.removeFromTop (20);
    {
        auto top = laneArea.reduced (14, 6).removeFromTop (26);
        section.box.setBounds (top.removeFromRight (100));
        section.label.setBounds (top.removeFromRight (100));
        top.removeFromRight (14);
        sectionReadout.setBounds (top.removeFromRight (260));
    }
    auto labels = inner.removeFromLeft (160);
    grid.setBounds (inner);
    const int rows = PatternGrid::numRows;
    const float rowH = (float) inner.getHeight() / (float) rows;
    for (int lane = 0; lane < forge::NumLanes; ++lane)
    {
        const int y = inner.getY() + (int) (rowH * (float) (lane + 1));
        auto row = juce::Rectangle<int> (labels.getX(), y, labels.getWidth(), (int) rowH).reduced (0, 2);
        laneButtons[(size_t) lane].setBounds (row.removeFromLeft (92));
        row.removeFromLeft (6);
        if (lane != forge::Bass) noteBoxes[(size_t) lane].setBounds (row.removeFromLeft (44));
    }

    // Bottom panels
    const int w = r.getWidth();
    grooveArea = r.removeFromLeft ((int) (w * 0.30f));
    r.removeFromLeft (10);
    mathsArea = r.removeFromLeft ((int) (w * 0.40f));
    r.removeFromLeft (10);
    bassArea = r;

    {
        auto a = grooveArea.reduced (12, 10).withTrimmedTop (22);
        auto top = a.removeFromTop (a.getHeight() / 2);
        const int kw = top.getWidth() / 3;
        layoutKnob (follow,  top.removeFromLeft (kw));
        layoutKnob (density, top.removeFromLeft (kw));
        layoutKnob (accent,  top);
        layoutKnob (variation, a.removeFromLeft (kw));
        layoutKnob (swing,     a.removeFromLeft (kw));
        layoutKnob (evolve,    a);
    }
    {
        auto a = mathsArea.reduced (12, 10).withTrimmedTop (22);
        auto top = a.removeFromTop (a.getHeight() / 2 - 4);
        auto toggles = top.removeFromLeft (top.getWidth() / 3);
        tresillo.setBounds (toggles.removeFromTop (toggles.getHeight() / 2).withSizeKeepingCentre (toggles.getWidth() - 6, 28));
        polymeter.setBounds (toggles.withSizeKeepingCentre (toggles.getWidth() - 6, 28));
        top.removeFromLeft (10);
        const int cw = (top.getWidth() - 10) / 2;
        layoutCombo (clave, top.removeFromLeft (cw).withSizeKeepingCentre (cw, 46));
        top.removeFromLeft (10);
        layoutCombo (cresc, top.withSizeKeepingCentre (top.getWidth(), 46));
        a.removeFromTop (8);
        const int kw = a.getWidth() / 4;
        layoutKnob (lenHat,  a.removeFromLeft (kw));
        layoutKnob (lenPerc, a.removeFromLeft (kw));
        layoutKnob (lenRim,  a.removeFromLeft (kw));
        layoutKnob (lenRide, a);
    }
    {
        auto a = bassArea.reduced (12, 10).withTrimmedTop (22);
        auto top = a.removeFromTop (a.getHeight() / 2 - 4);
        const int cw = (top.getWidth() - 10) / 2;
        layoutCombo (bassStyle, top.removeFromLeft (cw).withSizeKeepingCentre (cw, 46));
        top.removeFromLeft (10);
        layoutCombo (bassKey, top.withSizeKeepingCentre (top.getWidth(), 46));
        a.removeFromTop (8);
        const int kw = a.getWidth() / 4;
        layoutKnob (bassOct, a.removeFromLeft (kw));
        layoutKnob (gate, a.removeFromLeft (kw));
        thru.setBounds (a.withSizeKeepingCentre (a.getWidth() - 4, 30));
    }
}
