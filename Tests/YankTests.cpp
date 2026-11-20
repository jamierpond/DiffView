#include <Yank.h>

#include <NanoTest/NanoTest.h>

using namespace nano;
using namespace diffview;

namespace
{
FileDiff sample()
{
    return parseDiff(R"(diff --git a/f b/f
--- a/f
+++ b/f
@@ -1,2 +1,2 @@
-a
+b
 c
@@ -10,1 +10,1 @@
-x
+y
)")
        .front();
}

constexpr auto firstLineRow = 2;
constexpr auto contextLineRow = 4;
constexpr auto secondHunkHeaderRow = 5;
constexpr auto lastRow = 7;
} // namespace

auto tOneLine = test("Yank/one line is its source text and a newline") = []
{
    auto file = sample();
    auto rows = buildRows(file, {}, {}, 80);
    auto yanked = collectYankedLines(file, rows, contextLineRow, contextLineRow);

    check(yanked.text == "c\n");
    check(yanked.lineCount == 1);
};

auto tSkipsOtherRows =
    test("Yank/a range drops diff markers and skips headers and comments") = []
{
    auto file = sample();
    auto annotation = makeAnnotation(file, {0, 1}, 7, "a comment");
    auto rows = buildRows(file, {annotation}, {}, 80);
    auto yanked =
        collectYankedLines(file, rows, 0, static_cast<int>(rows.size()) - 1);

    check(yanked.text == "a\nb\nc\nx\ny\n");
    check(yanked.lineCount == 5);
};

auto tReversed = test("Yank/an anchor below the cursor yanks the same lines") = []
{
    auto file = sample();
    auto rows = buildRows(file, {}, {}, 80);

    check(collectYankedLines(file, rows, lastRow, firstLineRow).text
          == collectYankedLines(file, rows, firstLineRow, lastRow).text);
};

auto tNothing = test("Yank/a range with no line rows yanks nothing") = []
{
    auto file = sample();
    auto rows = buildRows(file, {}, {}, 80);
    auto yanked =
        collectYankedLines(file, rows, secondHunkHeaderRow, secondHunkHeaderRow);

    check(yanked.text.empty());
    check(yanked.lineCount == 0);
};

auto tOsc52 = test("Yank/OSC 52 wraps the base64 text for the clipboard") = []
{ check(makeOsc52CopySequence("hi\n") == "\033]52;c;aGkK\a"); };
