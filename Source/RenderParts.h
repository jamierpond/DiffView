#pragma once

#include <Highlight.h>

#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace diffview
{
// Threads sit under the wide layout's line-number gutter.
inline const auto wideThreadIndent = std::string(10, ' ');

struct TextRange
{
    std::size_t start = 0;
    std::size_t end = 0;

    bool contains(std::size_t index) const { return index >= start && index < end; }
};

ftxui::Color cursorBackground(bool focused);
ftxui::Element cursorLine(ftxui::Element line, bool focused);
std::string padLeft(const std::string& text, int width);

ftxui::Element highlightText(const std::string& text,
                             const std::vector<Token>& tokens,
                             std::optional<TextRange> changed,
                             std::optional<ftxui::Color> changedBackground,
                             std::optional<TextRange> match);
} // namespace diffview
