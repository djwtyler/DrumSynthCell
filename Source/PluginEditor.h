#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "TransMod.h"

// ============================================================
//  OscShapeDisplay — live waveform preview for the OSC section
// ============================================================
class OscShapeDisplay : public juce::Component
{
public:
    void setShape (float s, bool metallic, bool shaperOn)
    {
        shape = s;  isMetallic = metallic;  isShaperOn = shaperOn;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);

        g.setColour (juce::Colour (0xff0e0e1c));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (juce::Colour (0xff252540));
        g.drawRoundedRectangle (b, 3.0f, 1.0f);

        if (isMetallic || isShaperOn)
        {
            g.setColour (juce::Colour (0xff555577));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (isMetallic ? "metallic cluster" : "shaper active",
                        b, juce::Justification::centred);
            return;
        }

        // Morph: Sine (0) → Saw (0.5) → Square (1.0)
        const float s2   = juce::jlimit (0.0f, 0.9999f, shape) * 2.0f;
        const int   idx  = int (s2);
        const float frac = s2 - float (idx);

        const float cx  = b.getX();
        const float cy  = b.getCentreY();
        const float w   = b.getWidth();
        const float amp = b.getHeight() * 0.42f;
        const int   N   = int (w);

        // Draw one cycle; i < N avoids the wrap-around discontinuity at t=1
        juce::Path path;
        bool  started  = false;
        float prevVal  = 0.0f;

        for (int i = 0; i < N; ++i)
        {
            const float phase = float (i) / float (N);
            const float t     = phase;

            const float sine  = std::sin (t * juce::MathConstants<float>::twoPi);
            const float saw   = 2.0f * t - 1.0f;
            const float sq    = t < 0.5f ? 1.0f : -1.0f;
            const float ws[3] = { sine, saw, sq };
            const float val   = ws[idx] * (1.0f - frac)
                              + ws[idx < 2 ? idx + 1 : 2] * frac;

            const float px = cx + float (i);
            const float py = cy - val * amp;

            // Start a new subpath on discontinuities (saw/square resets)
            if (!started || std::abs (val - prevVal) > 1.2f)
            {
                path.startNewSubPath (px, py);
                started = true;
            }
            else
            {
                path.lineTo (px, py);
            }
            prevVal = val;
        }

        g.setColour (juce::Colour (0xff3ecfbe));
        g.strokePath (path, juce::PathStrokeType (1.5f));
    }

private:
    float shape     = 0.0f;
    bool  isMetallic = false;
    bool  isShaperOn = false;
};

// ============================================================
//  Custom look and feel — dark knob with teal arc + indicator
// ============================================================
class DrumSynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Font getLabelFont (juce::Label&) override
    {
        return juce::Font (juce::FontOptions (16.0f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const float cx     = float (x) + float (width)  * 0.5f;
        const float cy     = float (y) + float (height) * 0.5f;
        const float radius = juce::jmin (float (width), float (height)) * 0.5f - 2.0f;
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

        // Read TransMod properties
        const auto&       props       = slider.getProperties();
        bool              tmActive    = static_cast<bool>   (props.getWithDefault ("tmActive", false));
        float             tmBase      = static_cast<float>  (static_cast<double>  (props.getWithDefault ("tmBase", static_cast<double> (sliderPos))));
        juce::uint32      tmColorARGB = static_cast<juce::uint32> (static_cast<int> (props.getWithDefault ("tmColor", static_cast<int> (0xffccccdd))));
        juce::Colour      tmColor (tmColorARGB);

        if (tmActive)
        {
            const float baseAngle = rotaryStartAngle + tmBase * (rotaryEndAngle - rotaryStartAngle);

            // Base value arc — teal, dimmed
            if (tmBase > 0.001f)
            {
                juce::Path baseArc;
                baseArc.addArc (cx - trackR, cy - trackR, trackR * 2.0f, trackR * 2.0f,
                                rotaryStartAngle, baseAngle, true);
                juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded).createStrokedPath (baseArc, baseArc);
                g.setColour (juce::Colour (0xff3ecfbe).withAlpha (0.45f));
                g.fillPath (baseArc);
            }

            // Mod depth arc — source colour, from base to current (base + depth)
            if (std::abs (sliderPos - tmBase) > 0.005f)
            {
                float arcStart = rotaryStartAngle + tmBase    * (rotaryEndAngle - rotaryStartAngle);
                float arcEnd   = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
                if (arcStart > arcEnd) std::swap (arcStart, arcEnd);
                juce::Path depthArc;
                depthArc.addArc (cx - trackR, cy - trackR, trackR * 2.0f, trackR * 2.0f,
                                 arcStart, arcEnd, true);
                juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                      juce::PathStrokeType::rounded).createStrokedPath (depthArc, depthArc);
                g.setColour (tmColor);
                g.fillPath (depthArc);
            }
        }
        else
        {
            // Normal value arc — teal, start → current
            juce::Path val;
            val.addArc (cx - trackR, cy - trackR, trackR * 2.0f, trackR * 2.0f,
                        rotaryStartAngle, angle, true);
            juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                  juce::PathStrokeType::rounded).createStrokedPath (val, val);
            g.setColour (juce::Colour (0xff3ecfbe));
            g.fillPath (val);
        }

        // When focused by TransMod: draw a coloured ring on the knob border
        // so that clicking a source button is visually obvious even at depth=0
        if (tmActive)
        {
            g.setColour (tmColor.withAlpha (0.85f));
            g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 2.5f);
        }

        // Indicator line at current angle
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

class DrumSynthEditor : public juce::AudioProcessorEditor
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
        "Tune", "Pitch Env", "Attack", "Decay", "Volume", "Noise", "Flt Cut", "Resonance"
    };

    // ---------------------------------------------------------------
    // Advanced view
    // ---------------------------------------------------------------
    std::array<juce::TextButton, DrumSynth::NumChannels> advChanBtns;

    // == OSC section ==
    juce::Slider       pitchKnob;
    juce::Slider       oscShapeKnob;
    OscShapeDisplay    oscShapeDisplay;
    juce::ToggleButton metallicBtn  { "Metallic Cluster" };

    // Partial shaper
    juce::ToggleButton shaperEnabledBtn { "Partial Shaper" };
    juce::Slider       partPeakKnob,  partSpaceKnob,
                       partRollKnob,  partDecKnob;
    juce::ToggleButton membraneBtn  { "Membrane" };

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

    // == ENVELOPES section ==
    // Fast (pitch)
    juce::Slider       pEnvDepthKnob, pEnvDecKnob;
    // Slow (filter)
    juce::Slider       fEnvAttKnob, fEnvHoldKnob, fEnvDecKnob, fEnvDepthKnob;
    // Amp
    juce::Slider       ampAttKnob, ampHoldKnob, ampDecKnob;

    // == LFO section ==
    juce::Slider       lfo1RateKnob, lfo1DepthKnob;
    juce::ComboBox     lfo1WaveBox;
    juce::Slider       lfo2RateKnob, lfo2DepthKnob;
    juce::ComboBox     lfo2WaveBox;

    // == FX section ==
    juce::ComboBox     fx1TypeBox;
    juce::Slider       fx1AmtKnob, bitDepthKnob;

    // Per-knob name labels for advanced view (28 total, ordered by panel)
    // OSC:0-5  NOISE:6-9  DRIVE:10  FILTER:11-12
    // ENV:13-21  LFO:22-25  FX:26-27
    juce::Label advLbl[28];

    // ---------------------------------------------------------------
    // Layout constants
    // ---------------------------------------------------------------
    static constexpr int kPadSize  = 80;
    static constexpr int kPadGap   = 10;
    static constexpr int kHdrH     = 36;
    static constexpr int kMacroH   = 250;  // 2 rows: each row = 90 knob + 26 label + 4 gap; + 10 between rows

    static constexpr int kAdvW     = 1230;
    static constexpr int kAdvChanH = 2 * kPadSize + 3 * kPadGap;   // 190 — pad grid height
    static constexpr int kAdvH     = 958;
    static constexpr int kAdvTop   = kHdrH + kAdvChanH;   // 226
    static constexpr int kFxH      = 90;

    // Panel x positions (left edge) and widths
    static constexpr int kOscX  = 5,    kOscW  = 200;
    static constexpr int kNsX   = 210,  kNsW   = 200;
    static constexpr int kDrvX  = 415,  kDrvW  = 115;
    static constexpr int kFltX  = 535,  kFltW  = 200;
    static constexpr int kEnvX  = 740,  kEnvW  = 290;
    static constexpr int kLfoX  = 1035, kLfoW  = 190;

    static constexpr int kKnob  = 46;   // knob diameter in advanced view
    static constexpr int kLblH  = 28;   // text box height under knob
    static constexpr int kCell  = kKnob + kLblH + 3;  // 77px per knob cell

    // ---------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------
    void buildBasicView();
    void buildAdvancedView();
    void showBasicView();
    void showAdvancedView();

    void layoutBasicView();
    void layoutAdvancedView();

    void connectMacros();
    void connectAdvancedControls();

    void refreshMacros();
    void refreshAdvanced();

    void updatePadHighlight();
    void updateChanHighlight();

    // ---------------------------------------------------------------
    // TransMod UI state
    // ---------------------------------------------------------------
    int activeModSource = -1;  // -1 = none, else 0/1/2 → ModSource
    std::array<juce::TextButton, kNumModSources> modSrcBtns;

    static juce::Colour modSrcColour (int src) noexcept
    {
        switch (src)
        {
            case 0: return juce::Colour (0xffff8c00);  // LFO1 — orange
            case 1: return juce::Colour (0xffaa44ff);  // LFO2 — purple
            case 2: return juce::Colour (0xff44dd88);  // Velocity — green
            default: return juce::Colours::grey;
        }
    }

    void updateTransModUI();
    void setTransModKnobProps (juce::Slider& s, ModTarget t);

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
