#pragma once

#include "RenderParts.h"

#include <Highlight.h>
#include <NarrowLayout.h>
#include <NarrowPalette.h>
#include <Text.h>

#include <ftxui/dom/elements.hpp>

#include <optional>
#include <string>
#include <vector>

namespace diffview
{
struct NarrowHeader
{
    std::string backLabel;
    std::string title;
    std::string detail;
};

struct NarrowButton
{
    std::string label;
    TapAction action = TapAction::goBack;
};

// What every line of a file-list entry starts with.
struct NarrowEntryLead
{
    ftxui::Element mark;
    std::string indent;
};

// One diff line, worked out once and drawn a wrapped piece at a time.
struct NarrowLineLook
{
    std::string shown;
    const std::vector<Token>& tokens;
    int digits = 1;
    int number = 0;
    std::vector<ByteRange> pieces;
    std::string mark = " ";
    bool isEmphasised = false;
    std::optional<ChangeColours> colours;
    std::string marker = " ";
    std::optional<TextRange> changed;
};
} // namespace diffview
