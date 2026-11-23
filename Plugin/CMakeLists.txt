# TWIN-MU plugin target (JUCE). Built when TWINMU_BUILD_PLUGIN=ON.
include(FetchContent)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.tar.gz)
FetchContent_MakeAvailable(JUCE)

set(TWINMU_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND TWINMU_FORMATS AU)
endif()
# AAX needs Avid's SDK (developer account) and PACE signing to load in release Pro Tools.
if(DEFINED ENV{AAX_SDK_PATH} AND EXISTS "$ENV{AAX_SDK_PATH}")
    juce_set_aax_sdk_path("$ENV{AAX_SDK_PATH}")
    list(APPEND TWINMU_FORMATS AAX)
endif()

juce_add_plugin(TwinMu
    PRODUCT_NAME "Unfairboy 067"
    COMPANY_NAME "Unfairboy Audio"
    BUNDLE_ID com.unfairboyaudio.unfairboy067
    PLUGIN_MANUFACTURER_CODE Twmu
    PLUGIN_CODE Tmu1
    FORMATS ${TWINMU_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY AAX_ePlugInCategory_Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(TwinMu PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters/Parameters.cpp
    Source/PresetManager/PresetManager.cpp
    Source/PresetManager/FactoryPresets.cpp
    Source/GUI/TwinMuLookAndFeel.cpp
    Source/GUI/Controls.cpp
    Source/GUI/Meters.cpp
    Source/GUI/Visualizers.cpp)

target_include_directories(TwinMu PRIVATE Source)

target_compile_definitions(TwinMu PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(TwinMu PRIVATE /utf-8)
endif()

target_link_libraries(TwinMu
    PRIVATE
        twinmu_dsp
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Developer tool: renders the editor to PNG (GUI review without a DAW).
juce_add_console_app(UnfairboySnapshot PRODUCT_NAME "unfairboy_snapshot")
get_target_property(TWINMU_PLUGIN_SOURCES TwinMu SOURCES)
target_sources(UnfairboySnapshot PRIVATE Tools/Snapshot.cpp
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Parameters/Parameters.cpp
    Source/PresetManager/PresetManager.cpp Source/PresetManager/FactoryPresets.cpp
    Source/GUI/TwinMuLookAndFeel.cpp Source/GUI/Controls.cpp Source/GUI/Meters.cpp Source/GUI/Visualizers.cpp)
target_include_directories(UnfairboySnapshot PRIVATE Source)
target_compile_definitions(UnfairboySnapshot PRIVATE
    JucePlugin_Name="Unfairboy 067" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
if(MSVC)
    target_compile_options(UnfairboySnapshot PRIVATE /utf-8)
endif()
target_link_libraries(UnfairboySnapshot PRIVATE twinmu_dsp juce::juce_audio_utils juce::juce_dsp
    juce::juce_recommended_config_flags)
