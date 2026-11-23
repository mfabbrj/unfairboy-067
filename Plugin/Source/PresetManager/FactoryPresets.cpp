// TWIN-MU — factory presets. Only values that differ from the defaults are listed.
// Choice parameters use their index: mode 0 Compressor / 1 Limiter / 2 Vintage;
// stereoMode 0 Linked / 1 Dual Mono / 2 Mid-Side; relMode 0 VAR / 1 AUTO / 2..7 TC1..TC6;
// scFilter 0 Off / 1 HP / 2 LP / 3 BP; os 0..3 = 1x..8x.
#include "PresetManager.h"

namespace twinmu
{
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets {
        { "Vocal Smooth Leveler", "Vocal",
          { { "mode", 2 }, { "thrA", -24 }, { "ratioA", 3 }, { "relModeA", 3 }, { "mkA", 4 }, { "tube", 30 }, { "xfmr", 20 },
            { "scFilter", 1 }, { "scFreq", 100 } } },
        { "Vocal Aggressive Rock", "Vocal",
          { { "mode", 0 }, { "thrA", -30 }, { "ratioA", 8 }, { "atkA", 3 }, { "relA", 80 }, { "relModeA", 0 }, { "knee", 2 },
            { "mkA", 8 }, { "tube", 50 }, { "mix", 85 } } },
        { "Drum Bus Glue", "Drum Bus",
          { { "mode", 0 }, { "thrA", -18 }, { "ratioA", 2 }, { "atkA", 30 }, { "relA", 150 }, { "relModeA", 1 }, { "knee", 8 },
            { "mkA", 2 }, { "tube", 20 } } },
        { "Drum Bus Smash (Parallel)", "Drum Bus",
          { { "mode", 2 }, { "thrA", -40 }, { "ratioA", 20 }, { "relModeA", 2 }, { "mkA", 10 }, { "tube", 60 }, { "xfmr", 50 },
            { "mix", 40 } } },
        { "Kick Punch", "Kick",
          { { "mode", 0 }, { "thrA", -20 }, { "ratioA", 4 }, { "atkA", 25 }, { "relA", 60 }, { "relModeA", 0 }, { "knee", 3 },
            { "scFilter", 0 }, { "mkA", 3 }, { "tube", 15 } } },
        { "Snare Crack", "Snare",
          { { "mode", 0 }, { "thrA", -22 }, { "ratioA", 5 }, { "atkA", 8 }, { "relA", 90 }, { "relModeA", 0 }, { "scFreq", 150 },
            { "mkA", 4 }, { "tube", 35 } } },
        { "Bass Even Out", "Bass",
          { { "mode", 2 }, { "thrA", -24 }, { "ratioA", 4 }, { "relModeA", 4 }, { "scFilter", 0 }, { "mkA", 4 }, { "tube", 25 },
            { "xfmr", 45 } } },
        { "Master Bus Gentle Glue", "Master Bus",
          { { "mode", 2 }, { "thrA", -12 }, { "ratioA", 2 }, { "relModeA", 5 }, { "scFreq", 90 }, { "mkA", 1.5f },
            { "tube", 15 }, { "xfmr", 15 }, { "os", 2 } } },
        { "Master Limiter -1 dBTP", "Limiting",
          { { "mode", 0 }, { "thrA", 0 }, { "ratioA", 1 }, { "tube", 0 }, { "xfmr", 0 }, { "limOn", 1 }, { "limCeil", -1 },
            { "limRel", 80 }, { "limLook", 2 }, { "truePeak", 1 }, { "os", 2 } } },
        { "Mix Bus Vintage Warmth", "Mix Bus",
          { { "mode", 2 }, { "thrA", -22.4f }, { "ratioA", 4 }, { "relModeA", 3 }, { "knee", 9.6f }, { "mkA", 3.5f },
            { "tube", 45 }, { "xfmr", 30 }, { "mix", 85 }, { "outGain", 1 }, { "scFreq", 95 }, { "fdc", 1.5f } } },
        { "Piano Natural", "Piano",
          { { "mode", 0 }, { "thrA", -20 }, { "ratioA", 2 }, { "atkA", 20 }, { "relA", 300 }, { "relModeA", 1 }, { "knee", 12 },
            { "mkA", 2 }, { "tube", 10 }, { "xfmr", 10 } } },
        { "Strings Slow Swell", "Strings",
          { { "mode", 2 }, { "thrA", -20 }, { "ratioA", 2.5f }, { "relModeA", 5 }, { "mkA", 2.5f }, { "tube", 20 }, { "xfmr", 25 } } },
        { "Acoustic Guitar Polish", "Guitar",
          { { "mode", 0 }, { "thrA", -24 }, { "ratioA", 3 }, { "atkA", 10 }, { "relA", 200 }, { "relModeA", 1 }, { "scFreq", 120 },
            { "mkA", 3 }, { "tube", 20 } } },
        { "Synth Pad M/S Widen", "Synth",
          { { "mode", 2 }, { "stereoMode", 2 }, { "thrA", -20 }, { "ratioA", 3 }, { "thrB", -26 }, { "ratioB", 4 },
            { "relModeB", 3 }, { "mkA", 2 }, { "mkB", 3 }, { "width", 130 }, { "tube", 20 } } },
        { "Live Vocal Safety (ECO)", "Live Sound",
          { { "mode", 0 }, { "thrA", -18 }, { "ratioA", 4 }, { "atkA", 2 }, { "relA", 120 }, { "relModeA", 0 }, { "scFreq", 120 },
            { "mkA", 3 }, { "tube", 10 }, { "xfmr", 0 }, { "limOn", 1 }, { "limCeil", -3 }, { "limLook", 0 }, { "eco", 1 } } },
        { "Vintage TC5 Program", "Vintage Compression",
          { { "mode", 2 }, { "thrA", -20 }, { "ratioA", 6 }, { "relModeA", 6 }, { "mkA", 5 }, { "tube", 50 }, { "xfmr", 40 } } },
        { "Vintage Lat/Vert", "Vintage Compression",
          { { "mode", 2 }, { "stereoMode", 2 }, { "thrA", -20 }, { "ratioA", 4 }, { "thrB", -24 }, { "ratioB", 6 },
            { "relModeA", 3 }, { "relModeB", 3 }, { "mkA", 3 }, { "mkB", 2 }, { "tube", 40 }, { "xfmr", 35 } } },
    };
    return presets;
}
} // namespace twinmu
