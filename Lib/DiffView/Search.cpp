#include "Search.h"

#include <TextBuffer.h>

namespace diffview
{
std::vector<GrepHit> grepDiffs(const std::vector<FileDiff>& files,
                               std::string_view query,
                               std::size_t limit)
{
    auto hits = std::vector<GrepHit> {};

    if (query.empty())
        return hits;

    for (const auto& file: files)
    {
        auto flat = 0;

        for (const auto& hunk: file.hunks)
            for (const auto& line: hunk.lines)
            {
                if (!text::findAll(line.text, query).empty())
                {
                    hits.push_back({file.path,
                                    flat,
                                    line.prefixed(),
                                    line.newLine > 0 ? line.newLine : line.oldLine});

                    if (hits.size() >= limit)
                        return hits;
                }

                ++flat;
            }
    }

    return hits;
}
} // namespace diffview
