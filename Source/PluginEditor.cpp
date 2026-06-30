#include "PluginEditor.h"

static const char* kChanNames[DrumSynth::NumChannels] =
    { "Kick", "Snare", "Clap", "Tom 1", "Tom 2", "Closed Hat", "Open Hat", "Cymbal" };

// Colour palette — one blue family applied across the whole plugin, each
// other colour derived as a relative shade of kBg so the theme stays
// cohesive instead of mixing in unrelated hardcoded darks.
static const juce::Colour kBg        { 0xffacc6e2 };          // a shade lighter
static const juce::Colour kHeaderBg  { kBg.darker (0.28f) };  // header strip — a shade darker than before
static const juce::Colour kPanel     { kBg.darker (0.65f) };  // channel bar / combo / button backgrounds
static const juce::Colour kPanelLine { kBg.darker (0.72f) };  // separators
static const juce::Colour kAccent    { 0xff4f8ef7 };
static const juce::Colour kKnobArc   { 0xff4f8ef7 };
static const juce::Colour kText      { 0xfff2f2ff };   // near-white — for text on dark surfaces
static const juce::Colour kTextDark  { 0xff1a1a2e };   // dark navy — for text directly on kBg
static const juce::Colour kDim       { 0xff3a4154 };   // dark slate — for dim text directly on kBg
static const juce::Colour kPadActive { kBg.darker (0.5f) };
static const juce::Colour kPadSel    { 0xff4f8ef7 };

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
DrumSynthEditor::DrumSynthEditor (DrumSynthProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      viewToggleBtn ("Advanced")
{
    setLookAndFeel (&lnf);
    lnf.setDefaultSansSerifTypefaceName ("Helvetica Neue");
    setSize (kBasicW(), kBasicH());

    // Populate lfo wave and other combo items now so they're ready before connect
    setupCombo (lfo1WaveBox,   { "Sine", "Triangle", "Square", "Saw Up", "Saw Down" });
    setupCombo (lfo2WaveBox,   { "Sine", "Triangle", "Square", "Saw Up", "Saw Down" });
    setupCombo (driveTypeBox,  { "Soft Clip", "Hard Clip", "Wavefold" });
    setupCombo (filterModeBox, { "LP", "HP", "BP", "Peak", "Notch" });
    setupCombo (filterModelBox,{ "Clean", "Fat" });
    setupCombo (fx1TypeBox,    { "Off", "Soft Clip", "Hard Clip", "Bitcrusher", "Wavefold" });
    setupCombo (oscModeBox,    { "Single", "Metallic Cluster", "Partial Shaper", "Resonator" });

    // Knob ranges
    setupKnob (pitchKnob,       20.0, 20000.0, 80.0, " Hz");
    pitchKnob.setSkewFactorFromMidPoint (300.0);   // 300Hz at 12 o'clock, 20-20000 either side
    setupKnob (oscShapeKnob,    0.0,  1.0,  0.0);
    oscShapeKnob.textFromValueFunction = [] (double v) -> juce::String {
        if (v < 0.04)               return "Sine";
        if (v > 0.96)               return "Square";
        if (v > 0.295 && v < 0.37) return "Triangle";
        if (v > 0.63  && v < 0.705) return "Saw";
        if (v < 1.0 / 3.0)           return "Sine-Tri";
        if (v < 2.0 / 3.0)           return "Tri-Saw";
        return "Saw-Sq";
    };
    // setValue above ran before the formatter existed and left the text box
    // showing the raw number; force a real change now to refresh it
    oscShapeKnob.setValue (1.0, juce::dontSendNotification);
    oscShapeKnob.setValue (0.0, juce::dontSendNotification);
    setupKnob (partPeakKnob,    1.0,  8.0,  1.0);
    setupKnob (partSpaceKnob,   0.0,  1.0,  0.5);
    setupKnob (partRollKnob,    0.0,  1.0,  0.5);
    setupKnob (partDecKnob,     0.0,  1.0,  0.5);
    setupKnob (ringDecayKnob,   0.01, 2.0,  0.3, " s");
    setupKnob (noiseLevelKnob,  0.0,  1.0,  0.0);
    setupKnob (noiseDecKnob,    0.1,  2.0,  0.1);
    setupKnob (noiseBPFreqKnob, 100.0,20000.0, 8000.0, " Hz");
    setupKnob (noiseBPQKnob,    0.1,  10.0, 0.7);
    setupKnob (driveAmtKnob,    0.0,  1.0,  0.0);
    setupKnob (filterCutKnob,   20.0, 20000.0, 12000.0, " Hz");
    filterCutKnob.setSkewFactorFromMidPoint (1000.0);   // 1000Hz at 12 o'clock
    setupKnob (filterResKnob,   0.0,  1.0,  0.5);
    setupVSlider (oscLevelSlider,     0.0, 1.0, 1.0);
    setupVSlider (noiseMixGainSlider, 0.0, 1.0, 1.0);
    setupVSlider (pcmLevelSlider,     0.0, 1.0, 0.0);
    pcmLevelSlider.setEnabled (false);   // placeholder: PCM layer not implemented yet
    setupKnob (env1AttKnob,     0.001,1.0,  0.005," s");
    setupKnob (env1HoldKnob,    0.0,  1.0,  0.0,  " s");
    setupKnob (env1DecKnob,     0.1,  2.0,  0.1,  " s");
    setupKnob (env2AttKnob,     0.001,2.0,  0.005," s");
    setupKnob (env2HoldKnob,    0.0,  2.0,  0.0,  " s");
    setupKnob (env2DecKnob,     0.1,  4.0,  0.3,  " s");
    setupKnob (ampAttKnob,      0.001,1.0,  0.002," s");
    setupKnob (ampHoldKnob,     0.0,  1.0,  0.0,  " s");
    setupKnob (ampDecKnob,      0.1,  4.0,  0.5,  " s");
    setupKnob (lfo1RateKnob,    0.1,  10.0, 1.0, " Hz");
    setupKnob (lfo2RateKnob,    0.1,  10.0, 1.0, " Hz");
    setupKnob (fx1AmtKnob,      0.0,  1.0,  0.5);
    setupKnob (bitDepthKnob,    1.0,  24.0, 8.0,  " bit");
    setupKnob (masterVolKnob,   0.0,  1.5,  1.0);

    for (auto& k : macroKnobs)
    {
        k.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
        k.setColour (juce::Slider::rotarySliderOutlineColourId, kPanelLine);
    }
    setupKnob (macroKnobs[0], 20.0,    20000.0, 80.0,  " Hz");  // Tune
    macroKnobs[0].setSkewFactorFromMidPoint (300.0);
    setupKnob (macroKnobs[1],  0.1,    2.0,     0.1,   " s");   // Env 1 decay
    setupKnob (macroKnobs[2],  0.001,  1.0,     0.002, " s");   // Attack
    setupKnob (macroKnobs[3],  0.1,    4.0,     0.5,   " s");   // Decay
    setupKnob (macroKnobs[4],  0.0,    1.0,     0.8);            // Volume
    setupKnob (macroKnobs[5],  0.0,    1.0,     0.0);            // Noise
    setupKnob (macroKnobs[6],  20.0,   20000.0, 12000.0," Hz"); // Flt Cut
    macroKnobs[6].setSkewFactorFromMidPoint (1000.0);
    setupKnob (macroKnobs[7],  0.0,    1.0,     0.5);            // Resonance

    buildBasicView();
    buildAdvancedView();
    showAdvancedView();
    connectAdvancedControls();
    refreshAdvanced();

    startTimerHz (30);   // animates TransMod outer rings
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

void DrumSynthEditor::setupVSlider (juce::Slider& s, double lo, double hi, double def)
{
    s.setSliderStyle (juce::Slider::LinearVertical);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
    s.setNumDecimalPlacesToDisplay (2);
    s.setRange (lo, hi);
    s.setValue (def, juce::dontSendNotification);
    s.setColour (juce::Slider::textBoxTextColourId,       kDim);
    s.setColour (juce::Slider::textBoxBackgroundColourId, kBg);
    s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
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
        macroLabels[size_t (i)].setFont (juce::FontOptions (20.0f));
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
    addChildComponent (oscModeBox);
    addChildComponent (oscShapeKnob);
    addChildComponent (oscShapeDisplay);
    addChildComponent (partPeakKnob);
    addChildComponent (partSpaceKnob);
    addChildComponent (partRollKnob);
    addChildComponent (partDecKnob);
    addChildComponent (membraneBtn);
    addChildComponent (ringDecayKnob);

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

    // MIXER (separate section below Filter, same column)
    addChildComponent (oscLevelSlider);
    addChildComponent (noiseMixGainSlider);
    addChildComponent (pcmLevelSlider);

    // ENVELOPES
    addChildComponent (env1AttKnob);
    addChildComponent (env1HoldKnob);
    addChildComponent (env1DecKnob);
    addChildComponent (env2AttKnob);
    addChildComponent (env2HoldKnob);
    addChildComponent (env2DecKnob);
    addChildComponent (ampAttKnob);
    addChildComponent (ampHoldKnob);
    addChildComponent (ampDecKnob);
    addChildComponent (env1Hdr);
    addChildComponent (env2Hdr);
    addChildComponent (ampHdr);

    // LFO
    addChildComponent (lfo1RateKnob);
    addChildComponent (lfo1WaveBox);
    addChildComponent (lfo2RateKnob);
    addChildComponent (lfo2WaveBox);
    addChildComponent (lfo1Hdr);
    addChildComponent (lfo2Hdr);

    // FX
    addChildComponent (fx1TypeBox);
    addChildComponent (fx1AmtKnob);
    addChildComponent (bitDepthKnob);

    // MASTER
    addChildComponent (masterVolKnob);
    addChildComponent (limiterBtn);
    addChildComponent (masterMeter);
    addChildComponent (masterHdr);
    addChildComponent (playLoopBtn);

    // Per-knob labels for advanced view
    static const char* kAdvLblText[31] = {
        "Pitch", "Shape", "Peak", "Space", "Roll", "Decay", // OSC 0-5
        "Level", "Decay", "BP Freq", "BP Q",                // NOISE 6-9
        "Amount",                                            // DRIVE 10
        "Cutoff", "Res",                                     // FILTER 11-12
        "Attack", "Hold", "Decay",                          // ENV 1 13-15
        "Attack", "Hold", "Decay",                          // ENV 2 16-18
        "Attack", "Hold", "Decay",                          // AMP 19-21
        "Rate", "Rate",                                      // LFO1/2 22-23
        "Amount", "Bit Dep",                                 // FX 24-25
        "Vol",                                                // MASTER 26
        "Osc", "Noise", "PCM",                                // MIXER 27-29
        "Ring"                                                // OSC (Resonator only) 30
    };
    for (int i = 0; i < 31; ++i)
    {
        advLbl[i].setText (kAdvLblText[i], juce::dontSendNotification);
        advLbl[i].setJustificationType (juce::Justification::centred);
        advLbl[i].setFont (juce::FontOptions (15.0f));
        advLbl[i].setColour (juce::Label::textColourId, kTextDark);
        addChildComponent (advLbl[i]);
    }

    // Style toggles
    for (auto* btn : { &membraneBtn, &noisePinkBtn, &filter4PoleBtn })
    {
        btn->setColour (juce::ToggleButton::textColourId,      kTextDark);
        btn->setColour (juce::ToggleButton::tickColourId,      juce::Colours::white);
        btn->setColour (juce::ToggleButton::tickDisabledColourId, kDim);
    }

    // Sub-section headers — coloured to match their TransMod source where
    // applicable (Env1/Env2/LFO1/LFO2 are sources; Amp is not)
    auto styleHdr = [] (juce::Label& l, juce::Colour c)
    {
        l.setJustificationType (juce::Justification::centredLeft);
        l.setFont (juce::FontOptions (17.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, c);
    };
    styleHdr (env1Hdr, DrumSynthLookAndFeel::sourceColour (2));
    styleHdr (env2Hdr, DrumSynthLookAndFeel::sourceColour (3));
    styleHdr (ampHdr,  kTextDark);
    styleHdr (lfo1Hdr, DrumSynthLookAndFeel::sourceColour (0));
    styleHdr (lfo2Hdr, DrumSynthLookAndFeel::sourceColour (1));
    styleHdr (masterHdr, kTextDark);
    limiterBtn.setColour (juce::ToggleButton::textColourId,         kTextDark);
    limiterBtn.setColour (juce::ToggleButton::tickColourId,         juce::Colours::white);
    limiterBtn.setColour (juce::ToggleButton::tickDisabledColourId, kDim);

    playLoopBtn.setClickingTogglesState (true);
    playLoopBtn.setColour (juce::TextButton::buttonColourId, kPadActive);
    playLoopBtn.setColour (juce::TextButton::buttonOnColourId, kAccent);
    playLoopBtn.setColour (juce::TextButton::textColourOffId, kText);
    playLoopBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    playLoopBtn.onClick = [this]
    {
        playLoopOn = playLoopBtn.getToggleState();
        if (playLoopOn)
        {
            proc.getVoice (selectedChannel).trigger (1.0f);
            lastPlayLoopMs = juce::Time::getMillisecondCounter();
        }
    };

    // TransMod source selection buttons
    static const char* kSrcNames[kNumModSources] = { "LFO 1", "LFO 2", "Env 1", "Env 2", "Vel" };
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

    // Every continuous parameter is a TransMod target. Build the registry
    // (advanced knobs + basic macro knobs, several sharing the same
    // underlying target) and wire them all the same way: turning a knob
    // while a source is focused sets that source's depth; double-clicking
    // a knob while focused clears it (removes the routing).
    modKnobs = {
        { &pitchKnob,       ModTarget::PitchHz },
        { &oscShapeKnob,    ModTarget::OscShape },
        { &partPeakKnob,    ModTarget::PartialPeak },
        { &partSpaceKnob,   ModTarget::PartialSpace },
        { &partRollKnob,    ModTarget::PartialRoll },
        { &partDecKnob,     ModTarget::PartialDecay },
        { &ringDecayKnob,   ModTarget::RingDecay },
        { &env1AttKnob,     ModTarget::Env1Attack },
        { &env1HoldKnob,    ModTarget::Env1Hold },
        { &env1DecKnob,     ModTarget::Env1Decay },
        { &noiseLevelKnob,  ModTarget::NoiseLevel },
        { &noiseDecKnob,    ModTarget::NoiseDecay },
        { &noiseBPFreqKnob, ModTarget::NoiseBPFreq },
        { &noiseBPQKnob,    ModTarget::NoiseBPQ },
        { &driveAmtKnob,    ModTarget::DriveAmount },
        { &filterCutKnob,   ModTarget::FilterCutoff },
        { &filterResKnob,   ModTarget::FilterResonance },
        { &env2AttKnob,     ModTarget::Env2Attack },
        { &env2HoldKnob,    ModTarget::Env2Hold },
        { &env2DecKnob,     ModTarget::Env2Decay },
        { &ampAttKnob,      ModTarget::AmpAttack },
        { &ampHoldKnob,     ModTarget::AmpHold },
        { &ampDecKnob,      ModTarget::AmpDecay },
        { &lfo1RateKnob,    ModTarget::Lfo1Rate },
        { &lfo2RateKnob,    ModTarget::Lfo2Rate },
        { &fx1AmtKnob,      ModTarget::Fx1Amount },
        { &bitDepthKnob,    ModTarget::BitDepth },
        { &macroKnobs[0],   ModTarget::PitchHz,         true },
        { &macroKnobs[1],   ModTarget::Env1Decay,       true },
        { &macroKnobs[2],   ModTarget::AmpAttack,       true },
        { &macroKnobs[3],   ModTarget::AmpDecay,        true },
        { &macroKnobs[4],   ModTarget::OutputGain,      true },
        { &macroKnobs[5],   ModTarget::NoiseLevel,      true },
        { &macroKnobs[6],   ModTarget::FilterCutoff,    true },
        { &macroKnobs[7],   ModTarget::FilterResonance, true },
    };

    for (auto& entry : modKnobs)
    {
        connectModKnob (*entry.slider, entry.target);
        entry.slider->addMouseListener (this, false);
    }
}

// ---------------------------------------------------------------------------
// Show / hide views
// ---------------------------------------------------------------------------
void DrumSynthEditor::showBasicView()
{
    advancedMode = false;
    setSize (kBasicW(), kBasicH());

    // Basic view has no source-select UI; leaving focus mode active would
    // make turning a macro knob silently edit a hidden depth instead of
    // the base value
    activeModSource = -1;

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
                     &pitchKnob, &oscModeBox, &oscShapeKnob, &oscShapeDisplay,
                     &partPeakKnob, &partSpaceKnob, &partRollKnob, &partDecKnob, &membraneBtn, &ringDecayKnob,
                     &noiseDecKnob, &noiseBPFreqKnob, &noiseBPQKnob, &noisePinkBtn,
                     &driveAmtKnob, &driveTypeBox,
                     &filterModeBox, &filterModelBox, &filter4PoleBtn, &filterCutKnob, &filterResKnob, &noiseLevelKnob,
                     &oscLevelSlider, &noiseMixGainSlider, &pcmLevelSlider,
                     &env1AttKnob, &env1HoldKnob, &env1DecKnob,
                     &env2AttKnob, &env2HoldKnob, &env2DecKnob,
                     &env1Hdr, &env2Hdr, &ampHdr, &ampAttKnob, &ampHoldKnob, &ampDecKnob,
                     &lfo1Hdr, &lfo2Hdr,
                     &lfo1RateKnob, &lfo1WaveBox,
                     &lfo2RateKnob, &lfo2WaveBox,
                     &fx1TypeBox, &fx1AmtKnob, &bitDepthKnob,
                     &masterVolKnob, &limiterBtn, &masterMeter, &masterHdr, &playLoopBtn })
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
                     &pitchKnob, &oscModeBox, &oscShapeKnob, &oscShapeDisplay,
                     &partPeakKnob, &partSpaceKnob, &partRollKnob, &partDecKnob, &membraneBtn, &ringDecayKnob,
                     &noiseDecKnob, &noiseBPFreqKnob, &noiseBPQKnob, &noisePinkBtn,
                     &driveAmtKnob, &driveTypeBox,
                     &filterModeBox, &filterModelBox, &filter4PoleBtn, &filterCutKnob, &filterResKnob, &noiseLevelKnob,
                     &oscLevelSlider, &noiseMixGainSlider, &pcmLevelSlider,
                     &env1AttKnob, &env1HoldKnob, &env1DecKnob,
                     &env2AttKnob, &env2HoldKnob, &env2DecKnob,
                     &env1Hdr, &env2Hdr, &ampHdr, &ampAttKnob, &ampHoldKnob, &ampDecKnob,
                     &lfo1Hdr, &lfo2Hdr,
                     &lfo1RateKnob, &lfo1WaveBox,
                     &lfo2RateKnob, &lfo2WaveBox,
                     &fx1TypeBox, &fx1AmtKnob, &bitDepthKnob,
                     &masterVolKnob, &limiterBtn, &masterMeter, &masterHdr, &playLoopBtn })
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

    // 4×2 macro grid — each cell: 74px rotary (+12 reserved for the
    // TransMod outer ring) + 28px text box + 26px name label
    const int knobW  = 74;
    const int knobH  = 74 + 28;
    const int cellW  = kPadSize + kPadGap;   // 90px — matches pad column width
    const int rowH   = knobH + 4 + 26;
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
        const int btnW = 86;
        const int btnH = 44;
        for (int s = 0; s < kNumModSources; ++s)
            modSrcBtns[size_t (s)].setBounds (tmX + s * (btnW + 8), tmY, btnW, btnH);
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

        oscModeBox      .setBounds (x, y, kOscW - 14, 26);
        y += 34;

        lbl (0, x,         y + 22);  lbl (1, x + C + G, y + 22);  lbl (30, x + C + G, y + 22);
        pitchKnob       .setBounds (x,         y + 22, C, C);
        oscShapeKnob    .setBounds (x + C + G, y + 22, C, C);
        // Shares the Shape knob's slot — mutually exclusive (Shape: Single
        // only, Ring: Resonator only)
        ringDecayKnob   .setBounds (x + C + G, y + 22, C, C);
        y += 22 + C + 6;

        // oscShapeDisplay and the Partial Shaper knobs below share this same
        // vertical space rather than stacking — they're mutually exclusive
        // (display only shows in Single mode, these knobs only in Partial
        // Shaper), so there's no case where both are visible at once.
        {
            const int dispW = (kOscW - 16) / 3;
            const int knobRowW = 2 * C + G;   // Pitch + Shape knobs span
            oscShapeDisplay.setBounds (x + (knobRowW - dispW) / 2, y, dispW, 48);
        }

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

    // ===== MIXER (separate section below Filter, same column, divided by
    // a horizontal line + "MIXER" title drawn in paint(); vertical faders;
    // none of these three are TransMod targets) =====
    {
        const int x = kFltX + 8;
        int y = panelY + kFltContentH + 54;   // mirrors the "panelY + 54" every panel uses below its own title

        const int sliderW = 50, sliderGap = 15, sliderH = 200;
        advLbl[27].setBounds (x,                             y, sliderW, 16);   // Osc
        advLbl[28].setBounds (x + sliderW + sliderGap,       y, sliderW, 16);   // Noise
        advLbl[29].setBounds (x + 2 * (sliderW + sliderGap), y, sliderW, 16);   // PCM
        y += 20;
        oscLevelSlider    .setBounds (x,                             y, sliderW, sliderH);
        noiseMixGainSlider.setBounds (x + sliderW + sliderGap,       y, sliderW, sliderH);
        pcmLevelSlider    .setBounds (x + 2 * (sliderW + sliderGap), y, sliderW, sliderH);
    }

    // ===== ENVELOPES (Env1/Env2 general purpose, Amp hardwired) =====
    {
        const int x = kEnvX + 8;
        int y = panelY + 16;

        env1Hdr.setBounds (x, y, kEnvW - 16, 20);
        y += 24;
        lbl (13, x,             y + 22);
        lbl (14, x + C + G,     y + 22);
        lbl (15, x + (C + G)*2, y + 22);
        env1AttKnob.setBounds (x,             y + 22, C, C);
        env1HoldKnob.setBounds (x + C + G,     y + 22, C, C);
        env1DecKnob.setBounds (x + (C + G)*2, y + 22, C, C);
        y += 22 + C + 16;

        env2Hdr.setBounds (x, y, kEnvW - 16, 20);
        y += 24;
        lbl (16, x,             y + 22);
        lbl (17, x + C + G,     y + 22);
        lbl (18, x + (C + G)*2, y + 22);
        env2AttKnob.setBounds (x,             y + 22, C, C);
        env2HoldKnob.setBounds (x + C + G,     y + 22, C, C);
        env2DecKnob.setBounds (x + (C + G)*2, y + 22, C, C);
        y += 22 + C + 16;

        ampHdr.setBounds (x, y, kEnvW - 16, 20);
        y += 24;
        lbl (19, x,             y + 22);
        lbl (20, x + C + G,     y + 22);
        lbl (21, x + (C + G)*2, y + 22);
        ampAttKnob .setBounds (x,             y + 22, C, C);
        ampHoldKnob.setBounds (x + C + G,     y + 22, C, C);
        ampDecKnob .setBounds (x + (C + G)*2, y + 22, C, C);
    }

    // ===== LFO (TransMod sources only) =====
    {
        const int x = kLfoX + 8;
        int y = panelY + 16;

        lfo1Hdr.setBounds (x, y, kLfoW - 16, 20);
        y += 24;
        lbl (22, x, y + 22);
        lfo1RateKnob .setBounds (x, y + 22, C, C);
        y += 22 + C + 6;
        lfo1WaveBox  .setBounds (x, y, kLfoW - 14, 26);
        y += 26 + 24;

        lfo2Hdr.setBounds (x, y, kLfoW - 16, 20);
        y += 24;
        lbl (23, x, y + 22);
        lfo2RateKnob .setBounds (x, y + 22, C, C);
        y += 22 + C + 6;
        lfo2WaveBox  .setBounds (x, y, kLfoW - 14, 26);
    }

    // ===== FX strip =====
    {
        const int sepY = kAdvH - kFxH;
        const int fxY  = sepY + 22;   // clears the divider line + label row above it
        fx1TypeBox  .setBounds (8, sepY + 40, 160, 26);   // clears the "FX" title above it
        lbl (24, 174,           fxY);
        lbl (25, 174 + C + G,   fxY);
        fx1AmtKnob  .setBounds (174,         fxY, C, C);
        bitDepthKnob.setBounds (174 + C + G, fxY, C, C);

        // ===== MASTER (right side of the FX strip) =====
        const int mx = 420;
        masterHdr .setBounds (mx, sepY + 12, 64, 18);
        limiterBtn.setBounds (mx + 70, fxY + (C - 22) / 2, 80, 22);
        lbl (26, mx + 160, fxY);
        masterVolKnob.setBounds (mx + 160,         fxY, C, C);
        masterMeter  .setBounds (mx + 160 + C + G, fxY, 28, C);
        playLoopBtn  .setBounds (mx + 160 + C + G + 28 + 16, fxY + (C - 26) / 2, 64, 26);
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

    // Header strip — one shade darker than the main background, in both views
    g.setColour (kHeaderBg);
    g.fillRect (0, 0, getWidth(), kHdrH);

    // Title
    g.setColour (kTextDark);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
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
            if (title.isNotEmpty())
            {
                g.setColour (kTextDark);
                g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
                g.drawText (title, x + 3, kAdvTop + 2, w - 4, 30,
                            juce::Justification::centredLeft, false);
            }
        };
        drawPanel (kOscX,  kOscW,  "OSCILLATOR");
        drawPanel (kNsX,   kNsW,   "NOISE");
        drawPanel (kDrvX,  kDrvW,  "DRIVE");
        drawPanel (kFltX,  kFltW,  "FILTER");
        drawPanel (kEnvX,  kEnvW,  {});   // sub-headers (ENV 1/ENV 2/AMP) drawn as labels instead
        drawPanel (kLfoX,  kLfoW,  {});   // sub-headers (LFO 1/LFO 2) drawn as labels instead

        // MIXER — a totally separate section from Filter (same column),
        // divided by a horizontal line rather than the vertical lines used
        // between adjacent columns, with a title in the same style as
        // OSCILLATOR/NOISE/DRIVE/FILTER above.
        {
            const int mixDividerY = kAdvTop + kFltContentH;
            g.setColour (kPanelLine);
            g.drawHorizontalLine (mixDividerY, float (kFltX), float (kFltX + kFltW));
            g.setColour (kTextDark);
            g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
            g.drawText ("MIXER", kFltX + 3, mixDividerY + 2, kFltW - 4, 30,
                        juce::Justification::centredLeft, false);
        }

        // FX strip separator
        g.setColour (kPanelLine);
        g.drawHorizontalLine (kAdvH - kFxH, 0.0f, float (kAdvW));
        g.setColour (kTextDark);
        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
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
    for (auto& entry : modKnobs)
        if (entry.isMacro)
            setTransModKnobProps (*entry.slider, entry.target);
}

void DrumSynthEditor::refreshAdvanced()
{
    updatingUI = true;
    const auto& p = proc.getVoice (selectedChannel).params;

    oscModeBox      .setSelectedId (int (p.oscMode) + 1, juce::dontSendNotification);
    membraneBtn     .setToggleState (p.membraneMode, juce::dontSendNotification);
    noisePinkBtn    .setToggleState (p.noiseColor == DrumSynth::VoiceParams::NoiseColor::Pink,
                                     juce::dontSendNotification);
    driveTypeBox    .setSelectedId (int (p.driveType)  + 1, juce::dontSendNotification);
    filterModeBox   .setSelectedId (int (p.filterMode) + 1, juce::dontSendNotification);
    filterModelBox  .setSelectedId (int (p.filterModel)+ 1, juce::dontSendNotification);
    filter4PoleBtn  .setToggleState (p.filterFourPole, juce::dontSendNotification);
    lfo1WaveBox     .setSelectedId (int (p.lfo1Wave) + 1, juce::dontSendNotification);
    lfo2WaveBox     .setSelectedId (int (p.lfo2Wave) + 1, juce::dontSendNotification);
    fx1TypeBox      .setSelectedId (int (p.fx1Type)  + 1, juce::dontSendNotification);

    // Mixer faders aren't TransMod targets, so they're not in modKnobs —
    // refresh their displayed value directly from params
    oscLevelSlider    .setValue (p.oscLevel,     juce::dontSendNotification);
    noiseMixGainSlider.setValue (p.noiseMixGain, juce::dontSendNotification);

    // Every continuous knob's value + TransMod arc state comes from modKnobs
    for (auto& entry : modKnobs)
        if (!entry.isMacro)
            setTransModKnobProps (*entry.slider, entry.target);

    updateOscModeVisibility();

    updatingUI = false;
}

void DrumSynthEditor::updateOscModeVisibility()
{
    if (!advancedMode) return;

    using OM = DrumSynth::VoiceParams::OscMode;
    auto mode = proc.getVoice (selectedChannel).params.oscMode;
    bool single    = (mode == OM::Single);
    bool shaper    = (mode == OM::PartialShaper);
    bool resonator = (mode == OM::Resonator);

    oscShapeKnob.setVisible (single);
    oscShapeDisplay.setVisible (single);   // waveform graphic only means anything in Single mode
    advLbl[1].setVisible (single);   // "Shape"

    for (auto* c : std::initializer_list<juce::Component*> {
                     &partPeakKnob, &partSpaceKnob, &partRollKnob, &partDecKnob, &membraneBtn })
        c->setVisible (shaper);
    for (int i = 2; i <= 5; ++i) advLbl[i].setVisible (shaper);   // "Peak","Space","Roll","Decay"

    ringDecayKnob.setVisible (resonator);
    advLbl[30].setVisible (resonator);   // "Ring"

    if (single)
        oscShapeDisplay.setShape (float (oscShapeKnob.getValue()));
}

// ---------------------------------------------------------------------------
// Connect controls (UI → params)
// ---------------------------------------------------------------------------
void DrumSynthEditor::connectModKnob (juce::Slider& s, ModTarget t)
{
    s.onValueChange = [this, &s, t]
    {
        if (updatingUI) return;
        auto& tm = proc.getVoice (selectedChannel).transmod.get (t);
        float n  = modNorm (t, float (s.getValue()));
        if (activeModSource < 0) tm.base = n;
        else                     tm.depths[activeModSource] = juce::jlimit (-1.f, 1.f, n - tm.base);
        setTransModKnobProps (s, t);
    };
}

void DrumSynthEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (activeModSource < 0) return;   // double-click only removes a depth while a source is focused

    for (auto& entry : modKnobs)
    {
        if (entry.slider == e.eventComponent)
        {
            proc.getVoice (selectedChannel).transmod.get (entry.target).depths[activeModSource] = 0.0f;
            setTransModKnobProps (*entry.slider, entry.target);
            return;
        }
    }
}

// connectMacros() no longer needed — macro knobs are wired generically via
// modKnobs/connectModKnob in buildAdvancedView()

void DrumSynthEditor::connectAdvancedControls()
{
    using VP = DrumSynth::VoiceParams;

    // oscShapeKnob already has its TransMod onValueChange from connectModKnob;
    // wrap it so the waveform preview also updates on every change
    auto prevShapeChange = oscShapeKnob.onValueChange;
    oscShapeKnob.onValueChange = [this, prevShapeChange] {
        if (prevShapeChange) prevShapeChange();
        if (proc.getVoice(selectedChannel).params.oscMode == VP::OscMode::Single)
            oscShapeDisplay.setShape (float (oscShapeKnob.getValue()));
    };

    oscModeBox.onChange = [this] {
        if (updatingUI) return;
        proc.getVoice(selectedChannel).params.oscMode = VP::OscMode (oscModeBox.getSelectedId() - 1);
        updateOscModeVisibility();
    };
    membraneBtn     .onClick       = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.membraneMode   = membraneBtn.getToggleState(); };
    noisePinkBtn    .onClick       = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.noiseColor     = noisePinkBtn.getToggleState() ? VP::NoiseColor::Pink : VP::NoiseColor::White; };
    driveTypeBox    .onChange      = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.driveType     = VP::DriveType(driveTypeBox.getSelectedId() - 1); };
    filterModeBox   .onChange      = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.filterMode    = VP::FilterMode(filterModeBox.getSelectedId() - 1); };
    filterModelBox  .onChange      = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.filterModel   = VP::FilterModel(filterModelBox.getSelectedId() - 1); };
    filter4PoleBtn  .onClick       = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.filterFourPole= filter4PoleBtn.getToggleState(); };
    lfo1WaveBox     .onChange      = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.lfo1Wave      = VP::LfoWave(lfo1WaveBox.getSelectedId() - 1); };
    lfo2WaveBox     .onChange      = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.lfo2Wave      = VP::LfoWave(lfo2WaveBox.getSelectedId() - 1); };
    fx1TypeBox      .onChange      = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.fx1Type       = VP::FxDistType(fx1TypeBox.getSelectedId() - 1); };

    // Mixer faders — plain per-channel gain stages, deliberately not
    // TransMod targets, so they're wired directly rather than via modKnobs
    oscLevelSlider    .onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.oscLevel     = float (oscLevelSlider.getValue()); };
    noiseMixGainSlider.onValueChange = [this] { if (!updatingUI) proc.getVoice(selectedChannel).params.noiseMixGain = float (noiseMixGainSlider.getValue()); };

    // Master bus — global, not per-channel, so these write straight to the
    // processor rather than going through a voice/TransMod
    masterVolKnob.onValueChange = [this] { if (!updatingUI) proc.masterVolume   = float (masterVolKnob.getValue()); };
    limiterBtn    .onClick      = [this] { if (!updatingUI) proc.limiterEnabled = limiterBtn.getToggleState(); };
    masterVolKnob.setValue (proc.masterVolume,   juce::dontSendNotification);
    limiterBtn   .setToggleState (proc.limiterEnabled, juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
// TransMod helpers
// ---------------------------------------------------------------------------
void DrumSynthEditor::setTransModKnobProps (juce::Slider& s, ModTarget t)
{
    auto&       voice = proc.getVoice (selectedChannel);
    const auto& tm    = voice.transmod.get (t);
    bool        act   = (activeModSource >= 0);
    float       dep   = act ? tm.depths[activeModSource] : 0.f;
    float       eff   = juce::jlimit (0.f, 1.f, tm.base + dep);

    float live[kNumModSources];
    voice.getModSourceValues (live);

    s.getProperties().set ("tmBase", static_cast<double> (tm.base));
    for (int i = 0; i < kNumModSources; ++i)
    {
        s.getProperties().set ("tmDepth" + juce::String (i), static_cast<double> (tm.depths[i]));
        s.getProperties().set ("tmLive"  + juce::String (i), static_cast<double> (live[i]));
    }

    updatingUI = true;
    s.setValue (modDenorm (t, eff), juce::dontSendNotification);
    updatingUI = false;
    s.repaint();
}

void DrumSynthEditor::timerCallback()
{
    masterMeter.setLevel (proc.getMasterPeakLevel());

    if (playLoopOn)
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (now - lastPlayLoopMs >= 1000)
        {
            proc.getVoice (selectedChannel).trigger (1.0f);
            lastPlayLoopMs = now;
        }
    }

    // Only knobs that actually carry a routing need to animate; refreshing
    // (and repainting) all ~36 knobs at 30Hz regardless was needlessly
    // expensive and is the dominant idle CPU cost without this guard.
    const auto& tm = proc.getVoice (selectedChannel).transmod;
    for (auto& entry : modKnobs)
    {
        const auto& p = tm.get (entry.target);
        bool hasDepth = false;
        for (int i = 0; i < kNumModSources; ++i)
            if (std::abs (p.depths[i]) > 0.0005f) { hasDepth = true; break; }
        if (hasDepth)
            setTransModKnobProps (*entry.slider, entry.target);
    }
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

    // Only reachable from modSrcBtns, which are advanced-view-only
    for (auto& entry : modKnobs)
        if (!entry.isMacro)
            setTransModKnobProps (*entry.slider, entry.target);
}
