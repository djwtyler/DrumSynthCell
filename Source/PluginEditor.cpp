#include "PluginEditor.h"

static const char* kChanNames[DrumSynth::NumChannels] =
    { "Kick", "Snare", "Clap", "Tom 1", "Tom 2", "Closed Hat", "Open Hat", "Cymbal" };

// Colour palette
static const juce::Colour kBg        { 0xff1a1a2e };
static const juce::Colour kPanel     { 0xff22223a };
static const juce::Colour kPanelLine { 0xff383860 };
static const juce::Colour kAccent    { 0xff4f8ef7 };
static const juce::Colour kKnobArc   { 0xff4f8ef7 };
static const juce::Colour kText      { 0xfff2f2ff };   // near-white
static const juce::Colour kDim       { 0xffccccdd };   // light grey
static const juce::Colour kPadActive { 0xff3a3a60 };
static const juce::Colour kPadSel    { 0xff4f8ef7 };

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
DrumSynthEditor::DrumSynthEditor (DrumSynthProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      viewToggleBtn ("Advanced")
{
    setLookAndFeel (&lnf);
    setSize (kBasicW(), kBasicH());

    // Populate lfo wave and other combo items now so they're ready before connect
    setupCombo (lfo1WaveBox,   { "Sine", "Triangle", "Square", "Saw Up", "Saw Down" });
    setupCombo (lfo2WaveBox,   { "Sine", "Triangle", "Square", "Saw Up", "Saw Down" });
    setupCombo (driveTypeBox,  { "Soft Clip", "Hard Clip", "Wavefold" });
    setupCombo (filterModeBox, { "LP", "HP", "BP", "Peak", "Notch" });
    setupCombo (filterModelBox,{ "Clean", "Fat" });
    setupCombo (fx1TypeBox,    { "Off", "Soft Clip", "Hard Clip", "Bitcrusher", "Wavefold" });

    // Knob ranges
    setupKnob (pitchKnob,       20.0, 2000.0, 80.0, " Hz");
    setupKnob (oscShapeKnob,    0.0,  1.0,  0.0);
    oscShapeKnob.textFromValueFunction = [] (double v) -> juce::String {
        if (v < 0.04)              return "Sine";
        if (v > 0.96)              return "Square";
        if (v > 0.46 && v < 0.54) return "Saw";
        return v < 0.5 ? "Sine-Saw" : "Saw-Sq";
    };
    // setValue above ran before the formatter existed and left the text box
    // showing the raw number; force a real change now to refresh it
    oscShapeKnob.setValue (1.0, juce::dontSendNotification);
    oscShapeKnob.setValue (0.0, juce::dontSendNotification);
    setupKnob (partPeakKnob,    1.0,  8.0,  1.0);
    setupKnob (partSpaceKnob,   0.0,  1.0,  0.5);
    setupKnob (partRollKnob,    0.0,  1.0,  0.5);
    setupKnob (partDecKnob,     0.0,  1.0,  0.5);
    setupKnob (noiseLevelKnob,  0.0,  1.0,  0.0);
    setupKnob (noiseDecKnob,    0.001,4.0,  0.1);
    setupKnob (noiseBPFreqKnob, 100.0,20000.0, 8000.0, " Hz");
    setupKnob (noiseBPQKnob,    0.1,  10.0, 0.7);
    setupKnob (driveAmtKnob,    0.0,  1.0,  0.0);
    setupKnob (filterCutKnob,   20.0, 20000.0, 12000.0, " Hz");
    setupKnob (filterResKnob,   0.0,  1.0,  0.5);
    setupKnob (pEnvDepthKnob,   0.0,  48.0, 0.0, " st");
    setupKnob (pEnvDecKnob,     0.001,2.0,  0.04, " s");
    setupKnob (fEnvAttKnob,     0.001,2.0,  0.005," s");
    setupKnob (fEnvHoldKnob,    0.0,  2.0,  0.0,  " s");
    setupKnob (fEnvDecKnob,     0.001,4.0,  0.3,  " s");
    setupKnob (fEnvDepthKnob,  -48.0, 48.0, 0.0,  " st");
    setupKnob (ampAttKnob,      0.001,2.0,  0.002," s");
    setupKnob (ampHoldKnob,     0.0,  2.0,  0.0,  " s");
    setupKnob (ampDecKnob,      0.001,8.0,  0.5,  " s");
    setupKnob (lfo1RateKnob,    0.01, 1000.0, 1.0, " Hz");
    setupKnob (lfo1DepthKnob,   0.0,  24.0, 0.0,  " st");
    setupKnob (lfo2RateKnob,    0.01, 1000.0, 1.0, " Hz");
    setupKnob (lfo2DepthKnob,   0.0,  24.0, 0.0,  " st");
    setupKnob (fx1AmtKnob,      0.0,  1.0,  0.5);
    setupKnob (bitDepthKnob,    1.0,  24.0, 8.0,  " bit");

    for (auto& k : macroKnobs)
    {
        k.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        k.setColour (juce::Slider::rotarySliderOutlineColourId, kPanelLine);
    }
    setupKnob (macroKnobs[0], 20.0,    2000.0,  80.0,  " Hz");  // Tune
    setupKnob (macroKnobs[1],  0.0,    48.0,    0.0,   " st");  // Pitch Env depth
    setupKnob (macroKnobs[2],  0.001,  2.0,     0.002, " s");   // Attack
    setupKnob (macroKnobs[3],  0.001,  8.0,     0.5,   " s");   // Decay
    setupKnob (macroKnobs[4],  0.0,    1.0,     0.8);            // Volume
    setupKnob (macroKnobs[5],  0.0,    1.0,     0.0);            // Noise
    setupKnob (macroKnobs[6],  20.0,   20000.0, 12000.0," Hz"); // Flt Cut
    setupKnob (macroKnobs[7],  0.0,    1.0,     0.5);            // Resonance

    buildBasicView();
    buildAdvancedView();
    showAdvancedView();
    connectMacros();
    connectAdvancedControls();
    refreshAdvanced();
}

// ---------------------------------------------------------------------------
// Setup helpers
// ---------------------------------------------------------------------------
void DrumSynthEditor::setupKnob (juce::Slider& s, double lo, double hi, double def,
                                  const juce::String& suffix)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 28);
    s.setNumDecimalPlacesToDisplay (1);
    s.setRange (lo, hi);
    s.setValue (def, juce::dontSendNotification);
    if (suffix.isNotEmpty()) s.setTextValueSuffix (suffix);
    s.setColour (juce::Slider::rotarySliderFillColourId,    kAccent);
    s.setColour (juce::Slider::rotarySliderOutlineColourId, kPanelLine);
    s.setColour (juce::Slider::textBoxTextColourId,         kDim);
    s.setColour (juce::Slider::textBoxBackgroundColourId,   kBg);
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
}

void DrumSynthEditor::setupCombo (juce::ComboBox& b, const juce::StringArray& items)
{
    b.addItemList (items, 1);
    b.setSelectedId (1, juce::dontSendNotification);
    b.setColour (juce::ComboBox::backgroundColourId, kPanel);
    b.setColour (juce::ComboBox::textColourId,       kText);
    b.setColour (juce::ComboBox::outlineColourId,    kPanelLine);
    b.setColour (juce::ComboBox::arrowColourId,      kAccent);
}

// ---------------------------------------------------------------------------
// Build views (add components)
// ---------------------------------------------------------------------------
void DrumSynthEditor::buildBasicView()
{
    addChildComponent (viewToggleBtn);
    viewToggleBtn.onClick = [this] { showAdvancedView(); };

    for (int ch = 0; ch < DrumSynth::NumChannels; ++ch)
    {
        auto& btn = padBtns[size_t (ch)];
        btn.setButtonText (kChanNames[ch]);
        btn.onClick = [this, ch] {
            selectedChannel = ch;
            proc.getVoice (ch).trigger (1.0f);
            updatePadHighlight();
            if (advancedMode) refreshAdvanced();
            else              refreshMacros();
        };
        addChildComponent (btn);
    }

    for (int i = 0; i < 8; ++i)
    {
        macroLabels[size_t (i)].setText (kMacroNames[i], juce::dontSendNotification);
        macroLabels[size_t (i)].setJustificationType (juce::Justification::centred);
        macroLabels[size_t (i)].setColour (juce::Label::textColourId, kDim);
        macroLabels[size_t (i)].setFont (juce::FontOptions (18.0f));
        addChildComponent (macroKnobs[size_t (i)]);
        addChildComponent (macroLabels[size_t (i)]);
    }

    updatePadHighlight();
}

void DrumSynthEditor::buildAdvancedView()
{
    // Channel selector buttons
    for (int ch = 0; ch < DrumSynth::NumChannels; ++ch)
    {
        auto& btn = advChanBtns[size_t (ch)];
        btn.setButtonText (kChanNames[ch]);
        btn.onClick = [this, ch] {
            selectedChannel = ch;
            updateChanHighlight();
            refreshAdvanced();
        };
        addChildComponent (btn);
    }

    // "Basic View" toggle embedded in viewToggleBtn — we reuse it
    // (its text/callback is swapped in showAdvancedView/showBasicView)

    // OSC
    addChildComponent (pitchKnob);
    addChildComponent (oscShapeKnob);
    addChildComponent (oscShapeDisplay);
    addChildComponent (metallicBtn);
    addChildComponent (shaperEnabledBtn);
    addChildComponent (partPeakKnob);
    addChildComponent (partSpaceKnob);
    addChildComponent (partRollKnob);
    addChildComponent (partDecKnob);
    addChildComponent (membraneBtn);

    // NOISE
    addChildComponent (noiseLevelKnob);
    addChildComponent (noiseDecKnob);
    addChildComponent (noiseBPFreqKnob);
    addChildComponent (noiseBPQKnob);
    addChildComponent (noisePinkBtn);

    // DRIVE
    addChildComponent (driveAmtKnob);
    addChildComponent (driveTypeBox);

    // FILTER
    addChildComponent (filterModeBox);
    addChildComponent (filterModelBox);
    addChildComponent (filter4PoleBtn);
    addChildComponent (filterCutKnob);
    addChildComponent (filterResKnob);

    // ENVELOPES
    addChildComponent (pEnvDepthKnob);
    addChildComponent (pEnvDecKnob);
    addChildComponent (fEnvAttKnob);
    addChildComponent (fEnvHoldKnob);
    addChildComponent (fEnvDecKnob);
    addChildComponent (fEnvDepthKnob);
    addChildComponent (ampAttKnob);
    addChildComponent (ampHoldKnob);
    addChildComponent (ampDecKnob);

    // LFO
    addChildComponent (lfo1RateKnob);
    addChildComponent (lfo1DepthKnob);
    addChildComponent (lfo1WaveBox);
    addChildComponent (lfo2RateKnob);
    addChildComponent (lfo2DepthKnob);
    addChildComponent (lfo2WaveBox);

    // FX
    addChildComponent (fx1TypeBox);
    addChildComponent (fx1AmtKnob);
    addChildComponent (bitDepthKnob);

    // Per-knob labels for advanced view
    static const char* kAdvLblText[28] = {
        "Pitch", "Shape", "Peak", "Space", "Roll", "Decay", // OSC 0-5
        "Level", "Decay", "BP Freq", "BP Q",                // NOISE 6-9
        "Amount",                                            // DRIVE 10
        "Cutoff", "Res",                                     // FILTER 11-12
        "P Depth", "P Decay",                               // Pitch env 13-14
        "F Att", "F Hold", "F Decay", "F Depth",            // Filter env 15-18
        "A Att", "A Hold", "A Decay",                       // Amp env 19-21
        "Rate", "Depth", "Rate", "Depth",                   // LFO1/2 22-25
        "Amount", "Bit Dep"                                  // FX 26-27
    };
    for (int i = 0; i < 28; ++i)
    {
        advLbl[i].setText (kAdvLblText[i], juce::dontSendNotification);
        advLbl[i].setJustificationType (juce::Justification::centred);
        advLbl[i].setFont (juce::FontOptions (13.0f));
        advLbl[i].setColour (juce::Label::textColourId, kText);
        addChildComponent (advLbl[i]);
    }

    // Style toggles
    for (auto* btn : { &metallicBtn, &shaperEnabledBtn, &membraneBtn,
                       &noisePinkBtn, &filter4PoleBtn })
    {
        btn->setColour (juce::ToggleButton::textColourId,      kText);
        btn->setColour (juce::ToggleButton::tickColourId,      kAccent);
        btn->setColour (juce::ToggleButton::tickDisabledColourId, kDim);
    }

    // TransMod source selection buttons
    static const char* kSrcNames[kNumModSources] = { "LFO 1", "LFO 2", "Vel" };
    for (int s = 0; s < kNumModSources; ++s)
    {
        modSrcBtns[size_t (s)].setButtonText (kSrcNames[s]);
        modSrcBtns[size_t (s)].setColour (juce::TextButton::buttonColourId, kPadActive);
        modSrcBtns[size_t (s)].setColour (juce::TextButton::textColourOffId, kText);
        modSrcBtns[size_t (s)].onClick = [this, s]
        {
            activeModSource = (activeModSource == s) ? -1 : s;
            updateTransModUI();
        };
        addChildComponent (modSrcBtns[size_t (s)]);
    }
}

// ---------------------------------------------------------------------------
// Show / hide views
// ---------------------------------------------------------------------------
void DrumSynthEditor::showBasicView()
{
    advancedMode = false;
    setSize (kBasicW(), kBasicH());

    viewToggleBtn.setButtonText ("Advanced");
    viewToggleBtn.onClick = [this] { showAdvancedView(); };

    // Basic components visible
    viewToggleBtn.setVisible (true);
    for (auto& b : padBtns)     b.setVisible (true);
    for (auto& k : macroKnobs)  k.setVisible (true);
    for (auto& l : macroLabels) l.setVisible (true);

    // Advanced components hidden
    for (auto& l : advLbl)      l.setVisible (false);
    for (auto& b : advChanBtns) b.setVisible (false);
    for (auto& b : modSrcBtns)  b.setVisible (false);
    for (auto* c : std::initializer_list<juce::Component*> {
                     &pitchKnob, &oscShapeKnob, &oscShapeDisplay, &metallicBtn, &shaperEnabledBtn,
                     &partPeakKnob, &partSpaceKnob, &partRollKnob, &partDecKnob, &membraneBtn,
                     &noiseLevelKnob, &noiseDecKnob, &noiseBPFreqKnob, &noiseBPQKnob, &noisePinkBtn,
                     &driveAmtKnob, &driveTypeBox,
                     &filterModeBox, &filterModelBox, &filter4PoleBtn, &filterCutKnob, &filterResKnob,
                     &pEnvDepthKnob, &pEnvDecKnob, &fEnvAttKnob, &fEnvHoldKnob,
                     &fEnvDecKnob, &fEnvDepthKnob, &ampAttKnob, &ampHoldKnob, &ampDecKnob,
                     &lfo1RateKnob, &lfo1DepthKnob, &lfo1WaveBox,
                     &lfo2RateKnob, &lfo2DepthKnob, &lfo2WaveBox,
                     &fx1TypeBox, &fx1AmtKnob, &bitDepthKnob })
        c->setVisible (false);

    refreshMacros();
    resized();
}

void DrumSynthEditor::showAdvancedView()
{
    advancedMode = true;
    setSize (kAdvW, kAdvH);

    viewToggleBtn.setButtonText ("Basic View");
    viewToggleBtn.onClick = [this] { showBasicView(); };

    // Basic components: pads stay visible, macro knobs/labels hidden
    for (auto& b : padBtns)     b.setVisible (true);
    for (auto& k : macroKnobs)  k.setVisible (false);
    for (auto& l : macroLabels) l.setVisible (false);

    // Advanced components visible; advChanBtns not used in this view
    viewToggleBtn.setVisible (true);
    for (auto& l : advLbl)      l.setVisible (true);
    for (auto& b : advChanBtns) b.setVisible (false);
    for (auto& b : modSrcBtns)  b.setVisible (true);
    for (auto* c : std::initializer_list<juce::Component*> {
                     &pitchKnob, &oscShapeKnob, &oscShapeDisplay, &metallicBtn, &shaperEnabledBtn,
                     &partPeakKnob, &partSpaceKnob, &partRollKnob, &partDecKnob, &membraneBtn,
                     &noiseLevelKnob, &noiseDecKnob, &noiseBPFreqKnob, &noiseBPQKnob, &noisePinkBtn,
                     &driveAmtKnob, &driveTypeBox,
                     &filterModeBox, &filterModelBox, &filter4PoleBtn, &filterCutKnob, &filterResKnob,
                     &pEnvDepthKnob, &pEnvDecKnob, &fEnvAttKnob, &fEnvHoldKnob,
                     &fEnvDecKnob, &fEnvDepthKnob, &ampAttKnob, &ampHoldKnob, &ampDecKnob,
                     &lfo1RateKnob, &lfo1DepthKnob, &lfo1WaveBox,
                     &lfo2RateKnob, &lfo2DepthKnob, &lfo2WaveBox,
                     &fx1TypeBox, &fx1AmtKnob, &bitDepthKnob })
        c->setVisible (true);

    updatePadHighlight();
    refreshAdvanced();
    resized();
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------
void DrumSynthEditor::layoutBasicView()
{
    viewToggleBtn.setBounds (getWidth() - 104, 4, 98, kHdrH - 8);

    // 4×2 pad grid
    const int col0Y = kHdrH + kPadGap;
    const int col1Y = col0Y + kPadSize + kPadGap;
    for (int ch = 0; ch < DrumSynth::NumChannels; ++ch)
    {
        int col = ch % 4;
        int row = ch / 4;
        int x   = kPadGap + col * (kPadSize + kPadGap);
        int y   = kHdrH + kPadGap + row * (kPadSize + kPadGap);
        padBtns[size_t (ch)].setBounds (x, y, kPadSize, kPadSize);
    }

    // 4×2 macro grid — each cell: 62px rotary + 28px text box + 26px name label
    const int knobW  = 62;
    const int knobH  = 62 + 28;
    const int cellW  = kPadSize + kPadGap;   // 90px — matches pad column width
    const int rowH   = knobH + 4 + 26;       // 135px per macro row
    const int macroY = col1Y + kPadSize + kPadGap;

    for (int i = 0; i < 8; ++i)
    {
        int col = i % 4;
        int row = i / 4;
        int x   = kPadGap + col * cellW;
        int y   = macroY + row * (rowH + 10);
        macroKnobs[size_t (i)].setBounds (x + (cellW - knobW) / 2, y, knobW, knobH);
        macroLabels[size_t (i)].setBounds (x, y + knobH + 4, cellW, 26);
    }
}

void DrumSynthEditor::layoutAdvancedView()
{
    viewToggleBtn.setBounds (getWidth() - 110, 4, 104, kHdrH - 8);

    // TransMod source buttons — to the right of the 4×2 pad grid
    {
        const int tmX  = kPadGap + 4 * (kPadSize + kPadGap) + 24;
        const int tmY  = kHdrH + (kAdvChanH - 44) / 2;
        const int btnW = 100;
        const int btnH = 44;
        for (int s = 0; s < kNumModSources; ++s)
            modSrcBtns[size_t (s)].setBounds (tmX + s * (btnW + 10), tmY, btnW, btnH);
    }

    // 4×2 pad grid — identical layout to Basic View
    for (int ch = 0; ch < DrumSynth::NumChannels; ++ch)
    {
        int col = ch % 4;
        int row = ch / 4;
        int x   = kPadGap + col * (kPadSize + kPadGap);
        int y   = kHdrH + kPadGap + row * (kPadSize + kPadGap);
        padBtns[size_t (ch)].setBounds (x, y, kPadSize, kPadSize);
    }

    const int panelY = kAdvTop;
    const int C = kKnob + kLblH;   // 86
    const int G = 10;

    // Place advLbl[idx] just above the knob whose top is at ky
    auto lbl = [&] (int idx, int kx, int ky)
    {
        advLbl[idx].setBounds (kx, ky - 18, C, 16);
    };

    // ===== OSC =====
    {
        const int x = kOscX + 8;
        int y = panelY + 54;

        lbl (0, x,         y + 22);  lbl (1, x + C + G, y + 22);
        pitchKnob       .setBounds (x,         y + 22, C, C);
        oscShapeKnob    .setBounds (x + C + G, y + 22, C, C);
        y += 22 + C + 6;
        oscShapeDisplay .setBounds (x, y, (kOscW - 16) / 3, 48);
        y += 56;
        metallicBtn     .setBounds (x, y, kOscW - 14, 22);
        y += 30;
        shaperEnabledBtn.setBounds (x, y, kOscW - 14, 22);
        y += 30;

        lbl (2, x,         y + 22);  lbl (3, x + C + G, y + 22);
        partPeakKnob    .setBounds (x,         y + 22, C, C);
        partSpaceKnob   .setBounds (x + C + G, y + 22, C, C);
        y += 22 + C + 8;

        lbl (4, x,         y + 22);  lbl (5, x + C + G, y + 22);
        partRollKnob    .setBounds (x,         y + 22, C, C);
        partDecKnob     .setBounds (x + C + G, y + 22, C, C);
        y += 22 + C + 8;

        membraneBtn     .setBounds (x, y, kOscW - 14, 22);
    }

    // ===== NOISE =====
    {
        const int x = kNsX + 8;
        int y = panelY + 54;

        lbl (6, x,       y + 22);  lbl (7, x + C + G, y + 22);
        noiseLevelKnob  .setBounds (x,       y + 22, C, C);
        noiseDecKnob    .setBounds (x + C + G, y + 22, C, C);
        y += 22 + C + 8;
        noisePinkBtn    .setBounds (x, y, kNsW - 14, 22);
        y += 30;

        lbl (8, x,       y + 22);  lbl (9, x + C + G, y + 22);
        noiseBPFreqKnob .setBounds (x,       y + 22, C, C);
        noiseBPQKnob    .setBounds (x + C + G, y + 22, C, C);
    }

    // ===== DRIVE =====
    {
        const int x = kDrvX + 8;
        int y = panelY + 54;
        const int cx = x + (kDrvW - C - 16) / 2;
        lbl (10, cx, y + 22);
        driveAmtKnob.setBounds (cx, y + 22, C, C);
        y += 22 + C + 12;
        driveTypeBox.setBounds (x, y, kDrvW - 14, 26);
    }

    // ===== FILTER =====
    {
        const int x = kFltX + 8;
        int y = panelY + 54;
        filterModeBox .setBounds (x, y, kFltW - 14, 26);
        y += 34;
        filterModelBox.setBounds (x, y, (kFltW - 20) / 2, 26);
        filter4PoleBtn.setBounds (x + (kFltW - 20) / 2 + 8, y, (kFltW - 20) / 2, 26);
        y += 34;
        lbl (11, x,       y + 22);  lbl (12, x + C + G, y + 22);
        filterCutKnob .setBounds (x,       y + 22, C, C);
        filterResKnob .setBounds (x + C + G, y + 22, C, C);
    }

    // ===== ENVELOPES =====
    {
        const int x = kEnvX + 8;
        int y = panelY + 54;

        lbl (13, x,       y + 22);  lbl (14, x + C + G, y + 22);
        pEnvDepthKnob.setBounds (x,       y + 22, C, C);
        pEnvDecKnob  .setBounds (x + C + G, y + 22, C, C);
        y += 22 + C + 20;

        lbl (15, x,       y + 22);  lbl (16, x + C + G, y + 22);
        fEnvAttKnob  .setBounds (x,       y + 22, C, C);
        fEnvHoldKnob .setBounds (x + C + G, y + 22, C, C);
        y += 22 + C + 6;

        lbl (17, x,       y + 22);  lbl (18, x + C + G, y + 22);
        fEnvDecKnob  .setBounds (x,       y + 22, C, C);
        fEnvDepthKnob.setBounds (x + C + G, y + 22, C, C);
        y += 22 + C + 20;

        lbl (19, x,             y + 22);
        lbl (20, x + C + G,     y + 22);
        lbl (21, x + (C + G)*2, y + 22);
        ampAttKnob .setBounds (x,             y + 22, C, C);
        ampHoldKnob.setBounds (x + C + G,     y + 22, C, C);
        ampDecKnob .setBounds (x + (C + G)*2, y + 22, C, C);
    }

    // ===== LFO =====
    {
        const int x = kLfoX + 8;
        int y = panelY + 54;

        lbl (22, x,       y + 22);  lbl (23, x + C + G, y + 22);
        lfo1RateKnob .setBounds (x,       y + 22, C, C);
        lfo1DepthKnob.setBounds (x + C + G, y + 22, C, C);
        y += 22 + C + 6;
        lfo1WaveBox  .setBounds (x, y, kLfoW - 14, 26);
        y += 26 + 32;

        lbl (24, x,       y + 22);  lbl (25, x + C + G, y + 22);
        lfo2RateKnob .setBounds (x,       y + 22, C, C);
        lfo2DepthKnob.setBounds (x + C + G, y + 22, C, C);
        y += 22 + C + 6;
        lfo2WaveBox  .setBounds (x, y, kLfoW - 14, 26);
    }

    // ===== FX strip =====
    {
        const int sepY = kAdvH - kFxH;
        const int fxY  = sepY + 8;
        fx1TypeBox  .setBounds (8, fxY + 5, 160, 26);
        lbl (26, 174,           fxY);
        lbl (27, 174 + C + G,   fxY);
        fx1AmtKnob  .setBounds (174,         fxY, C, C);
        bitDepthKnob.setBounds (174 + C + G, fxY, C, C);
    }
}

void DrumSynthEditor::resized()
{
    if (advancedMode)
        layoutAdvancedView();
    else
        layoutBasicView();
}

// ---------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------
void DrumSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Title
    g.setColour (kText);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawText ("DrumSynthCell", 8, 0, 300, kHdrH, juce::Justification::centredLeft, false);

    if (advancedMode)
    {
        // Channel bar background
        g.setColour (kPanel);
        g.fillRect (0, kHdrH, kAdvW, kAdvChanH);

        // Panel separators and labels
        auto drawPanel = [&] (int x, int w, const juce::String& title)
        {
            g.setColour (kPanelLine);
            g.drawVerticalLine (x - 1, float (kAdvTop), float (kAdvH - kFxH));
            g.setColour (kText);
            g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
            g.drawText (title, x + 3, kAdvTop + 2, w - 4, 30,
                        juce::Justification::centredLeft, false);
        };
        drawPanel (kOscX,  kOscW,  "OSCILLATOR");
        drawPanel (kNsX,   kNsW,   "NOISE");
        drawPanel (kDrvX,  kDrvW,  "DRIVE");
        drawPanel (kFltX,  kFltW,  "FILTER");
        drawPanel (kEnvX,  kEnvW,  "ENVELOPES");
        drawPanel (kLfoX,  kLfoW,  "LFOs");

        // FX strip separator
        g.setColour (kPanelLine);
        g.drawHorizontalLine (kAdvH - kFxH, 0.0f, float (kAdvW));
        g.setColour (kText);
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText ("FX", 5, kAdvH - kFxH + 12, 36, 22,
                    juce::Justification::centredLeft, false);
    }
    else
    {
        // Pad selection highlight painted under the buttons
        // (actual background is the button colour — nothing extra needed)
    }
}

// ---------------------------------------------------------------------------
// Highlight helpers
// ---------------------------------------------------------------------------
void DrumSynthEditor::updatePadHighlight()
{
    for (int ch = 0; ch < DrumSynth::NumChannels; ++ch)
    {
        bool sel = (ch == selectedChannel);
        padBtns[size_t (ch)].setColour (juce::TextButton::buttonColourId,
                                         sel ? kPadSel : kPadActive);
        padBtns[size_t (ch)].setColour (juce::TextButton::textColourOffId,
                                         sel ? juce::Colours::white : kText);
    }
}

void DrumSynthEditor::updateChanHighlight()
{
    for (int ch = 0; ch < DrumSynth::NumChannels; ++ch)
    {
        bool sel = (ch == selectedChannel);
        advChanBtns[size_t (ch)].setColour (juce::TextButton::buttonColourId,
                                              sel ? kPadSel : kPadActive);
        advChanBtns[size_t (ch)].setColour (juce::TextButton::textColourOffId,
                                              sel ? juce::Colours::white : kText);
    }
}

// ---------------------------------------------------------------------------
// Refresh (params → UI)
// ---------------------------------------------------------------------------
void DrumSynthEditor::refreshMacros()
{
    updatingUI = true;
    const auto& p = proc.getVoice (selectedChannel).params;
    macroKnobs[0].setValue (p.pitchHz,         juce::dontSendNotification);
    macroKnobs[1].setValue (p.pitchEnvDepth,   juce::dontSendNotification);
    macroKnobs[2].setValue (p.ampAttack,        juce::dontSendNotification);
    macroKnobs[3].setValue (p.ampDecay,         juce::dontSendNotification);
    macroKnobs[4].setValue (p.outputGain,       juce::dontSendNotification);
    macroKnobs[5].setValue (p.noiseLevel,       juce::dontSendNotification);
    macroKnobs[6].setValue (p.filterCutoff,     juce::dontSendNotification);
    macroKnobs[7].setValue (p.filterResonance,  juce::dontSendNotification);
    updatingUI = false;
}

void DrumSynthEditor::refreshAdvanced()
{
    updatingUI = true;
    const auto& p = proc.getVoice (selectedChannel).params;

    pitchKnob       .setValue (p.pitchHz,  juce::dontSendNotification);
    oscShapeKnob    .setValue (p.oscShape, juce::dontSendNotification);
    metallicBtn     .setToggleState (p.metallic,      juce::dontSendNotification);
    shaperEnabledBtn.setToggleState (p.shaperEnabled, juce::dontSendNotification);
    oscShapeKnob    .setEnabled (!p.metallic && !p.shaperEnabled);
    oscShapeDisplay .setShape (p.oscShape, p.metallic, p.shaperEnabled);
    partPeakKnob    .setValue (p.partialPeak,     juce::dontSendNotification);
    partSpaceKnob   .setValue (p.partialSpace,    juce::dontSendNotification);
    partRollKnob    .setValue (p.partialRoll,     juce::dontSendNotification);
    partDecKnob     .setValue (p.partialDecay,    juce::dontSendNotification);
    membraneBtn     .setToggleState (p.membraneMode,      juce::dontSendNotification);

    noiseLevelKnob  .setValue (modDenorm (ModTarget::NoiseLevel, proc.getVoice(selectedChannel).transmod.get(ModTarget::NoiseLevel).base), juce::dontSendNotification);
    setTransModKnobProps (noiseLevelKnob, ModTarget::NoiseLevel);
    noiseDecKnob    .setValue (p.noiseDecay,      juce::dontSendNotification);
    noisePinkBtn    .setToggleState (p.noiseColor == DrumSynth::VoiceParams::NoiseColor::Pink,
                                     juce::dontSendNotification);
    noiseBPFreqKnob .setValue (p.noiseBPFreq,     juce::dontSendNotification);
    noiseBPQKnob    .setValue (p.noiseBPQ,        juce::dontSendNotification);

    driveAmtKnob    .setValue (modDenorm (ModTarget::DriveAmount, proc.getVoice(selectedChannel).transmod.get(ModTarget::DriveAmount).base), juce::dontSendNotification);
    setTransModKnobProps (driveAmtKnob, ModTarget::DriveAmount);
    driveTypeBox    .setSelectedId (int (p.driveType) + 1, juce::dontSendNotification);

    filterModeBox   .setSelectedId (int (p.filterMode)  + 1, juce::dontSendNotification);
    filterModelBox  .setSelectedId (int (p.filterModel) + 1, juce::dontSendNotification);
    filter4PoleBtn  .setToggleState (p.filterFourPole, juce::dontSendNotification);
    filterCutKnob   .setValue (modDenorm (ModTarget::FilterCutoff,    proc.getVoice(selectedChannel).transmod.get(ModTarget::FilterCutoff).base),    juce::dontSendNotification);
    filterResKnob   .setValue (modDenorm (ModTarget::FilterResonance, proc.getVoice(selectedChannel).transmod.get(ModTarget::FilterResonance).base), juce::dontSendNotification);
    setTransModKnobProps (filterCutKnob,  ModTarget::FilterCutoff);
    setTransModKnobProps (filterResKnob,  ModTarget::FilterResonance);

    pEnvDepthKnob   .setValue (p.pitchEnvDepth,   juce::dontSendNotification);
    pEnvDecKnob     .setValue (p.pitchEnvDecay,   juce::dontSendNotification);
    fEnvAttKnob     .setValue (p.filterEnvAttack, juce::dontSendNotification);
    fEnvHoldKnob    .setValue (p.filterEnvHold,   juce::dontSendNotification);
    fEnvDecKnob     .setValue (p.filterEnvDecay,  juce::dontSendNotification);
    fEnvDepthKnob   .setValue (p.filterEnvDepth,  juce::dontSendNotification);
    ampAttKnob      .setValue (p.ampAttack,        juce::dontSendNotification);
    ampHoldKnob     .setValue (p.ampHold,          juce::dontSendNotification);
    ampDecKnob      .setValue (modDenorm (ModTarget::AmpDecay, proc.getVoice(selectedChannel).transmod.get(ModTarget::AmpDecay).base), juce::dontSendNotification);
    setTransModKnobProps (ampDecKnob, ModTarget::AmpDecay);

    lfo1RateKnob    .setValue (p.lfo1Rate,         juce::dontSendNotification);
    lfo1DepthKnob   .setValue (p.lfo1Depth,        juce::dontSendNotification);
    lfo1WaveBox     .setSelectedId (int (p.lfo1Wave) + 1, juce::dontSendNotification);
    lfo2RateKnob    .setValue (p.lfo2Rate,         juce::dontSendNotification);
    lfo2DepthKnob   .setValue (p.lfo2Depth,        juce::dontSendNotification);
    lfo2WaveBox     .setSelectedId (int (p.lfo2Wave) + 1, juce::dontSendNotification);

    fx1TypeBox      .setSelectedId (int (p.fx1Type) + 1, juce::dontSendNotification);
    fx1AmtKnob      .setValue (p.fx1Amount,        juce::dontSendNotification);
    bitDepthKnob    .setValue (p.bitDepth,         juce::dontSendNotification);

    updatingUI = false;
}

// ---------------------------------------------------------------------------
// Connect controls (UI → params)
// ---------------------------------------------------------------------------
void DrumSynthEditor::connectMacros()
{
    macroKnobs[0].onValueChange = [this] { if (!updatingUI) proc.getVoice (selectedChannel).params.pitchHz        = float (macroKnobs[0].getValue()); };
    macroKnobs[1].onValueChange = [this] { if (!updatingUI) proc.getVoice (selectedChannel).params.pitchEnvDepth  = float (macroKnobs[1].getValue()); };
    macroKnobs[2].onValueChange = [this] { if (!updatingUI) proc.getVoice (selectedChannel).params.ampAttack      = float (macroKnobs[2].getValue()); };
    macroKnobs[3].onValueChange = [this] { if (!updatingUI) proc.getVoice (selectedChannel).params.ampDecay       = float (macroKnobs[3].getValue()); };
    macroKnobs[4].onValueChange = [this] { if (!updatingUI) proc.getVoice (selectedChannel).params.outputGain     = float (macroKnobs[4].getValue()); };
    macroKnobs[5].onValueChange = [this] { if (!updatingUI) proc.getVoice (selectedChannel).params.noiseLevel     = float (macroKnobs[5].getValue()); };
    macroKnobs[6].onValueChange = [this] { if (!updatingUI) proc.getVoice (selectedChannel).params.filterCutoff   = float (macroKnobs[6].getValue()); };
    macroKnobs[7].onValueChange = [this] { if (!updatingUI) proc.getVoice (selectedChannel).params.filterResonance= float (macroKnobs[7].getValue()); };
}

void DrumSynthEditor::connectAdvancedControls()
{
    using VP = DrumSynth::VoiceParams;

    pitchKnob       .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.pitchHz = float(pitchKnob.getValue()); };
    oscShapeKnob    .onValueChange = [this] {
        if (updatingUI) return;
        proc.getVoice(selectedChannel).params.oscShape = float(oscShapeKnob.getValue());
        oscShapeDisplay.setShape (float(oscShapeKnob.getValue()),
                                  metallicBtn.getToggleState(),
                                  shaperEnabledBtn.getToggleState());
    };
    metallicBtn     .onClick = [this] {
        if (updatingUI) return;
        bool m = metallicBtn.getToggleState();
        proc.getVoice(selectedChannel).params.metallic = m;
        oscShapeKnob.setEnabled (!m && !shaperEnabledBtn.getToggleState());
        oscShapeDisplay.setShape (float(oscShapeKnob.getValue()), m,
                                  shaperEnabledBtn.getToggleState());
    };
    shaperEnabledBtn.onClick = [this] {
        if (updatingUI) return;
        bool sh = shaperEnabledBtn.getToggleState();
        proc.getVoice(selectedChannel).params.shaperEnabled = sh;
        oscShapeKnob.setEnabled (!sh && !metallicBtn.getToggleState());
        oscShapeDisplay.setShape (float(oscShapeKnob.getValue()),
                                  metallicBtn.getToggleState(), sh);
    };
    partPeakKnob    .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.partialPeak    = float(partPeakKnob.getValue()); };
    partSpaceKnob   .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.partialSpace   = float(partSpaceKnob.getValue()); };
    partRollKnob    .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.partialRoll    = float(partRollKnob.getValue()); };
    partDecKnob     .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.partialDecay   = float(partDecKnob.getValue()); };
    membraneBtn     .onClick       = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.membraneMode   = membraneBtn.getToggleState(); };

    noiseLevelKnob.onValueChange = [this] {
        if (updatingUI) return;
        auto& voice = proc.getVoice (selectedChannel);
        auto& tm = voice.transmod.get (ModTarget::NoiseLevel);
        float n = modNorm (ModTarget::NoiseLevel, float (noiseLevelKnob.getValue()));
        if (activeModSource < 0) { tm.base = n; voice.params.noiseLevel = float (noiseLevelKnob.getValue()); }
        else                     { tm.depths[activeModSource] = juce::jlimit (-1.f, 1.f, n - tm.base); }
        setTransModKnobProps (noiseLevelKnob, ModTarget::NoiseLevel);
    };
    noiseDecKnob    .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.noiseDecay     = float(noiseDecKnob.getValue()); };
    noisePinkBtn    .onClick       = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.noiseColor     = noisePinkBtn.getToggleState() ? VP::NoiseColor::Pink : VP::NoiseColor::White; };
    noiseBPFreqKnob .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.noiseBPFreq   = float(noiseBPFreqKnob.getValue()); };
    noiseBPQKnob    .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.noiseBPQ      = float(noiseBPQKnob.getValue()); };

    driveAmtKnob.onValueChange = [this] {
        if (updatingUI) return;
        auto& voice = proc.getVoice (selectedChannel);
        auto& tm = voice.transmod.get (ModTarget::DriveAmount);
        float n = modNorm (ModTarget::DriveAmount, float (driveAmtKnob.getValue()));
        if (activeModSource < 0) { tm.base = n; voice.params.driveAmount = float (driveAmtKnob.getValue()); }
        else                     { tm.depths[activeModSource] = juce::jlimit (-1.f, 1.f, n - tm.base); }
        setTransModKnobProps (driveAmtKnob, ModTarget::DriveAmount);
    };
    driveTypeBox    .onChange      = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.driveType     = VP::DriveType(driveTypeBox.getSelectedId() - 1); };

    filterModeBox   .onChange      = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.filterMode    = VP::FilterMode(filterModeBox.getSelectedId() - 1); };
    filterModelBox  .onChange      = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.filterModel   = VP::FilterModel(filterModelBox.getSelectedId() - 1); };
    filter4PoleBtn  .onClick       = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.filterFourPole= filter4PoleBtn.getToggleState(); };
    filterCutKnob.onValueChange = [this] {
        if (updatingUI) return;
        auto& voice = proc.getVoice (selectedChannel);
        auto& tm = voice.transmod.get (ModTarget::FilterCutoff);
        float n = modNorm (ModTarget::FilterCutoff, float (filterCutKnob.getValue()));
        if (activeModSource < 0) { tm.base = n; voice.params.filterCutoff = float (filterCutKnob.getValue()); }
        else                     { tm.depths[activeModSource] = juce::jlimit (-1.f, 1.f, n - tm.base); }
        setTransModKnobProps (filterCutKnob, ModTarget::FilterCutoff);
    };
    filterResKnob.onValueChange = [this] {
        if (updatingUI) return;
        auto& voice = proc.getVoice (selectedChannel);
        auto& tm = voice.transmod.get (ModTarget::FilterResonance);
        float n = modNorm (ModTarget::FilterResonance, float (filterResKnob.getValue()));
        if (activeModSource < 0) { tm.base = n; voice.params.filterResonance = float (filterResKnob.getValue()); }
        else                     { tm.depths[activeModSource] = juce::jlimit (-1.f, 1.f, n - tm.base); }
        setTransModKnobProps (filterResKnob, ModTarget::FilterResonance);
    };

    pEnvDepthKnob   .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.pitchEnvDepth  = float(pEnvDepthKnob.getValue()); };
    pEnvDecKnob     .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.pitchEnvDecay  = float(pEnvDecKnob.getValue()); };
    fEnvAttKnob     .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.filterEnvAttack= float(fEnvAttKnob.getValue()); };
    fEnvHoldKnob    .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.filterEnvHold  = float(fEnvHoldKnob.getValue()); };
    fEnvDecKnob     .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.filterEnvDecay = float(fEnvDecKnob.getValue()); };
    fEnvDepthKnob   .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.filterEnvDepth = float(fEnvDepthKnob.getValue()); };
    ampAttKnob      .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.ampAttack      = float(ampAttKnob.getValue()); };
    ampHoldKnob     .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.ampHold        = float(ampHoldKnob.getValue()); };
    ampDecKnob.onValueChange = [this] {
        if (updatingUI) return;
        auto& voice = proc.getVoice (selectedChannel);
        auto& tm = voice.transmod.get (ModTarget::AmpDecay);
        float n = modNorm (ModTarget::AmpDecay, float (ampDecKnob.getValue()));
        if (activeModSource < 0) { tm.base = n; voice.params.ampDecay = float (ampDecKnob.getValue()); }
        else                     { tm.depths[activeModSource] = juce::jlimit (-1.f, 1.f, n - tm.base); }
        setTransModKnobProps (ampDecKnob, ModTarget::AmpDecay);
    };

    lfo1RateKnob    .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.lfo1Rate      = float(lfo1RateKnob.getValue()); };
    lfo1DepthKnob   .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.lfo1Depth     = float(lfo1DepthKnob.getValue()); };
    lfo1WaveBox     .onChange      = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.lfo1Wave      = VP::LfoWave(lfo1WaveBox.getSelectedId() - 1); };
    lfo2RateKnob    .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.lfo2Rate      = float(lfo2RateKnob.getValue()); };
    lfo2DepthKnob   .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.lfo2Depth     = float(lfo2DepthKnob.getValue()); };
    lfo2WaveBox     .onChange      = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.lfo2Wave      = VP::LfoWave(lfo2WaveBox.getSelectedId() - 1); };

    fx1TypeBox      .onChange      = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.fx1Type       = VP::FxDistType(fx1TypeBox.getSelectedId() - 1); };
    fx1AmtKnob      .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.fx1Amount     = float(fx1AmtKnob.getValue()); };
    bitDepthKnob    .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.bitDepth      = float(bitDepthKnob.getValue()); };
}

// ---------------------------------------------------------------------------
// TransMod helpers
// ---------------------------------------------------------------------------
void DrumSynthEditor::setTransModKnobProps (juce::Slider& s, ModTarget t)
{
    const auto& tm  = proc.getVoice (selectedChannel).transmod.get (t);
    bool        act = (activeModSource >= 0);
    float       dep = act ? tm.depths[activeModSource] : 0.f;
    float       eff = juce::jlimit (0.f, 1.f, tm.base + dep);

    s.getProperties().set ("tmActive", act);
    s.getProperties().set ("tmBase",   static_cast<double> (tm.base));
    s.getProperties().set ("tmColor",  act
                            ? static_cast<int> (modSrcColour (activeModSource).getARGB())
                            : static_cast<int> (0xffccccdd));

    updatingUI = true;
    s.setValue (modDenorm (t, eff), juce::dontSendNotification);
    updatingUI = false;
    s.repaint();
}

void DrumSynthEditor::updateTransModUI()
{
    for (int s = 0; s < kNumModSources; ++s)
    {
        bool sel = (activeModSource == s);
        modSrcBtns[size_t (s)].setColour (juce::TextButton::buttonColourId,
                                           sel ? modSrcColour (s) : kPadActive);
        modSrcBtns[size_t (s)].setColour (juce::TextButton::textColourOffId,
                                           sel ? juce::Colours::black : kText);
    }

    if (!advancedMode) return;

    setTransModKnobProps (filterCutKnob,  ModTarget::FilterCutoff);
    setTransModKnobProps (filterResKnob,  ModTarget::FilterResonance);
    setTransModKnobProps (ampDecKnob,     ModTarget::AmpDecay);
    setTransModKnobProps (noiseLevelKnob, ModTarget::NoiseLevel);
    setTransModKnobProps (driveAmtKnob,   ModTarget::DriveAmount);
}
