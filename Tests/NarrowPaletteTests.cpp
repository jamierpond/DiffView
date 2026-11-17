#include <NarrowPalette.h>

#include <NanoTest/NanoTest.h>

#include <algorithm>
#include <array>
#include <cmath>

using namespace nano;
using namespace diffview;

namespace
{
using Matrix = std::array<std::array<double, 3>, 3>;
using Triple = std::array<double, 3>;

// Machado, Oliveira and Fernandes (2009), severity 1.0, on linear RGB.
constexpr auto protanopia = Matrix {{{0.152286, 1.052583, -0.204868},
                                     {0.114503, 0.786281, 0.099216},
                                     {-0.003882, -0.048116, 1.051998}}};
constexpr auto deuteranopia = Matrix {{{0.367322, 0.860646, -0.227968},
                                       {0.280085, 0.672501, 0.047413},
                                       {-0.011820, 0.042940, 0.968881}}};
constexpr auto tritanopia = Matrix {{{1.255528, -0.076749, -0.178779},
                                     {-0.078411, 0.930809, 0.147602},
                                     {0.004733, 0.691367, 0.303900}}};

// CIE76 colour difference: about 2 is just noticeable, 40 is plainly different.
constexpr auto minimumColourDifference = 40.0;
constexpr auto minimumLightnessDifference = 8.0;

double convertToLinear(std::uint8_t channel)
{
    constexpr auto maximum = 255.0;
    constexpr auto linearBelow = 0.04045;
    constexpr auto linearSlope = 12.92;
    constexpr auto offset = 0.055;
    constexpr auto gamma = 2.4;

    auto value = channel / maximum;

    if (value <= linearBelow)
        return value / linearSlope;

    return std::pow((value + offset) / (1.0 + offset), gamma);
}

Triple simulateColourBlindness(Rgb colour, const Matrix& simulation)
{
    auto linear = Triple {convertToLinear(colour.red),
                          convertToLinear(colour.green),
                          convertToLinear(colour.blue)};
    auto seen = Triple {};

    for (auto row = std::size_t {0}; row < 3; ++row)
    {
        auto sum = 0.0;

        for (auto column = std::size_t {0}; column < 3; ++column)
            sum += simulation[row][column] * linear[column];

        seen[row] = std::clamp(sum, 0.0, 1.0);
    }

    return seen;
}

double compressForLab(double ratio)
{
    constexpr auto cubeBelow = 0.008856;
    constexpr auto slope = 7.787;
    constexpr auto offset = 16.0 / 116.0;

    if (ratio > cubeBelow)
        return std::cbrt(ratio);

    return slope * ratio + offset;
}

// Linear sRGB to CIE L*a*b* under D65.
Triple convertToLab(const Triple& linear)
{
    constexpr auto whiteX = 0.95047;
    constexpr auto whiteZ = 1.08883;
    constexpr auto toX = Triple {0.4124, 0.3576, 0.1805};
    constexpr auto toY = Triple {0.2126, 0.7152, 0.0722};
    constexpr auto toZ = Triple {0.0193, 0.1192, 0.9505};

    auto dot = [&](const Triple& weights)
    {
        return weights[0] * linear[0] + weights[1] * linear[1]
               + weights[2] * linear[2];
    };

    auto fx = compressForLab(dot(toX) / whiteX);
    auto fy = compressForLab(dot(toY));
    auto fz = compressForLab(dot(toZ) / whiteZ);

    constexpr auto lightnessScale = 116.0;
    constexpr auto lightnessOffset = 16.0;
    constexpr auto redGreenScale = 500.0;
    constexpr auto yellowBlueScale = 200.0;

    return {lightnessScale * fy - lightnessOffset,
            redGreenScale * (fx - fy),
            yellowBlueScale * (fy - fz)};
}

double findColourDifference(const Triple& a, const Triple& b)
{
    return std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
}

void checkTellApart(Rgb added, Rgb removed)
{
    for (const auto* simulation: {&protanopia, &deuteranopia, &tritanopia})
    {
        auto addedSeen = convertToLab(simulateColourBlindness(added, *simulation));
        auto removedSeen =
            convertToLab(simulateColourBlindness(removed, *simulation));

        check(findColourDifference(addedSeen, removedSeen)
              >= minimumColourDifference);
        check(std::abs(addedSeen[0] - removedSeen[0]) >= minimumLightnessDifference);
    }
}
} // namespace

auto tRowBackgrounds = test(
    "NarrowPalette/added and removed rows stay apart for colour blind eyes") = []
{
    checkTellApart(narrowAddedColours.rowBackground,
                   narrowRemovedColours.rowBackground);
};

auto tChangedSpans =
    test("NarrowPalette/changed spans stay apart for colour blind eyes") = []
{
    checkTellApart(narrowAddedColours.changedBackground,
                   narrowRemovedColours.changedBackground);
};

auto tMarkers =
    test("NarrowPalette/+ and - markers stay apart for colour blind eyes") = []
{ checkTellApart(narrowAddedColours.marker, narrowRemovedColours.marker); };

auto tWideFailsDeuteranopia =
    test("NarrowPalette/the simulation does catch red against green") = []
{
    constexpr auto wideAddedRow = Rgb {18, 48, 30};
    constexpr auto wideRemovedRow = Rgb {60, 22, 26};

    auto addedSeen =
        convertToLab(simulateColourBlindness(wideAddedRow, deuteranopia));
    auto removedSeen =
        convertToLab(simulateColourBlindness(wideRemovedRow, deuteranopia));

    check(findColourDifference(addedSeen, removedSeen) < minimumColourDifference);
};
