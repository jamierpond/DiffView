#pragma once

#include "Diff.h"

#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
struct GrepHit
{
    std::string path;
    int flat = 0; // index into flatLines(file)
    std::string line; // as git prints it, with its +/-/space
    int lineNumber = 0; // new side, else old side
};

std::vector<GrepHit> grepDiffs(const std::vector<FileDiff>& files,
                               std::string_view query,
                               std::size_t limit = 1000);
} // namespace diffview
