#pragma once

#include "Diff.h"

#include <cstddef>
#include <optional>
#include <string_view>

namespace diffview
{
// Byte range within one line's text.
struct ChangedSpan
{
    std::size_t start = 0;
    std::size_t length = 0;
};

struct ChangedSpanPair
{
    ChangedSpan removed;
    ChangedSpan added;
};

// What differs between a removed line and the added line that replaced it,
// past their common prefix and suffix; nothing when the lines are the same
// or when the changed parts cover nearly all of both lines.
std::optional<ChangedSpanPair> findChangedSpans(std::string_view removed,
                                                std::string_view added);

// GitHub's pairing: a run of N removed lines straight followed by exactly N
// added lines pairs them in order. Spans index displayable(line.text).
std::optional<ChangedSpan> findChangedSpanOfLine(const Hunk& hunk,
                                                 std::size_t lineIndex);
} // namespace diffview
