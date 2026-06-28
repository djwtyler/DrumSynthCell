juce_add_plugin(DrumSynthCell
    COMPANY_NAME             "DjWTyler"
    PLUGIN_MANUFACTURER_CODE "Djwt"
    PLUGIN_CODE              "Drmc"
    FORMATS                  Standalone VST3 AU
    PRODUCT_NAME             "DrumSynthCell"
    DESCRIPTION              "Hybrid drum synthesizer"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
)

target_sources(DrumSynthCell PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    DrumVoice.cpp
    DrumVoice.h
)

target_compile_definitions(DrumSynthCell
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(DrumSynthCell
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
