#include "PluginEditor.h"
#include "DSP/GainComputer.h"

namespace twinmu
{
using gui::TwinMuLookAndFeel;

namespace
{
const gui::Theme& startTheme() { return gui::themes()[0]; }
float rawValue (juce::AudioProcessorValueTreeState& s, const char* id) { return s.getRawParameterValue (id)->load(); }
} // namespace

// =====================================================================================================
MainPanel::MainPanel (TwinMuProcessor& p, gui::TwinMuLookAndFeel& l, std::function<void (int)> onThemeChanged)
    : proc (p), state (p.apvts), lnf (l), themeCallback (std::move (onThemeChanged)),
      ecoButton (state, ids::eco, "ECO", gui::fixed::amber, "ECO: 1x oversampling and lighter saturation for live use"),
      bypassButton (state, ids::bypass, "BYPASS", juce::Colour (0xffe0533d), "Latency-compensated bypass"),
      modeSelector (state, ids::mode, { "COMPRESSOR", "LIMITER", "VINTAGE" }, gui::fixed::amber, &p.undoManager),
      stereoSelector (state, ids::stereoMode, { "LINKED", "DUAL MONO", "MID / SIDE" }, startTheme().sky, &p.undoManager),
      thrA (state, ids::thrA, "THRESHOLD", startTheme().teal, 84),
      ratioA (state, ids::ratioA, "RATIO", startTheme().teal, 84, "Ratio A. In VINTAGE mode the ratio grows with level; this is the ratio 10 dB above threshold"),
      tcA (state, ids::relModeA, "TIME CONST", startTheme().teal, 58, "Release behaviour: VAR (knobs), AUTO (program dependent), TC1-TC6 (vintage time constants)"),
      atkA (state, ids::atkA, "ATTACK", startTheme().teal, 56),
      relA (state, ids::relA, "RELEASE", startTheme().teal, 56),
      mkA (state, ids::mkA, "MAKEUP", startTheme().teal, 56),
      knee (state, ids::knee, "KNEE", startTheme().teal, 56, "Knee width (0 dB = hard knee)"),
      thrB (state, ids::thrB, "THRESHOLD", startTheme().coral, 84),
      ratioB (state, ids::ratioB, "RATIO", startTheme().coral, 84),
      tcB (state, ids::relModeB, "TIME CONST", startTheme().coral, 58),
      atkB (state, ids::atkB, "ATTACK", startTheme().coral, 56),
      relB (state, ids::relB, "RELEASE", startTheme().coral, 56),
      mkB (state, ids::mkB, "MAKEUP", startTheme().coral, 56),
      link (state, ids::link, "LINK", startTheme().sky, 56, "Stereo link amount (LINKED mode)"),
      enA (state, ids::enA, "ON", startTheme().teal, "Channel A compression on/off"),
      enB (state, ids::enB, "ON", startTheme().coral, "Channel B compression on/off"),
      tube (state, ids::tube, "TUBE", startTheme().orange, 64, "Tube character: even-harmonic saturation"),
      xfmr (state, ids::xfmr, "TRANSFORMER", startTheme().violet, 64, "Transformer: low-frequency saturation and HF softening"),
      mix (state, ids::mix, "MIX", startTheme().sun, 64, "Dry/wet for parallel compression (latency compensated)"),
      outGain (state, ids::outGain, "OUTPUT", startTheme().sky, 64),
      scFreq (state, ids::scFreq, "SC FREQ", startTheme().lime, 58),
      scQ (state, ids::scQ, "SC Q", startTheme().lime, 58),
      scSens (state, ids::scSens, "SENSITIVITY", startTheme().lime, 58, "Detector gain: drives the compressor harder without changing the audio level"),
      scMix (state, ids::scMix, "EXT MIX", startTheme().lime, 58, "With EXTERNAL source: blend between internal (0 %) and external (100 %) key"),
      fdc (state, ids::fdc, "FREQ-DEP", startTheme().lime, 58, "Low-frequency emphasis of the detector: + compresses lows more, - less"),
      scSource (state, ids::scSource, { "INTERNAL", "EXTERNAL" }, startTheme().lime, &p.undoManager),
      scFilter (state, ids::scFilter, { "OFF", "HP", "LP", "BP" }, startTheme().lime, &p.undoManager),
      scListen (state, ids::scListen, "SC LISTEN", startTheme().lime, "Monitor the filtered detector signal"),
      limOn (state, ids::limOn, "LIMITER", startTheme().coral, "Output brick-wall limiter"),
      truePeak (state, ids::truePeak, "TRUE PEAK", startTheme().coral, "4x inter-sample peak detection"),
      softClip (state, ids::softClip, "SOFT CLIP", startTheme().coral, "Smooth safety clipper after the limiter"),
      limCeil (state, ids::limCeil, "CEILING", startTheme().coral, 58),
      limRel (state, ids::limRel, "RELEASE", startTheme().coral, 58),
      limLook (state, ids::limLook, "LOOKAHEAD", startTheme().coral, 58, "Lookahead (changes the plugin latency)"),
      limAmt (state, ids::limAmt, "AMOUNT", startTheme().coral, 58),
      inGain (state, ids::inGain, "INPUT", startTheme().sky, 58),
      width (state, ids::width, "WIDTH", startTheme().sky, 58, "Stereo width (exact M/S matrix, mono compatible)"),
      noise (state, ids::noise, "NOISE", startTheme().violet, 58, "Analog noise floor (OFF by default)"),
      autoMakeup (state, ids::autoMakeup, "AUTO MAKEUP", startTheme().sky, "Automatic makeup gain from the curve"),
      hum (state, ids::hum, { "OFF", "50 Hz", "60 Hz" }, startTheme().violet, &p.undoManager)
{
    setSize (designWidth, designHeight);

    for (auto* b : { &prevButton, &presetButton, &nextButton, &saveButton, &slotA, &slotB, &undoButton, &redoButton, &themeButton })
        addAndMakeVisible (b);
    prevButton.onClick = [this] { proc.presets.previous(); };
    nextButton.onClick = [this] { proc.presets.next(); };
    presetButton.onClick = [this] { showPresetMenu(); };
    presetButton.setTooltip ("Presets: load, save, rename, delete, import, export, copy/paste, randomize");
    saveButton.onClick = [this] { showSaveDialog(); };
    slotA.onClick = [this] { proc.switchToSlot (0); };
    slotB.onClick = [this] { proc.switchToSlot (1); };
    slotA.setTooltip ("A/B comparison: settings A");
    slotB.setTooltip ("A/B comparison: settings B");
    undoButton.onClick = [this] { proc.undoManager.undo(); };
    redoButton.onClick = [this] { proc.undoManager.redo(); };
    themeButton.onClick = [this] { themeCallback (-1); };
    themeButton.setTooltip ("Panel colour theme: Ardesia / Crema '60 / Menta");

    oversampling.addItemList ({ "OS 1x", "OS 2x", "OS 4x", "OS 8x" }, 1);
    oversampling.setTooltip ("Oversampling for the saturation stages (changes latency; ECO forces 1x)");
    addAndMakeVisible (oversampling);
    oversamplingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, ids::os, oversampling);
    addAndMakeVisible (ecoButton);
    addAndMakeVisible (bypassButton);

    for (auto* c : std::initializer_list<juce::Component*> { &inputMeter, &outputMeter, &vuA, &vuB, &curve, &modeSelector, &stereoSelector,
                                                            &thrA, &ratioA, &tcA, &atkA, &relA, &mkA, &knee, &thrB, &ratioB, &tcB,
                                                            &atkB, &relB, &mkB, &link, &enA, &enB, &tube, &xfmr, &mix, &outGain })
        addAndMakeVisible (c);

    const char* tabNames[] = { "SIDECHAIN", "LIMITER", "INPUT / STEREO" };
    for (int i = 0; i < 3; ++i)
    {
        tabButtons[i].setButtonText (tabNames[i]);
        tabButtons[i].setClickingTogglesState (false);
        tabButtons[i].onClick = [this, i] { selectTab (i); };
        addAndMakeVisible (tabButtons[i]);
    }
    tabContent[0] = { &scFreq, &scQ, &scSens, &scMix, &fdc, &scSource, &scFilter, &scListen, &filterView };
    tabContent[1] = { &limOn, &truePeak, &softClip, &limCeil, &limRel, &limLook, &limAmt };
    tabContent[2] = { &inGain, &width, &noise, &autoMakeup, &hum };
    for (auto& group : tabContent)
        for (auto* c : group)
            addChildComponent (c);

    layout();
    applyTheme();
    selectTab (0);
    lastTick = juce::Time::getMillisecondCounterHiRes();
    timerCallback(); // initial titles, curve and read-outs before the first frame
    startTimerHz (30);
}

MainPanel::~MainPanel() { stopTimer(); }

void MainPanel::layout()
{
    // top bar, right to left
    int x = 1066;
    auto placeRight = [&x] (juce::Component& c, int w)
    {
        x -= w;
        c.setBounds (x, 10, w, 24);
        x -= 6;
    };
    placeRight (bypassButton, 66);
    placeRight (themeButton, 60);
    placeRight (ecoButton, 44);
    placeRight (oversampling, 74);
    placeRight (redoButton, 48);
    placeRight (undoButton, 48);
    placeRight (slotB, 24);
    x += 6 - 1;
    placeRight (slotA, 24);
    placeRight (saveButton, 46);
    placeRight (nextButton, 24);
    placeRight (presetButton, 196);
    placeRight (prevButton, 24);

    inputMeter.setBounds (50, 72, 23, 176);
    vuA.setBounds (86, 80, 300, 166);
    curve.setBounds (404, 80, 292, 166);
    vuB.setBounds (714, 80, 300, 166);
    outputMeter.setBounds (1027, 72, 23, 176);

    modeSelector.setBounds (80, 266, 300, 26);
    stereoSelector.setBounds (694, 266, 300, 26);

    thrA.place (110, 326); ratioA.place (240, 326); tcA.place (370, 340);
    atkA.place (96, 442); relA.place (176, 442); mkA.place (256, 442); knee.place (336, 442);
    thrB.place (518, 326); ratioB.place (648, 326); tcB.place (778, 340);
    atkB.place (504, 442); relB.place (584, 442); mkB.place (664, 442); link.place (744, 442);
    enA.setBounds (382, 316, 40, 18);
    enB.setBounds (790, 316, 40, 18);
    tube.place (910, 328); xfmr.place (1006, 328); mix.place (910, 426); outGain.place (1006, 426);

    for (int i = 0; i < 3; ++i)
        tabButtons[i].setBounds (40 + i * 126, 540, 122, 23);

    // sidechain tab
    scFreq.place (98, 574); scQ.place (188, 574); scSens.place (278, 574); scMix.place (368, 574); fdc.place (458, 574);
    scSource.setBounds (584, 576, 180, 26);
    scFilter.setBounds (584, 614, 180, 26);
    scListen.setBounds (790, 576, 96, 26);
    filterView.setBounds (900, 576, 150, 76);
    // limiter tab
    limOn.setBounds (56, 592, 100, 28);
    limCeil.place (230, 574); limRel.place (320, 574); limLook.place (410, 574); limAmt.place (500, 574);
    truePeak.setBounds (580, 592, 100, 28);
    softClip.setBounds (690, 592, 100, 28);
    // input / stereo tab
    inGain.place (98, 574); width.place (188, 574); noise.place (278, 574);
    autoMakeup.setBounds (350, 592, 120, 28);
    hum.setBounds (560, 592, 190, 28);
}

void MainPanel::selectTab (int tab)
{
    currentTab = tab;
    for (int i = 0; i < 3; ++i)
    {
        tabButtons[i].setToggleState (i == tab, juce::dontSendNotification);
        for (auto* c : tabContent[i])
            c->setVisible (i == tab);
    }
    repaint();
}

void MainPanel::applyTheme()
{
    const auto& t = lnf.getTheme();
    for (auto* k : { &thrA, &ratioA, &tcA, &atkA, &relA, &mkA, &knee })
        k->setAccent (t.teal);
    for (auto* k : { &thrB, &ratioB, &tcB, &atkB, &relB, &mkB })
        k->setAccent (t.coral);
    for (auto* k : { &scFreq, &scQ, &scSens, &scMix, &fdc })
        k->setAccent (t.lime);
    for (auto* k : { &limCeil, &limRel, &limLook, &limAmt })
        k->setAccent (t.coral);
    link.setAccent (t.sky);
    outGain.setAccent (t.sky);
    inGain.setAccent (t.sky);
    width.setAccent (t.sky);
    tube.setAccent (t.orange);
    xfmr.setAccent (t.violet);
    noise.setAccent (t.violet);
    mix.setAccent (t.sun);
    stereoSelector.setAccent (t.sky);
    scSource.setAccent (t.lime);
    scFilter.setAccent (t.lime);
    hum.setAccent (t.violet);
    TwinMuLookAndFeel::setAccent (enA, t.teal);
    TwinMuLookAndFeel::setAccent (enB, t.coral);
    TwinMuLookAndFeel::setAccent (scListen, t.lime);
    for (auto* b : { &limOn, &truePeak, &softClip })
        TwinMuLookAndFeel::setAccent (*b, t.coral);
    TwinMuLookAndFeel::setAccent (autoMakeup, t.sky);
    for (auto& b : tabButtons)
        TwinMuLookAndFeel::setAccent (b, t.lime);
    vuA.setAccent (t.teal);
    vuB.setAccent (t.coral);
    themeButton.setButtonText (t.name);
    repaint();
}

void MainPanel::paintSection (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title, juce::Colour accent, bool flatTopLeft)
{
    const auto& t = lnf.getTheme();
    juce::Path shape;
    shape.addRoundedRectangle (area.getX(), area.getY(), area.getWidth(), area.getHeight(), 8.0f, 8.0f,
                               ! flatTopLeft, true, true, true);
    if (! accent.isTransparent())
    {
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (t.tint), area.getX(), area.getY(),
                                                 accent.withAlpha (t.tint * 0.2f), area.getX(), area.getBottom(), false));
        g.fillPath (shape);
    }
    g.setColour (t.sectionLine);
    g.strokePath (shape, juce::PathStrokeType (1.0f));

    if (title.isNotEmpty())
    {
        const auto font = TwinMuLookAndFeel::font (9.0f, true).withExtraKerningFactor (0.22f);
        const float w = juce::GlyphArrangement::getStringWidth (font, title) + (accent.isTransparent() ? 12.0f : 25.0f);
        const auto label = juce::Rectangle<float> (area.getX() + 14.0f, area.getY() - 7.0f, w, 14.0f);
        g.setColour (t.titleBackground);
        g.fillRoundedRectangle (label, 3.0f);
        float textX = label.getX() + 6.0f;
        if (! accent.isTransparent())
        {
            g.setColour (accent);
            g.fillEllipse (textX, label.getCentreY() - 3.5f, 7.0f, 7.0f);
            textX += 13.0f;
        }
        g.setColour (t.label);
        g.setFont (font);
        g.drawText (title, juce::Rectangle<float> (textX, label.getY(), w, 14.0f), juce::Justification::centredLeft);
    }
}

void MainPanel::paint (juce::Graphics& g)
{
    const auto& t = lnf.getTheme();
    const auto bounds = getLocalBounds().toFloat();

    // brushed panel
    juce::ColourGradient panel (t.panelTop, 0.0f, 0.0f, t.panelBottom, 0.0f, bounds.getHeight(), false);
    panel.addColour (0.45, t.panelMid);
    g.setGradientFill (panel);
    g.fillRect (bounds);
    g.setColour (t.brush);
    for (int x = 0; x < designWidth; x += 3)
        g.drawVerticalLine (x, 44.0f, bounds.getHeight());

    // screws
    for (auto p : { juce::Point<float> (17.0f, 61.0f), juce::Point<float> (1083.0f, 61.0f), juce::Point<float> (17.0f, 663.0f), juce::Point<float> (1083.0f, 663.0f) })
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe4e4e4), p.x - 3.0f, p.y - 3.0f, juce::Colour (0xff3b3c3e), p.x + 7.0f, p.y + 7.0f, true));
        g.fillEllipse (p.x - 7.0f, p.y - 7.0f, 14.0f, 14.0f);
        g.setColour (juce::Colour (0xff2b2b2b));
        g.drawLine (juce::Line<float> (p.getPointOnCircumference (5.0f, 2.18f), p.getPointOnCircumference (5.0f, 5.32f)), 2.0f);
    }

    // top bar
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1f21), 0.0f, 0.0f, gui::fixed::topBar, 0.0f, 44.0f, false));
    g.fillRect (0.0f, 0.0f, bounds.getWidth(), 44.0f);
    g.setColour (juce::Colours::black);
    g.drawHorizontalLine (44, 0.0f, bounds.getWidth());
    g.setFont (TwinMuLookAndFeel::font (17.0f, true).withExtraKerningFactor (0.14f));
    g.setColour (gui::fixed::ink);
    g.drawText ("UNFAIRBOY", 34, 10, 120, 24, juce::Justification::centredLeft);
    g.setColour (t.coral);
    g.drawText ("067", 150, 10, 44, 24, juce::Justification::centredLeft);
    g.setColour (gui::fixed::ink2);
    g.setFont (TwinMuLookAndFeel::font (8.5f).withExtraKerningFactor (0.12f));
    g.drawText ("VARI-MU COMP / LIMITER", 194, 10, 130, 24, juce::Justification::centredLeft);

    // sections
    paintSection (g, { 34.0f, 62.0f, 1032.0f, 196.0f }, "METERS", juce::Colours::transparentBlack);
    paintSection (g, { 34.0f, 310.0f, 396.0f, 216.0f }, titleA, t.teal);
    paintSection (g, { 442.0f, 310.0f, 396.0f, 216.0f }, titleB, t.coral);
    paintSection (g, { 850.0f, 310.0f, 216.0f, 216.0f }, "CHARACTER", t.violet);
    paintSection (g, { 34.0f, 563.0f, 1032.0f, 104.0f }, {}, currentTab == 0 ? t.lime : (currentTab == 1 ? t.coral : t.sky), true);

    // chips
    g.setColour (t.label2);
    g.setFont (TwinMuLookAndFeel::font (9.0f, true).withExtraKerningFactor (0.14f));
    g.drawText ("IN", 48, 250, 30, 10, juce::Justification::left);
    g.drawText ("OUT", 1022, 250, 40, 10, juce::Justification::left);
    g.drawText ("MODE", 36, 272, 44, 14, juce::Justification::left);
    g.drawText ("STEREO", 640, 272, 54, 14, juce::Justification::left);

    if (currentTab == 0)
    {
        g.drawText ("SOURCE", 520, 582, 60, 14, juce::Justification::left);
        g.drawText ("FILTER", 520, 620, 60, 14, juce::Justification::left);
        const bool ext = proc.isExternalSidechainActive();
        g.drawText ("EXT SC", 790, 622, 50, 14, juce::Justification::left);
        g.setColour (ext ? gui::fixed::amber : juce::Colour (0xff3a1a14));
        g.fillEllipse (842.0f, 625.5f, 7.0f, 7.0f);
        if (! ext && rawValue (state, ids::scSource) > 0.5f)
        {
            g.setColour (t.label2);
            g.drawText ("not connected: internal", 790, 638, 110, 14, juce::Justification::left);
        }
    }
    else if (currentTab == 1)
    {
        g.drawText ("LIMITER GR", 820, 586, 100, 14, juce::Justification::left);
        g.setColour (t.coral);
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::bold)));
        g.drawText (juce::String (-limiterGr, 1) + " dB", 820, 602, 140, 24, juce::Justification::left);
    }
    else
    {
        g.drawText ("HUM", 520, 600, 40, 14, juce::Justification::left);
        g.drawText ("LATENCY", 820, 586, 100, 14, juce::Justification::left);
        g.setColour (t.label);
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 14.0f, juce::Font::bold)));
        g.drawText (juce::String (proc.getLatencySamples()) + " samples", 820, 602, 160, 20, juce::Justification::left);
    }
}

void MainPanel::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.001, 0.2, (now - lastTick) * 0.001);
    lastTick = now;

    auto& m = proc.getMeters();
    const float inL = m.inputPeak[0].exchange (0.0f), inR = m.inputPeak[1].exchange (0.0f);
    const float outL = m.outputPeak[0].exchange (0.0f), outR = m.outputPeak[1].exchange (0.0f);
    const float grA = m.gainReductionDb[0].exchange (0.0f), grB = m.gainReductionDb[1].exchange (0.0f);
    limiterGr = m.limiterGainReductionDb.exchange (0.0f);
    m.clip.store (false);

    const int stereoMode = juce::roundToInt (rawValue (state, ids::stereoMode));
    const bool linked = stereoMode == 0;
    inputMeter.update (inL, inR, dt);
    outputMeter.update (outL, outR, dt);
    vuA.update (grA, dt);
    vuB.update (linked ? grA : grB, dt);

    // transfer curve from the current parameter values (same functions as the engine)
    const int mode = juce::roundToInt (rawValue (state, ids::mode));
    const float thr = rawValue (state, ids::thrA), ratio = rawValue (state, ids::ratioA), kneeDb = rawValue (state, ids::knee);
    auto grFunction = [mode, thr, ratio, kneeDb] (float level)
    {
        if (mode == 2) return dsp::variableMuGainReduction (level, thr, ratio, kneeDb);
        return dsp::cleanGainReduction (level, thr, mode == 1 ? std::max (ratio, 20.0f) : ratio, kneeDb);
    };
    static const char* modeNames[] = { "Compressor", "Limiter", "Vintage (variable-mu)" };
    curve.update (grFunction, thr, dsp::gainToDb (std::max (inL, inR), -90.0f), grA,
                  modeNames[juce::jlimit (0, 2, mode)]);

    if (stereoMode != lastStereoMode)
    {
        lastStereoMode = stereoMode;
        static const char* aNames[] = { "CHANNEL A - LEFT + RIGHT", "CHANNEL A - LEFT", "CHANNEL A - MID" };
        static const char* bNames[] = { "CHANNEL B - FOLLOWS A", "CHANNEL B - RIGHT", "CHANNEL B - SIDE" };
        titleA = aNames[juce::jlimit (0, 2, stereoMode)];
        titleB = bNames[juce::jlimit (0, 2, stereoMode)];
        for (auto* c : std::initializer_list<juce::Component*> { &thrB, &ratioB, &tcB, &atkB, &relB, &mkB, &enB })
        {
            c->setEnabled (! linked);
            c->setAlpha (linked ? 0.55f : 1.0f);
        }
        link.setEnabled (linked);
        link.setAlpha (linked ? 1.0f : 0.55f);
        vuA.setCaption (stereoMode == 2 ? "GR - MID" : "GAIN REDUCTION");
        vuB.setCaption (stereoMode == 2 ? "GR - SIDE" : "GAIN REDUCTION");
        repaint();
    }

    filterView.setFilter (juce::roundToInt (rawValue (state, ids::scFilter)), rawValue (state, ids::scFreq),
                          rawValue (state, ids::scQ), true);

    presetButton.setButtonText (proc.presets.getCurrentName());
    slotA.setToggleState (proc.getActiveSlot() == 0, juce::dontSendNotification);
    slotB.setToggleState (proc.getActiveSlot() == 1, juce::dontSendNotification);
    undoButton.setEnabled (proc.undoManager.canUndo());
    redoButton.setEnabled (proc.undoManager.canRedo());

    // one undo step per gesture
    if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        proc.undoManager.beginNewTransaction();

    repaint (juce::Rectangle<int> (500, 570, 560, 95)); // tab read-outs
}

// ------------------------------------------------------------------ presets
void MainPanel::showPresetMenu()
{
    auto& pm = proc.presets;
    pm.refresh();
    juce::PopupMenu menu;
    menu.setLookAndFeel (&lnf);
    const auto& entries = pm.getEntries();
    for (const auto& category : PresetManager::categories())
    {
        juce::PopupMenu sub;
        for (int i = 0; i < entries.size(); ++i)
            if (entries[i].category == category)
                sub.addItem (1000 + i, entries[i].name + (entries[i].factory ? "" : "  (user)"), true, i == pm.getCurrentIndex());
        if (sub.getNumItems() > 0)
            menu.addSubMenu (category, sub);
    }
    const bool isUser = juce::isPositiveAndBelow (pm.getCurrentIndex(), entries.size()) && ! entries[pm.getCurrentIndex()].factory;
    menu.addSeparator();
    menu.addItem (1, "Save as...");
    menu.addItem (2, "Rename...", isUser);
    menu.addItem (3, "Delete", isUser);
    menu.addSeparator();
    menu.addItem (4, "Import...");
    menu.addItem (5, "Export...");
    menu.addSeparator();
    menu.addItem (6, "Copy settings");
    menu.addItem (7, "Paste settings");
    menu.addItem (8, "Randomize (gentle)");
    menu.addItem (9, "Open preset folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetButton), [this] (int result)
    {
        auto& presets = proc.presets;
        if (result >= 1000) presets.load (result - 1000);
        else if (result == 1) showSaveDialog();
        else if (result == 2) showRenameDialog();
        else if (result == 3) confirmDelete();
        else if (result == 4) importPreset();
        else if (result == 5) exportPreset();
        else if (result == 6) juce::SystemClipboard::copyTextToClipboard (presets.toXmlString());
        else if (result == 7) presets.fromXmlString (juce::SystemClipboard::getTextFromClipboard());
        else if (result == 8) presets.randomize (0.15f);
        else if (result == 9)
        {
            presets.getUserDirectory().createDirectory();
            presets.getUserDirectory().startAsProcess();
        }
    });
}

void MainPanel::showSaveDialog()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name and category", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presets.getCurrentName().upToFirstOccurrenceOf (" *", false, false));
    w->addComboBox ("category", PresetManager::categories());
    w->getComboBoxComponent ("category")->setText ("User", juce::dontSendNotification);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MainPanel> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (r == 1 && safe != nullptr)
            safe->proc.presets.saveUser (w->getTextEditorContents ("name"), w->getComboBoxComponent ("category")->getText());
    }), true);
}

void MainPanel::showRenameDialog()
{
    const int index = proc.presets.getCurrentIndex();
    auto* w = new juce::AlertWindow ("Rename preset", {}, juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.presets.getCurrentName());
    w->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<MainPanel> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, index] (int r)
    {
        if (r == 1 && safe != nullptr)
            safe->proc.presets.renameUser (index, w->getTextEditorContents ("name"));
    }), true);
}

void MainPanel::confirmDelete()
{
    const int index = proc.presets.getCurrentIndex();
    juce::Component::SafePointer<MainPanel> safe (this);
    juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                      .withTitle ("Delete preset")
                                      .withMessage ("Move \"" + proc.presets.getCurrentName() + "\" to the recycle bin?")
                                      .withButton ("Delete")
                                      .withButton ("Cancel")
                                      .withAssociatedComponent (this),
                                  [safe, index] (int r)
                                  {
                                      if (r == 1 && safe != nullptr)
                                          safe->proc.presets.deleteUser (index);
                                  });
}

void MainPanel::importPreset()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Import preset", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                       juce::String ("*") + PresetManager::fileExtension);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult().existsAsFile())
                                      proc.presets.importFile (fc.getResult());
                              });
}

void MainPanel::exportPreset()
{
    const auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                           .getChildFile (juce::File::createLegalFileName (proc.presets.getCurrentName()) + PresetManager::fileExtension);
    fileChooser = std::make_unique<juce::FileChooser> ("Export preset", start, juce::String ("*") + PresetManager::fileExtension);
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  if (fc.getResult() != juce::File())
                                      proc.presets.exportCurrent (fc.getResult());
                              });
}

// =====================================================================================================
TwinMuEditor::TwinMuEditor (TwinMuProcessor& p)
    : AudioProcessorEditor (p),
      panel (p, lookAndFeel, [this] (int index) { setTheme (index); })
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Unfairboy 067";
    options.filenameSuffix = ".settings";
    options.folderName = "Unfairboy 067";
    options.osxLibrarySubFolder = "Application Support";
    settings.setStorageParameters (options);

    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (panel);

    auto* props = settings.getUserSettings();
    setTheme (props != nullptr ? props->getIntValue ("theme", 0) : 0);

    setResizable (true, true);
    setResizeLimits (MainPanel::designWidth, MainPanel::designHeight, MainPanel::designWidth * 2, MainPanel::designHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) MainPanel::designWidth / MainPanel::designHeight);
    const double scale = props != nullptr ? juce::jlimit (1.0, 2.0, props->getDoubleValue ("scale", 1.0)) : 1.0;
    setSize (juce::roundToInt (MainPanel::designWidth * scale), juce::roundToInt (MainPanel::designHeight * scale));
}

TwinMuEditor::~TwinMuEditor() { setLookAndFeel (nullptr); }

void TwinMuEditor::setTheme (int index)
{
    auto* props = settings.getUserSettings();
    if (index < 0) // cycle
    {
        int current = 0;
        for (int i = 0; i < (int) gui::themes().size(); ++i)
            if (&gui::themes()[(size_t) i] == &lookAndFeel.getTheme())
                current = i;
        index = (current + 1) % (int) gui::themes().size();
    }
    index = juce::jlimit (0, (int) gui::themes().size() - 1, index);
    lookAndFeel.setTheme (gui::themes()[(size_t) index]);
    if (props != nullptr)
        props->setValue ("theme", index);
    panel.applyTheme();
}

void TwinMuEditor::resized()
{
    const float scale = (float) getWidth() / (float) MainPanel::designWidth;
    panel.setTransform (juce::AffineTransform::scale (scale));
    panel.setBounds (0, 0, MainPanel::designWidth, MainPanel::designHeight);
    if (auto* props = settings.getUserSettings())
        props->setValue ("scale", (double) scale);
}
} // namespace twinmu
