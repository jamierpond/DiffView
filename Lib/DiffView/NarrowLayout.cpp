#include "NarrowLayout.h"

#include <algorithm>

namespace diffview
{
namespace
{
constexpr auto buttonGap = 1;
} // namespace

bool isNarrowLayout(LayoutRequest request, int terminalWidth)
{
    if (request == LayoutRequest::alwaysNarrow)
        return true;

    return terminalWidth < narrowLayoutBelowWidth;
}

int findDiffLineHeight(int wrappedPieceCount)
{
    return std::max(minimumTappableHeight, wrappedPieceCount);
}

int chooseEntryHeight(int entryCount, int viewHeight)
{
    if (entryCount * roomyTappableHeight <= viewHeight)
        return roomyTappableHeight;

    return minimumTappableHeight;
}

NarrowPage findNarrowPage(const NarrowPageState& state)
{
    if (state.isConfirming)
        return NarrowPage::confirm;

    if (state.isEditingComment)
        return NarrowPage::comment;

    if (state.isFileListFocused)
        return NarrowPage::files;

    return NarrowPage::diff;
}

std::vector<LineSpan> stackSpans(const std::vector<int>& heights)
{
    auto spans = std::vector<LineSpan> {};
    auto next = 0;

    for (auto height: heights)
    {
        spans.push_back({next, height});
        next += height;
    }

    return spans;
}

int getTotalHeight(const std::vector<LineSpan>& spans)
{
    if (spans.empty())
        return 0;

    return spans.back().end();
}

int clampScroll(LineSpan view, int totalHeight)
{
    return std::clamp(view.first, 0, std::max(0, totalHeight - view.height));
}

int scrollToReveal(LineSpan view, LineSpan span)
{
    if (span.first < view.first || span.height >= view.height)
        return span.first;

    if (span.end() > view.end())
        return span.end() - view.height;

    return view.first;
}

bool ScreenArea::contains(ScreenPoint point) const
{
    return point.x >= column && point.x < column + width && point.y >= row
           && point.y < row + height;
}

std::optional<TapTarget> findTapTarget(const std::vector<TapTarget>& targets,
                                       ScreenPoint point)
{
    for (const auto& target: targets)
        if (target.area.contains(point))
            return target;

    return std::nullopt;
}

std::vector<ScreenArea> layOutButtons(int count, ScreenArea bar)
{
    auto areas = std::vector<ScreenArea> {};

    if (count <= 0)
        return areas;

    auto widthForButtons = bar.width - buttonGap * (count - 1);
    auto each = std::max(1, widthForButtons / count);
    auto column = bar.column;
    auto barEnd = bar.column + bar.width;

    for (auto i = 0; i < count; ++i)
    {
        auto isLast = i == count - 1;
        auto buttonWidth = isLast ? std::max(1, barEnd - column) : each;
        areas.push_back({column, bar.row, buttonWidth, bar.height});
        column += buttonWidth + buttonGap;
    }

    return areas;
}
} // namespace diffview
