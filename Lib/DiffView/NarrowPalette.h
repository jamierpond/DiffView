#pragma once

#include <cstdint>

namespace diffview
{
struct Rgb
{
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
};

struct ChangeColours
{
    Rgb rowBackground;
    Rgb changedBackground;
    Rgb marker;
};

// Blue for added, orange for removed, the orange lighter: red against green
// is the pair that red-green colour blindness loses, blue against orange
// survives it and tritanopia too. NarrowPaletteTests checks this.
constexpr auto narrowAddedColours = ChangeColours {
    .rowBackground = {12, 32, 78},
    .changedBackground = {28, 64, 150},
    .marker = {80, 150, 255},
};

constexpr auto narrowRemovedColours = ChangeColours {
    .rowBackground = {96, 58, 10},
    .changedBackground = {150, 94, 16},
    .marker = {255, 186, 90},
};
} // namespace diffview
