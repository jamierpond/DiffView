#include <IntraLine.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
DiffLine makeLine(LineKind kind, std::string text)
{
    auto line = DiffLine {};
    line.kind = kind;
    line.text = std::move(text);
    return line;
}

Hunk makeHunk(std::vector<DiffLine> lines)
{
    auto hunk = Hunk {};
    hunk.lines = std::move(lines);
    return hunk;
}
} // namespace

auto tMiddleSpan = test("IntraLine/the changed middle is one span per line") = []
{
    auto spans = findChangedSpans("int count = 1;", "int total = 12;");

    check(spans.has_value());
    check(spans->removed.start == 4 && spans->removed.length == 9);
    check(spans->added.start == 4 && spans->added.length == 10);
};

auto tInsertion = test("IntraLine/an insertion leaves an empty removed span") = []
{
    auto spans = findChangedSpans("foo(a)", "foo(a, b)");

    check(spans.has_value());
    check(spans->removed.start == 5 && spans->removed.length == 0);
    check(spans->added.start == 5 && spans->added.length == 3);
};

auto tSame = test("IntraLine/identical lines have no spans") = []
{ check(!findChangedSpans("same", "same").has_value()); };

auto tRewrite = test("IntraLine/a whole-line rewrite is not highlighted") = []
{ check(!findChangedSpans("return alpha;", "throw omega();").has_value()); };

auto tUtf8 = test("IntraLine/spans never split a code point") = []
{
    // é is C3 A9, è is C3 A8: they share a lead byte.
    auto spans = findChangedSpans("caf\xC3\xA9 bar", "caf\xC3\xA8 bar");

    check(spans.has_value());
    check(spans->removed.start == 3 && spans->removed.length == 2);
    check(spans->added.start == 3 && spans->added.length == 2);
};

auto tPairing = test("IntraLine/equal runs pair removed with added in order") = []
{
    auto hunk = makeHunk({makeLine(LineKind::context, "x"),
                          makeLine(LineKind::removed, "value = 1"),
                          makeLine(LineKind::removed, "other = 2"),
                          makeLine(LineKind::added, "value = 3"),
                          makeLine(LineKind::added, "other = 4"),
                          makeLine(LineKind::context, "y")});

    check(!findChangedSpanOfLine(hunk, 0).has_value());
    check(findChangedSpanOfLine(hunk, 1)->start == 8);
    check(findChangedSpanOfLine(hunk, 2)->start == 8);
    check(findChangedSpanOfLine(hunk, 3)->length == 1);
    check(findChangedSpanOfLine(hunk, 4)->start == 8);
};

auto tUnequal = test("IntraLine/unequal runs are not paired") = []
{
    auto hunk = makeHunk({makeLine(LineKind::removed, "value = 1"),
                          makeLine(LineKind::added, "value = 2"),
                          makeLine(LineKind::added, "value = 3")});

    check(!findChangedSpanOfLine(hunk, 0).has_value());
    check(!findChangedSpanOfLine(hunk, 1).has_value());
    check(!findChangedSpanOfLine(hunk, 2).has_value());
};

auto tLoneAdded = test("IntraLine/added lines after context are not paired") = []
{
    auto hunk = makeHunk({makeLine(LineKind::context, "value = 1"),
                          makeLine(LineKind::added, "value = 2")});

    check(!findChangedSpanOfLine(hunk, 1).has_value());
};
