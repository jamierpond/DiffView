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
