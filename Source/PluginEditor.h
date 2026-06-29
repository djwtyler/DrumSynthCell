#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "PluginProcessor.h"
#include "TransMod.h"

// ============================================================
//  OscShapeDisplay — live waveform preview for the OSC section
// ============================================================
// Only meaningful in Single oscillator mode; the editor hides this
// component entirely in Metallic Cluster / Partial Shaper modes rather
// than swapping in a placeholder caption.
class OscShapeDisplay : public juce::Component
{
public:
    void setShape (float s)
    {
        shape = s;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);

        g.setColour (juce::Colour (0xff0e0e1c));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (b, 3.0f, 1.0f);

        // Morph: Sine (0) → Triangle (1/3) → Saw (2/3) → Square (1)
        const float s2   = juce::jlimit (0.0f, 0.9999f, shape) * 3.0f;
        const int   idx  = int (s2);
        const float frac = s2 - float (idx);

        const float cx  = b.getX();
        const float cy  = b.getCentreY();
        const float w   = b.getWidth();
        const float amp = b.getHeight() * 0.42f;
        const int   N   = int (w);

        // Draw one cycle; i < N avoids the wrap-around discontinuity at t=1
        juce::Path path;

        for (int i = 0; i < N; ++i)
        {
            const float t = float (i) / float (N);

            const float sine  = std::sin (t * juce::MathConstants<float>::twoPi);
            const float tri   = t < 0.5f ? (4.0f * t - 1.0f) : (3.0f - 4.0f * t);
            const float saw   = 2.0f * t - 1.0f;
            const float sq    = t < 0.5f ? 1.0f : -1.0f;
            const float ws[4] = { sine, tri, saw, sq };
            const float val   = ws[idx] * (1.0f - frac)
                              + ws[idx < 3 ? idx + 1 : 3] * frac;

            const float px = cx + float (i);
            const float py = cy - val * amp;

            // Always connect — the square-wave step is a genuine vertical
            // edge we want drawn, not a wraparound artefact to hide
            if (i == 0) path.startNewSubPath (px, py);
            else        path.lineTo (px, py);
        }

        g.setColour (juce::Colour (0xff3ecfbe));
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }

private:
    float shape = 0.0f;
};

// ============================================================
//  MasterMeter — vertical peak meter for the master bus, with a fast
//  attack / slow decay ballistic so it reads as a smooth level rather
//  than jittering at the UI refresh rate.
// ============================================================
class MasterMeter : public juce::Component
{
public:
    void setLevel (float peak) noexcept
    {
        // Skip the repaint entirely once there's nothing left to animate —
        // silence shouldn't cost a 30Hz repaint forever.
        if (peak < 0.001f && displayLevel < 0.001f && heldPeak < 0.001f)
            return;

        if (peak > displayLevel) displayLevel = peak;                  // instant attack
        else                     displayLevel *= 0.85f;                // ~30Hz-tick decay
        if (peak > heldPeak) { heldPeak = peak; holdCounter = 0; }
        else if (++holdCounter > 45) heldPeak *= 0.93f;                 // peak-hold then slow fall
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colour (0xff0e0e1c));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (juce::Colour (0xff252540));
        g.drawRoundedRectangle (b, 3.0f, 1.0f);

        auto bar = b.reduced (2.0f);
        const float h = bar.getHeight() * juce::jlimit (0.0f, 1.0f, displayLevel);
        if (h > 0.5f)
        {
            auto filled = bar.removeFromBottom (h);
            juce::ColourGradient grad (juce::Colour (0xff3ecfbe), filled.getBottomLeft(),
                                       juce::Colour (0xffff4f4f), filled.getTopLeft(), false);
            grad.addColour (0.75, juce::Colour (0xffe8d23e));
            g.setGradientFill (grad);
            g.fillRect (filled);
        }

        // Peak-hold tick
        const float peakY = bar.getBottom() - bar.getHeight() * juce::jlimit (0.0f, 1.0f, heldPeak);
        g.setColour (juce::Colours::white);
        g.fillRect (bar.getX(), peakY - 1.0f, bar.getWidth(), 1.5f);
    }

private:
    float displayLevel = 0.0f;
    float heldPeak     = 0.0f;
    int   holdCounter  = 0;
};

// ============================================================
//  Custom look and feel — dark knob with teal arc + indicator
// ============================================================
class DrumSynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions (18.0f));
    }

    static juce::Colour sourceColour (int src) noexcept
    {
        switch (src)
        {
            case 0: return juce::Colour (0xffff8c00);  // LFO1 — orange
            case 1: return juce::Colour (0xffaa44ff);  // LFO2 — purple
            case 2: return juce::Colour (0xff14397a);  // Env1 — dark blue
            case 3: return juce::Colour (0xffff4fa3);  // Env2 — pink
            case 4: return juce::Colour (0xff44dd88);  // Velocity — green
            default: return juce::Colours::grey;
        }
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const float cx     = float (x) + float (width)  * 0.5f;
        const float cy     = float (y) + float (height) * 0.5f;
        // outerR is the full available radius; the visible knob body shrinks
        // a little to leave room for the TransMod rings drawn outside it
        const float outerR = juce::jmin (float (width), float (height)) * 0.5f - 1.0f;
        const float radius = outerR - 7.0f;
        const float angle  = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Knob body
        g.setColour (juce::Colour (0xff0e0e1c));
        g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        // Outer edge ring
        g.setColour (juce::Colour (0xff252540));
        g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // Track arc — full sweep, dark
        const float trackR = radius - 5.0f;
        {
            juce::Path track;
            track.addArc (cx - trackR, cy - trackR, trackR * 2.0f, trackR * 2.0f,
                          rotaryStartAngle, rotaryEndAngle, true);
            juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                  juce::PathStrokeType::rounded).createStrokedPath (track, track);
            g.setColour (juce::Colour (0xff1c1c32));
            g.fillPath (track);
        }

        // Read TransMod properties pushed by the editor (base value, and
        // per-source depth + live source value, refreshed continuously by
        // a timer so the rings animate even outside edit mode)
        const auto& props  = slider.getProperties();
        const float tmBase = (float) (double) props.getWithDefault ("tmBase", (double) sliderPos);
        float depths[kNumModSources], lives[kNumModSources];
        bool  anyDepth = false;
        for (int i = 0; i < kNumModSources; ++i)
        {
            depths[i] = (float) (double) props.getWithDefault ("tmDepth" + juce::String (i), 0.0);
            lives[i]  = (float) (double) props.getWithDefault ("tmLive"  + juce::String (i), 0.0);
            if (std::abs (depths[i]) > 0.0005f) anyDepth = true;
        }

        // Inner arc — base value only. Dimmed when this knob carries any
        // modulation (its effective value moves around this anchor);
        // otherwise identical to a plain, unmodulated knob.
        {
            const float baseAngle = rotaryStartAngle + tmBase * (rotaryEndAngle - rotaryStartAngle);
            juce::Path val;
            val.addArc (cx - trackR, cy - trackR, trackR * 2.0f, trackR * 2.0f,
                        rotaryStartAngle, anyDepth ? baseAngle : angle, true);
            juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                  juce::PathStrokeType::rounded).createStrokedPath (val, val);
            g.setColour (juce::Colour (0xff3ecfbe).withAlpha (anyDepth ? 0.45f : 1.0f));
            g.fillPath (val);
        }

        // Outer rings — one per source with a non-zero depth: a dim range
        // arc spanning base→base+depth, plus a bright dot at the
        // instantaneous live position (base + depth × current source value)
        int slot = 0;
        for (int i = 0; i < kNumModSources; ++i)
        {
            if (std::abs (depths[i]) < 0.0005f) continue;

            const float ringR     = outerR - 1.0f - float (slot) * 3.0f;
            const float baseAngle = rotaryStartAngle + tmBase * (rotaryEndAngle - rotaryStartAngle);
            const float depthEff  = juce::jlimit (0.f, 1.f, tmBase + depths[i]);
            const float depthAngle= rotaryStartAngle + depthEff * (rotaryEndAngle - rotaryStartAngle);
            float a0 = baseAngle, a1 = depthAngle;
            if (a0 > a1) std::swap (a0, a1);

            juce::Path range;
            range.addArc (cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f, a0, a1, true);
            juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                  juce::PathStrokeType::rounded).createStrokedPath (range, range);
            g.setColour (sourceColour (i).withAlpha (0.5f));
            g.fillPath (range);

            const float liveEff   = juce::jlimit (0.f, 1.f, tmBase + depths[i] * lives[i]);
            const float liveAngle = rotaryStartAngle + liveEff * (rotaryEndAngle - rotaryStartAngle);
            const float lsin = std::sin (liveAngle), lcos = std::cos (liveAngle);
            g.setColour (juce::Colours::white);
            g.fillEllipse (cx + lsin * ringR - 2.0f, cy - lcos * ringR - 2.0f, 4.0f, 4.0f);

            ++slot;
        }

        // Indicator line at the current drag position (base, or base+depth
        // of the source being edited while a source is focused)
        const float sinA  = std::sin (angle);
        const float cosA  = std::cos (angle);
        const float inner = radius * 0.38f;
        const float outer = radius - 2.0f;
        g.setColour (juce::Colour (0xff5ae8d6));
        g.drawLine (cx + sinA * inner, cy - cosA * inner,
                    cx + sinA * outer, cy - cosA * outer, 2.5f);

        // Disabled state: dark overlay dims the whole knob
        if (!slider.isEnabled())
        {
            g.setColour (juce::Colour (0xbb1a1a2e));
            g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
        }
    }
};

class DrumSynthEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit DrumSynthEditor (DrumSynthProcessor&);
    ~DrumSynthEditor() override = default;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    DrumSynthProcessor& proc;
    DrumSynthLookAndFeel lnf;

    // ---------------------------------------------------------------
    // Shared state
    // ---------------------------------------------------------------
    bool advancedMode    = false;
    int  selectedChannel = 0;
    bool updatingUI      = false;   // guard against onChange feedback loops

    // ---------------------------------------------------------------
    // Both views: view toggle button
    // ---------------------------------------------------------------
    juce::TextButton viewToggleBtn;

    // ---------------------------------------------------------------
    // Basic view
    // ---------------------------------------------------------------
    std::array<juce::TextButton, DrumSynth::NumChannels> padBtns;

    // 8 macro knobs + labels
    std::array<juce::Slider, 8> macroKnobs;
    std::array<juce::Label,  8> macroLabels;
    static constexpr const char* kMacroNames[8] = {
        "Tune", "Env 1 Dec", "Attack", "Decay", "Volume", "Noise", "Flt Cut", "Resonance"
    };

    // ---------------------------------------------------------------
    // Advanced view
    // ---------------------------------------------------------------
    std::array<juce::TextButton, DrumSynth::NumChannels> advChanBtns;

    // == OSC section ==
    // Single / Metallic Cluster / Partial Shaper are mutually exclusive;
    // oscModeBox selects one and updateOscModeVisibility() shows only the
    // controls that apply to it.
    juce::Slider       pitchKnob;
    juce::ComboBox     oscModeBox;
    juce::Slider       oscShapeKnob;          // Single only
    OscShapeDisplay    oscShapeDisplay;
    juce::Slider       partPeakKnob,  partSpaceKnob,    // Partial Shaper only
                       partRollKnob,  partDecKnob;
    juce::ToggleButton membraneBtn  { "Membrane" };      // Partial Shaper only

    // == NOISE section ==
    juce::Slider       noiseLevelKnob, noiseDecKnob,
                       noiseBPFreqKnob, noiseBPQKnob;
    juce::ToggleButton noisePinkBtn { "Pink" };

    // == DRIVE section ==
    juce::Slider       driveAmtKnob;
    juce::ComboBox     driveTypeBox;

    // == FILTER section ==
    juce::ComboBox     filterModeBox, filterModelBox;
    juce::ToggleButton filter4PoleBtn { "4-pole" };
    juce::Slider       filterCutKnob, filterResKnob;

    // == ENVELOPES section (Env1/Env2 general purpose, Amp hardwired) ==
    juce::Slider       env1AttKnob, env1HoldKnob, env1DecKnob;
    juce::Slider       env2AttKnob, env2HoldKnob, env2DecKnob;
    juce::Slider       ampAttKnob, ampHoldKnob, ampDecKnob;
    juce::Label        env1Hdr { {}, "ENV 1" }, env2Hdr { {}, "ENV 2" }, ampHdr { {}, "AMP" };

    // == LFO section (TransMod sources only) ==
    juce::Slider       lfo1RateKnob;
    juce::ComboBox     lfo1WaveBox;
    juce::Label        lfo1Hdr { {}, "LFO 1" }, lfo2Hdr { {}, "LFO 2" };
    juce::Slider       lfo2RateKnob;
    juce::ComboBox     lfo2WaveBox;

    // == FX section ==
    juce::ComboBox     fx1TypeBox;
    juce::Slider       fx1AmtKnob, bitDepthKnob;

    // == MASTER section (FX strip, right side; Advanced View only) ==
    juce::Slider       masterVolKnob;
    juce::ToggleButton limiterBtn { "Limiter" };
    MasterMeter        masterMeter;
    juce::Label        masterHdr { {}, "MASTER" };

    // Per-knob name labels for advanced view, ordered by panel
    // OSC:0-5  NOISE:6-9  DRIVE:10  FILTER:11-12
    // ENV:13-21  LFO:22-23  FX:24-25  MASTER:26
    juce::Label advLbl[27];

    // ---------------------------------------------------------------
    // Layout constants
    // ---------------------------------------------------------------
    static constexpr int kPadSize  = 80;
    static constexpr int kPadGap   = 10;
    static constexpr int kHdrH     = 36;
    static constexpr int kMacroH   = 274;  // 2 rows: each row = 102 knob + 26 label + 4 gap; + 10 between rows

    static constexpr int kAdvW     = 1230;
    static constexpr int kAdvChanH = 2 * kPadSize + 3 * kPadGap;   // 190 — pad grid height
    static constexpr int kAdvH     = 958;
    static constexpr int kAdvTop   = kHdrH + kAdvChanH;   // 226
    static constexpr int kFxH      = 120;  // tall enough that the FX/Master row's
                                             // knob labels sit below the divider line

    // Panel x positions (left edge) and widths
    static constexpr int kOscX  = 5,    kOscW  = 200;
    static constexpr int kNsX   = 210,  kNsW   = 200;
    static constexpr int kDrvX  = 415,  kDrvW  = 115;
    static constexpr int kFltX  = 535,  kFltW  = 200;
    static constexpr int kEnvX  = 740,  kEnvW  = 290;
    static constexpr int kLfoX  = 1035, kLfoW  = 190;

    static constexpr int kKnob  = 58;   // knob diameter in advanced view (+12 over the
                                          // rendered circle vs. pre-ring sizing, reserved
                                          // for the TransMod outer ring band)
    static constexpr int kLblH  = 28;   // text box height under knob
    static constexpr int kCell  = kKnob + kLblH + 3;

    // ---------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------
    void buildBasicView();
    void buildAdvancedView();
    void showBasicView();
    void showAdvancedView();

    void layoutBasicView();
    void layoutAdvancedView();

    void connectAdvancedControls();

    void refreshMacros();
    void refreshAdvanced();
    void updateOscModeVisibility();

    void updatePadHighlight();
    void updateChanHighlight();

    // ---------------------------------------------------------------
    // TransMod UI state
    // ---------------------------------------------------------------
    int activeModSource = -1;  // -1 = none, else 0..4 → ModSource
    std::array<juce::TextButton, kNumModSources> modSrcBtns;

    static juce::Colour modSrcColour (int src) noexcept
    {
        switch (src)
        {
            case 0: return juce::Colour (0xffff8c00);  // LFO1 — orange
            case 1: return juce::Colour (0xffaa44ff);  // LFO2 — purple
            case 2: return juce::Colour (0xff14397a);  // Env1 — dark blue
            case 3: return juce::Colour (0xffff4fa3);  // Env2 — pink
            case 4: return juce::Colour (0xff44dd88);  // Velocity — green
            default: return juce::Colours::grey;
        }
    }

    void updateTransModUI();
    void setTransModKnobProps (juce::Slider& s, ModTarget t);

    // Every continuous parameter is a valid TransMod target. connectModKnob
    // wires a slider's onValueChange to edit either the base value (no
    // source focused) or that source's depth (focused); modKnobs is the
    // registry used to refresh all of them on channel/source change and to
    // resolve double-click-to-remove.
    struct ModKnobEntry { juce::Slider* slider; ModTarget target; bool isMacro = false; };
    std::vector<ModKnobEntry> modKnobs;
    void connectModKnob (juce::Slider& s, ModTarget t);
    void mouseDoubleClick (const juce::MouseEvent&) override;

    // Drives the live modulation-ring animation (refreshes tmLive* / tmDepth*
    // properties on every modulatable knob so the outer ring tracks the
    // running LFO/velocity value even when no source is focused)
    void timerCallback() override;

    void setupKnob (juce::Slider& s, double lo, double hi, double def,
                    const juce::String& suffix = {});
    void setupCombo (juce::ComboBox& b, const juce::StringArray& items);

    static constexpr int kBasicW()
    {
        return kPadGap + 4 * (kPadSize + kPadGap);   // 4 columns
    }
    static constexpr int kBasicH()
    {
        // header + 2 pad rows + 2 pad gaps + bottom gap + 2 macro rows
        return kHdrH + 3 * kPadGap + 2 * kPadSize + kMacroH;
    }


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumSynthEditor)
};
