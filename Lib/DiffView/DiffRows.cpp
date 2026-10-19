#include "DiffRows.h"
#include "Text.h"

#include <algorithm>
#include <ranges>
#include <span>
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

int findOffsetInThread(const std::vector<DiffRow>& rows, std::size_t index)
{
    auto upward = std::span {rows}.first(index + 1) | std::views::reverse;
    auto top = std::ranges::find(upward, RowKind::annotationTop, &DiffRow::kind);
    return static_cast<int>(top - upward.begin());
}

std::optional<int>
    findRowInThread(const std::vector<DiffRow>& rows, int annotation, int offset)
{
    auto isTop = [&](const DiffRow& row)
    { return row.kind == RowKind::annotationTop && row.annotation == annotation; };
    auto top = std::ranges::find_if(rows, isTop);

    if (top == rows.end())
        return std::nullopt;

    auto bottom = std::ranges::find(
        top, rows.end(), RowKind::annotationBottom, &DiffRow::kind);
    auto steps = std::min<std::ptrdiff_t>(offset, bottom - top);
    return static_cast<int>(top - rows.begin() + steps);
}
} // namespace diffview
