#include <NarrowLayout.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
constexpr auto phoneWidth = 45;
constexpr auto phoneHeight = 35;
constexpr auto laptopWidth = 150;
} // namespace

auto tLayoutByWidth = test("NarrowLayout/narrow below 60 columns, wide from 60") = []
{
    constexpr auto narrowestTried = 30;
    constexpr auto justBelowThreshold = 59;
    constexpr auto threshold = 60;
    constexpr auto commonWidth = 80;

    check(isNarrowLayout(LayoutRequest::byWidth, narrowestTried));
    check(isNarrowLayout(LayoutRequest::byWidth, phoneWidth));
    check(isNarrowLayout(LayoutRequest::byWidth, justBelowThreshold));
    check(!isNarrowLayout(LayoutRequest::byWidth, threshold));
    check(!isNarrowLayout(LayoutRequest::byWidth, commonWidth));
    check(!isNarrowLayout(LayoutRequest::byWidth, laptopWidth));
};

auto tLayoutForced = test("NarrowLayout/--mobile is narrow at any width") = []
{
    check(isNarrowLayout(LayoutRequest::alwaysNarrow, phoneWidth));
    check(isNarrowLayout(LayoutRequest::alwaysNarrow, laptopWidth));
};

auto tDiffLineHeight =
    test("NarrowLayout/a diff line is at least two rows, more when it wraps") = []
{
    constexpr auto longLinePieces = 4;

    check(findDiffLineHeight(1) == minimumTappableHeight);
    check(findDiffLineHeight(2) == 2);
    check(findDiffLineHeight(longLinePieces) == longLinePieces);
};

auto tEntryHeight =
    test("NarrowLayout/file entries are three rows tall when they all fit") = []
{
    constexpr auto fewFiles = 5;
    constexpr auto manyFiles = 40;
    constexpr auto exactlyFittingFiles = 10;
    constexpr auto exactViewHeight = 30;

    check(chooseEntryHeight(fewFiles, phoneHeight) == roomyTappableHeight);
    check(chooseEntryHeight(manyFiles, phoneHeight) == minimumTappableHeight);
    check(chooseEntryHeight(exactlyFittingFiles, exactViewHeight)
          == roomyTappableHeight);
};

auto tPages =
    test("NarrowLayout/pages: confirm over comment over files or diff") = []
{
    check(findNarrowPage({.isFileListFocused = true}) == NarrowPage::files);
    check(findNarrowPage({}) == NarrowPage::diff);
    check(findNarrowPage({.isEditingComment = true, .isFileListFocused = true})
          == NarrowPage::comment);
    check(findNarrowPage({.isConfirming = true, .isEditingComment = true})
          == NarrowPage::confirm);
};

auto tStackSpans = test("NarrowLayout/spans stack their heights") = []
{
    auto spans = stackSpans({2, 3, 1});

    check(spans.size() == 3);
    check(spans[0].first == 0 && spans[0].height == 2);
    check(spans[1].first == 2 && spans[1].height == 3);
    check(spans[2].first == 5);
    check(getTotalHeight(spans) == 6);
    check(getTotalHeight({}) == 0);
};

auto tClampScroll = test("NarrowLayout/scroll stays between top and last page") = []
{
    constexpr auto total = 100;
    constexpr auto view = 30;
    constexpr auto lastTop = total - view;

    check(clampScroll({-5, view}, total) == 0);
    check(clampScroll({lastTop + 10, view}, total) == lastTop);
    check(clampScroll({10, view}, total) == 10);
    check(clampScroll({10, view}, view - 1) == 0);
};

auto tReveal = test("NarrowLayout/revealing a row scrolls as little as it can") = []
{
    constexpr auto view = 10;

    check(scrollToReveal({5, view}, {7, 2}) == 5);
    check(scrollToReveal({5, view}, {2, 2}) == 2);
    check(scrollToReveal({5, view}, {14, 3}) == 7);
    check(scrollToReveal({0, view}, {20, 15}) == 20);
};

auto tHitTest = test("NarrowLayout/a tap lands on the target under it") = []
{
    constexpr auto headerRow = 0;
    constexpr auto bodyRow = 4;
    constexpr auto rowIndex = 17;

    auto targets = std::vector<TapTarget> {
        {{0, headerRow, phoneWidth, minimumTappableHeight}, TapAction::goBack},
        {{0, bodyRow, phoneWidth, roomyTappableHeight},
         TapAction::selectRow,
         rowIndex},
    };

    check(findTapTarget(targets, {3, headerRow + 1})->action == TapAction::goBack);
    check(findTapTarget(targets, {phoneWidth - 1, bodyRow + 2})->index == rowIndex);
    check(!findTapTarget(targets, {0, bodyRow - 1}));
    check(!findTapTarget(targets, {phoneWidth, bodyRow}));
    check(!findTapTarget(targets, {0, bodyRow + roomyTappableHeight}));
};

auto tButtons = test("NarrowLayout/buttons share the width with a gap") = []
{
    constexpr auto footerRow = 32;
    constexpr auto bar = ScreenArea {0, footerRow, phoneWidth, roomyTappableHeight};
    auto two = layOutButtons(2, bar);

    check(two.size() == 2);
    check(two[0].column == 0 && two[0].width == 22);
    check(two[1].column == 23 && two[1].width == 22);
    check(two[1].column + two[1].width == phoneWidth);
    check(two[0].row == footerRow && two[1].height == roomyTappableHeight);

    auto three = layOutButtons(3, bar);
    check(three[2].column + three[2].width == phoneWidth);
    check(!three[0].contains({three[0].width, footerRow}));
    check(three[1].contains({three[1].column, footerRow}));

    check(layOutButtons(0, bar).empty());
};
