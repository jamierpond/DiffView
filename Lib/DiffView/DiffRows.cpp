#include "DiffRows.h"
#include "Text.h"

#include <algorithm>
#include <map>

namespace diffview
{
namespace
{
void appendBlock(std::vector<DiffRow>& rows,
                 const Annotation& annotation,
                 int wrapWidth)
{
    rows.push_back({.kind = RowKind::annotationTop, .annotation = annotation.id});

    if (!annotation.replies.empty())
        rows.push_back({.kind = RowKind::replyHeader, .annotation = annotation.id});

    auto lines = splitLines(annotation.text);

    if (lines.empty())
        lines.emplace_back();

    for (const auto& line: lines)
        for (auto& piece: wrap(displayable(line), wrapWidth))
            rows.push_back({.kind = RowKind::annotationText,
                            .annotation = annotation.id,
                            .text = std::move(piece)});

    for (std::size_t r = 0; r < annotation.replies.size(); ++r)
    {
        const auto& reply = annotation.replies[r];
        auto index = static_cast<int>(r);
        rows.push_back(
            {.kind = RowKind::annotationText, .annotation = annotation.id});
        rows.push_back({.kind = RowKind::replyHeader,
                        .annotation = annotation.id,
                        .reply = index});

        for (const auto& line: splitLines(reply.text))
            for (auto& piece: wrap(displayable(line), std::max(1, wrapWidth - 3)))
                rows.push_back({.kind = RowKind::annotationText,
                                .annotation = annotation.id,
                                .text = std::move(piece),
                                .reply = index});
    }

    rows.push_back({.kind = RowKind::annotationBottom, .annotation = annotation.id});
}
} // namespace

std::vector<DiffRow> buildRows(const FileDiff& file,
                               const std::vector<Annotation>& annotations,
                               int wrapWidth)
{
    auto rows = std::vector<DiffRow> {{.kind = RowKind::fileHeader}};

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
        rows.push_back({.kind = RowKind::message, .text = "Binary file"});
    else if (!file.note.empty())
        rows.push_back({.kind = RowKind::message, .text = file.note});
    else if (file.hunks.empty())
        rows.push_back({.kind = RowKind::message,
                        .text = "No content changes (mode or empty file)"});

    auto flat = 0;

    for (std::size_t h = 0; h < file.hunks.size(); ++h)
    {
        rows.push_back({.kind = RowKind::hunkHeader, .hunk = static_cast<int>(h)});

        for (std::size_t l = 0; l < file.hunks[h].lines.size(); ++l)
        {
            rows.push_back({.kind = RowKind::line,
                            .hunk = static_cast<int>(h),
                            .line = static_cast<int>(l),
                            .flat = flat});

            auto [begin, end] = placed.equal_range(flat);

            for (auto it = begin; it != end; ++it)
                appendBlock(rows, *it->second, wrapWidth);

            ++flat;
        }
    }

    return rows;
}

int findThreadOffset(const std::vector<DiffRow>& rows, int index)
{
    auto top = index;

    while (top > 0
           && rows[static_cast<std::size_t>(top)].kind != RowKind::annotationTop)
        --top;

    return index - top;
}

std::optional<int>
    findThreadRow(const std::vector<DiffRow>& rows, int annotation, int offset)
{
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        if (rows[i].kind != RowKind::annotationTop
            || rows[i].annotation != annotation)
            continue;

        auto row = static_cast<int>(i);

        for (auto step = 0; step < offset; ++step)
        {
            if (rows[static_cast<std::size_t>(row)].kind
                == RowKind::annotationBottom)
                break;

            ++row;
        }

        return row;
    }

    return std::nullopt;
}
} // namespace diffview
