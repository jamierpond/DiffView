#include "DiffRows.h"
#include "Text.h"

#include <algorithm>
#include <ranges>
#include <span>
#include <map>
#include <sstream>

namespace diffview
{
namespace
{
constexpr auto collapsedWord = std::string_view {"collapsed"};
constexpr auto allWord = std::string_view {"all"};

std::size_t countNewReplies(const Annotation& annotation, const ThreadFold& fold)
{
    auto count = std::size_t {0};

    for (auto r = fold.repliesWhenCollapsed; r < annotation.replies.size(); ++r)
    {
        if (annotation.replies[r].author != reviewerName)
            ++count;
    }

    return count;
}

std::size_t findFirstShownReply(const Annotation& annotation, const ThreadFold& fold)
{
    auto count = annotation.replies.size();

    if (fold.view == ThreadView::all || count <= recentReplyCount)
        return 0;

    return count - recentReplyCount;
}

std::string describeEarlierReplies(std::size_t hidden)
{
    auto noun = hidden == 1 ? " earlier reply" : " earlier replies";
    return "⋯ " + std::to_string(hidden) + noun + " · Enter to show all";
}

void appendBlock(std::vector<DiffRow>& rows,
                 const Annotation& annotation,
                 const ThreadFolds& folds,
                 int wrapWidth)
{
    auto fold = findFold(folds, annotation.id);

    if (fold.view == ThreadView::collapsed)
    {
        rows.push_back(
            {.kind = RowKind::collapsedThread,
             .annotation = annotation.id,
             .text = describeCollapsedThread(annotation, fold, wrapWidth)});
        return;
    }

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

    auto firstShown = findFirstShownReply(annotation, fold);

    if (firstShown > 0)
    {
        rows.push_back(
            {.kind = RowKind::annotationText, .annotation = annotation.id});
        rows.push_back({.kind = RowKind::earlierReplies,
                        .annotation = annotation.id,
                        .text = describeEarlierReplies(firstShown)});
    }

    for (auto r = firstShown; r < annotation.replies.size(); ++r)
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

std::string formatThreadFolds(const ThreadFolds& folds)
{
    auto text = std::string {};

    for (const auto& [id, fold]: folds)
    {
        if (fold.view == ThreadView::collapsed)
            text += std::to_string(id) + " " + std::string {collapsedWord} + " "
                    + std::to_string(fold.repliesWhenCollapsed) + "\n";
        else if (fold.view == ThreadView::all)
            text += std::to_string(id) + " " + std::string {allWord} + "\n";
    }

    return text;
}

ThreadFolds parseThreadFolds(const std::string& text)
{
    auto folds = ThreadFolds {};

    for (const auto& line: splitLines(text))
    {
        auto in = std::istringstream {line};
        auto id = 0;
        auto word = std::string {};
        auto replies = std::size_t {0};

        if (!(in >> id >> word))
            continue;

        if (word == collapsedWord && in >> replies)
            folds[id] = {ThreadView::collapsed, replies};
        else if (word == allWord)
            folds[id] = {ThreadView::all, 0};
    }

    return folds;
}

ThreadFold findFold(const ThreadFolds& folds, int id)
{
    if (auto found = folds.find(id); found != folds.end())
        return found->second;

    return {};
}

std::string describeCollapsedThread(const Annotation& annotation,
                                    const ThreadFold& fold,
                                    int width)
{
    auto lines = splitLines(annotation.text);
    auto first = lines.empty() ? std::string {} : displayable(lines.front());
    auto replies = annotation.replies.size();
    auto suffix = std::string {};

    if (replies > 0)
        suffix +=
            " · " + std::to_string(replies) + (replies == 1 ? " reply" : " replies");

    if (auto fresh = countNewReplies(annotation, fold); fresh > 0)
        suffix += " · " + std::to_string(fresh) + " new";

    auto head = "#" + std::to_string(annotation.id) + " · " + reviewerName + ": ";
    auto room = std::max(1, width - codepointCount(head) - codepointCount(suffix));
    auto isCut = codepointCount(first) > room || lines.size() > 1;

    if (isCut)
        first = wrap(first, std::max(1, room - 1)).front() + "…";

    return head + first + suffix;
}

bool isThreadStart(const DiffRow& row)
{
    return row.kind == RowKind::annotationTop
           || row.kind == RowKind::collapsedThread;
}

std::vector<DiffRow> buildRows(const FileDiff& file,
                               const std::vector<Annotation>& annotations,
                               const ThreadFolds& folds,
                               int wrapWidth)
{
    auto rows = std::vector<DiffRow> {{.kind = RowKind::fileHeader}};

    for (const auto& annotation: annotations)
        if (annotation.path == file.path && annotation.fileLevel)
            appendBlock(rows, annotation, folds, wrapWidth);

    auto placed = std::multimap<int, const Annotation*> {};

    for (const auto& annotation: annotations)
    {
        if (annotation.path != file.path || annotation.fileLevel)
            continue;

        auto range = annotation.stale ? std::nullopt : findAnchor(annotation, file);

        if (range)
            placed.emplace(range->last, &annotation);
        else
            appendBlock(rows, annotation, folds, wrapWidth);
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
                appendBlock(rows, *it->second, folds, wrapWidth);

            ++flat;
        }
    }

    return rows;
}

int findOffsetInThread(const std::vector<DiffRow>& rows, std::size_t index)
{
    auto upward = std::span {rows}.first(index + 1) | std::views::reverse;
    auto top = std::ranges::find_if(upward, isThreadStart);
    return static_cast<int>(top - upward.begin());
}

std::optional<int>
    findRowInThread(const std::vector<DiffRow>& rows, int annotation, int offset)
{
    auto isTop = [&](const DiffRow& row)
    { return isThreadStart(row) && row.annotation == annotation; };
    auto top = std::ranges::find_if(rows, isTop);

    if (top == rows.end())
        return std::nullopt;

    if (top->kind == RowKind::collapsedThread)
        return static_cast<int>(top - rows.begin());

    auto bottom = std::ranges::find(
        top, rows.end(), RowKind::annotationBottom, &DiffRow::kind);
    auto steps = std::min<std::ptrdiff_t>(offset, bottom - top);
    return static_cast<int>(top - rows.begin() + steps);
}
} // namespace diffview
