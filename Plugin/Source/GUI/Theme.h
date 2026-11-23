// TWIN-MU — the three colour themes (Ardesia, Crema anni '60, Verde menta retro).
// Displays, meters and VU faces are theme-independent so they stay readable.
#pragma once

#include <juce_graphics/juce_graphics.h>

#include <array>

namespace twinmu::gui
{
struct Theme
{
    juce::String name;
    juce::Colour panelTop, panelMid, panelBottom, brush;
    juce::Colour label, label2, titleBackground, sectionLine, tick;
    juce::Colour teal, coral, violet, sun, orange, sky, lime; // section accents
    float tint;                                               // opacity of the section washes
};

inline const std::array<Theme, 3>& themes()
{
    using juce::Colour;
    static const std::array<Theme, 3> list { {
        { "ARDESIA", Colour (0xff33404f), Colour (0xff2a3442), Colour (0xff232b37), Colour (0x05ffffff),
          Colour (0xffe9e2cf), Colour (0xffa9a393), Colour (0xff2b3543), Colour (0x8c000000), Colour (0xff8d8879),
          Colour (0xff2ed3c0), Colour (0xffff6b7a), Colour (0xffa98bff), Colour (0xffffcf4a), Colour (0xffff8a3d),
          Colour (0xff4db5ff), Colour (0xffb6ea5a), 0.10f },
        { "CREMA '60", Colour (0xfff2e9d3), Colour (0xffe9dec3), Colour (0xffdccfb0), Colour (0x08503c14),
          Colour (0xff2b2418), Colour (0xff6e624a), Colour (0xffe9dec3), Colour (0x473c2d14), Colour (0xff6e624a),
          Colour (0xff12948a), Colour (0xffe2503a), Colour (0xff7a5cc9), Colour (0xffe2a21b), Colour (0xffe8742a),
          Colour (0xff2f7fc4), Colour (0xff5f9a1c), 0.13f },
        { "MENTA", Colour (0xffb4e0d0), Colour (0xffa2d5c3), Colour (0xff8cc6b1), Colour (0x080a3c32),
          Colour (0xff123533), Colour (0xff3a635c), Colour (0xffa2d5c3), Colour (0x4d0a322d), Colour (0xff2f5a53),
          Colour (0xff0a6d7a), Colour (0xffee5560), Colour (0xff6a4cc2), Colour (0xfff0b000), Colour (0xfff0732a),
          Colour (0xff2a5fd0), Colour (0xff3a8a24), 0.14f },
    } };
    return list;
}

/** Fixed colours shared by all themes. */
namespace fixed
{
inline const juce::Colour topBar { 0xff17181a };
inline const juce::Colour ink { 0xffe9e2cf };
inline const juce::Colour ink2 { 0xffa9a393 };
inline const juce::Colour screen { 0xff0d1411 };
inline const juce::Colour screenGrid { 0xff1d2b24 };
inline const juce::Colour screenText { 0xff5f8f74 };
inline const juce::Colour amber { 0xfff3a83c };
inline const juce::Colour vuInk { 0xff2a2016 };
} // namespace fixed
} // namespace twinmu::gui
