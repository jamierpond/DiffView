#include "DiffRows.h"
#include "Text.h"

#include <map>

namespace diffview
{
namespace
{
void appendBlock(std::vector<DiffRow>& rows,
                 const Annotation& annotation,
                 int wrapWidth)
{
    rows.push_back({RowKind::annotationTop, -1, -1, -1, annotation.id, {}});

    auto lines = splitLines(annotation.text);

    if (lines.empty())
        lines.emplace_back();

    for (const auto& line: lines)
        for (auto& piece: wrap(displayable(line), wrapWidth))
            rows.push_back({RowKind::annotationText,
                            -1,
                            -1,
                            -1,
                            annotation.id,
                            std::move(piece)});

    rows.push_back({RowKind::annotationBottom, -1, -1, -1, annotation.id, {}});
}
} // namespace

std::vector<DiffRow> buildRows(const FileDiff& file,
                               const std::vector<Annotation>& annotations,
                               int wrapWidth)
{
    auto rows = std::vector<DiffRow> {{RowKind::fileHeader, -1, -1, -1, 0, {}}};

    for (const auto& annotation: annotations)
        if (annotation.path == file.path && annotation.fileLevel)
            appendBlock(rows, annotation, wrapWidth);

    auto placed = std::multimap<int, const Annotation*> {};

    for (const auto& annotation: annotations)
    {
        if (annotation.path != file.path || annotation.fileLevel)
            continue;

        auto range = annotation.stale ? std::nullopt : findAnchor(annotation, file);

        if (range)
            placed.emplace(range->last, &annotation);
        else
            appendBlock(rows, annotation, wrapWidth);
    }

    if (file.binary)
        rows.push_back({RowKind::message, -1, -1, -1, 0, "Binary file"});
    else if (!file.note.empty())
        rows.push_back({RowKind::message, -1, -1, -1, 0, file.note});
    else if (file.hunks.empty())
        rows.push_back({RowKind::message,
                        -1,
                        -1,
                        -1,
                        0,
                        "No content changes (mode or empty file)"});

    auto flat = 0;

    for (std::size_t h = 0; h < file.hunks.size(); ++h)
    {
        rows.push_back({RowKind::hunkHeader, static_cast<int>(h), -1, -1, 0, {}});

        for (std::size_t l = 0; l < file.hunks[h].lines.size(); ++l)
        {
            rows.push_back({RowKind::line,
                            static_cast<int>(h),
                            static_cast<int>(l),
                            flat,
                            0,
                            {}});

            auto [begin, end] = placed.equal_range(flat);

            for (auto it = begin; it != end; ++it)
                appendBlock(rows, *it->second, wrapWidth);

            ++flat;
        }
    }

    return rows;
}
} // namespace diffview
