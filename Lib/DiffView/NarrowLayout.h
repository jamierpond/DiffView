#pragma once

#include <optional>
#include <vector>

namespace diffview
{
// Below this the side-by-side layout leaves under 26 columns of code: its tree
// never gets narrower than 20 columns and the diff's gutter takes 13.
constexpr auto narrowLayoutBelowWidth = 60;

enum class LayoutRequest
{
    byWidth,
    alwaysNarrow
};

bool isNarrowLayout(LayoutRequest request, int terminalWidth);

// A phone shows a cell about 8 x 18 points; a thumb wants about 44.
constexpr auto minimumTappableHeight = 2;
constexpr auto roomyTappableHeight = 3;

int findDiffLineHeight(int wrappedPieceCount);

// Roomy when every entry fits on screen that way, otherwise the minimum.
int chooseEntryHeight(int entryCount, int viewHeight);

enum class NarrowPage
{
    files,
    diff,
    comment,
    confirm
};

struct NarrowPageState
{
    bool isConfirming = false;
    bool isEditingComment = false;
    bool isFileListFocused = false;
};

NarrowPage findNarrowPage(const NarrowPageState& state);

// Visual lines: a wrapped diff line is one item several lines tall.
struct LineSpan
{
    int first = 0;
    int height = 0;

    int end() const { return first + height; }
};

std::vector<LineSpan> stackSpans(const std::vector<int>& heights);
int getTotalHeight(const std::vector<LineSpan>& spans);

// The part of the span inside the view, if any.
std::optional<LineSpan> findVisiblePart(LineSpan span, LineSpan view);

// The view's first line, kept between the top and the last full screen.
int clampScroll(LineSpan view, int totalHeight);

// The view's first line after the smallest scroll that shows the whole span,
// or its top when it is taller than the view.
int scrollToReveal(LineSpan view, LineSpan span);

struct ScreenPoint
{
    int x = 0;
    int y = 0;
};

struct ScreenArea
{
    int column = 0;
    int row = 0;
    int width = 0;
    int height = 0;

    bool contains(ScreenPoint point) const;
};

enum class TapAction
{
    goBack,
    openEntry,
    selectRow,
    openAtSelection,
    submitRound,
    saveComment,
    cancelComment,
    confirmYes,
    confirmNo,
    confirmCancel
};

struct TapTarget
{
    ScreenArea area;
    TapAction action = TapAction::goBack;

    // The entry or diff row tapped, for openEntry and selectRow.
    int index = 0;
};

std::optional<TapTarget> findTapTarget(const std::vector<TapTarget>& targets,
                                       ScreenPoint point);

// Side by side across the bar with one column between them; the last takes
// what division leaves over.
std::vector<ScreenArea> layOutButtons(int count, ScreenArea bar);
} // namespace diffview
