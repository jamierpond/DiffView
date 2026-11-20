#include <Text.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

auto tSplitLines = test("Text/splitLines ignores a trailing newline") = []
{
    check(splitLines("a\nb\n").size() == 2);
    check(splitLines("a\nb").size() == 2);
    check(splitLines("").empty());
    check(splitLines("\n\n").size() == 2);
};

auto tSplitNul = test("Text/splitNul reads -z output") = []
{
    auto parts = splitNul(std::string_view {"a b\0c\0", 6});

    check(parts.size() == 2);
    check(parts[0] == "a b" && parts[1] == "c");
};

auto tDisplayable =
    test("Text/displayable expands tabs and hides control bytes") = []
{
    check(displayable("\tx") == "    x");
    check(displayable("ab\tx") == "ab  x");
    check(displayable("a\x1b[31mb\r") == "a?[31mb?");
};

auto tWrap = test("Text/wrap breaks at spaces, then hard") = []
{
    auto pieces = wrap("the quick brown fox", 10);

    check(pieces.size() == 2);
    check(pieces[0] == "the quick");
    check(pieces[1] == "brown fox");

    check(wrap("abcdefghij", 4).size() == 3);
    check(wrap("", 4).size() == 1);
};

auto tWrapUtf8 = test("Text/wrap counts code points, not bytes") = []
{
    auto pieces = wrap("\xC3\xA9\xC3\xA9\xC3\xA9", 2);

    check(pieces.size() == 2);
    check(pieces[0] == "\xC3\xA9\xC3\xA9");
};

auto tWrapRanges = test("Text/findWrapRanges keeps every byte of the line") = []
{
    constexpr auto width = 10;
    auto line = std::string_view {"the quick brown fox"};
    auto ranges = findWrapRanges(line, width);

    check(ranges.size() == 2);
    check(line.substr(ranges[0].start, ranges[0].length) == "the quick ");
    check(line.substr(ranges[1].start, ranges[1].length) == "brown fox");

    auto hard = findWrapRanges("abcdefghij", 4);
    check(hard.size() == 3);
    check(hard[2].start == 8 && hard[2].length == 2);

    check(findWrapRanges("", 4).size() == 1);
};

auto tWrapRangesUtf8 = test("Text/findWrapRanges counts code points") = []
{
    auto line = std::string_view {"\xC3\xA9\xC3\xA9\xC3\xA9"};
    auto ranges = findWrapRanges(line, 2);

    check(ranges.size() == 2);
    check(ranges[0].length == 4 && ranges[1].start == 4);
};

auto tKeepLast = test("Text/keepLastCodepoints keeps the end of the text") = []
{
    check(keepLastCodepoints("Source/Render.cpp", 10) == "Render.cpp");
    check(keepLastCodepoints("ab", 5) == "ab");
    check(keepLastCodepoints("a\xC3\xA9", 1) == "\xC3\xA9");
};

auto tRemoveTerminalColourCodes =
    test("Text/removeTerminalColourCodes keeps the text and drops the colours") = []
{
    check(removeTerminalColourCodes("\x1b[1mdiff --git a/x b/x\x1b[m")
          == "diff --git a/x b/x");
    check(removeTerminalColourCodes("\x1b[32m+added\x1b[0m\n") == "+added\n");
    check(removeTerminalColourCodes("\x1b[38;5;208m-x") == "-x");
    check(removeTerminalColourCodes("plain\x1b") == "plain\x1b");
    check(removeTerminalColourCodes("unfinished \x1b[31") == "unfinished ");
};
