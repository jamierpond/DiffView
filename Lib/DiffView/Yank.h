#pragma once

#include "DiffRows.h"

#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
struct YankedLines
{
    std::string text;
    int lineCount = 0;
};

// The source lines of every line row from `firstRow` to `lastRow` (either
// order), each ending in '\n' as vim's line-wise yank does; other rows skip.
YankedLines collectYankedLines(const FileDiff& file,
                               const std::vector<DiffRow>& rows,
                               int firstRow,
                               int lastRow);

// OSC 52: asks the terminal, local or at the far end of ssh, to set its
// clipboard to `text`.
std::string makeOsc52CopySequence(std::string_view text);
} // namespace diffview
